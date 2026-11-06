#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/Editing.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

using namespace Gyo::Tools::VoxelMapEditor;
namespace Math = Engine::Math;

// Judgments W1 to W3 and W5 to W7 of the v8-tool-voxel-map-editor-window
// declaration (docs/tools/plans/v8/voxel-map-editor.md), and W5 to W7 of
// v8-tool-voxel-window-1 (docs/tools/plans/v8/voxel-map-window-goxel.md).

namespace {

constexpr Cell kCube{1, 0};

std::string Text(const GridDocument& document) { return WriteGrid(document); }

GridDocument TwoBlocks() {
    auto document = NewGrid();
    document.blocks.push_back({"red", 0, {200, 40, 40, 255}});
    return document;
}

std::filesystem::path Scratch(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("gyo_voxel_editing_" + name + "_" + std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
}

Ray RayOf(const Math::Vec3 origin, const Math::Vec3 direction) { return {origin, Math::Normalize(direction)}; }

} // namespace

TEST_CASE("W1 edits: place, erase and paint change only their cell") {
    EditSession session(TwoBlocks());
    REQUIRE(session.SetCell({2, 0, 3}, {1, 5}));
    REQUIRE(session.SetCell({2, 1, 3}, kCube));
    CHECK(session.Document().At({2, 0, 3}) == Cell{1, 5});
    CHECK(session.Document().At({2, 1, 3}) == kCube);
    CHECK(session.Document().At({3, 0, 3}) == Cell{});

    REQUIRE(session.PaintCell({2, 0, 3}, 2));
    CHECK(session.Document().At({2, 0, 3}) == Cell{2, 5});  // the orientation stays
    REQUIRE(session.PaintCell({9, 0, 9}, 2));
    CHECK(session.Document().At({9, 0, 9}) == Cell{});      // paint fills no empty cell

    REQUIRE(session.SetCell({2, 1, 3}, {}));
    CHECK(session.Document().At({2, 1, 3}) == Cell{});
    CHECK(session.Document().At({2, 0, 3}) == Cell{2, 5});
}

TEST_CASE("W1 edits: refused values and cells leave the document as it was") {
    EditSession session(TwoBlocks());
    REQUIRE(session.SetCell({0, 0, 0}, kCube));
    const auto before = Text(session.Document());
    const auto revision = session.Revision();
    CHECK_FALSE(session.SetCell({CellLimit, 0, 0}, kCube));
    CHECK_FALSE(session.SetCell({0, -CellLimit - 1, 0}, kCube));
    CHECK_FALSE(session.SetCell({1, 0, 0}, {3, 0}));   // no block 3
    CHECK_FALSE(session.SetCell({1, 0, 0}, {1, 24}));  // no orientation 24
    CHECK_FALSE(session.SetCell({1, 0, 0}, {0, 2}));   // empty with an orientation
    CHECK_FALSE(session.PaintCell({0, 0, 0}, 0));
    CHECK_FALSE(session.FillBox({0, 0, 0}, {3, 0, 3}, {7, 0}));
    CHECK_FALSE(session.FillBox({0, 0, 0}, {CellLimit, 0, 0}, kCube));
    CHECK(Text(session.Document()) == before);
    CHECK(session.Revision() == revision);
    // The last cell in range is accepted.
    CHECK(session.SetCell({CellLimit - 1, 0, -CellLimit}, kCube));
}

TEST_CASE("W1 edits: a box fills and clears every cell between two corners, in any order") {
    EditSession session(TwoBlocks());
    REQUIRE(session.FillBox({3, 1, -2}, {1, 0, 0}, {2, 4}));
    int filled = 0;
    for (int y = -1; y <= 2; ++y)
        for (int z = -3; z <= 1; ++z)
            for (int x = 0; x <= 4; ++x) {
                const bool inside = x >= 1 && x <= 3 && y >= 0 && y <= 1 && z >= -2 && z <= 0;
                CHECK(session.Document().At({x, y, z}) == (inside ? Cell{2, 4} : Cell{}));
                filled += inside ? 1 : 0;
            }
    CHECK(filled == 18);
    REQUIRE(session.FillBox({2, 0, -1}, {2, 1, -1}, {}));
    CHECK(session.Document().At({2, 0, -1}) == Cell{});
    CHECK(session.Document().At({2, 1, -1}) == Cell{});
    CHECK(session.Document().At({1, 0, -1}) == Cell{2, 4});
    // A box is one undo step.
    REQUIRE(session.Undo());
    CHECK(session.Document().At({2, 0, -1}) == Cell{2, 4});
}

TEST_CASE("W2 undo: a drag is one step and every step comes back byte for byte") {
    EditSession session(TwoBlocks());
    std::vector<std::string> states{Text(session.Document())};
    session.BeginStroke();
    REQUIRE(session.SetCell({0, 0, 0}, kCube));
    REQUIRE(session.SetCell({1, 0, 0}, kCube));
    REQUIRE(session.SetCell({0, 0, 0}, {2, 3}));  // the same cell twice in one drag
    session.EndStroke();
    states.push_back(Text(session.Document()));
    REQUIRE(session.SetCell({1, 0, 0}, {}));
    states.push_back(Text(session.Document()));
    REQUIRE(session.UpdateBlock(1, {"blue", 1, {20, 40, 200, 255}}));
    states.push_back(Text(session.Document()));

    for (std::size_t i = states.size() - 1; i > 0; --i) {
        REQUIRE(session.Undo());
        CHECK(Text(session.Document()) == states[i - 1]);
    }
    CHECK_FALSE(session.Undo());
    for (std::size_t i = 1; i < states.size(); ++i) {
        REQUIRE(session.Redo());
        CHECK(Text(session.Document()) == states[i]);
    }
    CHECK_FALSE(session.Redo());
}

TEST_CASE("W2 undo: a new edit clears redo; the saved state is tracked") {
    EditSession session(TwoBlocks());
    CHECK_FALSE(session.Modified());
    REQUIRE(session.SetCell({0, 0, 0}, kCube));
    CHECK(session.Modified());
    session.MarkSaved();
    CHECK_FALSE(session.Modified());
    REQUIRE(session.SetCell({1, 0, 0}, kCube));
    REQUIRE(session.Undo());
    CHECK_FALSE(session.Modified());
    CHECK(session.CanRedo());
    REQUIRE(session.Undo());
    CHECK(session.Modified());
    const auto beforeNew = Text(session.Document());
    REQUIRE(session.SetCell({5, 0, 0}, kCube));  // the saved state is no longer reachable
    CHECK_FALSE(session.CanRedo());
    CHECK_FALSE(session.Redo());
    CHECK(session.Modified());
    REQUIRE(session.Undo());
    CHECK(session.Modified());
    CHECK(Text(session.Document()) == beforeNew);
    // The step that was undone before the new edit is gone: one more undo is the first state.
    REQUIRE_FALSE(session.Undo());
    CHECK(Text(session.Document()) == Text(TwoBlocks()));
    // An edit that changes nothing is no step.
    const auto revision = session.Revision();
    REQUIRE(session.SetCell({9, 9, 9}, {}));
    CHECK(session.Revision() == revision);
}

TEST_CASE("W3 pick: along an axis, the first filled cell and the face it entered") {
    auto document = NewGrid();
    document.Set({3, 0, 0}, kCube);
    document.Set({5, 0, 0}, kCube);
    const auto hit = Pick(document, RayOf({0.5F, 0.5F, 0.5F}, {1, 0, 0}), 0);
    REQUIRE(hit);
    CHECK(*hit == PickHit{{3, 0, 0}, {-1, 0, 0}, false});
    CHECK(PlacementCell(*hit) == Vec3i{2, 0, 0});

    const auto back = Pick(document, RayOf({9.5F, 0.5F, 0.5F}, {-1, 0, 0}), 0);
    REQUIRE(back);
    CHECK(*back == PickHit{{5, 0, 0}, {1, 0, 0}, false});
    CHECK(PlacementCell(*back) == Vec3i{6, 0, 0});

    const auto down = Pick(document, RayOf({3.5F, 7.25F, 0.5F}, {0, -1, 0}), 0);
    REQUIRE(down);
    CHECK(*down == PickHit{{3, 0, 0}, {0, 1, 0}, false});
}

TEST_CASE("W3 pick: a diagonal ray, negative cells, an offset origin and cell size") {
    auto document = NewGrid();
    document.Set({-2, 0, -3}, kCube);
    // From (0.5, 0.5, 0.5) along (-2, 0, -3), in units of that vector: z = 0 at t = 1/6, x = 0 at 1/4,
    // z = -1 at 1/2, x = -1 at 3/4, z = -2 at 5/6. Cells: (0,0,0) (0,0,-1) (-1,0,-1) (-1,0,-2) (-2,0,-2)
    // (-2,0,-3), the last entered through its +z face.
    const auto hit = Pick(document, RayOf({0.5F, 0.5F, 0.5F}, {-2, 0, -3}), 0);
    REQUIRE(hit);
    CHECK(*hit == PickHit{{-2, 0, -3}, {0, 0, 1}, false});

    document.origin = {10, 0, -4};
    document.cellSize = 2.0F;
    // Cell (-2, 0, -3) is world [16, 18] x [0, 2] x [-14, -12].
    const auto scaled = Pick(document, RayOf({17.0F, 9.0F, -13.0F}, {0, -1, 0}), 0);
    REQUIRE(scaled);
    CHECK(*scaled == PickHit{{-2, 0, -3}, {0, 1, 0}, false});
}

TEST_CASE("W3 pick: from inside a cell, and onto the edit plane when nothing is hit") {
    auto document = NewGrid();
    document.Set({1, 1, 1}, kCube);
    const auto inside = Pick(document, RayOf({1.5F, 1.5F, 1.5F}, {0.3F, -1, 0.2F}), 0);
    REQUIRE(inside);
    CHECK(*inside == PickHit{{1, 1, 1}, {0, 0, 0}, false});

    // Down onto the plane y = 2: its top face is at y = 3.
    const auto plane = Pick(document, RayOf({4.5F, 5.0F, -0.5F}, {1, -2, 0}), 2);
    REQUIRE(plane);
    CHECK(*plane == PickHit{{5, 2, -1}, {0, 0, 0}, true});
    CHECK(PlacementCell(*plane) == Vec3i{5, 2, -1});
    // Up from below meets the plane's bottom (y = 2): x = 4.5 + 1.5.
    const auto up = Pick(document, RayOf({4.5F, -1.0F, 0.5F}, {1, 2, 0}), 2);
    REQUIRE(up);
    CHECK(*up == PickHit{{6, 2, 0}, {0, 0, 0}, true});
    // Away from the plane, or along it: nothing.
    CHECK_FALSE(Pick(document, RayOf({4.5F, 5.0F, 0.5F}, {0, 1, 0}), 2));
    CHECK_FALSE(Pick(document, RayOf({4.5F, 5.0F, 0.5F}, {1, 0, 0}), 2));
}

TEST_CASE("W3 pick: a cell projected to a pixel is picked back through that pixel") {
    CameraPose camera;
    camera.position = {6.0F, 9.0F, -14.0F};
    camera.rotationRadians = {0.55F, -0.35F, 0.0F};
    camera.verticalFieldOfViewRadians = 0.9F;
    camera.nearClip = 0.1F;
    camera.farClip = 200.0F;
    constexpr int width = 1280;
    constexpr int height = 720;
    int checked = 0;
    for (int x = -4; x <= 4; x += 2)
        for (int z = -4; z <= 4; z += 2)
            for (int y = 0; y <= 2; ++y) {
                auto document = NewGrid();
                document.Set({x, y, z}, kCube);
                const Math::Vec3 centre = CellCorner(document, {x, y, z}) + Math::Vec3{0.5F, 0.5F, 0.5F};
                const auto pixel = ProjectToPixel(camera, width, height, centre);
                REQUIRE(pixel);
                if (pixel->x < 0 || pixel->y < 0 || pixel->x >= width || pixel->y >= height) continue;
                const auto ray = PixelRay(camera, width, height, pixel->x, pixel->y);
                const auto toCentre = Math::Normalize(centre - ray.origin);
                CHECK(Math::Dot(toCentre, ray.direction) > 0.99999F);
                const auto hit = Pick(document, ray, -100);
                REQUIRE(hit);
                CHECK(hit->cell == Vec3i{x, y, z});
                ++checked;
            }
    CHECK(checked > 40);
    // The image centre looks along the camera's +Z.
    CameraPose straight;
    const auto ray = PixelRay(straight, 640, 480, 319.5F, 239.5F);
    CHECK(std::abs(ray.direction.z - 1.0F) < 1e-5F);
    // +X is to the right and +Y up on the image.
    const auto right = ProjectToPixel(straight, 640, 480, {1.0F, 0.0F, 5.0F});
    const auto up = ProjectToPixel(straight, 640, 480, {0.0F, 1.0F, 5.0F});
    REQUIRE(right);
    REQUIRE(up);
    CHECK(right->x > 320.0F);
    CHECK(up->y < 240.0F);
}

TEST_CASE("W7 palette: blocks in use stay, the rest renumber, limits hold") {
    EditSession session(TwoBlocks());
    REQUIRE(session.AddBlock({"green", 1, {40, 200, 40, 255}}));
    REQUIRE(session.SetCell({0, 0, 0}, {3, 2}));
    REQUIRE(session.SetCell({1, 0, 0}, kCube));
    const auto before = Text(session.Document());
    CHECK_FALSE(session.RemoveBlock(2));  // green is in use
    CHECK_FALSE(session.RemoveBlock(0));  // block is in use
    CHECK_FALSE(session.RemoveBlock(3));  // no such block
    CHECK_FALSE(session.AddBlock({"red", 0, {1, 2, 3, 255}}));        // the name repeats
    CHECK_FALSE(session.AddBlock({"Bad Name", 0, {1, 2, 3, 255}}));   // not a name
    CHECK_FALSE(session.AddBlock({"far", 9, {1, 2, 3, 255}}));        // no shape 9
    CHECK_FALSE(session.UpdateBlock(1, {"block", 0, {1, 2, 3, 255}})); // the name of another block
    CHECK(Text(session.Document()) == before);

    REQUIRE(session.RemoveBlock(1));  // red is unused; green moves down to 2
    CHECK(session.Document().blocks.size() == 2);
    CHECK(session.Document().At({0, 0, 0}) == Cell{2, 2});
    CHECK(session.Document().At({1, 0, 0}) == kCube);
    REQUIRE(session.Undo());
    CHECK(Text(session.Document()) == before);

    EditSession full(NewGrid());
    for (std::size_t i = 1; i < MaxBlocks; ++i) REQUIRE(full.AddBlock({"b" + std::to_string(i), 0, {1, 1, 1, 255}}));
    CHECK_FALSE(full.AddBlock({"one_more", 0, {1, 1, 1, 255}}));
}

TEST_CASE("W7 shapes: boxes within [0, 16], shapes in use stay") {
    EditSession session(NewGrid());
    REQUIRE(session.AddShape({"post", {{{6, 0, 6}, {10, 16, 10}}}}));
    const auto before = Text(session.Document());
    CHECK_FALSE(session.UpdateShape(3, {"post", {{{6, 0, 6}, {10, 17, 10}}}}));  // beyond 16
    CHECK_FALSE(session.UpdateShape(3, {"post", {{{6, 0, 6}, {6, 16, 10}}}}));   // min is not below max
    CHECK_FALSE(session.UpdateShape(3, {"post", {}}));                           // no boxes
    CHECK_FALSE(session.AddShape({"cube", {{{0, 0, 0}, {16, 16, 16}}}}));       // the name repeats
    CHECK_FALSE(session.RemoveShape(0));                                         // the block uses the cube
    CHECK(Text(session.Document()) == before);

    REQUIRE(session.AddBlock({"pillar", 3, {90, 90, 90, 255}}));
    CHECK_FALSE(session.RemoveShape(3));
    REQUIRE(session.RemoveShape(1));  // the slab: the post moves down to 2
    CHECK(session.Document().blocks[1].shape == 2);
    CHECK(session.Document().shapes[2].name == "post");
    REQUIRE(ParseGrid(Text(session.Document())));
}

TEST_CASE("W1 edits: a box changes at most 2^20 cells") {
    EditSession session(TwoBlocks());
    CHECK_FALSE(session.FillBox({0, 0, 0}, {1024, 0, 1024}, kCube));
    CHECK(session.Document().chunks.empty());
    CHECK_FALSE(session.CanUndo());
    REQUIRE(session.FillBox({0, 0, 0}, {255, 15, 255}, {}));  // exactly 2^20: nothing to clear, no step
    CHECK_FALSE(session.CanUndo());
}

TEST_CASE("W2 undo: a cell set and set back in one drag leaves no step") {
    EditSession session(TwoBlocks());
    session.BeginStroke();
    REQUIRE(session.SetCell({0, 0, 0}, kCube));
    REQUIRE(session.SetCell({0, 0, 0}, {}));
    session.EndStroke();
    CHECK_FALSE(session.CanUndo());
    CHECK_FALSE(session.Modified());
}

TEST_CASE("W2 undo: saving during a drag saves the drag's cells as one step") {
    EditSession session(TwoBlocks());
    session.BeginStroke();
    REQUIRE(session.SetCell({0, 0, 0}, kCube));
    session.MarkSaved();  // the window saves while the button is still down
    CHECK_FALSE(session.InStroke());
    CHECK_FALSE(session.Modified());
    REQUIRE(session.Undo());
    CHECK(session.Modified());  // the cell on disk is no longer shown
    REQUIRE(session.Redo());
    CHECK_FALSE(session.Modified());
}

TEST_CASE("W3 pick: the plane is met within maxCells, and broken rays pick nothing") {
    const auto document = NewGrid();
    // Nearly level: the plane's top (y = 1) is 1000 cells away along the ray.
    const auto far = RayOf({0.5F, 2.0F, 0.5F}, {1.0F, -0.001F, 0.0F});
    CHECK(Pick(document, far, 0, 2000));
    CHECK_FALSE(Pick(document, far, 0, 500));
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();
    CHECK_FALSE(Pick(document, Ray{{0.5F, 2.0F, 0.5F}, {0.0F, nan, 0.0F}}, 0));
    CHECK_FALSE(Pick(document, Ray{{nan, 2.0F, 0.5F}, {0.0F, -1.0F, 0.0F}}, 0));
    CHECK_FALSE(Pick(document, Ray{{inf, 2.0F, 0.5F}, {0.0F, -1.0F, 0.0F}}, 0));
    CHECK_FALSE(Pick(document, Ray{{3e9F, 2.0F, 0.5F}, {0.0F, -1.0F, 0.0F}}, 0));
}

// ---- v8-tool-voxel-window-1: saving the grid and its chunked map (W5) ----

namespace {

std::span<const std::byte> BytesOf(const std::string& text) { return std::as_bytes(std::span{text.data(), text.size()}); }

// Reads a saved map with Engine only, as a game would; the chunks in key order.
std::vector<Engine::VoxelMesh::VoxelChunkDocument> ReadMap(const std::filesystem::path& folder) {
    const auto index = Engine::VoxelMesh::ParseVoxelMap(ReadText(folder / "map.json"));
    REQUIRE(index);
    std::vector<Engine::VoxelMesh::VoxelChunkDocument> chunks;
    for (const auto& entry : index->regions) {
        const auto region = Engine::VoxelMesh::ParseVoxelRegion(ReadText(folder / entry.file), *index, entry.at);
        REQUIRE(region);
        chunks.insert(chunks.end(), region->chunks.begin(), region->chunks.end());
    }
    return chunks;
}

struct Opened final {
    OpenedEditor editor;
    EditSession session;
    MapPreview preview;
};

Opened OpenFor(const std::string& gridPath, const std::string& mapDirectory) {
    auto opened = OpenEditor(gridPath, mapDirectory);
    REQUIRE(opened);
    auto preview = MapPreview::Create(opened->document);
    REQUIRE(preview);
    EditSession session(opened->document);
    return {std::move(*opened), std::move(session), std::move(*preview)};
}

void Sync(Opened& o) { REQUIRE(o.preview.Apply(o.session.Document(), o.session.TakeChanges())); }

} // namespace

TEST_CASE("W5 save: the grid, then the map of the previewed chunks; verify-map's bytes; known bytes follow") {
    const auto folder = Scratch("save");
    const auto gridPath = (folder / "map.voxel_grid.json").string();
    const auto mapDir = (folder / "sub" / "map").string();
    auto o = OpenFor(gridPath, mapDir);
    CHECK_FALSE(o.editor.files.grid.known);
    CHECK_FALSE(o.editor.files.mapIndex.known);
    REQUIRE(o.session.FillBox({0, 0, 0}, {20, 0, 2}, kCube));
    REQUIRE(o.session.SetCell({1, 1, 1}, {1, 7}));
    Sync(o);
    const auto saved = SaveEditor(o.editor.files, o.session.Document(), o.preview);
    REQUIRE(saved);
    CHECK(saved->mapWritten);

    const auto gridText = ReadText(gridPath);
    CHECK(gridText == Text(o.session.Document()));
    // What verify-map does: the saved grid bakes to the saved files, byte for byte.
    const auto reread = ParseGrid(gridText);
    REQUIRE(reread);
    const auto baked = BakeMap(*reread);
    REQUIRE(baked);
    const auto text = WriteMap(*baked, BytesOf(gridText));
    REQUIRE(text);
    CHECK(ReadText(folder / "sub" / "map" / "map.json") == text->index);
    for (const auto& region : text->regions) CHECK(ReadText(folder / "sub" / "map" / region.file) == region.text);
    CHECK(ReadMap(folder / "sub" / "map").size() == baked->chunks.size());

    // A second save after another edit works: the known bytes followed the save.
    REQUIRE(o.session.SetCell({3, 1, 2}, kCube));
    Sync(o);
    REQUIRE(SaveEditor(o.editor.files, o.session.Document(), o.preview));
    CHECK(ReadText(gridPath) == Text(o.session.Document()));
    std::filesystem::remove_all(folder);
}

TEST_CASE("W5 save: a changed grid or map.json stops a checked save; overwrite replaces them") {
    const auto folder = Scratch("changed");
    const auto gridPath = (folder / "map.voxel_grid.json").string();
    const auto mapDir = (folder / "map").string();
    auto o = OpenFor(gridPath, mapDir);
    REQUIRE(o.session.SetCell({0, 0, 0}, kCube));
    Sync(o);
    REQUIRE(SaveEditor(o.editor.files, o.session.Document(), o.preview));
    const auto savedGrid = ReadText(gridPath);
    const auto savedIndex = ReadText(folder / "map" / "map.json");

    // Someone else rewrites map.json: nothing is written.
    WriteText(folder / "map" / "map.json", savedIndex + " ");
    REQUIRE(o.session.SetCell({1, 0, 0}, kCube));
    Sync(o);
    const auto refused = SaveEditor(o.editor.files, o.session.Document(), o.preview);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == VoxelMapErrorCode::FileChanged);
    CHECK(ReadText(gridPath) == savedGrid);
    CHECK(ReadText(folder / "map" / "map.json") == savedIndex + " ");

