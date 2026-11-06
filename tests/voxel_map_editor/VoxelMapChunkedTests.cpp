#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/ChunkedMap.hpp"
#include "gyo/voxel_map_editor/MarkersFile.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace Gyo::Tools::VoxelMapEditor;
namespace VM = Engine::VoxelMesh;

// Judgments T1 to T4 of the v8-arch-big-map-tool-chunked declaration
// (docs/tools/plans/v8/voxel-map-tool-chunked.md).

namespace {

std::string ReadText(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file, path);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string ReadFixture(const std::string& name) { return ReadText(std::string(GYO_VOXEL_MAP_FIXTURES) + "/" + name); }

std::span<const std::byte> Bytes(const std::string& text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

GridDocument TestGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"stone", 0, {100, 100, 100, 255}}, {"half", 1, {10, 20, 30, 255}}, {"stairs", 2, {1, 2, 3, 255}}};
    return grid;
}

// A fixed linear congruential generator: the same sets on every platform.
struct Random final {
    std::uint64_t state;
    std::int32_t Next(const std::uint32_t bound) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::int32_t>((state >> 33) % bound);
    }
    Cell AnyCell() {
        const auto block = static_cast<std::uint16_t>(1 + Next(3));
        return {block, static_cast<std::uint8_t>(Next(24))};
    }
    // A cell near chunk borders (around -16, 0 and 16 on x and z, 0 to 17 on y).
    Vec3i NearBorders() {
        const auto around = [this](const std::int32_t centre) { return centre - 3 + Next(6); };
        const std::array<std::int32_t, 3> centres{-16, 0, 16};
        return {around(centres[static_cast<std::size_t>(Next(3))]), Next(18),
                around(centres[static_cast<std::size_t>(Next(3))])};
    }
};

// Three random shapes of 1 to 3 boxes anywhere in the cell (off the 8-unit
// lattice, overlapping), one block each.
void RandomShapes(GridDocument& grid, Random& random) {
    grid.shapes.clear();
    for (int s = 0; s < 3; ++s) {
        Shape shape{"s" + std::to_string(s), {}};
        const int boxes = 1 + random.Next(3);
        for (int b = 0; b < boxes; ++b) {
            Box box;
            for (int axis = 0; axis < 3; ++axis) {
                box.minimum[axis] = random.Next(16);
                box.maximum[axis] = box.minimum[axis] + 1 + random.Next(static_cast<std::uint32_t>(16 - box.minimum[axis]));
            }
            shape.boxes.push_back(box);
        }
        grid.shapes.push_back(shape);
    }
    for (std::size_t b = 0; b < grid.blocks.size(); ++b) grid.blocks[b].shape = static_cast<std::uint32_t>(b);
}

GridDocument RandomGrid(Random& random, const bool randomShapes = false) {
    auto grid = TestGrid();
    if (randomShapes) RandomShapes(grid, random);
    const int count = 20 + random.Next(80);
    for (int i = 0; i < count; ++i) grid.Set(random.NearBorders(), random.AnyCell());
    grid.Set({40, 0, 40}, {1, 0}); // never edited, so a grid never becomes empty
    return grid;
}

MapText Texts(const BakedMap& map) {
    const std::string source = "test";
    auto text = WriteMap(map, Bytes(source));
    REQUIRE_MESSAGE(text, (text ? std::string() : text.error().message + " " + text.error().detail));
    return std::move(text).value();
}

bool SameTexts(const MapText& a, const MapText& b) {
    if (a.index != b.index || a.regions.size() != b.regions.size()) return false;
    for (std::size_t i = 0; i < a.regions.size(); ++i)
        if (a.regions[i].file != b.regions[i].file || a.regions[i].text != b.regions[i].text) return false;
    return true;
}

// Every face square of a quad set on an 8-unit lattice (the default shapes'
// boxes all lie on it): (direction, plane, palette name, u, v), in map units.
using Square = std::tuple<int, std::int32_t, std::string, std::int32_t, std::int32_t>;
constexpr std::int32_t Step = 8;

