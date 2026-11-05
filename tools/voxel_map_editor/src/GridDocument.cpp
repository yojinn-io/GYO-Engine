#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "JsonRules.hpp"

#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

using Json = nlohmann::json;
using namespace JsonRules;

Vec3i ChunkOf(const Vec3i& cell) {
    Vec3i chunk{};
    for (int axis = 0; axis < 3; ++axis) {
        // Floor division, so negative cells land in negative chunks.
        chunk[axis] = cell[axis] >= 0 ? cell[axis] / ChunkSize : -((-cell[axis] + ChunkSize - 1) / ChunkSize);
    }
    return chunk;
}

Box ParseBox(const Json& value, const std::string& path) {
    if (!value.is_object()) Fail("a box is an object", path);
    RequireExactKeys(value, {"min", "max"}, path);
    const auto minimum = Triple(value.at("min"), path + ".min");
    const auto maximum = Triple(value.at("max"), path + ".max");
    Box box;
    for (int axis = 0; axis < 3; ++axis) {
        if (minimum[axis] < 0 || minimum[axis] > Units) Fail("a shape coordinate is 0 to 16", At(path + ".min", axis));
        if (maximum[axis] < 0 || maximum[axis] > Units) Fail("a shape coordinate is 0 to 16", At(path + ".max", axis));
        if (!(minimum[axis] < maximum[axis])) Fail("min is below max on every axis", path);
        box.minimum[axis] = static_cast<std::int32_t>(minimum[axis]);
        box.maximum[axis] = static_cast<std::int32_t>(maximum[axis]);
    }
    return box;
}