    // The user chooses to overwrite: both are replaced, and the next checked save works.
    REQUIRE(SaveEditor(o.editor.files, o.session.Document(), o.preview, SaveMode::Overwrite));
    CHECK(ReadText(gridPath) == Text(o.session.Document()));
    CHECK(ReadMap(folder / "map").size() == 1);
    REQUIRE(o.session.SetCell({2, 0, 0}, kCube));
    Sync(o);
    CHECK(SaveEditor(o.editor.files, o.session.Document(), o.preview));

    // A grid file that appears after opening a new one is not overwritten either.
    const auto other = (folder / "other.voxel_grid.json").string();
    auto fresh = OpenFor(other, (folder / "other").string());
    WriteText(other, "someone else's");
    CHECK(SaveEditor(fresh.editor.files, o.session.Document(), o.preview).error().code ==
          VoxelMapErrorCode::FileChanged);
    CHECK(ReadText(other) == "someone else's");
    CHECK_FALSE(std::filesystem::exists(folder / "other"));
    std::filesystem::remove_all(folder);
}

TEST_CASE("W5 save: when the grid cannot be written, the map is not written either") {
    const auto folder = Scratch("unwritable");
    WriteText(folder / "blocker", "a file, not a folder");
    const auto gridPath = (folder / "blocker" / "map.voxel_grid.json").string();
    auto o = OpenFor(gridPath, (folder / "map").string());
    REQUIRE(o.session.SetCell({0, 0, 0}, kCube));
    Sync(o);
    const auto refused = SaveEditor(o.editor.files, o.session.Document(), o.preview);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == VoxelMapErrorCode::WriteFailed);
    CHECK_FALSE(std::filesystem::exists(folder / "map"));
    std::filesystem::remove_all(folder);
}