void AddSquares(std::vector<Square>& squares, const VM::VoxelQuad& q, const Vec3i& offset, const std::string& name,
                const std::int32_t step = Step) {
    const auto dir = static_cast<int>(q.face);
    const int axis = dir / 2;
    const std::array<std::array<int, 2>, 3> inPlane{{{2, 1}, {0, 2}, {0, 1}}};
    const std::array<std::int32_t, 3> corner{q.x + offset[0], q.y + offset[1], q.z + offset[2]};
    const auto u = static_cast<std::size_t>(inPlane[static_cast<std::size_t>(axis)][0]);
    const auto v = static_cast<std::size_t>(inPlane[static_cast<std::size_t>(axis)][1]);
    REQUIRE(corner[u] % step == 0);
    REQUIRE(corner[v] % step == 0);
    REQUIRE(q.width % step == 0);
    REQUIRE(q.height % step == 0);
    for (auto a = corner[u]; a < corner[u] + q.width; a += step)
        for (auto b = corner[v]; b < corner[v] + q.height; b += step)
            squares.emplace_back(dir, corner[static_cast<std::size_t>(axis)], name, a, b);
}

// The faces the single file bakes for a grid (one quad per uncovered box side,
// no merging): an oracle built by other code.
std::vector<Square> SingleFileSquares(const GridDocument& grid, const std::int32_t step = Step) {
    const std::string source = "oracle";
    const auto built = BuildMesh(grid, Bytes(source));
    REQUIRE(built);
    const auto mesh = VM::ParseVoxelMesh(built->text);
    REQUIRE(mesh);
    std::vector<Square> squares;
    for (const auto& q : mesh->quads) AddSquares(squares, q, {0, 0, 0}, mesh->palette[q.palette].name, step);
    std::sort(squares.begin(), squares.end());
    squares.erase(std::unique(squares.begin(), squares.end()), squares.end());
    return squares;
}

Vec3i ChunkOffset(const VM::VoxelChunkKey& at) {
    return {at.x * ChunkSize * Units, at.y * ChunkSize * Units, at.z * ChunkSize * Units};
}

std::set<Vec3i> UnitBlocks(const std::vector<Box>& boxes) {
    std::set<Vec3i> units;
    for (const auto& b : boxes)
        for (auto x = b.minimum[0]; x < b.maximum[0]; x += Step)
            for (auto y = b.minimum[1]; y < b.maximum[1]; y += Step)
                for (auto z = b.minimum[2]; z < b.maximum[2]; z += Step) units.insert({x, y, z});
    return units;
}

std::int64_t Volume(const Box& b) {
    return std::int64_t{b.maximum[0] - b.minimum[0]} * (b.maximum[1] - b.minimum[1]) * (b.maximum[2] - b.minimum[2]);
}

std::filesystem::path Work(const std::string& name) {
    const auto path = std::filesystem::path(GYO_VOXEL_MAP_WORK) / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::set<std::string> RegionFilesIn(const std::filesystem::path& directory) {
    std::set<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(directory / "regions"))
        names.insert(entry.path().filename().string());
    return names;
}

// Reads a saved map with Engine only, as a game would.
std::vector<VM::VoxelChunkDocument> ReadSaved(const std::filesystem::path& directory) {
    const auto index = VM::ParseVoxelMap(ReadText((directory / "map.json").string()));
    REQUIRE(index);
    std::vector<VM::VoxelChunkDocument> chunks;
    for (const auto& entry : index->regions) {
        const auto region = VM::ParseVoxelRegion(ReadText((directory / entry.file).string()), *index, entry.at);
        REQUIRE_MESSAGE(region, entry.file);
        chunks.insert(chunks.end(), region->chunks.begin(), region->chunks.end());
    }
    return chunks;
}

} // namespace

// ---- T2: determinism ----

