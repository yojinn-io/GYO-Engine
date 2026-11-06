#include "MapMeasure.hpp"

#include "gyo/voxel_map_editor/ChunkedMap.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <tuple>
#include <vector>

// The measurement of v8-arch-big-map-tool-chunked (docs/tools/plans/v8/voxel-map-tool-chunked.md,
// "量測"): its method is fixed by the declaration and is not changed after seeing results.
namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace VM = Engine::VoxelMesh;
using Clock = std::chrono::steady_clock;

double Milliseconds(const Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

std::span<const std::byte> Bytes(const std::string& text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

// The building-like synthetic map: minstd_rand(20261106), 256 x 48 x 256 cells
// (16 x 3 x 16 chunks). A one-cell floor every 8 cells, a wall every 16 cells
// along x and z with random door holes, and per room random stairs and slabs.
GridDocument BuildingGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes(); // cube, slab, stair
    grid.blocks = {{"floor", 0, {200, 180, 150, 255}},
                   {"wall", 0, {120, 130, 140, 255}},
                   {"stairs", 2, {60, 120, 200, 255}},
                   {"slab", 1, {220, 40, 40, 255}}};
    std::minstd_rand random(20261106);
    const auto pick = [&random](const int bound) { return static_cast<std::int32_t>(random() % static_cast<unsigned>(bound)); };
    constexpr std::int32_t Size = 256, Height = 48, Storey = 8, Room = 16;
    for (std::int32_t floor = 0; floor < Height; floor += Storey) {
        for (std::int32_t z = 0; z < Size; ++z)
            for (std::int32_t x = 0; x < Size; ++x) grid.Set({x, floor, z}, {1, 0});
        // Walls on x = 16k (running along z) and z = 16k (running along x), between this floor and the next.
        for (std::int32_t line = 0; line < Size; line += Room) {
            for (std::int32_t segment = 0; segment < Size; segment += Room) {
                const bool door = pick(10) < 6;
                const auto doorAt = segment + 2 + pick(Room - 5);
                for (std::int32_t along = segment; along < segment + Room; ++along)
                    for (std::int32_t y = floor + 1; y < floor + Storey; ++y) {
                        if (door && along >= doorAt && along < doorAt + 2 && y < floor + 4) continue;
                        grid.Set({line, y, along}, {2, 0});
                    }
                const bool otherDoor = pick(10) < 6;
                const auto otherAt = segment + 2 + pick(Room - 5);
                for (std::int32_t along = segment; along < segment + Room; ++along)
                    for (std::int32_t y = floor + 1; y < floor + Storey; ++y) {
                        if (otherDoor && along >= otherAt && along < otherAt + 2 && y < floor + 4) continue;
                        grid.Set({along, y, line}, {2, 0});
                    }
            }
        }
        // Per room: a stair run of up to 6 steps along +x (30 %), and up to 4 slabs.
        for (std::int32_t rz = 0; rz < Size; rz += Room)
            for (std::int32_t rx = 0; rx < Size; rx += Room) {
                if (pick(10) < 3) {
                    const auto z = rz + 3 + pick(Room - 6);
                    for (std::int32_t step = 0; step < 6; ++step)
                        grid.Set({rx + 3 + step, floor + 1 + step, z}, {3, static_cast<std::uint8_t>(pick(4))});
                }
                const auto slabs = pick(5);
                for (std::int32_t i = 0; i < slabs; ++i)
                    grid.Set({rx + 2 + pick(Room - 3), floor + 1, rz + 2 + pick(Room - 3)}, {4, 0});
            }
    }
    return grid;
}

// One chunk of a cube checkerboard: 2,048 cubes, no face culled, nothing merged.
GridDocument CheckerboardGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"stone", 0, {100, 100, 100, 255}}};
    for (std::int32_t y = 0; y < ChunkSize; ++y)
        for (std::int32_t z = 0; z < ChunkSize; ++z)
            for (std::int32_t x = 0; x < ChunkSize; ++x)
                if ((x + y + z) % 2 == 0) grid.Set({x, y, z}, {1, 0});
    return grid;
}