TEST_CASE("W5 save: when the grid is written but the map is not, the next save does not see a change") {
    const auto folder = Scratch("mapfails");
    const auto gridPath = (folder / "map.voxel_grid.json").string();
    WriteText(folder / "blocker", "a file, not a folder");
    auto o = OpenFor(gridPath, (folder / "blocker" / "map").string());
    REQUIRE(o.session.SetCell({0, 0, 0}, kCube));
    Sync(o);
    const auto failed = SaveEditor(o.editor.files, o.session.Document(), o.preview);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code == VoxelMapErrorCode::WriteFailed);
    CHECK(ReadText(gridPath) == Text(o.session.Document()));
    REQUIRE(o.editor.files.grid.known);
    std::filesystem::remove(folder / "blocker");
    REQUIRE(SaveEditor(o.editor.files, o.session.Document(), o.preview));
    CHECK(std::filesystem::exists(folder / "blocker" / "map" / "map.json"));
    std::filesystem::remove_all(folder);
}

TEST_CASE("W5 save: a grid without filled cells saves the grid and no map") {
    const auto folder = Scratch("empty");
    const auto gridPath = (folder / "map.voxel_grid.json").string();
    auto o = OpenFor(gridPath, (folder / "map").string());
    CHECK_FALSE(o.preview.Map());
    const auto saved = SaveEditor(o.editor.files, o.session.Document(), o.preview);
    REQUIRE(saved);
    CHECK_FALSE(saved->mapWritten);
    CHECK(ReadText(gridPath) == Text(o.session.Document()));
    CHECK_FALSE(std::filesystem::exists(folder / "map"));
    std::filesystem::remove_all(folder);
}

