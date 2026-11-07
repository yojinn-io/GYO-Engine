#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/Tools.hpp"

#include <algorithm>
#include <set>
#include <vector>

using namespace Gyo::Tools::VoxelMapEditor;
namespace Math = Engine::Math;

// Judgments W1 to W3 of v8-tool-voxel-window-1 (docs/tools/plans/v8/voxel-map-window-goxel.md):
// the tools' cell sets and their application, on the CPU.

namespace {

constexpr Cell kStone{1, 0};

GridDocument Blocks() {
    auto document = NewGrid();
    document.blocks.push_back({"red", 0, {200, 40, 40, 255}});
    document.blocks.push_back({"steps", 2, {1, 2, 3, 255}});
    return document;
}

std::set<Vec3i> AsSet(const std::vector<Vec3i>& cells) { return {cells.begin(), cells.end()}; }

std::set<Vec3i> Filled(const GridDocument& document) {
    std::set<Vec3i> out;
    for (const auto& [at, chunk] : document.chunks)
        for (std::int32_t y = 0; y < ChunkSize; ++y)
            for (std::int32_t z = 0; z < ChunkSize; ++z)
                for (std::int32_t x = 0; x < ChunkSize; ++x)
                    if (chunk[CellIndex(x, y, z)].block != 0)
                        out.insert({at[0] * ChunkSize + x, at[1] * ChunkSize + y, at[2] * ChunkSize + z});
    return out;
}

Ray RayOf(const Math::Vec3 origin, const Math::Vec3 direction) { return {origin, Math::Normalize(direction)}; }

} // namespace

TEST_CASE("W1 brush: cube, sphere and cylinder stamps, odd and even sizes") {
    CHECK(BrushCells({4, 5, 6}, 1, BrushShape::Cube) == std::vector<Vec3i>{{4, 5, 6}});
    CHECK(BrushCells({4, 5, 6}, 1, BrushShape::Sphere) == std::vector<Vec3i>{{4, 5, 6}});
    // Size 2 is centred on the cell's +X +Y +Z corner: the cell and the seven after it.
    const auto two = AsSet(BrushCells({0, 0, 0}, 2, BrushShape::Cube));
    CHECK(two.size() == 8);
    CHECK(two.contains(Vec3i{1, 1, 1}));
    CHECK_FALSE(two.contains(Vec3i{-1, 0, 0}));
    CHECK(BrushCells({0, 0, 0}, 3, BrushShape::Cube).size() == 27);
    // A sphere of size 3 is the cube without its eight corners; a cylinder keeps the cube's height.
    const auto sphere = AsSet(BrushCells({0, 0, 0}, 3, BrushShape::Sphere));
    CHECK(sphere.size() == 19);
    CHECK_FALSE(sphere.contains(Vec3i{1, 1, 1}));
    CHECK(sphere.contains(Vec3i{1, 1, 0}));
    const auto cylinder = AsSet(BrushCells({0, 0, 0}, 3, BrushShape::Cylinder));
    CHECK(cylinder.size() == 27);
    const auto cylinder5 = AsSet(BrushCells({0, 0, 0}, 5, BrushShape::Cylinder));
    CHECK_FALSE(cylinder5.contains(Vec3i{2, 0, 2}));
    CHECK(cylinder5.contains(Vec3i{2, 2, 0}));
    CHECK(cylinder5.contains(Vec3i{2, -2, 1}));
    // Sizes are clamped to 1 to 16; results are sorted without repeats.
    CHECK(BrushCells({0, 0, 0}, 0, BrushShape::Cube).size() == 1);
    CHECK(BrushCells({0, 0, 0}, 99, BrushShape::Cube).size() == 16 * 16 * 16);
    const auto five = BrushCells({0, 0, 0}, 5, BrushShape::Sphere);
    CHECK(std::is_sorted(five.begin(), five.end()));
    CHECK(AsSet(five).size() == five.size());
}

