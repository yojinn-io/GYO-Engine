#include "TestSupport.hpp"

#include "voxel_map_editor/ChunkedMap.hpp"
#include "voxel_map_editor/Grid.hpp"
#include "voxel_map_editor/Synthetic.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/voxel_map/VoxelMap.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// The chunked bake: master's golden map baked again byte for byte, every text read back by the voxel_map Module
// (one validator), greedy faces against an oracle of per-box faces, collision boxes against the cells, saving.
// Master's tests of the same code (tests/voxel_map_editor/VoxelMapChunkedTests.cpp) on gyo2's API; master's oracle
// was its single-file bake, which is not carried, so the oracle here is written in the test.

using namespace voxel_map_editor;
using voxel_map_editor::test::ReadFixture;
using voxel_map_editor::test::ReadText;
using voxel_map_editor::test::TestGrid;
namespace VM = GYO::Engine::VoxelMap;

namespace {

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

// Three random shapes of 1 to 3 boxes anywhere in the cell (off the 8-unit lattice, overlapping), one block each.
void RandomShapes(GridDocument& grid, Random& random) {
    grid.shapes.clear();
    for (int s = 0; s < 3; ++s) {
        Shape shape{"s" + std::to_string(s), {}};
        const int boxes = 1 + random.Next(3);
        for (int b = 0; b < boxes; ++b) {
            Box box;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                box.minimum[axis] = random.Next(16);
                box.maximum[axis] =
                    box.minimum[axis] + 1 + random.Next(static_cast<std::uint32_t>(16 - box.minimum[axis]));
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
    auto text = WriteMap(map, AsBytes(source));
    REQUIRE_MESSAGE(text, (text ? std::string() : text.error().message + " " + text.error().detail));
    return std::move(text).value();
}

bool SameTexts(const MapText& a, const MapText& b) {
    if (a.index != b.index || a.regions.size() != b.regions.size()) return false;
    for (std::size_t i = 0; i < a.regions.size(); ++i)
        if (a.regions[i].file != b.regions[i].file || a.regions[i].text != b.regions[i].text) return false;
    return true;
}

// Every face square of a quad set: (direction, plane, palette name, u, v), in map units.
using Square = std::tuple<int, std::int32_t, std::string, std::int32_t, std::int32_t>;
constexpr std::int32_t kStep = 8; // the default shapes' boxes all lie on an 8-unit lattice

// The in-plane axes of each face axis: X faces (Z, Y), Y faces (X, Z), Z faces (X, Y).
constexpr std::array<std::array<std::size_t, 2>, 3> kInPlane{{{2, 1}, {0, 2}, {0, 1}}};

void AddSquares(std::vector<Square>& squares, const VM::VoxelQuad& q, const Vec3i& offset, const std::string& name,
                const std::int32_t step) {
    const auto dir = static_cast<int>(q.face);
    const auto axis = static_cast<std::size_t>(dir / 2);
    const std::array<std::int32_t, 3> corner{q.x + offset[0], q.y + offset[1], q.z + offset[2]};
    const auto u = kInPlane[axis][0];
    const auto v = kInPlane[axis][1];
    REQUIRE(corner[u] % step == 0);
    REQUIRE(corner[v] % step == 0);
    REQUIRE(q.width % step == 0);
    REQUIRE(q.height % step == 0);
    for (auto a = corner[u]; a < corner[u] + q.width; a += step)
        for (auto b = corner[v]; b < corner[v] + q.height; b += step) squares.emplace_back(dir, corner[axis], name, a, b);
}

std::vector<Box> TurnedBoxes(const GridDocument& grid, const Cell cell) {
    std::vector<Box> boxes;
    for (const auto& box : grid.shapes[grid.blocks[cell.block - 1u].shape].boxes)
        boxes.push_back(RotateBox(box, cell.orientation));
    return boxes;
}

// The squares of a cell side (axis, at 0 or 16) its boxes cover, in cell units.
std::set<std::pair<std::int32_t, std::int32_t>> SideCover(const std::vector<Box>& boxes, const std::size_t axis,
                                                          const std::int32_t at) {
    std::set<std::pair<std::int32_t, std::int32_t>> cover;
    const auto u = kInPlane[axis][0];
    const auto v = kInPlane[axis][1];
    for (const auto& b : boxes) {
        if ((at == 0 ? b.minimum[axis] : b.maximum[axis]) != at) continue;
        for (auto a = b.minimum[u]; a < b.maximum[u]; ++a)
            for (auto c = b.minimum[v]; c < b.maximum[v]; ++c) cover.insert({a, c});
    }
    return cover;
}

// The oracle: one face per box side that is not on a cell side the neighbour covers whole, cut into squares and with
// repeats removed (no merging), from code written here.
std::vector<Square> OracleSquares(const GridDocument& grid, const std::int32_t step) {
    std::vector<Square> squares;
    for (const auto& [at, cells] : grid.chunks)
        for (std::int32_t y = 0; y < kChunkSize; ++y)
            for (std::int32_t z = 0; z < kChunkSize; ++z)
                for (std::int32_t x = 0; x < kChunkSize; ++x) {
                    const auto cell = cells[CellIndex(x, y, z)];
                    if (cell.block == 0) continue;
                    const Vec3i position{at[0] * kChunkSize + x, at[1] * kChunkSize + y, at[2] * kChunkSize + z};
                    const auto& name = grid.blocks[cell.block - 1u].name;
                    for (const auto& part : TurnedBoxes(grid, cell)) {
                        for (int dir = 0; dir < 6; ++dir) {
                            const auto axis = static_cast<std::size_t>(dir / 2);
                            const bool positive = dir % 2 == 0;
                            if (positive ? part.maximum[axis] == kUnits : part.minimum[axis] == 0) {
                                Vec3i neighbour = position;
                                neighbour[axis] += positive ? 1 : -1;
                                const auto other = grid.At(neighbour);
                                if (other.block != 0 &&
                                    SideCover(TurnedBoxes(grid, other), axis, positive ? 0 : kUnits).size() ==
                                        static_cast<std::size_t>(kUnits * kUnits))
                                    continue;
                            }
                            Box world;
                            for (std::size_t a = 0; a < 3; ++a) {
                                world.minimum[a] = position[a] * kUnits + part.minimum[a];
                                world.maximum[a] = position[a] * kUnits + part.maximum[a];
                            }
                            const auto u = kInPlane[axis][0];
                            const auto v = kInPlane[axis][1];
                            const auto plane = positive ? world.maximum[axis] : world.minimum[axis];
                            for (auto a = world.minimum[u]; a < world.maximum[u]; a += step)
                                for (auto b = world.minimum[v]; b < world.maximum[v]; b += step)
                                    squares.emplace_back(dir, plane, name, a, b);
                        }
                    }
                }
    std::sort(squares.begin(), squares.end());
    squares.erase(std::unique(squares.begin(), squares.end()), squares.end());
    return squares;
}

Vec3i ChunkOffset(const VM::VoxelChunkKey& at) {
    return {at.x * kChunkSize * kUnits, at.y * kChunkSize * kUnits, at.z * kChunkSize * kUnits};
}

std::vector<Square> BakedSquares(const BakedMap& map, const std::int32_t step) {
    std::vector<Square> squares;
    for (const auto& [at, chunk] : map.chunks)
        for (const auto& q : chunk.quads) AddSquares(squares, q, ChunkOffset(chunk.at), map.index.palette[q.palette].name, step);
    std::sort(squares.begin(), squares.end());
    return squares;
}

std::set<Vec3i> UnitBlocks(const std::vector<Box>& boxes) {
    std::set<Vec3i> units;
    for (const auto& b : boxes)
        for (auto x = b.minimum[0]; x < b.maximum[0]; x += kStep)
            for (auto y = b.minimum[1]; y < b.maximum[1]; y += kStep)
                for (auto z = b.minimum[2]; z < b.maximum[2]; z += kStep) units.insert({x, y, z});
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

// Reads a saved map with the Module only, as a game would.
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

using QuadKey = std::array<std::int32_t, 7>;
std::vector<QuadKey> QuadsOf(const VM::VoxelChunkDocument& chunk) {
    std::vector<QuadKey> quads;
    for (const auto& q : chunk.quads)
        quads.push_back({q.x, q.y, q.z, static_cast<std::int32_t>(q.face), q.width, q.height,
                         static_cast<std::int32_t>(q.palette)});
    return quads;
}

} // namespace

// ---- One wire and one bake with master ----

TEST_CASE("map: master's golden map reads through the Module and bakes again byte for byte, twice the same") {
    const auto source = ReadFixture("sample.voxel_grid.json");
    const auto goldenIndex = ReadFixture("sample_map/map.json");
    const auto index = VM::ParseVoxelMap(goldenIndex);
    REQUIRE(index);
    for (const auto& entry : index->regions)
        CHECK_MESSAGE(VM::ParseVoxelRegion(ReadFixture("sample_map/" + entry.file), *index, entry.at), entry.file);

    const auto grid = ParseGrid(source);
    REQUIRE(grid);
    const auto first = BakeMap(*grid);
    const auto second = BakeMap(*grid);
    REQUIRE(first);
    REQUIRE(second);
    const auto a = WriteMap(*first, AsBytes(source));
    const auto b = WriteMap(*second, AsBytes(source));
    REQUIRE(a);
    REQUIRE(b);
    CHECK(SameTexts(*a, *b));
    CHECK(a->index == goldenIndex);
    REQUIRE(a->regions.size() == 2);
    for (const auto& region : a->regions) CHECK(region.text == ReadFixture("sample_map/" + region.file));
}

// ---- Faces and boxes ----

TEST_CASE("map: cullface, one cube, two cubes merged, a cube beside a slab, a stair") {
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {1, 0});
    auto map = BakeMap(grid);
    REQUIRE(map);
    using Q = std::vector<QuadKey>;
    CHECK(QuadsOf(map->chunks.at({0, 0, 0})) == Q{{0, 0, 0, 1, 16, 16, 0}, {0, 0, 0, 3, 16, 16, 0}, {0, 0, 0, 5, 16, 16, 0},
                                                  {0, 0, 16, 4, 16, 16, 0}, {0, 16, 0, 2, 16, 16, 0}, {16, 0, 0, 0, 16, 16, 0}});
    // Two cubes along x: the touching faces are gone, and each other side is one merged face.
    grid.Set({1, 0, 0}, {1, 0});
    map = BakeMap(grid);
    REQUIRE(map);
    CHECK(QuadsOf(map->chunks.at({0, 0, 0})) == Q{{0, 0, 0, 1, 16, 16, 0}, {0, 0, 0, 3, 32, 16, 0}, {0, 0, 0, 5, 32, 16, 0},
                                                  {0, 0, 16, 4, 32, 16, 0}, {0, 16, 0, 2, 32, 16, 0}, {32, 0, 0, 0, 16, 16, 0}});
    CHECK(map->chunks.at({0, 0, 0}).colliders == std::vector<VM::VoxelBox>{{{0, 0, 0}, {32, 16, 16}}});

    // A cube (stone, palette 0) beside a lower slab at +X (half, palette 1): the cube's +X face stays (the slab covers
    // half of it), the slab's -X face is hidden by the cube's whole side.
    auto slab = TestGrid();
    slab.Set({0, 0, 0}, {1, 0});
    slab.Set({1, 0, 0}, {2, 0});
    const auto slabMap = BakeMap(slab);
    REQUIRE(slabMap);
    const auto quads = QuadsOf(slabMap->chunks.at({0, 0, 0}));
    CHECK(std::count(quads.begin(), quads.end(), QuadKey{16, 0, 0, 0, 16, 16, 0}) == 1);
    CHECK(std::none_of(quads.begin(), quads.end(), [](const auto& q) { return q[3] == 1 && q[0] == 16; }));

    // A stair's +Z side is whole, its -Z side only the lower half: a cube on its -Z side keeps its +Z face, a cube on
    // its +Z side loses its -Z face (stone 0, stairs 2).
    auto stair = TestGrid();
    stair.Set({0, 0, 1}, {3, 0});
    stair.Set({0, 0, 0}, {1, 0});
    stair.Set({0, 0, 2}, {1, 0});
    const auto stairMap = BakeMap(stair);
    REQUIRE(stairMap);
    const auto around = QuadsOf(stairMap->chunks.at({0, 0, 0}));
    CHECK(std::count(around.begin(), around.end(), QuadKey{0, 0, 16, 4, 16, 16, 0}) == 1);
    CHECK(std::none_of(around.begin(), around.end(), [](const auto& q) { return q[2] == 32 && q[3] == 5 && q[6] == 0; }));
}

TEST_CASE("map: greedy keeps every face square once (200 random grids against the oracle)") {
    Random random{0x9E3779B97F4A7C15ULL};
    for (int round = 0; round < 200; ++round) {
        const auto grid = RandomGrid(random);
        const auto map = BakeMap(grid);
        REQUIRE(map);
        const auto squares = BakedSquares(*map, kStep);
        CHECK_MESSAGE(std::adjacent_find(squares.begin(), squares.end()) == squares.end(), "round " << round << ": two quads overlap");
        CHECK_MESSAGE(squares == OracleSquares(grid, kStep), "round " << round);
    }
}

TEST_CASE("map: greedy keeps every face square once with random shapes (100 grids, unit squares)") {
    Random random{0x6A09E667F3BCC909ULL};
    for (int round = 0; round < 100; ++round) {
        const auto grid = RandomGrid(random, true);
        const auto map = BakeMap(grid);
        REQUIRE(map);
        const auto squares = BakedSquares(*map, 1);
        CHECK_MESSAGE(std::adjacent_find(squares.begin(), squares.end()) == squares.end(), "round " << round << ": two quads overlap");
        CHECK_MESSAGE(squares == OracleSquares(grid, 1), "round " << round);
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
            for (std::int32_t y = 0; y < kChunkSize; ++y)
                for (std::int32_t z = 0; z < kChunkSize; ++z)
                    for (std::int32_t x = 0; x < kChunkSize; ++x) {
                        const auto& cell = cells[CellIndex(x, y, z)];
                        if (cell.block == 0) continue;
                        for (auto turned : TurnedBoxes(grid, cell)) {
                            const std::array<std::int32_t, 3> local{x, y, z};
                            for (std::size_t axis = 0; axis < 3; ++axis) {
                                turned.minimum[axis] += local[axis] * kUnits;
                                turned.maximum[axis] += local[axis] * kUnits;
                            }
                            expected.push_back(turned);
                        }
                    }
            const auto units = UnitBlocks(boxes);
            CHECK(units == UnitBlocks(expected));
            CHECK_MESSAGE(volume == static_cast<std::int64_t>(units.size()) * kStep * kStep * kStep,
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

// ---- One validator ----

TEST_CASE("map: an empty grid has no map") {
    const auto map = BakeMap(TestGrid());
    REQUIRE_FALSE(map);
    CHECK(map.error().code == ErrorCode::EmptyMap);
}

TEST_CASE("map: an index the Module refuses is not returned") {
    // The grid allows the cell, but its chunk's far side ends past +-2^20 units (rule M6), because the origin is not
    // on the chunk grid.
    auto grid = TestGrid();
    grid.origin = {8, 0, 0};
    grid.Set({(1 << 16) - 9, 0, 0}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE_FALSE(map);
    CHECK(map.error().code == ErrorCode::BuildRefused);
    CHECK(map.error().detail.find("InvalidVoxelMap") != std::string::npos);
}

TEST_CASE("map: a chunk the Module refuses is not kept") {
    // A document no grid file can hold (a box below its cell): the Module's ValidateVoxelChunk refuses it (rule C4).
    auto grid = TestGrid();
    grid.shapes.push_back({"bad", {{{1, 1, -1}, {15, 15, 15}}}});
    grid.blocks.push_back({"bad", 3, {5, 5, 5, 255}});
    grid.Set({20, 0, 0}, {1, 0});
    grid.Set({0, 0, 0}, {4, 0});
    const auto refused = BakeMap(grid);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == ErrorCode::BuildRefused);
    CHECK(refused.error().message.find("a chunk") != std::string::npos);
}

TEST_CASE("map: a region the Module refuses is not returned") {
    // Checkerboards in 49 chunks of one region: more than the 16 MiB a region may hold (rule R5).
    auto grid = TestGrid();
    for (std::int32_t cx = 0; cx < 7; ++cx)
        for (std::int32_t cz = 0; cz < 7; ++cz)
            for (std::int32_t y = 0; y < kChunkSize; ++y)
                for (std::int32_t z = 0; z < kChunkSize; ++z)
                    for (std::int32_t x = 0; x < kChunkSize; ++x)
                        if ((x + y + z) % 2 == 0) grid.Set({cx * kChunkSize + x, y, cz * kChunkSize + z}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const std::string source = "big";
    const auto text = WriteMap(*map, AsBytes(source));
    REQUIRE_FALSE(text);
    CHECK(text.error().code == ErrorCode::BuildRefused);
    CHECK(text.error().message.find("regions/r_0_0_") != std::string::npos);
}

TEST_CASE("map: the index names the source's bytes, the bake version and the grid's origin and cell size") {
    auto grid = TestGrid();
    grid.origin = {-3, 0, 5};
    grid.cellSize = 0.25F;
    grid.Set({0, 0, 0}, {1, 0});
    grid.Set({200, 0, -40}, {2, 0}); // a second region
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const std::string source = "the grid's bytes";
    const auto text = WriteMap(*map, AsBytes(source));
    REQUIRE(text);
    const auto index = VM::ParseVoxelMap(text->index);
    REQUIRE(index);
    CHECK(index->sourceSha256 == GYO::Engine::Base::Sha256(AsBytes(source)));
    CHECK(index->buildVersion == kMapBuildVersion);
    CHECK(index->origin == GYO::Engine::Math::Vec3i{-3, 0, 5});
    CHECK(index->cellSize == 0.25F);
    CHECK(index->palette.size() == grid.blocks.size());
    REQUIRE(index->regions.size() == 2);
    for (std::size_t r = 0; r < index->regions.size(); ++r) {
        CHECK(index->regions[r].file == text->regions[r].file);
        CHECK(index->regions[r].sha256 == GYO::Engine::Base::Sha256(AsBytes(text->regions[r].text)));
        CHECK(text->regions[r].file == "regions/r_" + std::to_string(index->regions[r].at.x) + "_" +
                                           std::to_string(index->regions[r].at.z) + "_" +
                                           index->regions[r].sha256.substr(0, 16) + ".json");
    }
}

TEST_CASE("map: a palette entry is the block's colour shaded per face") {
    auto grid = TestGrid();
    grid.blocks[0].color = {200, 101, 0, 77};
    grid.Set({0, 0, 0}, {1, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    // +X, -X 80 %, +Y 100 %, -Y 50 %, +Z, -Z 65 %, rounded half up; alpha unchanged.
    CHECK(map->index.palette[0].faces == std::array<VM::VoxelColor, VM::kFaceCount>{{{160, 81, 0, 77},
                                                                                      {160, 81, 0, 77},
                                                                                      {200, 101, 0, 77},
                                                                                      {100, 51, 0, 77},
                                                                                      {130, 66, 0, 77},
                                                                                      {130, 66, 0, 77}}});
    CHECK(map->index.palette[0].name == "stone");
}

// ---- Saving ----

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
    // Only files the previous index listed are removed: not other files, not a region file no index listed.
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

TEST_CASE("map: a summary reads a saved map with the Module only") {
    const auto directory = Work("summary");
    auto grid = TestGrid();
    grid.Set({0, 0, 0}, {1, 0});
    grid.Set({1, 0, 0}, {1, 0});
    grid.Set({200, 0, 0}, {2, 0});
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const auto text = Texts(*map);
    REQUIRE(SaveMap(directory.string(), text));
    const auto summary = ReadMapSummary(directory.string());
    REQUIRE(summary);
    CHECK(summary->chunks == 2);
    CHECK(summary->quads == 12);
    CHECK(summary->colliders == 2);
    std::size_t bytes = text.index.size();
    for (const auto& region : text.regions) bytes += region.text.size();
    CHECK(summary->bytes == bytes);
    // One changed byte in a region: its sha256 no longer matches the index (rule R3).
    { std::ofstream(directory / text.regions.back().file, std::ios::app) << " "; }
    const auto broken = ReadMapSummary(directory.string());
    REQUIRE_FALSE(broken);
    CHECK(broken.error().code == ErrorCode::BuildRefused);
}

// ---- Synthetic maps ----

TEST_CASE("synthetic: the km2 maps take their size, and refuse a size that is not whole chunks") {
    const auto ground = SyntheticByName("km2-ground", 64);
    REQUIRE(ground);
    // 64 x 64 cells of ground and walls on x, z = 0 (64 is the wall spacing), two cells high.
    std::size_t cells = 0;
    for (const auto& [at, chunk] : ground->chunks)
        for (const auto& cell : chunk) cells += cell.block != 0 ? 1 : 0;
    CHECK(cells == 64 * 64 + 2 * (64 + 63));
    CHECK(ground->At({63, 0, 63}) == Cell{1, 0});
    CHECK(ground->At({64, 0, 0}) == Cell{});
    CHECK(ground->At({0, 2, 17}) == Cell{2, 0});
    CHECK(ground->At({1, 1, 1}) == Cell{});
    // The tiled building at one tile is the building itself.
    const auto tiled = SyntheticByName("km2-building", 256);
    REQUIRE(tiled);
    CHECK(*tiled == BuildingGrid());
    const auto two = TiledBuildingGrid(512);
    CHECK(two.chunks.size() == 4 * BuildingGrid().chunks.size());
    CHECK_FALSE(SyntheticByName("km2-ground", 100));
    CHECK_FALSE(SyntheticByName("km2-ground", 0));
    CHECK_FALSE(SyntheticByName("km2-ground", kMaxSyntheticCells + 16));
    CHECK_FALSE(SyntheticByName("city", 64));
    // The default is a kilometre at 0.5 m a cell.
    CHECK(kKm2Cells == 2000);
}

TEST_CASE("synthetic: the building map bakes and reads back, the same every time") {
    const auto grid = BuildingGrid();
    CHECK(grid.chunks.size() == 768);
    const auto map = BakeMap(grid);
    REQUIRE(map);
    const auto text = Texts(*map);
    const auto again = BakeMap(BuildingGrid());
    REQUIRE(again);
    CHECK(SameTexts(text, Texts(*again)));
}