TEST_CASE("W5 open: an existing grid is read, a broken one refused") {
    const auto folder = Scratch("open");
    const auto gridPath = (folder / "map.voxel_grid.json").string();
    auto document = TwoBlocks();
    document.Set({4, 2, 1}, {2, 9});
    WriteText(gridPath, Text(document));
    auto opened = OpenEditor(gridPath, (folder / "map").string());
    REQUIRE(opened);
    CHECK(Text(opened->document) == Text(document));
    CHECK(opened->files.grid.known);
    CHECK_FALSE(opened->files.mapIndex.known);
    WriteText(gridPath, "{}");
    CHECK(OpenEditor(gridPath, (folder / "map").string()).error().code == VoxelMapErrorCode::InvalidGrid);
    std::filesystem::remove_all(folder);
}

// ---- W6 and W7: what is drawn ----

namespace {

void CheckSameGeometry(const Engine::VoxelMesh::VoxelChunkGeometry& drawn, const Engine::VoxelMesh::VoxelChunkGeometry& expected) {
    REQUIRE(drawn.vertices.size() == expected.vertices.size());
    for (std::size_t i = 0; i < expected.vertices.size(); ++i) {
        const auto& a = drawn.vertices[i];
        const auto& b = expected.vertices[i];
        CHECK((std::memcmp(&a.position, &b.position, sizeof a.position) == 0 && std::memcmp(&a.uv, &b.uv, sizeof a.uv) == 0));
    }
    CHECK(drawn.indices == expected.indices);
}

// The chunks a preview has given so far, kept as a window keeps its meshes.
using Drawn = std::map<Vec3i, Engine::VoxelMesh::VoxelChunkGeometry>;

void Take(MapPreview& preview, Drawn& drawn, std::vector<std::byte>& palette) {
    auto changes = preview.TakeChanges();
    for (auto& [at, geometry] : changes.chunks) {
        if (geometry) drawn.insert_or_assign(at, std::move(*geometry));
        else drawn.erase(at);
    }
    if (changes.palette) palette = changes.palette->rgba8;
}

} // namespace