TEST_CASE("map: the sample bakes to the golden map, twice the same") {
    const auto source = ReadFixture("sample.voxel_grid.json");
    const auto grid = ParseGrid(source);
    REQUIRE(grid);
    const auto first = BakeMap(*grid);
    const auto second = BakeMap(*grid);
    REQUIRE(first);
    REQUIRE(second);
    const auto a = WriteMap(*first, Bytes(source));
    const auto b = WriteMap(*second, Bytes(source));
    REQUIRE(a);
    REQUIRE(b);
    CHECK(SameTexts(*a, *b));
    CHECK(a->index == ReadFixture("sample_map/map.json"));
    REQUIRE(a->regions.size() == 2);
    for (const auto& region : a->regions) CHECK(region.text == ReadFixture("sample_map/" + region.file));
}

TEST_CASE("map: a rebake of the touched chunks equals a whole bake (200 edit sequences)") {
    Random random{0x2545F4914F6CDD1DULL};
    for (int round = 0; round < 200; ++round) {
        auto grid = RandomGrid(random);
        auto map = BakeMap(grid);
        REQUIRE(map);
        for (int step = 0; step < 4; ++step) {
            std::vector<Vec3i> cells;
            const int count = 1 + random.Next(8);
            for (int i = 0; i < count; ++i) {
                const auto cell = random.NearBorders();
                grid.Set(cell, random.Next(3) == 0 ? Cell{} : random.AnyCell());
                cells.push_back(cell);
            }
            const auto touched = ChunksTouchedBy(cells);
            const std::vector<Vec3i> keys(touched.begin(), touched.end());
            REQUIRE(RebakeChunks(grid, *map, keys));
            const auto whole = BakeMap(grid);
            REQUIRE(whole);
            CHECK_MESSAGE(SameTexts(Texts(*map), Texts(*whole)), "round " << round << " step " << step);
        }
    }
}

TEST_CASE("map: a palette change rebakes everything") {
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {1, 0});
    grid.Set({20, 0, 0}, {2, 0});
    auto map = BakeMap(grid);
    REQUIRE(map);
    grid.blocks[0].color = {9, 9, 9, 255};
    const std::vector<Vec3i> none;
    REQUIRE(RebakeChunks(grid, *map, none));
    const auto whole = BakeMap(grid);
    REQUIRE(whole);
    CHECK(SameTexts(Texts(*map), Texts(*whole)));
}

TEST_CASE("map: the chunks a change touches") {
    const std::vector<Vec3i> corner{{0, 0, 0}};
    CHECK(ChunksTouchedBy(corner) == std::set<Vec3i>{{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 0}});
    const std::vector<Vec3i> farSide{{15, 5, 15}};
    CHECK(ChunksTouchedBy(farSide) == std::set<Vec3i>{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}});
    const std::vector<Vec3i> negative{{-1, 5, 7}};
    CHECK(ChunksTouchedBy(negative) == std::set<Vec3i>{{-1, 0, 0}, {0, 0, 0}});
    const std::vector<Vec3i> inside{{-24, 7, 40}};
    CHECK(ChunksTouchedBy(inside) == std::set<Vec3i>{{-2, 0, 2}});
}

// ---- T3: lossless ----

TEST_CASE("map: greedy keeps every face square once (200 random grids against the single file)") {
    Random random{0x9E3779B97F4A7C15ULL};
    for (int round = 0; round < 200; ++round) {
        const auto grid = RandomGrid(random);
        const auto map = BakeMap(grid);
        REQUIRE(map);
        std::vector<Square> squares;
        for (const auto& [at, chunk] : map->chunks)
            for (const auto& q : chunk.quads)
                AddSquares(squares, q, ChunkOffset(chunk.at), map->index.palette[q.palette].name);
        std::sort(squares.begin(), squares.end());
        const auto unique = std::unique(squares.begin(), squares.end()) == squares.end();
        CHECK_MESSAGE(unique, "round " << round << ": two quads overlap");
        CHECK_MESSAGE(squares == SingleFileSquares(grid), "round " << round);
    }
}