TEST_CASE("W1 line and stroke: every step touches the last, a stroke leaves no gap") {
    const auto line = LineCells({0, 0, 0}, {5, 2, -3});
    REQUIRE(line.size() == 6);
    CHECK(line.front() == Vec3i{0, 0, 0});
    CHECK(line.back() == Vec3i{5, 2, -3});
    for (std::size_t i = 1; i < line.size(); ++i)
        for (int axis = 0; axis < 3; ++axis) CHECK(std::abs(line[i][axis] - line[i - 1][axis]) <= 1);
    CHECK(LineCells({2, 2, 2}, {2, 2, 2}) == std::vector<Vec3i>{{2, 2, 2}});
    const auto stroke = AsSet(StrokeCells({0, 0, 0}, {10, 0, 0}, 1, BrushShape::Cube));
    CHECK(stroke.size() == 11);
    CHECK(AsSet(StrokeCells({0, 0, 0}, {0, 0, 0}, 3, BrushShape::Sphere)) ==
          AsSet(BrushCells({0, 0, 0}, 3, BrushShape::Sphere)));
}

TEST_CASE("W1 shape: a box, the sphere and the cylinder inscribed in it, in any corner order") {
    CHECK(ShapeCells({0, 0, 0}, {2, 1, 3}, BrushShape::Cube).size() == 24);
    CHECK(ShapeCells({2, 1, 3}, {0, 0, 0}, BrushShape::Cube) == ShapeCells({0, 0, 0}, {2, 1, 3}, BrushShape::Cube));
    // A 3x3x3 box: the inscribed sphere has 7 cells (the centre and its six neighbours) ... plus the edges
    // whose centres are within the unit sphere scaled to the box: (1/3)^2 * 2 = 2/9 + ... see the count.
    const auto sphere = AsSet(ShapeCells({0, 0, 0}, {2, 2, 2}, BrushShape::Sphere));
    CHECK(sphere.contains(Vec3i{1, 1, 1}));
    CHECK(sphere.contains(Vec3i{0, 1, 1}));
    CHECK_FALSE(sphere.contains(Vec3i{0, 0, 0}));
    CHECK(sphere == AsSet(BrushCells({1, 1, 1}, 3, BrushShape::Sphere)));
    const auto cylinder = AsSet(ShapeCells({0, 0, 0}, {4, 1, 4}, BrushShape::Cylinder));
    CHECK_FALSE(cylinder.contains(Vec3i{0, 0, 0}));
    CHECK(cylinder.contains(Vec3i{0, 1, 2}));
    CHECK(cylinder.contains(Vec3i{2, 0, 2}));
    // Too large a box gives nothing.
    CHECK(ShapeCells({0, 0, 0}, {1023, 1023, 1023}, BrushShape::Cube).empty());
}

TEST_CASE("W1 modes: add sets, sub empties, paint changes only filled cells and keeps orientation") {
    EditSession session(Blocks());
    const std::vector<Vec3i> cells{{0, 0, 0}, {1, 0, 0}};
    REQUIRE(ApplyCells(session, cells, EditMode::Add, {3, 5}));
    CHECK(session.Document().At({0, 0, 0}) == Cell{3, 5});
    REQUIRE(ApplyCells(session, std::vector<Vec3i>{{0, 0, 0}, {2, 0, 0}}, EditMode::Paint, {2, 9}));
    CHECK(session.Document().At({0, 0, 0}) == Cell{2, 5});
    CHECK(session.Document().At({2, 0, 0}) == Cell{});
    REQUIRE(ApplyCells(session, cells, EditMode::Sub, {}));
    CHECK(Filled(session.Document()).empty());
    // One application is one undo step.
    CHECK(session.Undo());
    CHECK(Filled(session.Document()).size() == 2);
    // A value the grid cannot hold changes nothing.
    CHECK_FALSE(ApplyCells(session, cells, EditMode::Add, {9, 0}));
    CHECK_FALSE(ApplyCells(session, cells, EditMode::Add, {}));
    // Cells beyond the range are skipped, not refused.
    REQUIRE(ApplyCells(session, std::vector<Vec3i>{{1 << 17, 0, 0}, {5, 0, 0}}, EditMode::Add, kStone));
    CHECK(session.Document().At({5, 0, 0}) == kStone);
}

