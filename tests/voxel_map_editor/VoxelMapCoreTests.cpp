#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Gyo::Tools::VoxelMapEditor;

// Judgments E1 to E6 of the v8-tool-voxel-map-editor-core declaration
// (docs/tools/plans/v8/voxel-map-editor.md).

namespace {

std::string ReadFixture(const std::string& name) {
    std::ifstream file(std::string(GYO_VOXEL_MAP_FIXTURES) + "/" + name, std::ios::binary);
    REQUIRE(file);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::span<const std::byte> Bytes(const std::string& text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

GridDocument NewGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"stone", 0, {100, 100, 100, 255}}, {"half", 1, {10, 20, 30, 255}}, {"stairs", 2, {1, 2, 3, 255}}};
    return grid;
}

std::string Rejected(const std::string& text) {
    const auto parsed = ParseGrid(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == VoxelMapErrorCode::InvalidGrid);
    return parsed.error().detail;
}

// A minimal valid grid whose fields can be replaced one by one.
struct Doc final {
    std::string version = "1";
    std::string origin = "[0, 0, 0]";
    std::string cellSize = "1.0";
    std::string shapes = R"([{ "name": "cube", "boxes": [{ "min": [0, 0, 0], "max": [16, 16, 16] }] }])";
    std::string blocks = R"([{ "name": "stone", "shape": 0, "color": [1, 2, 3, 255] }])";
    std::string chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]] }])";
    std::string extra;
    [[nodiscard]] std::string Text() const {
        return R"({"schema": "gyo.voxel_grid", "version": )" + version + R"(, "origin": )" + origin +
               R"(, "cell_size": )" + cellSize + R"(, "shapes": )" + shapes + R"(, "blocks": )" + blocks +
               R"(, "chunks": )" + chunks + extra + "}";
    }
};

std::vector<std::array<std::int32_t, 7>> QuadsOf(const GridDocument& grid) {
    const std::string source = "test";
    const auto built = BuildMesh(grid, Bytes(source));
    REQUIRE_MESSAGE(built, (built ? std::string() : built.error().message));
    const auto mesh = Engine::VoxelMesh::ParseVoxelMesh(built->text);
    REQUIRE(mesh);
    std::vector<std::array<std::int32_t, 7>> quads;
    for (const auto& q : mesh->quads)
        quads.push_back({q.x, q.y, q.z, static_cast<std::int32_t>(q.face), q.width, q.height,
                         static_cast<std::int32_t>(q.palette)});
    return quads;
}

// Cells of a box set, on the 1/16 grid, for union comparisons.
std::set<std::array<std::int32_t, 3>> UnitCells(const std::vector<Box>& boxes) {
    std::set<std::array<std::int32_t, 3>> units;
    for (const auto& b : boxes)
        for (auto x = b.minimum[0]; x < b.maximum[0]; ++x)
            for (auto y = b.minimum[1]; y < b.maximum[1]; ++y)
                for (auto z = b.minimum[2]; z < b.maximum[2]; ++z) units.insert({x, y, z});
    return units;
}

bool Overlap(const Box& a, const Box& b) {
    for (int axis = 0; axis < 3; ++axis)
        if (a.maximum[axis] <= b.minimum[axis] || b.maximum[axis] <= a.minimum[axis]) return false;
    return true;
}

} // namespace

// ---- E1: gyo.voxel_grid ----

TEST_CASE("grid: the minimal document is accepted and the sample round-trips byte for byte") {
    const auto grid = ParseGrid(Doc{}.Text());
    REQUIRE(grid);
    CHECK(grid->At({0, 0, 0}) == Cell{1, 0});
    CHECK(grid->At({1, 0, 0}) == Cell{});
    const auto sample = ReadFixture("sample.voxel_grid.json");
    const auto parsed = ParseGrid(sample);
    REQUIRE_MESSAGE(parsed, (parsed ? std::string() : parsed.error().message + " at " + parsed.error().detail));
    CHECK(WriteGrid(*parsed) == sample);
    const auto again = ParseGrid(WriteGrid(*parsed));
    REQUIRE(again);
    CHECK(WriteGrid(*again) == sample);
}