TEST_CASE("map: each chunk's boxes are disjoint and fill its blocks (200 random grids)") {
    Random random{0xD1B54A32D192ED03ULL};
    for (int round = 0; round < 200; ++round) {
        const auto grid = RandomGrid(random);
        const auto map = BakeMap(grid);
        REQUIRE(map);
        for (const auto& [at, chunk] : map->chunks) {
            std::vector<Box> boxes;
            std::int64_t volume = 0;
            for (const auto& c : chunk.colliders) {
                boxes.push_back({{c.minimum.x, c.minimum.y, c.minimum.z}, {c.maximum.x, c.maximum.y, c.maximum.z}});
                volume += Volume(boxes.back());
            }
            std::vector<Box> expected;
            const auto& cells = grid.chunks.at(at);
            for (std::int32_t y = 0; y < ChunkSize; ++y)
                for (std::int32_t z = 0; z < ChunkSize; ++z)
                    for (std::int32_t x = 0; x < ChunkSize; ++x) {
                        const auto& cell = cells[CellIndex(x, y, z)];
                        if (cell.block == 0) continue;
                        for (const auto& part : grid.shapes[grid.blocks[cell.block - 1u].shape].boxes) {
                            auto turned = RotateBox(part, cell.orientation);
                            for (int axis = 0; axis < 3; ++axis) {
                                const auto offset = std::array<std::int32_t, 3>{x, y, z}[static_cast<std::size_t>(axis)] * Units;
                                turned.minimum[axis] += offset;
                                turned.maximum[axis] += offset;
                            }
                            expected.push_back(turned);
                        }
                    }
            const auto units = UnitBlocks(boxes);
            CHECK(units == UnitBlocks(expected));
            CHECK_MESSAGE(volume == static_cast<std::int64_t>(units.size()) * Step * Step * Step,
                          "round " << round << ": boxes overlap");
        }
    }
}