// Quad corners that lie strictly inside an edge of another quad of the same
// direction and plane (map units; across chunks).
std::size_t TJunctions(const BakedMap& map) {
    constexpr std::array<std::array<int, 2>, 3> InPlane{{{2, 1}, {0, 2}, {0, 1}}};
    using Line = std::tuple<int, std::int64_t, int, std::int64_t>; // dir, plane, which axis is fixed, its value
    std::map<Line, std::vector<std::pair<std::int64_t, std::int64_t>>> edges;
    std::vector<std::tuple<int, std::int64_t, std::int64_t, std::int64_t>> corners; // dir, plane, u, v
    for (const auto& [at, chunk] : map.chunks) {
        const std::array<std::int64_t, 3> offset{std::int64_t{chunk.at.x} * 256, std::int64_t{chunk.at.y} * 256,
                                                 std::int64_t{chunk.at.z} * 256};
        for (const auto& q : chunk.quads) {
            const auto dir = static_cast<int>(q.face);
            const auto axis = static_cast<std::size_t>(dir / 2);
            const std::array<std::int64_t, 3> corner{q.x + offset[0], q.y + offset[1], q.z + offset[2]};
            const auto u0 = corner[static_cast<std::size_t>(InPlane[axis][0])];
            const auto v0 = corner[static_cast<std::size_t>(InPlane[axis][1])];
            const auto u1 = u0 + q.width, v1 = v0 + q.height;
            const auto plane = corner[axis];
            edges[{dir, plane, 0, u0}].push_back({v0, v1});
            edges[{dir, plane, 0, u1}].push_back({v0, v1});
            edges[{dir, plane, 1, v0}].push_back({u0, u1});
            edges[{dir, plane, 1, v1}].push_back({u0, u1});
            for (const auto u : {u0, u1})
                for (const auto v : {v0, v1}) corners.emplace_back(dir, plane, u, v);
        }
    }
    std::size_t count = 0;
    for (const auto& [dir, plane, u, v] : corners) {
        const auto inside = [&](const Line& line, const std::int64_t at) {
            const auto found = edges.find(line);
            if (found == edges.end()) return false;
            return std::any_of(found->second.begin(), found->second.end(),
                               [at](const auto& s) { return s.first < at && at < s.second; });
        };
        if (inside({dir, plane, 0, u}, v) || inside({dir, plane, 1, v}, u)) ++count;
    }
    return count;
}

struct Summary final {
    std::string name;
    std::string gridSha256;
    std::size_t cells{};
    std::size_t singleQuads{}, singleColliders{}, singleBytes{};
    double singleBuildMs{};
    std::string singleRefused; // why the single file was refused, if it was
    std::size_t chunks{}, regions{}, mapQuads{}, mapColliders{}, mapBytes{};
    double mapBakeMs{}, mapWriteMs{};
    std::size_t tJunctions{};
    std::size_t requested{}, measured{}, failed{};
    double p50{}, p95{}, max{};
};

double Percentile(std::vector<double> values, const double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    // Nearest rank.
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(values.size())));
    return values[std::max<std::size_t>(rank, 1) - 1];
}

