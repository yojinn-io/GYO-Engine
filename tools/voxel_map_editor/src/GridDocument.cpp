#include "voxel_map_editor/Grid.hpp"

#include "JsonRules.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace voxel_map_editor {
namespace {

using namespace JsonRules;

Vec3i ChunkOf(const Vec3i& cell) {
    Vec3i chunk{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        // Floor division, so negative cells land in negative chunks.
        chunk[axis] = cell[axis] >= 0 ? cell[axis] / kChunkSize : -((-cell[axis] + kChunkSize - 1) / kChunkSize);
    }
    return chunk;
}

Check ParseBox(const Json::Value& value, const std::string& path, Box& box) {
    std::optional<Json::ObjectView> object;
    if (Check broken = ExactObject(value, {"min", "max"}, path, "a box", object)) return broken;
    std::array<std::int64_t, 3> minimum{}, maximum{};
    if (Check broken = Triple(Get(*object, "min"), path + ".min", minimum)) return broken;
    if (Check broken = Triple(Get(*object, "max"), path + ".max", maximum)) return broken;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (minimum[axis] < 0 || minimum[axis] > kUnits) return Broken("a shape coordinate is 0 to 16", At(path + ".min", axis));
        if (maximum[axis] < 0 || maximum[axis] > kUnits) return Broken("a shape coordinate is 0 to 16", At(path + ".max", axis));
        if (!(minimum[axis] < maximum[axis])) return Broken("min is below max on every axis", path);
        box.minimum[axis] = static_cast<std::int32_t>(minimum[axis]);
        box.maximum[axis] = static_cast<std::int32_t>(maximum[axis]);
    }
    return std::nullopt;
}

Check ParseShapes(const Json::Value& value, GridDocument& result) {
    const auto shapes = value.AsArray();
    if (!shapes || shapes->Size() == 0 || shapes->Size() > kMaxShapes) return Broken("shapes lists 1 to 256 shapes", "shapes");
    std::set<std::string> names;
    std::size_t i = 0;
    for (const Json::Value entry : *shapes) {
        const auto path = At("shapes", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(entry, {"name", "boxes"}, path, "a shape", object)) return broken;
        Shape shape;
        if (Check broken = Name(Get(*object, "name"), path + ".name", shape.name)) return broken;
        if (!names.insert(shape.name).second) return Broken("a shape name repeats", path + ".name");
        const auto boxes = Get(*object, "boxes").AsArray();
        if (!boxes || boxes->Size() == 0) return Broken("boxes lists one or more boxes", path + ".boxes");
        std::size_t b = 0;
        for (const Json::Value item : *boxes) {
            Box box;
            if (Check broken = ParseBox(item, At(path + ".boxes", b++), box)) return broken;
            shape.boxes.push_back(box);
        }
        result.shapes.push_back(std::move(shape));
    }
    return std::nullopt;
}

Check ParseBlocks(const Json::Value& value, GridDocument& result) {
    const auto blocks = value.AsArray();
    if (!blocks || blocks->Size() == 0 || blocks->Size() > kMaxBlocks) return Broken("blocks lists 1 to 256 blocks", "blocks");
    std::set<std::string> names;
    std::size_t i = 0;
    for (const Json::Value entry : *blocks) {
        const auto path = At("blocks", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(entry, {"name", "shape", "color"}, path, "a block", object)) return broken;
        Block block;
        if (Check broken = Name(Get(*object, "name"), path + ".name", block.name)) return broken;
        if (!names.insert(block.name).second) return Broken("a block name repeats", path + ".name");
        std::int64_t shape = 0;
        if (Check broken = Integer(Get(*object, "shape"), path + ".shape", shape)) return broken;
        if (shape < 0 || shape >= static_cast<std::int64_t>(result.shapes.size()))
            return Broken("shape names a shape", path + ".shape");
        block.shape = static_cast<std::uint32_t>(shape);
        if (Check broken = Colour(Get(*object, "color"), path + ".color", block.color)) return broken;
        result.blocks.push_back(std::move(block));
    }
    return std::nullopt;
}