TEST_CASE("map: cullface and greedy across a chunk border") {
    auto grid = TestGrid();
    // Three cubes in a row, the last across the border at x = 16.
    grid.Set({14, 0, 0}, {1, 0});
    grid.Set({15, 0, 0}, {1, 0});
    grid.Set({16, 0, 0}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    REQUIRE(map->chunks.size() == 2);
    const auto& left = map->chunks.at({0, 0, 0});
    const auto& right = map->chunks.at({1, 0, 0});
    // Left: no +X face on the border; one merged face per other side.
    CHECK(left.quads.size() == 5);
    for (const auto& q : left.quads) CHECK(q.face != VM::VoxelFace::PositiveX);
    const auto top = std::find_if(left.quads.begin(), left.quads.end(),
                                  [](const VM::VoxelQuad& q) { return q.face == VM::VoxelFace::PositiveY; });
    REQUIRE(top != left.quads.end());
    CHECK(top->x == 224);
    CHECK(top->y == 16);
    CHECK(top->width == 32);
    CHECK(top->height == 16);
    // Right: no -X face on the border.
    CHECK(right.quads.size() == 5);
    for (const auto& q : right.quads) CHECK(q.face != VM::VoxelFace::NegativeX);
    CHECK(left.colliders.size() == 1);
    CHECK(right.colliders.size() == 1);
}

// ---- T1: one validator ----

TEST_CASE("map: an empty grid has no map") {
    const auto map = BakeMap(TestGrid());
    REQUIRE_FALSE(map);
    CHECK(map.error().code == VoxelMapErrorCode::EmptyMap);
}

TEST_CASE("map: an index Engine refuses is not returned") {
    // The grid allows the cell, but its chunk's far side ends past Engine's
    // +-2^20 units (rule M6), because the origin is not on the chunk grid.
    auto grid = TestGrid();
    grid.origin = {8, 0, 0};
    grid.Set({(1 << 16) - 9, 0, 0}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE_FALSE(map);
    CHECK(map.error().code == VoxelMapErrorCode::BuildRefused);
}

TEST_CASE("map: a region Engine refuses is not returned") {
    // Checkerboards in 49 chunks of one region: more than the 16 MiB a region may hold (rule R5).
    auto grid = TestGrid();
    for (std::int32_t cx = 0; cx < 7; ++cx)
        for (std::int32_t cz = 0; cz < 7; ++cz)
            for (std::int32_t y = 0; y < ChunkSize; ++y)
                for (std::int32_t z = 0; z < ChunkSize; ++z)
                    for (std::int32_t x = 0; x < ChunkSize; ++x)
                        if ((x + y + z) % 2 == 0) grid.Set({cx * ChunkSize + x, y, cz * ChunkSize + z}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const std::string source = "big";
    const auto text = WriteMap(*map, Bytes(source));
    REQUIRE_FALSE(text);
    CHECK(text.error().code == VoxelMapErrorCode::BuildRefused);
    CHECK(text.error().message.find("regions/r_0_0_") != std::string::npos);
}

// ---- T4: saving ----

TEST_CASE("map: region file names") {
    CHECK(IsRegionFileName("r_0_0_0123456789abcdef.json"));
    CHECK(IsRegionFileName("r_-12_7_0123456789abcdef.json"));
    CHECK_FALSE(IsRegionFileName("r_0_0.json"));
    CHECK_FALSE(IsRegionFileName("r_0_0_0123456789ABCDEF.json"));
    CHECK_FALSE(IsRegionFileName("r_0_0_0123456789abcde.json"));
    CHECK_FALSE(IsRegionFileName("r__0_0123456789abcdef.json"));
    CHECK_FALSE(IsRegionFileName("r_-_0_0123456789abcdef.json"));
    CHECK_FALSE(IsRegionFileName("x_0_0_0123456789abcdef.json"));
    CHECK_FALSE(IsRegionFileName("r_0_0_0123456789abcdef.json.tmp"));
}

TEST_CASE("map: a save stopped before the index leaves the previous map readable") {
    const auto directory = Work("save");
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {1, 0});
    grid.Set({200, 0, 0}, {2, 0});
    const auto before = BakeMap(grid);
    REQUIRE(before);
    const auto beforeText = Texts(*before);
    REQUIRE(SaveMap(directory.string(), beforeText));
    const auto saved = ReadSaved(directory);

    grid.Set({0, 0, 0}, {3, 4});
    const auto after = BakeMap(grid);
    REQUIRE(after);
    const auto afterText = Texts(*after);
    // Stopped after the region files: the old index and its regions still read, unchanged.
    REQUIRE(WriteRegionFiles(directory.string(), afterText));
    CHECK(ReadText((directory / "map.json").string()) == beforeText.index);
    const auto still = ReadSaved(directory);
    CHECK(still.size() == saved.size());
    // Only files the previous index listed are removed: not other files, not
    // a region file no index listed.
    { std::ofstream(directory / "regions" / "notes.txt") << "x"; }
    { std::ofstream(directory / "regions" / "r_9_9_0123456789abcdef.json") << "x"; }
    const auto previous = ListedRegionFiles(directory.string());
    CHECK(previous.size() == beforeText.regions.size());
    REQUIRE(ReplaceMapIndex(directory.string(), afterText));
    REQUIRE(RemoveStaleRegionFiles(directory.string(), afterText, previous));
    std::set<std::string> expected{"notes.txt", "r_9_9_0123456789abcdef.json"};
    for (const auto& region : afterText.regions) expected.insert(region.file.substr(std::string("regions/").size()));
    CHECK(RegionFilesIn(directory) == expected);
    CHECK(ReadSaved(directory).size() == after->chunks.size());
    // A file the old index listed but that is already gone is fine.
    REQUIRE(RemoveStaleRegionFiles(directory.string(), afterText, previous));
    // A file an index lists that does not have the region file form stays.
    { std::ofstream(directory / "regions" / "kept.json") << "x"; }
    { std::ofstream(directory / "outside.json") << "x"; }
    const std::vector<std::string> foreign{"regions/kept.json", "outside.json", "regions/../outside.json"};
    REQUIRE(RemoveStaleRegionFiles(directory.string(), afterText, foreign));
    CHECK(std::filesystem::exists(directory / "regions" / "kept.json"));
    CHECK(std::filesystem::exists(directory / "outside.json"));
}

TEST_CASE("map: saving the same map twice changes no file") {
    const auto directory = Work("twice");
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const auto text = Texts(*map);
    REQUIRE(SaveMap(directory.string(), text));
    const auto region = directory / text.regions.front().file;
    const auto written = std::filesystem::last_write_time(region);
    REQUIRE(SaveMap(directory.string(), text));
    CHECK(std::filesystem::last_write_time(region) == written);
}

// ---- Wider shapes (review of tc-bake, item 8): random boxes off the 8-unit lattice ----

TEST_CASE("map: greedy keeps every face square once with random shapes (100 grids, unit squares)") {
    Random random{0x6A09E667F3BCC909ULL};
    for (int round = 0; round < 100; ++round) {
        const auto grid = RandomGrid(random, true);
        const auto map = BakeMap(grid);
        REQUIRE(map);
        std::vector<Square> squares;
        for (const auto& [at, chunk] : map->chunks)
            for (const auto& q : chunk.quads)
                AddSquares(squares, q, ChunkOffset(chunk.at), map->index.palette[q.palette].name, 1);
        std::sort(squares.begin(), squares.end());
        const auto unique = std::unique(squares.begin(), squares.end()) == squares.end();
        CHECK_MESSAGE(unique, "round " << round << ": two quads overlap");
        CHECK_MESSAGE(squares == SingleFileSquares(grid, 1), "round " << round);
    }
}

TEST_CASE("map: a rebake of the touched chunks equals a whole bake with random shapes (100 edit sequences)") {
    Random random{0xBB67AE8584CAA73BULL};
    for (int round = 0; round < 100; ++round) {
        auto grid = RandomGrid(random, true);
        auto map = BakeMap(grid);
        REQUIRE(map);
        for (int step = 0; step < 4; ++step) {
            std::vector<Vec3i> cells;
            const int count = 1 + random.Next(8);
            for (int i = 0; i < count; ++i) {
                const auto cell = random.NearBorders();
                grid.Set(cell, random.Next(3) == 0 ? Cell{} : random.AnyCell());
                cells.push_back(cell);
            }
            const auto touched = ChunksTouchedBy(cells);
            const std::vector<Vec3i> keys(touched.begin(), touched.end());
            REQUIRE(RebakeChunks(grid, *map, keys));
            const auto whole = BakeMap(grid);
            REQUIRE(whole);
            CHECK_MESSAGE(SameTexts(Texts(*map), Texts(*whole)), "round " << round << " step " << step);
        }
    }
}

TEST_CASE("map: a shape edit or a block's new shape rebakes everything") {
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {2, 0});
    grid.Set({40, 0, 0}, {2, 0});
    auto map = BakeMap(grid);
    REQUIRE(map);
    const std::vector<Vec3i> none;
    grid.shapes[1].boxes = {{{0, 0, 0}, {16, 4, 16}}};
    REQUIRE(RebakeChunks(grid, *map, none));
    auto whole = BakeMap(grid);
    REQUIRE(whole);
    CHECK(SameTexts(Texts(*map), Texts(*whole)));
    grid.blocks[1].shape = 2;
    REQUIRE(RebakeChunks(grid, *map, none));
    whole = BakeMap(grid);
    REQUIRE(whole);
    CHECK(SameTexts(Texts(*map), Texts(*whole)));
}

TEST_CASE("map: a chunk Engine refuses is not kept") {
    // A document no grid file can hold (a box below its cell): Engine's
    // ValidateVoxelChunk is what refuses it (rule C4), in BakeMap and in RebakeChunks.
    auto grid = TestGrid();
    grid.shapes.push_back({"bad", {{{1, 1, -1}, {15, 15, 15}}}});
    grid.blocks.push_back({"bad", 3, {5, 5, 5, 255}});
    grid.Set({20, 0, 0}, {1, 0});
    grid.Set({0, 0, 0}, {4, 0});
    const auto refused = BakeMap(grid);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == VoxelMapErrorCode::BuildRefused);
    CHECK(refused.error().message.find("a chunk") != std::string::npos);

    grid.Set({0, 0, 0}, {});
    auto map = BakeMap(grid);
    REQUIRE(map);
    const auto before = Texts(*map);
    grid.Set({0, 0, 0}, {4, 0});
    const std::vector<Vec3i> keys{{0, 0, 0}};
    const auto rebaked = RebakeChunks(grid, *map, keys);
    REQUIRE_FALSE(rebaked);
    CHECK(rebaked.error().code == VoxelMapErrorCode::BuildRefused);
    CHECK(SameTexts(Texts(*map), before));
}