TEST_CASE("grid rule: fields, schema and version") {
    Doc unknown;
    unknown.extra = R"(, "layers": [])";
    CHECK(Rejected(unknown.Text()).empty());
    Doc repeated;
    repeated.extra = R"(, "version": 1)";
    CHECK(Rejected(repeated.Text()).empty());
    Doc version;
    version.version = "2";
    CHECK(Rejected(version.Text()) == "version");
    CHECK(Rejected(R"({"schema": "gyo.voxel_mesh"})").empty());
}

TEST_CASE("grid rule: nested fields, integers and text size") {
    Doc nested;
    nested.shapes = R"([{ "name": "cube", "solid": true, "boxes": [{ "min": [0, 0, 0], "max": [16, 16, 16] }] }])";
    CHECK(Rejected(nested.Text()) == "shapes[0]");
    nested = Doc{};
    nested.shapes = R"([{ "name": "cube", "boxes": [{ "min": [0, 0, 0], "max": [16, 16, 16], "x": 1 }] }])";
    CHECK(Rejected(nested.Text()) == "shapes[0].boxes[0]");
    nested = Doc{};
    nested.blocks = R"([{ "name": "stone", "shape": 0, "color": [1, 2, 3, 255], "shape": 0 }])";
    CHECK(Rejected(nested.Text()).empty());
    nested = Doc{};
    nested.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]], "layer": 0 }])";
    CHECK(Rejected(nested.Text()) == "chunks[0]");
    Doc fraction;
    fraction.blocks = R"([{ "name": "stone", "shape": 0.0, "color": [1, 2, 3, 255] }])";
    CHECK(Rejected(fraction.Text()) == "blocks[0].shape");
    Doc huge;
    huge.origin = "[1073741825, 0, 0]";
    CHECK(Rejected(huge.Text()) == "origin[0]");
    Doc padded;
    padded.extra = std::string(MaxGridTextBytes, ' ');
    CHECK(Rejected(padded.Text()).empty());
}

TEST_CASE("grid rule: origin and cell size") {
    Doc low;
    low.origin = "[-65537, 0, 0]";
    CHECK(Rejected(low.Text()) == "origin[0]");
    Doc y;
    y.origin = "[0, 1, 0]";
    CHECK(Rejected(y.Text()) == "origin[1]");
    Doc beyond;
    beyond.origin = "[65537, 0, 0]";
    CHECK(Rejected(beyond.Text()) == "origin[0]");
    Doc cell;
    for (const auto* value : {"0", "1e-5", "65536.5", "\"1\""}) {
        cell.cellSize = value;
        CHECK(Rejected(cell.Text()) == "cell_size");
    }
}

TEST_CASE("grid rule: shapes") {
    Doc empty;
    empty.shapes = "[]";
    CHECK(Rejected(empty.Text()) == "shapes");
    Doc outside;
    outside.shapes = R"([{ "name": "cube", "boxes": [{ "min": [0, 0, 0], "max": [16, 17, 16] }] }])";
    CHECK(Rejected(outside.Text()) == "shapes[0].boxes[0].max[1]");
    Doc flat;
    flat.shapes = R"([{ "name": "cube", "boxes": [{ "min": [0, 4, 0], "max": [16, 4, 16] }] }])";
    CHECK(Rejected(flat.Text()) == "shapes[0].boxes[0]");
    Doc noBoxes;
    noBoxes.shapes = R"([{ "name": "cube", "boxes": [] }])";
    CHECK(Rejected(noBoxes.Text()) == "shapes[0].boxes");
    std::string many;
    for (int i = 0; i < 257; ++i)
        many += (i ? "," : "") + std::string(R"({ "name": "s)") + std::to_string(i) +
                R"(", "boxes": [{ "min": [0, 0, 0], "max": [1, 1, 1] }] })";
    Doc tooMany;
    tooMany.shapes = "[" + many + "]";
    CHECK(Rejected(tooMany.Text()) == "shapes");
    Doc names;
    names.shapes = R"([{ "name": "a", "boxes": [{ "min": [0, 0, 0], "max": [1, 1, 1] }] }, { "name": "a", "boxes": [{ "min": [0, 0, 0], "max": [1, 1, 1] }] }])";
    CHECK(Rejected(names.Text()) == "shapes[1].name");
}