GridDocument ParseDocument(const std::string_view text) {
    if (text.size() > MaxGridTextBytes) Fail("the text exceeds 16 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document, {"schema", "version", "origin", "cell_size", "shapes", "blocks", "chunks"}, "");
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != GridSchema)
        Fail("schema is '" + std::string(GridSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != GridVersion)
        Fail("version is " + std::to_string(GridVersion), "version");

    GridDocument result;
    const auto origin = Triple(document.at("origin"), "origin");
    for (int axis = 0; axis < 3; ++axis) {
        if (origin[axis] < -CellLimit || origin[axis] > CellLimit) Fail("origin is within +-2^16 cells", At("origin", axis));
        result.origin[axis] = static_cast<std::int32_t>(origin[axis]);
    }
    if (result.origin[1] != 0) Fail("origin y is 0: the floor is the y = 0 plane", At("origin", 1));
    const auto& cellSize = document.at("cell_size");
    if (!cellSize.is_number()) Fail("cell_size is a number", "cell_size");
    result.cellSize = static_cast<float>(cellSize.get<double>());
    if (!(result.cellSize >= Engine::VoxelMesh::MinCellSize) || !(result.cellSize <= Engine::VoxelMesh::MaxCellSize))
        Fail("cell_size is within [2^-16, 2^16]", "cell_size");

    const auto& shapes = document.at("shapes");
    if (!shapes.is_array() || shapes.empty() || shapes.size() > MaxShapes) Fail("shapes lists 1 to 256 shapes", "shapes");
    std::set<std::string> names;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const auto path = At("shapes", i);
        const auto& entry = shapes[i];
        if (!entry.is_object()) Fail("a shape is an object", path);
        RequireExactKeys(entry, {"name", "boxes"}, path);
        Shape shape{Name(entry.at("name"), path + ".name"), {}};
        if (!names.insert(shape.name).second) Fail("a shape name repeats", path + ".name");
        const auto& boxes = entry.at("boxes");
        if (!boxes.is_array() || boxes.empty()) Fail("boxes lists one or more boxes", path + ".boxes");
        for (std::size_t b = 0; b < boxes.size(); ++b) shape.boxes.push_back(ParseBox(boxes[b], At(path + ".boxes", b)));
        result.shapes.push_back(std::move(shape));
    }

    const auto& blocks = document.at("blocks");
    if (!blocks.is_array() || blocks.empty() || blocks.size() > MaxBlocks) Fail("blocks lists 1 to 256 blocks", "blocks");
    names.clear();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto path = At("blocks", i);
        const auto& entry = blocks[i];
        if (!entry.is_object()) Fail("a block is an object", path);
        RequireExactKeys(entry, {"name", "shape", "color"}, path);
        Block block{Name(entry.at("name"), path + ".name"), 0, {}};
        if (!names.insert(block.name).second) Fail("a block name repeats", path + ".name");
        const auto shape = Integer(entry.at("shape"), path + ".shape");
        if (shape < 0 || shape >= static_cast<std::int64_t>(result.shapes.size()))
            Fail("shape names a shape", path + ".shape");
        block.shape = static_cast<std::uint32_t>(shape);
        block.color = Colour(entry.at("color"), path + ".color");
        result.blocks.push_back(std::move(block));
    }

    const auto& chunks = document.at("chunks");
    if (!chunks.is_array()) Fail("chunks is an array", "chunks");
    const Vec3i* previous = nullptr;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        const auto path = At("chunks", i);
        const auto& entry = chunks[i];
        if (!entry.is_object()) Fail("a chunk is an object", path);
        RequireExactKeys(entry, {"at", "runs"}, path);
        const auto at = Triple(entry.at("at"), path + ".at");
        Vec3i key{};
        for (int axis = 0; axis < 3; ++axis) {
            // Loose bound only: which cells lie in range is checked per filled cell below.
            if (at[axis] < -CellLimit || at[axis] > CellLimit) Fail("a chunk lies beyond +-2^16 cells", At(path + ".at", axis));
            key[axis] = static_cast<std::int32_t>(at[axis]);
        }
        if (previous && !(*previous < key)) Fail("chunks are in strictly increasing order", path + ".at");
        const auto& runs = entry.at("runs");
        if (!runs.is_array() || runs.empty()) Fail("runs lists one or more runs", path + ".runs");
        ChunkCellsArray cells{};
        std::int64_t filled = 0;
        bool anyBlock = false;
        Cell last{};
        for (std::size_t r = 0; r < runs.size(); ++r) {
            const auto runPath = At(path + ".runs", r);
            const auto& run = runs[r];
            if (!run.is_array() || run.size() != 3) Fail("a run is [block, orientation, count]", runPath);
            const auto block = Integer(run[0], At(runPath, 0));
            const auto orientation = Integer(run[1], At(runPath, 1));
            const auto count = Integer(run[2], At(runPath, 2));
            if (block < 0 || block > static_cast<std::int64_t>(result.blocks.size()))
                Fail("block is 0 (empty) or names a block", At(runPath, 0));
            if (orientation < 0 || orientation >= static_cast<std::int64_t>(OrientationCount))
                Fail("orientation is 0 to 23", At(runPath, 1));
            if (block == 0 && orientation != 0) Fail("an empty cell has orientation 0", At(runPath, 1));
            if (count < 1 || filled + count > ChunkCells) Fail("the runs of a chunk cover exactly 4096 cells", At(runPath, 2));
            const Cell cell{static_cast<std::uint16_t>(block), static_cast<std::uint8_t>(orientation)};
            if (r > 0 && cell == last) Fail("a run repeats the run before it", runPath);
            std::fill_n(cells.begin() + filled, count, cell);
            filled += count;
            anyBlock = anyBlock || block != 0;
            last = cell;
        }
        if (filled != ChunkCells) Fail("the runs of a chunk cover exactly 4096 cells", path + ".runs");
        if (!anyBlock) Fail("a chunk has at least one block", path);
        // Every filled cell, after adding origin, lies within [-2^16, 2^16 - 1].
        for (std::int32_t y = 0; y < ChunkSize; ++y)
            for (std::int32_t z = 0; z < ChunkSize; ++z)
                for (std::int32_t x = 0; x < ChunkSize; ++x) {
                    if (cells[CellIndex(x, y, z)].block == 0) continue;
                    const std::array<std::int64_t, 3> cell{key[0] * std::int64_t{ChunkSize} + x + result.origin[0],
                                                           key[1] * std::int64_t{ChunkSize} + y + result.origin[1],
                                                           key[2] * std::int64_t{ChunkSize} + z + result.origin[2]};
                    for (int axis = 0; axis < 3; ++axis)
                        if (cell[axis] < -CellLimit || cell[axis] > CellLimit - 1)
                            Fail("a cell lies beyond [-2^16, 2^16 - 1] after adding origin", path);
                }
        const auto inserted = result.chunks.emplace(key, cells);
        previous = &inserted.first->first;
    }
    return result;
}

void WriteBox(std::string& out, const Box& box) {
    out += "{ \"min\": [" + std::to_string(box.minimum[0]) + ", " + std::to_string(box.minimum[1]) + ", " +
           std::to_string(box.minimum[2]) + "], \"max\": [" + std::to_string(box.maximum[0]) + ", " +
           std::to_string(box.maximum[1]) + ", " + std::to_string(box.maximum[2]) + "] }";
}

} // namespace