// ---- T6: markers through Engine (tc-markers) ----

namespace {
namespace MM = Engine::MapMarkers;

MM::MapMarkersDocument SampleMarkers() {
    MM::MapMarkersDocument document;
    document.markers.push_back({"m1", "a", {}, MM::MarkerPoint{{0, 16, 0}, 0.1F}});
    document.markers.push_back({"m2", "b", {{"k", "v"}, {"x", "q \"r\" \\s"}},
                                MM::MarkerBox{{-32, 0, -32}, {32, 48, 32}}});
    document.markers.push_back({"m3", "c", {}, MM::MarkerPath{{{0, 0, 0}, {16, 0, 0}, {16, 0, 16}}}});
    document.markers.push_back({"m4", "a", {}, MM::MarkerPoint{{1, 2, 3}, -3.14159274F}});
    return document;
}
} // namespace

TEST_CASE("markers: a document writes one text that reads back the same") {
    const auto document = SampleMarkers();
    const auto text = WriteMarkers(document);
    REQUIRE_MESSAGE(text, (text ? std::string() : text.error().message));
    CHECK(*text == ReadFixture("sample.map_markers.json"));
    const auto again = ReadMarkers(*text);
    REQUIRE(again);
    const auto second = WriteMarkers(*again);
    REQUIRE(second);
    CHECK(*second == *text);
    REQUIRE(again->markers.size() == 4);
    CHECK(std::get<MM::MarkerPoint>(again->markers[0].shape).yaw == 0.1F);
    CHECK(std::get<MM::MarkerPoint>(again->markers[3].shape).yaw == -3.14159274F);
    CHECK(again->markers[1].tags == document.markers[1].tags);
    const auto empty = WriteMarkers({});
    REQUIRE(empty);
    CHECK(ReadMarkers(*empty));
}