TEST_CASE("W1 laser: the cells along the ray, within the brush") {
    auto document = Blocks();
    const auto cells = LaserCells(document, RayOf({0.5F, 0.5F, -2.5F}, {0, 0, 1}), 1, 5);
    CHECK(cells == std::vector<Vec3i>{{0, 0, -3}, {0, 0, -2}, {0, 0, -1}, {0, 0, 0}, {0, 0, 1}, {0, 0, 2}});
    CHECK(LaserCells(document, RayOf({0.5F, 0.5F, 0.5F}, {1, 0, 0}), 3, 0).size() == 27);
}

TEST_CASE("W1 extrude: the connected uncovered face of a layer, outward and inward") {
    EditSession session(Blocks());
    // A 3x3 floor and one cube on it: the floor's top face is the eight cells without the cube.
    REQUIRE(session.FillBox({0, 0, 0}, {2, 0, 2}, kStone));
    REQUIRE(session.SetCell({1, 1, 1}, kStone));
    REQUIRE(session.SetCell({5, 0, 5}, kStone)); // another layer piece, not connected
    const auto face = ExtrudeFace(session.Document(), {0, 0, 0}, {0, 1, 0});
    CHECK(face.size() == 8);
    CHECK_FALSE(AsSet(face).contains(Vec3i{1, 0, 1}));
    CHECK(ExtrudeFace(session.Document(), {1, 0, 1}, {0, 1, 0}).empty()); // covered
    CHECK(ExtrudeFace(session.Document(), {0, 0, 0}, {1, 1, 0}).empty());  // not a unit normal
    REQUIRE(Extrude(session, face, {0, 1, 0}, 2));
    for (const auto& cell : face) {
        CHECK(session.Document().At({cell[0], 1, cell[2]}) == kStone);
        CHECK(session.Document().At({cell[0], 2, cell[2]}) == kStone);
    }
    CHECK(session.Document().At({1, 2, 1}) == Cell{});
    CHECK(session.Undo());
    REQUIRE(Extrude(session, face, {0, 1, 0}, -1));
    for (const auto& cell : face) CHECK(session.Document().At(cell) == Cell{});
    CHECK(session.Document().At({1, 0, 1}) == kStone);
    CHECK_FALSE(Extrude(session, face, {0, 2, 0}, 1));
}

TEST_CASE("W1 plane: through a picked face, on any axis") {
    CHECK(PlaneThrough({{3, 4, 5}, {0, 1, 0}, false}) == EditPlane{1, 5});
    CHECK(PlaneThrough({{3, 4, 5}, {-1, 0, 0}, false}) == EditPlane{0, 2});
    CHECK(PlaneThrough({{3, 4, 5}, {0, 0, 1}, false}) == EditPlane{2, 6});
    CHECK_FALSE(PlaneThrough({{3, 4, 5}, {0, 0, 0}, true}));
    CHECK_FALSE(PlaneThrough({{3, 4, 5}, {0, 0, 0}, false}));
}

TEST_CASE("W3 pick: snaps to the volume, to a plane on any axis, or to both") {
    auto document = Blocks();
    document.Set({0, 0, 5}, kStone);
    const auto ray = RayOf({0.5F, 0.5F, 0.5F}, {0, 0, 1});
    // Volume first: the cube, entered through its -Z face.
    const auto volume = Pick(document, ray, EditPlane{2, 9}, PickOptions{true, true});
    REQUIRE(volume);
    CHECK(volume->cell == Vec3i{0, 0, 5});
    CHECK(volume->normal == Vec3i{0, 0, -1});
    CHECK(PlacementCell(*volume) == Vec3i{0, 0, 4});
    // Plane only: the cube is passed, the plane x/y layer at z == 9 is met on its near side.
    const auto plane = Pick(document, ray, EditPlane{2, 9}, PickOptions{false, true});
    REQUIRE(plane);
    CHECK(plane->onPlane);
    CHECK(plane->cell == Vec3i{0, 0, 9});
    // Coming from beyond, the plane is met on its far side, in the same layer.
    const auto back = Pick(document, RayOf({0.5F, 0.5F, 20.5F}, {0, 0, -1}), EditPlane{2, 9}, PickOptions{false, true});
    REQUIRE(back);
    CHECK(back->cell == Vec3i{0, 0, 9});
    // Nothing to snap to.
    CHECK_FALSE(Pick(document, ray, EditPlane{2, 9}, PickOptions{false, false}));
    CHECK_FALSE(Pick(document, ray, EditPlane{1, 0}, PickOptions{false, true})); // parallel to the plane
    // The old form is the y plane with both snaps.
    CHECK(Pick(document, ray, 0) == Pick(document, ray, EditPlane{1, 0}, PickOptions{}));
}