TEST_CASE("grid rule: blocks") {
    Doc shape;
    shape.blocks = R"([{ "name": "stone", "shape": 1, "color": [1, 2, 3, 255] }])";
    CHECK(Rejected(shape.Text()) == "blocks[0].shape");
    Doc colour;
    colour.blocks = R"([{ "name": "stone", "shape": 0, "color": [1, 2, 300, 255] }])";
    CHECK(Rejected(colour.Text()) == "blocks[0].color[2]");
    Doc name;
    name.blocks = R"([{ "name": "Stone", "shape": 0, "color": [1, 2, 3, 255] }])";
    CHECK(Rejected(name.Text()) == "blocks[0].name");
    Doc empty;
    empty.blocks = "[]";
    CHECK(Rejected(empty.Text()) == "blocks");
    Doc repeated;
    repeated.blocks = R"([{ "name": "stone", "shape": 0, "color": [1, 2, 3, 255] }, { "name": "stone", "shape": 0, "color": [1, 2, 3, 255] }])";
    CHECK(Rejected(repeated.Text()) == "blocks[1].name");
    std::string many;
    for (int i = 0; i < 257; ++i)
        many += (i ? "," : "") + std::string(R"({ "name": "b)") + std::to_string(i) + R"(", "shape": 0, "color": [1, 2, 3, 255] })";
    Doc tooMany;
    tooMany.blocks = "[" + many + "]";
    CHECK(Rejected(tooMany.Text()) == "blocks");
}

TEST_CASE("grid rule: chunks and runs") {
    Doc count;
    count.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 4094]] }])";
    CHECK(Rejected(count.Text()) == "chunks[0].runs");
    Doc over;
    over.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 2], [0, 0, 4095]] }])";
    CHECK(Rejected(over.Text()) == "chunks[0].runs[1][2]");
    Doc emptyTurned;
    emptyTurned.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 3, 4095]] }])";
    CHECK(Rejected(emptyTurned.Text()) == "chunks[0].runs[1][1]");
    Doc orientation;
    orientation.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 24, 1], [0, 0, 4095]] }])";
    CHECK(Rejected(orientation.Text()) == "chunks[0].runs[0][1]");
    Doc block;
    block.chunks = R"([{ "at": [0, 0, 0], "runs": [[2, 0, 1], [0, 0, 4095]] }])";
    CHECK(Rejected(block.Text()) == "chunks[0].runs[0][0]");
    Doc split;
    split.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 1], [0, 0, 4094]] }])";
    CHECK(Rejected(split.Text()) == "chunks[0].runs[2]");
    Doc allEmpty;
    allEmpty.chunks = R"([{ "at": [0, 0, 0], "runs": [[0, 0, 4096]] }])";
    CHECK(Rejected(allEmpty.Text()) == "chunks[0]");
    Doc order;
    order.chunks = R"([{ "at": [1, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]] }, { "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]] }])";
    CHECK(Rejected(order.Text()) == "chunks[1].at");
    Doc noRuns;
    noRuns.chunks = R"([{ "at": [0, 0, 0], "runs": [] }])";
    CHECK(Rejected(noRuns.Text()) == "chunks[0].runs");
    Doc zero;
    zero.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 0], [1, 0, 4095]] }])";
    CHECK(Rejected(zero.Text()) == "chunks[0].runs[1][2]");
    Doc shape;
    shape.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0], [0, 0, 4095]] }])";
    CHECK(Rejected(shape.Text()) == "chunks[0].runs[0]");
    // Only filled cells count: the first cell of chunk 4096 is cell 65536, one beyond.
    Doc range;
    range.chunks = R"([{ "at": [4096, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]] }])";
    CHECK(Rejected(range.Text()) == "chunks[0]");
    Doc lastInRange;
    lastInRange.chunks = R"([{ "at": [4095, 0, 0], "runs": [[0, 0, 15], [1, 0, 1], [0, 0, 4080]] }])";
    CHECK(ParseGrid(lastInRange.Text()));
    Doc belowWithOrigin;
    belowWithOrigin.origin = "[-65536, 0, 0]";
    belowWithOrigin.chunks = R"([{ "at": [-1, 0, 0], "runs": [[0, 0, 15], [1, 0, 1], [0, 0, 4080]] }])";
    CHECK(Rejected(belowWithOrigin.Text()) == "chunks[0]");
    Doc unaligned; // an origin off the chunk grid with an in-range cell near the edge
    unaligned.origin = "[65530, 0, 0]";
    unaligned.chunks = R"([{ "at": [0, 0, 0], "runs": [[1, 0, 1], [0, 0, 4095]] }])";
    CHECK(ParseGrid(unaligned.Text()));
}