TEST_CASE("markers: a document Engine refuses is not written") {
    auto unordered = SampleMarkers();
    std::swap(unordered.markers[0], unordered.markers[1]);
    const auto refused = WriteMarkers(unordered);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == VoxelMapErrorCode::BuildRefused);
    CHECK(refused.error().detail == "markers[1].id");
    // Bytes that are not UTF-8 are dropped from the text, which then reads back different.
    auto notUtf8 = SampleMarkers();
    notUtf8.markers[0].id = std::string("m\xff") + "0";
    const auto different = WriteMarkers(notUtf8);
    REQUIRE_FALSE(different);
    CHECK(different.error().code == VoxelMapErrorCode::BuildRefused);
    CHECK(different.error().message.find("reads back different") != std::string::npos);
    auto badTag = SampleMarkers();
    badTag.markers[1].tags = {{"K", "v"}};
    const auto tag = WriteMarkers(badTag);
    REQUIRE_FALSE(tag);
    CHECK(tag.error().code == VoxelMapErrorCode::BuildRefused);
}

TEST_CASE("markers: files and kinds are read through Engine") {
    const auto markers = ReadMarkers(R"({"schema": "gyo.map_markers", "version": 2, "markers": []})");
    REQUIRE_FALSE(markers);
    CHECK(markers.error().code == VoxelMapErrorCode::InvalidMarkers);
    CHECK(markers.error().detail == "version");
    const auto kinds = ReadMarkerKinds(R"({"schema": "gyo.marker_kinds", "version": 1, "kinds": ["b", "a"]})");
    REQUIRE_FALSE(kinds);
    CHECK(kinds.error().code == VoxelMapErrorCode::InvalidMarkers);
    CHECK(kinds.error().detail == "kinds[1]");
    const auto listed = ReadMarkerKinds(R"({"schema": "gyo.marker_kinds", "version": 1, "kinds": ["a", "b"]})");
    REQUIRE(listed);
    CHECK(*listed == std::vector<std::string>{"a", "b"});
}