// Rebakes every chunk 5 times (RebakeChunks for that chunk, ValidateVoxelChunk,
// ExpandVoxelChunk) and keeps each chunk's median, in milliseconds.
Summary Measure(const std::string& name, const GridDocument& grid, const std::string& gridText, std::ostringstream& csv) {
    Summary s;
    s.name = name;
    s.gridSha256 = Engine::Base::Sha256(Bytes(gridText));
    for (const auto& [at, cells] : grid.chunks)
        for (const auto& cell : cells) s.cells += cell.block != 0 ? 1 : 0;

    auto start = Clock::now();
    const auto single = BuildMesh(grid, Bytes(gridText));
    s.singleBuildMs = Milliseconds(Clock::now() - start);
    if (single) {
        s.singleQuads = single->stats.quads;
        s.singleColliders = single->stats.colliders;
        s.singleBytes = single->text.size();
    } else {
        s.singleRefused = Engine::Base::Describe(single.error());
    }

    start = Clock::now();
    auto map = BakeMap(grid);
    s.mapBakeMs = Milliseconds(Clock::now() - start);
    if (!map) {
        std::cerr << name << ": " << Engine::Base::Describe(map.error()) << '\n';
        return s;
    }
    start = Clock::now();
    const auto text = WriteMap(*map, Bytes(gridText));
    s.mapWriteMs = Milliseconds(Clock::now() - start);
    if (text) {
        s.regions = text->regions.size();
        s.mapBytes = text->index.size();
        for (const auto& region : text->regions) s.mapBytes += region.text.size();
    }
    s.chunks = map->chunks.size();
    for (const auto& [at, chunk] : map->chunks) {
        s.mapQuads += chunk.quads.size();
        s.mapColliders += chunk.colliders.size();
    }
    s.tJunctions = TJunctions(*map);

    std::vector<Vec3i> keys;
    for (const auto& [at, chunk] : map->chunks) keys.push_back(at);
    std::vector<double> medians;
    s.requested = keys.size();
    for (const auto& key : keys) {
        std::vector<double> runs;
        bool ok = true;
        for (int run = 0; run < 5; ++run) {
            const std::vector<Vec3i> one{key};
            const auto t0 = Clock::now();
            const auto rebaked = RebakeChunks(grid, *map, one);
            const auto& chunk = map->chunks.at(key);
            const auto valid = VM::ValidateVoxelChunk(chunk, map->index);
            const auto geometry = VM::ExpandVoxelChunk(chunk, map->index);
            runs.push_back(Milliseconds(Clock::now() - t0));
            ok = ok && rebaked && valid && !geometry.vertices.empty();
        }
        if (!ok) {
            ++s.failed;
            continue;
        }
        std::sort(runs.begin(), runs.end());
        medians.push_back(runs[2]);
        ++s.measured;
        const auto& chunk = map->chunks.at(key);
        csv << name << ',' << key[0] << ',' << key[1] << ',' << key[2] << ',' << chunk.quads.size() << ','
            << chunk.colliders.size() << ',' << runs[0] << ',' << runs[2] << ',' << runs[4] << '\n';
    }
    s.p50 = Percentile(medians, 0.50);
    s.p95 = Percentile(medians, 0.95);
    s.max = medians.empty() ? 0.0 : *std::max_element(medians.begin(), medians.end());
    return s;
}

void Print(const Summary& s) {
    std::cout << "== " << s.name << "\n"
              << "grid sha256: " << s.gridSha256 << ", cells: " << s.cells << "\n"
              << "single file: " << s.singleQuads << " quads, " << s.singleColliders << " colliders, " << s.singleBytes
              << " bytes, build " << s.singleBuildMs << " ms"
              << (s.singleRefused.empty() ? std::string() : " (refused: " + s.singleRefused + ")") << "\n"
              << "chunked map: " << s.chunks << " chunks, " << s.regions << " regions, " << s.mapQuads << " quads, "
              << s.mapColliders << " colliders, " << s.mapBytes << " bytes, bake " << s.mapBakeMs << " ms, write "
              << s.mapWriteMs << " ms\n"
              << "t-junction corners: " << s.tJunctions << "\n"
              << "chunk rebake (median of 5 per chunk): requested " << s.requested << ", measured " << s.measured
              << ", failed " << s.failed << "; p50 " << s.p50 << " ms, p95 " << s.p95 << " ms, max " << s.max
              << " ms\n";
}

} // namespace

int MeasureMap(const std::string& outputDirectory, const std::vector<std::string>& gridPaths) {
    std::ostringstream csv;
    csv << "map,x,y,z,quads,colliders,min_ms,median_ms,max_ms\n";
    std::vector<Summary> summaries;
    const auto building = BuildingGrid();
    const auto buildingText = WriteGrid(building);
    if (auto written = WriteFileReplacing(outputDirectory + "/building.voxel_grid.json", buildingText); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    summaries.push_back(Measure("building", building, buildingText, csv));
    const auto checker = CheckerboardGrid();
    summaries.push_back(Measure("checkerboard", checker, WriteGrid(checker), csv));
    for (const auto& path : gridPaths) {
        auto bytes = ReadFile(path, MaxGridTextBytes + 1);
        if (!bytes) {
            std::cerr << "error: " << Engine::Base::Describe(bytes.error()) << '\n';
            return 3;
        }
        const std::string text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        auto grid = ParseGrid(text);
        if (!grid) {
            std::cerr << "error: " << Engine::Base::Describe(grid.error()) << '\n';
            return 1;
        }
        summaries.push_back(Measure(path, *grid, text, csv));
    }
    // Written as every other output: atomic replace through engine/io.
    if (auto written = WriteFileReplacing(outputDirectory + "/chunk_rebake.csv", csv.str()); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    for (const auto& s : summaries) Print(s);
    return 0;
}

} // namespace Gyo::Tools::VoxelMapEditor