// ---- E2: orientations ----

TEST_CASE("orientation: 24 distinct rotations, identity first, up as declared") {
    std::set<Matrix3i> seen;
    const std::array<std::array<std::int32_t, 3>, 6> ups{{{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (std::size_t o = 0; o < OrientationCount; ++o) {
        const auto& m = OrientationMatrix(o);
        seen.insert(m);
        // Orthonormal with determinant +1: a rotation, no mirror.
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                int dot = 0;
                for (int k = 0; k < 3; ++k) dot += m[i][k] * m[j][k];
                CHECK(dot == (i == j ? 1 : 0));
            }
        const int det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        CHECK(det == 1);
        // The cell's up (+Y) goes where o / 4 says.
        CHECK(std::array<std::int32_t, 3>{m[0][1], m[1][1], m[2][1]} == ups[o / 4]);
    }
    CHECK(seen.size() == OrientationCount);
    CHECK(OrientationMatrix(0) == Matrix3i{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}});
}

TEST_CASE("orientation: the table is part of the format") {
    // An orientation number is stored in grid files, so each number's matrix
    // is fixed (docs/voxel_map_editor/architecture.md, "Orientations").
    const std::array<Matrix3i, OrientationCount> table{{
        {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}},
        {{{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}}},
        {{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}},
        {{{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}},
        {{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}}},
        {{{0, 0, 1}, {0, -1, 0}, {1, 0, 0}}},
        {{{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}}},
        {{{0, 0, -1}, {0, -1, 0}, {-1, 0, 0}}},
        {{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}},
        {{{0, 1, 0}, {0, 0, -1}, {-1, 0, 0}}},
        {{{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}},
        {{{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}},
        {{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}},
        {{{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}}},
        {{{0, -1, 0}, {-1, 0, 0}, {0, 0, -1}}},
        {{{0, -1, 0}, {0, 0, -1}, {1, 0, 0}}},
        {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}},
        {{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}},
        {{{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}},
        {{{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}},
        {{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}}},
        {{{0, 0, 1}, {-1, 0, 0}, {0, -1, 0}}},
        {{{-1, 0, 0}, {0, 0, -1}, {0, -1, 0}}},
        {{{0, 0, -1}, {1, 0, 0}, {0, -1, 0}}}
    }};
    for (std::size_t o = 0; o < OrientationCount; ++o) CHECK(OrientationMatrix(o) == table[o]);
    // o % 4 turns about up: with up +Y, orientation 1 takes +Z to +X.
    const auto& quarter = OrientationMatrix(1);
    CHECK(std::array<std::int32_t, 3>{quarter[0][2], quarter[1][2], quarter[2][2]} == std::array<std::int32_t, 3>{1, 0, 0});
}

TEST_CASE("orientation: a stair turned a quarter about up, and upside down") {
    const Box riser{{0, 8, 8}, {16, 16, 16}}; // the stair's upper step, at +Z
    // Quarter turn about +Y: +Z goes to +X.
    CHECK(RotateBox(riser, 1) == Box{{8, 8, 0}, {16, 16, 16}});
    // Half turn about +Y: +Z goes to -Z.
    CHECK(RotateBox(riser, 2) == Box{{0, 8, 0}, {16, 16, 8}});
    // Upside down (half turn about X): y and z both flip.
    CHECK(RotateBox(riser, 4) == Box{{0, 0, 0}, {16, 8, 8}});
    // Every rotation keeps a whole cube whole.
    for (std::size_t o = 0; o < OrientationCount; ++o)
        CHECK(RotateBox({{0, 0, 0}, {16, 16, 16}}, o) == Box{{0, 0, 0}, {16, 16, 16}});
}

// ---- E3: cullface ----

TEST_CASE("cullface: one cube, two cubes, a cube beside a slab") {
    using Q = std::vector<std::array<std::int32_t, 7>>;
    auto grid = NewGrid();
    grid.Set({0, 0, 0}, {1, 0});
    CHECK(QuadsOf(grid) == Q{{0, 0, 0, 1, 16, 16, 0}, {0, 0, 0, 3, 16, 16, 0}, {0, 0, 0, 5, 16, 16, 0},
                             {0, 0, 16, 4, 16, 16, 0}, {0, 16, 0, 2, 16, 16, 0}, {16, 0, 0, 0, 16, 16, 0}});

    grid.Set({1, 0, 0}, {1, 0});
    const auto two = QuadsOf(grid);
    CHECK(two == Q{{0, 0, 0, 1, 16, 16, 0}, {0, 0, 0, 3, 16, 16, 0}, {0, 0, 0, 5, 16, 16, 0},
                   {0, 0, 16, 4, 16, 16, 0}, {0, 16, 0, 2, 16, 16, 0}, {16, 0, 0, 3, 16, 16, 0},
                   {16, 0, 0, 5, 16, 16, 0}, {16, 0, 16, 4, 16, 16, 0}, {16, 16, 0, 2, 16, 16, 0},
                   {32, 0, 0, 0, 16, 16, 0}});
    // The touching faces at x = 16 are gone; the outer ones stay.
    CHECK(std::none_of(two.begin(), two.end(), [](const auto& q) { return q[0] == 16 && (q[3] == 0 || q[3] == 1); }));
    CHECK(std::count_if(two.begin(), two.end(), [](const auto& q) { return q[3] == 0 && q[0] == 32; }) == 1);

    auto slab = NewGrid();
    slab.Set({0, 0, 0}, {1, 0}); // cube
    slab.Set({1, 0, 0}, {2, 0}); // lower slab at +X
    const auto quads = QuadsOf(slab);
    // The cube's +X face stays: the slab covers only half of it.
    CHECK(std::count(quads.begin(), quads.end(), std::array<std::int32_t, 7>{16, 0, 0, 0, 16, 16, 1}) == 1);
    // The slab's -X face is hidden by the cube's whole side.
    CHECK(std::none_of(quads.begin(), quads.end(), [](const auto& q) { return q[3] == 1 && q[0] == 16; }));
    // Palette by name: half 0, stone 1. The cube keeps all six faces; the slab
    // loses its -X face.
    CHECK(quads == Q{{0, 0, 0, 1, 16, 16, 1}, {0, 0, 0, 3, 16, 16, 1}, {0, 0, 0, 5, 16, 16, 1},
                     {0, 0, 16, 4, 16, 16, 1}, {0, 16, 0, 2, 16, 16, 1}, {16, 0, 0, 0, 16, 16, 1},
                     {16, 0, 0, 3, 16, 16, 0}, {16, 0, 0, 5, 16, 8, 0}, {16, 0, 16, 4, 16, 8, 0},
                     {16, 8, 0, 2, 16, 16, 0}, {32, 0, 0, 0, 16, 8, 0}});

    // A stair is not symmetric: its +Z side is whole, its -Z side is only the
    // lower half. A cube on its -Z side keeps its +Z face (the stair's -Z side
    // touches it), and a cube on its +Z side loses its -Z face.
    auto stair = NewGrid();
    stair.Set({0, 0, 1}, {3, 0}); // stair, orientation 0
    stair.Set({0, 0, 0}, {1, 0}); // cube on the stair's -Z side
    stair.Set({0, 0, 2}, {1, 0}); // cube on the stair's +Z side
    const auto around = QuadsOf(stair);
    // Palette by name: stairs 0, stone 1.
    CHECK(std::count(around.begin(), around.end(), std::array<std::int32_t, 7>{0, 0, 16, 4, 16, 16, 1}) == 1);
    CHECK(std::none_of(around.begin(), around.end(), [](const auto& q) { return q[2] == 32 && q[3] == 5 && q[6] == 1; }));
}

// ---- E4: collision boxes ----

TEST_CASE("merge: a 2x1x1 bar is one box; an L is two") {
    CHECK(MergeBoxes(std::vector<Box>{{{0, 0, 0}, {16, 16, 16}}, {{16, 0, 0}, {32, 16, 16}}}) ==
          std::vector<Box>{{{0, 0, 0}, {32, 16, 16}}});
    const auto l = MergeBoxes(std::vector<Box>{{{0, 0, 0}, {16, 16, 16}}, {{16, 0, 0}, {32, 16, 16}},
                                               {{0, 0, 16}, {16, 16, 32}}});
    CHECK(l.size() == 2);
    // A tower of three cubes is one box.
    CHECK(MergeBoxes(std::vector<Box>{{{0, 0, 0}, {16, 16, 16}}, {{0, 16, 0}, {16, 32, 16}},
                                      {{0, 32, 0}, {16, 48, 16}}}) == std::vector<Box>{{{0, 0, 0}, {16, 48, 16}}});
}

TEST_CASE("merge: walls along z keep the corners, walls along x stop at them") {
    // A ring of walls one cell thick and three high around a 2x2 room, and a
    // pillar: cell boxes in, the strip boxes out (z runs first, equal runs
    // merged along x), as a hand-written level of axis walls would list them.
    std::vector<Box> cells;
    const auto cell = [&cells](const int x, const int z, const int height) {
        for (int y = 0; y < height; ++y)
            cells.push_back({{x * 16, y * 16, z * 16}, {x * 16 + 16, y * 16 + 16, z * 16 + 16}});
    };
    for (int z = 0; z < 6; ++z) {
        cell(0, z, 3);
        cell(5, z, 3);
    }
    for (int x = 1; x < 5; ++x) {
        cell(x, 0, 3);
        cell(x, 5, 3);
    }
    cell(2, 2, 2);
    cell(3, 2, 2);
    cell(2, 3, 2);
    cell(3, 3, 2);
    CHECK(MergeBoxes(cells) == std::vector<Box>{{{0, 0, 0}, {16, 48, 96}},
                                                {{16, 0, 0}, {80, 48, 16}},
                                                {{16, 0, 80}, {80, 48, 96}},
                                                {{32, 0, 32}, {64, 32, 64}},
                                                {{80, 0, 0}, {96, 48, 96}}});
}

TEST_CASE("merge: 200 random sets keep their union and never overlap") {
    // A fixed linear congruential generator: the same sets on every platform.
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    const auto next = [&state](const std::uint32_t bound) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::int32_t>((state >> 33) % bound);
    };
    for (int round = 0; round < 200; ++round) {
        std::vector<Box> boxes;
        const int count = 1 + next(12);
        for (int i = 0; i < count; ++i) {
            Box b;
            for (int axis = 0; axis < 3; ++axis) {
                b.minimum[axis] = next(12);
                b.maximum[axis] = b.minimum[axis] + 1 + next(6);
            }
            boxes.push_back(b);
        }
        const auto merged = MergeBoxes(boxes);
        CHECK(UnitCells(merged) == UnitCells(boxes));
        for (std::size_t i = 0; i < merged.size(); ++i)
            for (std::size_t j = i + 1; j < merged.size(); ++j) CHECK_FALSE(Overlap(merged[i], merged[j]));
        CHECK(std::is_sorted(merged.begin(), merged.end()));
    }
}

// ---- E5 and E6: determinism and the one validator ----

TEST_CASE("build: the sample bakes to the golden file, twice the same") {
    const auto source = ReadFixture("sample.voxel_grid.json");
    const auto golden = ReadFixture("sample.voxel_mesh.json");
    const auto grid = ParseGrid(source);
    REQUIRE(grid);
    const auto first = BuildMesh(*grid, Bytes(source));
    const auto second = BuildMesh(*grid, Bytes(source));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->text == golden);
    CHECK(second->text == first->text);
    CHECK(Engine::VoxelMesh::ParseVoxelMesh(golden));
    // source_sha256 is the source's bytes, not the parsed grid's.
    CHECK(golden.find("57f788711cbccda84b08a69e4fbf2b8807ffe98858c849a4856c3b9eb39263e1") != std::string::npos);
}