TEST_CASE("W2 cancel: a drag put back leaves the document and the steps as they were") {
    EditSession session(Blocks());
    REQUIRE(session.SetCell({0, 0, 0}, kStone));
    const auto before = WriteGrid(session.Document());
    static_cast<void>(session.TakeChanges());
    session.BeginStroke();
    REQUIRE(ApplyCells(session, ShapeCells({0, 0, 0}, {3, 1, 2}, BrushShape::Cube), EditMode::Add, {2, 3}));
    REQUIRE(ApplyCells(session, std::vector<Vec3i>{{0, 0, 0}}, EditMode::Sub, {}));
    // A window takes the changes every frame, so the drag's cells are gone from them before Esc.
    static_cast<void>(session.TakeChanges());
    session.CancelStroke();
    CHECK(WriteGrid(session.Document()) == before);
    CHECK_FALSE(session.InStroke());
    // Esc reports the cells it put back, so a preview puts them back too.
    const auto changes = session.TakeChanges();
    CHECK(changes.cells.size() >= 24);
    CHECK(std::find(changes.cells.begin(), changes.cells.end(), Vec3i{3, 1, 2}) != changes.cells.end());
    // Undo still takes back the edit before the drag, not the drag.
    CHECK(session.Undo());
    CHECK(Filled(session.Document()).empty());
    CHECK_FALSE(session.Undo());
}

TEST_CASE("W3 pick: the plane wins when the ray meets it before a filled cell (a second floor over the first)") {
    auto document = Blocks();
    document.Set({0, 0, 0}, kStone); // the first floor
    const auto down = RayOf({0.5F, 10.5F, 0.5F}, {0, -1, 0});
    // The plane at y = 5 lies between the camera and the floor: Add goes onto the plane.
    const auto hit = Pick(document, down, EditPlane{1, 5}, PickOptions{true, true});
    REQUIRE(hit);
    CHECK(hit->onPlane);
    CHECK(hit->cell == Vec3i{0, 5, 0});
    // Below the floor, the floor is met first.
    const auto under = Pick(document, down, EditPlane{1, -3}, PickOptions{true, true});
    REQUIRE(under);
    CHECK_FALSE(under->onPlane);
    CHECK(under->cell == Vec3i{0, 0, 0});
    // A filled cell in the plane's own layer is the cell, not the plane.
    const auto same = Pick(document, down, EditPlane{1, 0}, PickOptions{true, true});
    REQUIRE(same);
    CHECK_FALSE(same->onPlane);
}

TEST_CASE("W1 limits: shapes, extrudes and lasers change at most 2^20 cells") {
    CHECK(ShapeCells({-2147483647, 0, 0}, {2147483647, 2147483647, 2147483647}, BrushShape::Cube).empty());
    CHECK(ShapeCells({0, 0, 0}, {1 << 20, 0, 0}, BrushShape::Sphere).empty());
    EditSession session(Blocks());
    REQUIRE(session.FillBox({0, 0, 0}, {1023, 0, 1}, kStone));
    std::vector<Vec3i> face;
    for (std::int32_t x = 0; x < 1024; ++x)
        for (std::int32_t z = 0; z < 2; ++z) face.push_back({x, 0, z});
    CHECK(Extrude(session, face, {0, 1, 0}, 512));  // 2^20 cells: allowed
    CHECK(session.Document().At({1023, 512, 1}) == kStone);
    CHECK(session.Undo());
    const auto refused = Extrude(session, face, {0, 1, 0}, 513);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == VoxelMapErrorCode::InvalidGrid);
    CHECK(LaserCells(Blocks(), RayOf({0.5F, 0.5F, 0.5F}, {1, 0.3F, 0.2F}), 16, 100000).size() <=
          static_cast<std::size_t>(MaxBoxCells) + 16 * 16 * 16);
}