Cell GridDocument::At(const Vec3i& cell) const {
    const auto chunk = ChunkOf(cell);
    const auto found = chunks.find(chunk);
    if (found == chunks.end()) return {};
    return found->second[CellIndex(cell[0] - chunk[0] * ChunkSize, cell[1] - chunk[1] * ChunkSize,
                                   cell[2] - chunk[2] * ChunkSize)];
}

void GridDocument::Set(const Vec3i& cell, Cell value) {
    // An empty cell has no orientation: the format's one form of "empty".
    if (value.block == 0) value.orientation = 0;
    const auto chunk = ChunkOf(cell);
    const auto index = CellIndex(cell[0] - chunk[0] * ChunkSize, cell[1] - chunk[1] * ChunkSize,
                                 cell[2] - chunk[2] * ChunkSize);
    auto found = chunks.find(chunk);
    if (found == chunks.end()) {
        if (value.block == 0) return;
        found = chunks.emplace(chunk, ChunkCellsArray{}).first;
    }
    found->second[index] = value;
    if (value.block == 0 &&
        std::all_of(found->second.begin(), found->second.end(), [](const Cell& c) { return c.block == 0; }))
        chunks.erase(found);
}

std::vector<Shape> DefaultShapes() {
    return {
        {"cube", {{{0, 0, 0}, {16, 16, 16}}}},
        {"slab", {{{0, 0, 0}, {16, 8, 16}}}},
        {"stair", {{{0, 0, 0}, {16, 8, 16}}, {{0, 8, 8}, {16, 16, 16}}}},
    };
}

Result<GridDocument> ParseGrid(const std::string_view text) {
    try {
        return ParseDocument(text);
    } catch (Invalid& invalid) {
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::InvalidGrid,
                                                     "gyo.voxel_grid: " + std::move(invalid.message),
                                                     std::move(invalid.path)));
    }
}

std::string WriteGrid(const GridDocument& document) {
    std::string out = "{\n  \"schema\": \"" + std::string(GridSchema) + "\",\n  \"version\": " +
                      std::to_string(GridVersion) + ",\n";
    out += "  \"origin\": [" + std::to_string(document.origin[0]) + ", " + std::to_string(document.origin[1]) + ", " +
           std::to_string(document.origin[2]) + "],\n";
    out += "  \"cell_size\": " + FloatText(document.cellSize) + ",\n";
    out += "  \"shapes\": [\n";
    for (std::size_t i = 0; i < document.shapes.size(); ++i) {
        const auto& shape = document.shapes[i];
        out += "    { \"name\": " + Json(shape.name).dump() + ", \"boxes\": [";
        for (std::size_t b = 0; b < shape.boxes.size(); ++b) {
            if (b) out += ", ";
            WriteBox(out, shape.boxes[b]);
        }
        out += "] }";
        out += i + 1 < document.shapes.size() ? ",\n" : "\n";
    }
    out += "  ],\n  \"blocks\": [\n";
    for (std::size_t i = 0; i < document.blocks.size(); ++i) {
        const auto& block = document.blocks[i];
        out += "    { \"name\": " + Json(block.name).dump() + ", \"shape\": " + std::to_string(block.shape) +
               ", \"color\": [" + std::to_string(block.color[0]) + ", " + std::to_string(block.color[1]) + ", " +
               std::to_string(block.color[2]) + ", " + std::to_string(block.color[3]) + "] }";
        out += i + 1 < document.blocks.size() ? ",\n" : "\n";
    }
    out += "  ],\n  \"chunks\": [";
    std::size_t written = 0;
    for (const auto& [at, cells] : document.chunks) {
        out += written++ ? ",\n" : "\n";
        out += "    { \"at\": [" + std::to_string(at[0]) + ", " + std::to_string(at[1]) + ", " + std::to_string(at[2]) +
               "], \"runs\": [";
        std::size_t start = 0;
        bool first = true;
        for (std::size_t i = 1; i <= cells.size(); ++i) {
            if (i < cells.size() && cells[i] == cells[start]) continue;
            if (!first) out += ", ";
            first = false;
            out += "[" + std::to_string(cells[start].block) + ", " + std::to_string(cells[start].orientation) + ", " +
                   std::to_string(i - start) + "]";
            start = i;
        }
        out += "] }";
    }
    out += document.chunks.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return out;
}

} // namespace Gyo::Tools::VoxelMapEditor