// The runs at chunkPath.runs: every cell of the chunk, at least one of them a block.
Check ParseRuns(const Json::Value& value, const std::string& chunkPath, const GridDocument& result,
                ChunkCellsArray& cells) {
    const auto path = chunkPath + ".runs";
    const auto runs = value.AsArray();
    if (!runs || runs->Size() == 0) return Broken("runs lists one or more runs", path);
    std::int64_t filled = 0;
    bool anyBlock = false;
    Cell last{};
    std::size_t r = 0;
    for (const Json::Value run : *runs) {
        const auto runPath = At(path, r);
        std::array<std::int64_t, 3> parts{};
        if (Check broken = Integers(run, runPath, "a run is [block, orientation, count]", parts)) return broken;
        const auto [block, orientation, count] = parts;
        if (block < 0 || block > static_cast<std::int64_t>(result.blocks.size()))
            return Broken("block is 0 (empty) or names a block", At(runPath, 0));
        if (orientation < 0 || orientation >= static_cast<std::int64_t>(kOrientationCount))
            return Broken("orientation is 0 to 23", At(runPath, 1));
        if (block == 0 && orientation != 0) return Broken("an empty cell has orientation 0", At(runPath, 1));
        if (count < 1 || filled + count > kChunkCells)
            return Broken("the runs of a chunk cover exactly 4096 cells", At(runPath, 2));
        const Cell cell{static_cast<std::uint16_t>(block), static_cast<std::uint8_t>(orientation)};
        if (r > 0 && cell == last) return Broken("a run repeats the run before it", runPath);
        std::fill_n(cells.begin() + filled, count, cell);
        filled += count;
        anyBlock = anyBlock || block != 0;
        last = cell;
        ++r;
    }
    if (filled != kChunkCells) return Broken("the runs of a chunk cover exactly 4096 cells", path);
    if (!anyBlock) return Broken("a chunk has at least one block", chunkPath);
    return std::nullopt;
}

Check ParseChunks(const Json::Value& value, GridDocument& result) {
    const auto chunks = value.AsArray();
    if (!chunks) return Broken("chunks is an array", "chunks");
    const Vec3i* previous = nullptr;
    std::size_t i = 0;
    for (const Json::Value entry : *chunks) {
        const auto path = At("chunks", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(entry, {"at", "runs"}, path, "a chunk", object)) return broken;
        std::array<std::int64_t, 3> at{};
        if (Check broken = Triple(Get(*object, "at"), path + ".at", at)) return broken;
        Vec3i key{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            // Loose bound only: which cells lie in range is checked per filled cell below.
            if (at[axis] < -kCellLimit || at[axis] > kCellLimit)
                return Broken("a chunk lies beyond +-2^16 cells", At(path + ".at", axis));
            key[axis] = static_cast<std::int32_t>(at[axis]);
        }
        if (previous && !(*previous < key)) return Broken("chunks are in strictly increasing order", path + ".at");
        ChunkCellsArray cells{};
        if (Check broken = ParseRuns(Get(*object, "runs"), path, result, cells)) return broken;
        // Every filled cell, after adding origin, lies within [-2^16, 2^16 - 1].
        for (std::int32_t y = 0; y < kChunkSize; ++y)
            for (std::int32_t z = 0; z < kChunkSize; ++z)
                for (std::int32_t x = 0; x < kChunkSize; ++x) {
                    if (cells[CellIndex(x, y, z)].block == 0) continue;
                    const std::array<std::int64_t, 3> cell{key[0] * std::int64_t{kChunkSize} + x + result.origin[0],
                                                           key[1] * std::int64_t{kChunkSize} + y + result.origin[1],
                                                           key[2] * std::int64_t{kChunkSize} + z + result.origin[2]};
                    for (std::size_t axis = 0; axis < 3; ++axis)
                        if (cell[axis] < -kCellLimit || cell[axis] > kCellLimit - 1)
                            return Broken("a cell lies beyond [-2^16, 2^16 - 1] after adding origin", path);
                }
        const auto inserted = result.chunks.emplace(key, cells);
        previous = &inserted.first->first;
    }
    return std::nullopt;
}

