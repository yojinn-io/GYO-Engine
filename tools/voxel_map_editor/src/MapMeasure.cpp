#include "MapMeasure.hpp"

#include "gyo/voxel_map_editor/ChunkedMap.hpp"
#include "gyo/voxel_map_editor/Synthetic.hpp"
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

namespace {

struct Timings final {
    std::vector<double> ms;
    std::size_t failed{};
    [[nodiscard]] double Median() const {
        if (ms.empty()) return 0.0;
        auto sorted = ms;
        std::sort(sorted.begin(), sorted.end());
        return sorted[sorted.size() / 2];
    }
    [[nodiscard]] double Max() const { return ms.empty() ? 0.0 : *std::max_element(ms.begin(), ms.end()); }
};

constexpr int ScaleRuns = 5;

} // namespace

int MeasureScale(const std::string& outputDirectory, const std::string& map) {
    std::ostringstream out;
    const auto made = Clock::now();
    auto grid = SyntheticByName(map);
    if (!grid || map == "building") {
        std::cerr << "error: --scale takes km2-ground or km2-building\n";
        return 2;
    }
    const double makeMs = Milliseconds(Clock::now() - made);
    std::size_t cells = 0;
    for (const auto& [at, chunk] : grid->chunks)
        for (const auto& cell : chunk) cells += cell.block != 0 ? 1 : 0;
    out << "map " << map << "\nchunks " << grid->chunks.size() << "\ncells " << cells << "\nmake_ms " << makeMs << '\n';

    // SM1: the grid text.
    Timings write;
    std::size_t textBytes = 0;
    for (int run = 0; run < ScaleRuns; ++run) {
        const auto t0 = Clock::now();
        const auto text = WriteGrid(*grid);
        write.ms.push_back(Milliseconds(Clock::now() - t0));
        textBytes = text.size();
    }
    out << "SM1 grid_text_bytes " << textBytes << " over_16MiB " << (textBytes > MaxGridTextBytes ? 1 : 0)
        << " write_ms_median " << write.Median() << " write_ms_max " << write.Max() << " runs " << write.ms.size()
        << '\n';

    // SM2: memory of the cells (computed). The process's peak resident memory is read from outside (on macOS,
    // /usr/bin/time -l), so the tool needs no platform header.
    const auto cellBytes = static_cast<long long>(grid->chunks.size()) * static_cast<long long>(sizeof(ChunkCellsArray));
    out << "SM2 cell_bytes " << cellBytes << '\n';

    // SM3: the whole-document copy a drag starts with (Drag::base).
    Timings copy;
    for (int run = 0; run < ScaleRuns; ++run) {
        const auto t0 = Clock::now();
        const GridDocument base = *grid;
        copy.ms.push_back(Milliseconds(Clock::now() - t0));
        if (base.chunks.size() != grid->chunks.size()) ++copy.failed;
    }
    out << "SM3 copy_ms_median " << copy.Median() << " copy_ms_max " << copy.Max() << " runs " << copy.ms.size()
        << " failed " << copy.failed << '\n';

    // SM4: one palette undo step keeps the document before and after (DocumentStep).
    Timings step;
    for (int run = 0; run < ScaleRuns; ++run) {
        const auto t0 = Clock::now();
        const GridDocument before = *grid;
        const GridDocument after = *grid;
        step.ms.push_back(Milliseconds(Clock::now() - t0));
        if (before.chunks.size() + after.chunks.size() != 2 * grid->chunks.size()) ++step.failed;
    }
    out << "SM4 step_bytes " << 2 * cellBytes << " step_copy_ms_median " << step.Median() << " runs "
        << step.ms.size() << " failed " << step.failed << '\n';

    // SM5: the whole bake and write.
    Timings bake, mapWrite;
    std::size_t mapBytes = 0, regions = 0, quads = 0, colliders = 0;
    const std::string source = "scale";
    for (int run = 0; run < ScaleRuns; ++run) {
        auto t0 = Clock::now();
        auto baked = BakeMap(*grid);
        bake.ms.push_back(Milliseconds(Clock::now() - t0));
        if (!baked) {
            ++bake.failed;
            out << "SM5 bake_failed " << Engine::Base::Describe(baked.error()) << '\n';
            continue;
        }
        t0 = Clock::now();
        const auto text = WriteMap(*baked, Bytes(source));
        mapWrite.ms.push_back(Milliseconds(Clock::now() - t0));
        if (!text) {
            ++mapWrite.failed;
            out << "SM5 write_failed " << Engine::Base::Describe(text.error()) << '\n';
            continue;
        }
        mapBytes = text->index.size();
        for (const auto& region : text->regions) mapBytes += region.text.size();
        regions = text->regions.size();
        quads = colliders = 0;
        for (const auto& [at, chunk] : baked->chunks) {
            quads += chunk.quads.size();
            colliders += chunk.colliders.size();
        }
    }
    out << "SM5 bake_ms_median " << bake.Median() << " bake_ms_max " << bake.Max() << " bake_runs " << bake.ms.size()
        << " bake_failed " << bake.failed << " write_ms_median " << mapWrite.Median() << " write_runs "
        << mapWrite.ms.size() << " write_failed " << mapWrite.failed << " map_bytes " << mapBytes << " regions "
        << regions << " quads " << quads << " colliders " << colliders << '\n';

    // SM7: the pick's reach (1024 cells) against the farthest cell seen from a corner, level with the ground.
    Vec3i low{}, high{};
    bool first = true;
    for (const auto& [at, chunk] : grid->chunks)
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = first ? at[axis] * ChunkSize : std::min(low[axis], at[axis] * ChunkSize);
            high[axis] = first ? at[axis] * ChunkSize + ChunkSize : std::max(high[axis], at[axis] * ChunkSize + ChunkSize);
            if (axis == 2) first = false;
        }
    const double dx = high[0] - low[0];
    const double dz = high[2] - low[2];
    const double diagonal = std::sqrt(dx * dx + dz * dz);
    out << "SM7 pick_reach_cells 1024 map_diagonal_cells " << diagonal << " reaches " << (diagonal <= 1024.0 ? 1 : 0)
        << '\n';

    const auto text = out.str();
    if (auto written = WriteFileReplacing(outputDirectory + "/scale-" + map + ".txt", text); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    std::cout << text;
    return 0;
}

} // namespace Gyo::Tools::VoxelMapEditor