TEST_CASE("W6 preview: what is drawn is Engine's expansion of the saved text, chunk by chunk") {
    auto document = TwoBlocks();
    document.Set({0, 0, 0}, kCube);
    document.Set({1, 0, 0}, {2, 0});
    document.Set({0, 1, 0}, {1, 6});
    document.Set({17, 0, -3}, kCube);
    document.blocks[1].shape = 2;  // a stair
    auto preview = MapPreview::Create(document);
    REQUIRE(preview);
    Drawn drawn;
    std::vector<std::byte> palette;
    Take(*preview, drawn, palette);

    // The text saving writes, read back by Engine as the game reads it.
    const std::string gridText = Text(document);
    const auto text = WriteMap(*preview->Map(), BytesOf(gridText));
    REQUIRE(text);
    const auto index = Engine::VoxelMesh::ParseVoxelMap(text->index);
    REQUIRE(index);
    std::size_t chunks = 0;
    for (std::size_t r = 0; r < text->regions.size(); ++r) {
        const auto region = Engine::VoxelMesh::ParseVoxelRegion(text->regions[r].text, *index, index->regions[r].at);
        REQUIRE(region);
        for (const auto& chunk : region->chunks) {
            const Vec3i at{chunk.at.x, chunk.at.y, chunk.at.z};
            REQUIRE(drawn.contains(at));
            CheckSameGeometry(drawn.at(at), Engine::VoxelMesh::ExpandVoxelChunk(chunk, *index));
            ++chunks;
        }
    }
    CHECK(chunks == drawn.size());
    CHECK(palette == Engine::VoxelMesh::ExpandVoxelPalette(*index).rgba8);
    // No cells: nothing to draw.
    auto empty = MapPreview::Create(NewGrid());
    REQUIRE(empty);
    CHECK_FALSE(empty->Map());
    CHECK(empty->TakeChanges().chunks.empty());
}