TEST_CASE("build: source_sha256 is of the bytes given, not of the rewritten grid") {
    const auto text = Doc{}.Text(); // valid, but not the writer's layout
    const auto grid = ParseGrid(text);
    REQUIRE(grid);
    REQUIRE(WriteGrid(*grid) != text);
    const auto built = BuildMesh(*grid, Bytes(text));
    REQUIRE(built);
    const auto mesh = Engine::VoxelMesh::ParseVoxelMesh(built->text);
    REQUIRE(mesh);
    const auto again = BuildMesh(*grid, Bytes(WriteGrid(*grid)));
    REQUIRE(again);
    const auto other = Engine::VoxelMesh::ParseVoxelMesh(again->text);
    REQUIRE(other);
    CHECK(mesh->sourceSha256 != other->sourceSha256);
}

TEST_CASE("build: what Engine refuses is not returned") {
    // An empty grid bakes to no quads, which gyo.voxel_mesh does not allow.
    auto grid = NewGrid();
    const std::string source = "empty";
    const auto built = BuildMesh(grid, Bytes(source));
    REQUIRE_FALSE(built);
    CHECK(built.error().code == VoxelMapErrorCode::BuildRefused);
}

TEST_CASE("import: boxes in whole cells become cubes") {
    const auto grid = ImportBoxes(R"({"walls": [{"min": [-1, 0, -1], "max": [1, 2, 0]}], "other": 1})", "/walls",
                                  {1, 2, 3, 255});
    REQUIRE(grid);
    CHECK(grid->origin == Vec3i{-1, 0, -1});
    CHECK(grid->At({0, 0, 0}) == Cell{1, 0});
    CHECK(grid->At({1, 1, 0}) == Cell{1, 0});
    CHECK(grid->At({2, 0, 0}) == Cell{});
    const auto half = ImportBoxes(R"({"walls": [{"min": [0, 0, 0], "max": [0.5, 1, 1]}]})", "/walls", {1, 2, 3, 255});
    REQUIRE_FALSE(half);
    CHECK(half.error().code == VoxelMapErrorCode::InvalidBoxes);
    CHECK_FALSE(ImportBoxes(R"({"walls": []})", "/walls", {1, 2, 3, 255}));
    CHECK_FALSE(ImportBoxes(R"({"walls": [{"min": [0, -1, 0], "max": [1, 1, 1]}]})", "/walls", {1, 2, 3, 255}));
    // A pointer nlohmann cannot follow is an input error, not an exception.
    const auto badPointer = ImportBoxes(R"({"w": [1]})", "/w/18446744073709551616", {1, 2, 3, 255});
    REQUIRE_FALSE(badPointer);
    CHECK(badPointer.error().code == VoxelMapErrorCode::InvalidBoxes);
    CHECK_FALSE(ImportBoxes(R"({"w": []})", "not a pointer", {1, 2, 3, 255}));
    // Boxes far from the chunk grid still give a grid that reads back.
    const auto distant = ImportBoxes(R"({"w": [{"min": [65530, 0, 0], "max": [65531, 1, 1]}]})", "/w", {1, 2, 3, 255});
    REQUIRE(distant);
    CHECK(ParseGrid(WriteGrid(*distant)));
}