Check ParseDocument(const std::string_view text, GridDocument& result) {
    if (text.size() > kMaxGridTextBytes) return Broken("the text exceeds 16 MiB", "");
    auto document = Json::Parse(text);
    if (!document) return Broken("not JSON: " + GYO::Engine::Base::Describe(document.error()), "");
    std::optional<Json::ObjectView> root;
    if (Check broken = ExactObject(document->Root(), {"schema", "version", "origin", "cell_size", "shapes", "blocks", "chunks"},
                                   "", "the document", root))
        return broken;
    const auto schema = Get(*root, "schema").AsString();
    if (!schema || *schema != kGridSchema) return Broken("schema is '" + std::string(kGridSchema) + "'", "schema");
    std::int64_t version = 0;
    if (Integer(Get(*root, "version"), "version", version) || version != kGridVersion)
        return Broken("version is " + std::to_string(kGridVersion), "version");

    std::array<std::int64_t, 3> origin{};
    if (Check broken = Triple(Get(*root, "origin"), "origin", origin)) return broken;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (origin[axis] < -kCellLimit || origin[axis] > kCellLimit)
            return Broken("origin is within +-2^16 cells", At("origin", axis));
        result.origin[axis] = static_cast<std::int32_t>(origin[axis]);
    }
    if (result.origin[1] != 0) return Broken("origin y is 0: the floor is the y = 0 plane", At("origin", 1));
    const auto cellSize = Get(*root, "cell_size").AsDouble();
    if (!cellSize) return Broken("cell_size is a number", "cell_size");
    result.cellSize = static_cast<float>(*cellSize);
    if (!(result.cellSize >= kMinCellSize) || !(result.cellSize <= kMaxCellSize))
        return Broken("cell_size is within [2^-16, 2^16]", "cell_size");

    if (Check broken = ParseShapes(Get(*root, "shapes"), result)) return broken;
    if (Check broken = ParseBlocks(Get(*root, "blocks"), result)) return broken;
    return ParseChunks(Get(*root, "chunks"), result);
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
    return found->second[CellIndex(cell[0] - chunk[0] * kChunkSize, cell[1] - chunk[1] * kChunkSize,
                                   cell[2] - chunk[2] * kChunkSize)];
}

void GridDocument::Set(const Vec3i& cell, Cell value) {
    // An empty cell has no orientation: the format's one form of "empty".
    if (value.block == 0) value.orientation = 0;
    const auto chunk = ChunkOf(cell);
    const auto index = CellIndex(cell[0] - chunk[0] * kChunkSize, cell[1] - chunk[1] * kChunkSize,
                                 cell[2] - chunk[2] * kChunkSize);
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
    GridDocument result;
    if (Check broken = ParseDocument(text, result))
        return GYO::Engine::Base::Err(
            Error::Make(ErrorCode::InvalidGrid, "gyo.voxel_grid: " + std::move(broken->message), std::move(broken->path)));
    return result;
}

std::string WriteGrid(const GridDocument& document) {
    std::string out = "{\n  \"schema\": \"" + std::string(kGridSchema) + "\",\n  \"version\": " +
                      std::to_string(kGridVersion) + ",\n";
    out += "  \"origin\": [" + std::to_string(document.origin[0]) + ", " + std::to_string(document.origin[1]) + ", " +
           std::to_string(document.origin[2]) + "],\n";
    out += "  \"cell_size\": " + FloatText(document.cellSize) + ",\n";
    out += "  \"shapes\": [\n";
    for (std::size_t i = 0; i < document.shapes.size(); ++i) {
        const auto& shape = document.shapes[i];
        out += "    { \"name\": " + Quoted(shape.name) + ", \"boxes\": [";
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
        out += "    { \"name\": " + Quoted(block.name) + ", \"shape\": " + std::to_string(block.shape) +
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

} // namespace voxel_map_editor