TEST_CASE("W7 preview: after every edit, undo and redo, the drawn chunks are those of a whole bake (200 sequences)") {
    std::uint64_t state = 0x853C49E6748FEA9BULL;
    const auto next = [&state](const std::uint32_t bound) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::int32_t>((state >> 33) % bound);
    };
    for (int round = 0; round < 200; ++round) {
        auto document = TwoBlocks();
        document.blocks.push_back({"half", 1, {10, 20, 30, 255}});
        document.blocks.push_back({"steps", 2, {1, 2, 3, 255}});
        EditSession session(document);
        auto preview = MapPreview::Create(document);
        REQUIRE(preview);
        Drawn drawn;
        std::vector<std::byte> palette;
        Take(*preview, drawn, palette);
        for (int step = 0; step < 6; ++step) {
            const int action = next(10);
            if (action < 6) {
                session.BeginStroke();
                const int count = 1 + next(6);
                for (int i = 0; i < count; ++i) {
                    const Vec3i cell{-17 + next(34), next(18), -17 + next(34)};
                    const Cell value = next(3) == 0 ? Cell{} : Cell{static_cast<std::uint16_t>(1 + next(4)),
                                                                     static_cast<std::uint8_t>(next(24))};
                    REQUIRE(session.SetCell(cell, value));
                }
                session.EndStroke();
            } else if (action < 8) {
                static_cast<void>(session.Undo());
            } else if (action < 9) {
                static_cast<void>(session.Redo());
            } else {
                auto block = session.Document().blocks[0];
                block.color[0] = static_cast<std::uint8_t>(next(256));
                REQUIRE(session.UpdateBlock(0, block));
            }
            REQUIRE(preview->Apply(session.Document(), session.TakeChanges()));
            Take(*preview, drawn, palette);
            // Expected: a whole bake of the document now.
            auto whole = MapPreview::Create(session.Document());
            REQUIRE(whole);
            Drawn expected;
            std::vector<std::byte> expectedPalette;
            Take(*whole, expected, expectedPalette);
            REQUIRE_MESSAGE(drawn.size() == expected.size(), "round " << round << " step " << step);
            for (const auto& [at, geometry] : expected) {
                REQUIRE(drawn.contains(at));
                CheckSameGeometry(drawn.at(at), geometry);
            }
            // Without a map nothing is drawn, so the palette a window still holds does not matter.
            if (whole->Map()) CHECK(palette == expectedPalette);
            CHECK(static_cast<bool>(preview->Map()) == static_cast<bool>(whole->Map()));
        }
    }
}

