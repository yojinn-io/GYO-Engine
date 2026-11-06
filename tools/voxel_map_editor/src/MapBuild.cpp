#include "gyo/voxel_map_editor/ChunkedMap.hpp"

#include "BuildParts.hpp"
#include "JsonRules.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace VM = Engine::VoxelMesh;
using BuildParts::Ints;
using BuildParts::Quad;

// The grid's chunks are Engine's chunks, in the same units.
static_assert(ChunkSize == VM::ChunkCells);
static_assert(Units == VM::UnitsPerCell);

const std::string PlaceholderSha(64, '0');

std::int32_t FloorDiv(const std::int32_t value, const std::int32_t divisor) {
    const auto q = value / divisor;
    return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? q - 1 : q;
}

VM::VoxelChunkKey KeyOf(const Vec3i& at) { return {at[0], at[1], at[2]}; }

// A shape turned to one orientation, and whether it covers each whole side of
// its cell; computed when first asked for.
struct TurnedShape final {
    std::vector<Box> boxes;
    std::array<bool, 6> coversSide{};
};

class ShapeCache final {
public:
    explicit ShapeCache(const GridDocument& grid) : grid_(grid) {}
    const TurnedShape& Get(const Cell cell) {
        const auto shape = grid_.blocks[cell.block - 1u].shape;
        const auto key = std::make_pair(shape, cell.orientation);
        const auto found = turned_.find(key);
        if (found != turned_.end()) return found->second;
        TurnedShape result;
        for (const auto& box : grid_.shapes[shape].boxes) result.boxes.push_back(RotateBox(box, cell.orientation));
        for (int side = 0; side < 6; ++side)
            result.coversSide[static_cast<std::size_t>(side)] = BuildParts::CoversSide(result.boxes, side);
        return turned_.emplace(key, std::move(result)).first->second;
    }

private:
    const GridDocument& grid_;
    std::map<std::pair<std::uint32_t, std::uint8_t>, TurnedShape> turned_;
};

// The in-plane axes of a face direction, as Engine's quads use them: X faces
// (Z, Y), Y faces (X, Z), Z faces (X, Y).
constexpr std::array<std::array<int, 2>, 3> InPlane{{{2, 1}, {0, 2}, {0, 1}}};

using Rectangle = std::array<std::int32_t, 4>; // u0, v0, u1, v1

// Merges rectangles of one plane into fewer rectangles with the same union and
// no overlap. Coordinates are compressed to the rectangles' own edges; the scan
// goes row by row along v, then along u; a rectangle grows along u as far as
// it can, then along v while the whole row below is free. Integers only, so the
// result is the same on every platform.
std::vector<Rectangle> Greedy(const std::vector<Rectangle>& rectangles) {
    std::vector<std::int32_t> us, vs;
    for (const auto& r : rectangles) {
        us.insert(us.end(), {r[0], r[2]});
        vs.insert(vs.end(), {r[1], r[3]});
    }
    std::sort(us.begin(), us.end());
    us.erase(std::unique(us.begin(), us.end()), us.end());
    std::sort(vs.begin(), vs.end());
    vs.erase(std::unique(vs.begin(), vs.end()), vs.end());
    const auto nu = us.size() - 1;
    const auto nv = vs.size() - 1;
    std::vector<char> open(nu * nv, 0); // filled and not yet in a result
    const auto at = [nu](const std::size_t u, const std::size_t v) { return v * nu + u; };
    const auto indexOf = [](const std::vector<std::int32_t>& edges, const std::int32_t value) {
        return static_cast<std::size_t>(std::lower_bound(edges.begin(), edges.end(), value) - edges.begin());
    };
    for (const auto& r : rectangles) {
        const auto u0 = indexOf(us, r[0]), u1 = indexOf(us, r[2]);
        const auto v0 = indexOf(vs, r[1]), v1 = indexOf(vs, r[3]);
        for (auto v = v0; v < v1; ++v)
            for (auto u = u0; u < u1; ++u) open[at(u, v)] = 1;
    }
    std::vector<Rectangle> result;
    for (std::size_t v = 0; v < nv; ++v) {
        for (std::size_t u = 0; u < nu; ++u) {
            if (!open[at(u, v)]) continue;
            auto u1 = u + 1;
            while (u1 < nu && open[at(u1, v)]) ++u1;
            auto v1 = v + 1;
            while (v1 < nv) {
                bool whole = true;
                for (auto x = u; x < u1 && whole; ++x) whole = open[at(x, v1)] != 0;
                if (!whole) break;
                ++v1;
            }
            for (auto y = v; y < v1; ++y)
                for (auto x = u; x < u1; ++x) open[at(x, y)] = 0;
            result.push_back({us[u], vs[v], us[u1], vs[v1]});
        }
    }
    return result;
}

VM::VoxelQuad QuadOf(const int dir, const std::int32_t plane, const Rectangle& r, const std::uint32_t palette) {
    const int axis = dir / 2;
    std::array<std::int32_t, 3> corner{};
    corner[static_cast<std::size_t>(axis)] = plane;
    corner[static_cast<std::size_t>(InPlane[static_cast<std::size_t>(axis)][0])] = r[0];
    corner[static_cast<std::size_t>(InPlane[static_cast<std::size_t>(axis)][1])] = r[1];
    return {corner[0], corner[1], corner[2], static_cast<VM::VoxelFace>(dir), r[2] - r[0], r[3] - r[1], palette};
}

Quad Ordered(const VM::VoxelQuad& q) {
    return {q.x, q.y, q.z, static_cast<std::int32_t>(q.face), q.width, q.height, static_cast<std::int32_t>(q.palette)};
}

// One chunk's quads and boxes in units from its minimum corner; nullopt when
// it has no filled cell.
std::optional<VM::VoxelChunkDocument> BakeChunk(const GridDocument& grid, ShapeCache& shapes, const Vec3i& at) {
    const auto found = grid.chunks.find(at);
    if (found == grid.chunks.end()) return std::nullopt;
    const auto& cells = found->second;
    // (dir, plane, palette) -> the faces left after culling, in its plane.
    std::map<std::tuple<int, std::int32_t, std::uint32_t>, std::vector<Rectangle>> faces;
    std::vector<Box> solids;
    for (std::int32_t y = 0; y < ChunkSize; ++y)
        for (std::int32_t z = 0; z < ChunkSize; ++z)
            for (std::int32_t x = 0; x < ChunkSize; ++x) {
                const auto& cell = cells[CellIndex(x, y, z)];
                if (cell.block == 0) continue;
                const Vec3i local{x, y, z};
                const Vec3i position{at[0] * ChunkSize + x, at[1] * ChunkSize + y, at[2] * ChunkSize + z};
                const auto& shape = shapes.Get(cell);
                const auto palette = static_cast<std::uint32_t>(cell.block - 1u);
                for (const auto& part : shape.boxes) {
                    Box box;
                    for (int axis = 0; axis < 3; ++axis) {
                        box.minimum[axis] = local[axis] * Units + part.minimum[axis];
                        box.maximum[axis] = local[axis] * Units + part.maximum[axis];
                    }
                    solids.push_back(box);
                    for (int dir = 0; dir < 6; ++dir) {
                        const int axis = dir / 2;
                        const bool positive = dir % 2 == 0;
                        const bool onSide = positive ? part.maximum[axis] == Units : part.minimum[axis] == 0;
                        if (onSide) {
                            Vec3i neighbour = position;
                            neighbour[axis] += positive ? 1 : -1;
                            const auto other = grid.At(neighbour);
                            // The neighbour's side that touches this face (cullface).
                            if (other.block != 0 && shapes.Get(other).coversSide[static_cast<std::size_t>(dir ^ 1)])
                                continue;
                        }
                        const auto& plane = InPlane[static_cast<std::size_t>(axis)];
                        const auto u = static_cast<std::size_t>(plane[0]);
                        const auto v = static_cast<std::size_t>(plane[1]);
                        faces[{dir, positive ? box.maximum[axis] : box.minimum[axis], palette}].push_back(
                            {box.minimum[u], box.minimum[v], box.maximum[u], box.maximum[v]});
                    }
                }
            }
    if (solids.empty()) return std::nullopt;
    VM::VoxelChunkDocument chunk;
    chunk.at = KeyOf(at);
    for (const auto& [group, rectangles] : faces) {
        const auto& [dir, plane, palette] = group;
        for (const auto& r : Greedy(rectangles)) chunk.quads.push_back(QuadOf(dir, plane, r, palette));
    }
    std::sort(chunk.quads.begin(), chunk.quads.end(),
              [](const VM::VoxelQuad& a, const VM::VoxelQuad& b) { return Ordered(a) < Ordered(b); });
    for (const auto& box : MergeBoxes(solids))
        chunk.colliders.push_back({{box.minimum[0], box.minimum[1], box.minimum[2]},
                                   {box.maximum[0], box.maximum[1], box.maximum[2]}});
    return chunk;
}

std::vector<VM::VoxelPaletteEntry> PaletteOf(const GridDocument& grid) {
    std::vector<VM::VoxelPaletteEntry> palette;
    for (const auto& block : grid.blocks) {
        VM::VoxelPaletteEntry entry{block.name, {}};
        for (std::size_t dir = 0; dir < VM::FaceCount; ++dir) {
            const auto percent = FaceBrightnessPercent[dir];
            entry.faces[dir] = {BuildParts::Shade(block.color[0], percent), BuildParts::Shade(block.color[1], percent),
                                BuildParts::Shade(block.color[2], percent), block.color[3]};
        }
        palette.push_back(std::move(entry));
    }
    return palette;
}

bool SamePalette(const std::vector<VM::VoxelPaletteEntry>& a, const std::vector<VM::VoxelPaletteEntry>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const auto& x, const auto& y) { return x.name == y.name && x.faces == y.faces; });
}

std::vector<std::uint32_t> BlockShapes(const GridDocument& grid) {
    std::vector<std::uint32_t> shapes;
    for (const auto& block : grid.blocks) shapes.push_back(block.shape);
    return shapes;
}

bool SameShapes(const std::vector<Shape>& a, const std::vector<Shape>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const Shape& x, const Shape& y) { return x.name == y.name && x.boxes == y.boxes; });
}

bool SameQuad(const VM::VoxelQuad& a, const VM::VoxelQuad& b) { return Ordered(a) == Ordered(b); }
bool SameBox(const VM::VoxelBox& a, const VM::VoxelBox& b) {
    return a.minimum.x == b.minimum.x && a.minimum.y == b.minimum.y && a.minimum.z == b.minimum.z &&
           a.maximum.x == b.maximum.x && a.maximum.y == b.maximum.y && a.maximum.z == b.maximum.z;
}
bool SameChunk(const VM::VoxelChunkDocument& a, const VM::VoxelChunkDocument& b) {
    return a.at == b.at && std::equal(a.quads.begin(), a.quads.end(), b.quads.begin(), b.quads.end(), SameQuad) &&
           std::equal(a.colliders.begin(), a.colliders.end(), b.colliders.begin(), b.colliders.end(), SameBox);
}

struct RegionEntry final {
    VM::VoxelRegionKey at{};
    std::string file;
    std::string sha256;
};

// The index text. Keys in the contract's order; numbers as the single file writes them.
std::string IndexText(const GridDocument& grid, const std::vector<VM::VoxelPaletteEntry>& palette,
                      const VM::VoxelChunkKey& minimum, const VM::VoxelChunkKey& maximum,
                      const std::vector<RegionEntry>& regions, const std::string& sourceSha256, const float cellSize) {
    std::string out = "{\n  \"schema\": \"" + std::string(VM::VoxelMapSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(VM::VoxelMapVersion) + ",\n";
    out += "  \"source_sha256\": \"" + sourceSha256 + "\",\n";
    out += "  \"build_version\": \"" + std::string(MapBuildVersion) + "\",\n";
    out += "  \"origin\": " + Ints({grid.origin[0], grid.origin[1], grid.origin[2]}) + ",\n";
    out += "  \"cell_size\": " + JsonRules::FloatText(cellSize) + ",\n";
    out += "  \"chunk_size\": " + std::to_string(VM::ChunkCells) + ",\n";
    out += "  \"region_size\": " + std::to_string(VM::RegionChunks) + ",\n";
    out += "  \"bounds\": { \"min\": " + Ints({minimum.x, minimum.y, minimum.z}) +
           ", \"max\": " + Ints({maximum.x, maximum.y, maximum.z}) + " },\n";
    out += "  \"palette\": [";
    for (std::size_t i = 0; i < palette.size(); ++i) {
        out += i ? ",\n" : "\n";
        out += "    { \"name\": " + JsonRules::Json(palette[i].name).dump() + ", \"faces\": [";
        for (std::size_t dir = 0; dir < VM::FaceCount; ++dir) {
            const auto& f = palette[i].faces[dir];
            if (dir) out += ", ";
            out += Ints({f[0], f[1], f[2], f[3]});
        }
        out += "] }";
    }
    out += "\n  ],\n  \"regions\": [";
    for (std::size_t i = 0; i < regions.size(); ++i) {
        out += i ? ",\n" : "\n";
        out += "    { \"at\": " + Ints({regions[i].at.x, regions[i].at.z}) + ", \"file\": \"" + regions[i].file +
               "\", \"sha256\": \"" + regions[i].sha256 + "\" }";
    }
    out += "\n  ]\n}\n";
    return out;
}

std::string RegionText(const VM::VoxelRegionKey& at, const std::vector<const VM::VoxelChunkDocument*>& chunks) {
    std::string out = "{\n  \"schema\": \"" + std::string(VM::VoxelRegionSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(VM::VoxelRegionVersion) + ",\n";
    out += "  \"at\": " + Ints({at.x, at.z}) + ",\n";
    out += "  \"chunks\": [";
    for (std::size_t c = 0; c < chunks.size(); ++c) {
        const auto& chunk = *chunks[c];
        out += c ? ",\n" : "\n";
        out += "    { \"at\": " + Ints({chunk.at.x, chunk.at.y, chunk.at.z}) + ",\n      \"quads\": [";
        for (std::size_t i = 0; i < chunk.quads.size(); ++i) {
            const auto q = Ordered(chunk.quads[i]);
            out += i ? ",\n        " : "\n        ";
            out += Ints({q[0], q[1], q[2], q[3], q[4], q[5], q[6]});
        }
        out += chunk.quads.empty() ? "],\n" : "\n      ],\n";
        out += "      \"colliders\": [";
        for (std::size_t i = 0; i < chunk.colliders.size(); ++i) {
            const auto& b = chunk.colliders[i];
            out += i ? ",\n        " : "\n        ";
            out += "{ \"min\": " + Ints({b.minimum.x, b.minimum.y, b.minimum.z}) +
                   ", \"max\": " + Ints({b.maximum.x, b.maximum.y, b.maximum.z}) + " }";
        }
        out += chunk.colliders.empty() ? "] }" : "\n      ] }";
    }
    out += "\n  ]\n}\n";
    return out;
}

std::string RegionFile(const VM::VoxelRegionKey& at, const std::string& sha256) {
    return std::string(MapRegionDirectory) + "/r_" + std::to_string(at.x) + "_" + std::to_string(at.z) + "_" +
           sha256.substr(0, 16) + ".json";
}

std::string Sha256Of(const std::string_view text) {
    return Engine::Base::Sha256({reinterpret_cast<const std::byte*>(text.data()), text.size()});
}

VoxelMapError Refused(const std::string& what, const VM::VoxelMeshError& error) {
    return VoxelMapError::Make(VoxelMapErrorCode::BuildRefused, "Engine refuses " + what + ": " + error.message,
                               error.detail);
}

// The index for a set of chunks, read back through Engine (placeholders for
// what only WriteMap knows).
Result<VM::VoxelMapDocument> PlaceholderIndex(const GridDocument& grid,
                                              const std::vector<VM::VoxelPaletteEntry>& palette,
                                              const std::vector<Vec3i>& keys) {
    if (keys.empty())
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::EmptyMap, "the grid has no filled cell", ""));
    VM::VoxelChunkKey minimum = KeyOf(keys.front()), maximum = minimum;
    std::set<VM::VoxelRegionKey> regionKeys;
    for (const auto& key : keys) {
        minimum = {std::min(minimum.x, key[0]), std::min(minimum.y, key[1]), std::min(minimum.z, key[2])};
        maximum = {std::max(maximum.x, key[0]), std::max(maximum.y, key[1]), std::max(maximum.z, key[2])};
        regionKeys.insert(VM::RegionOf(KeyOf(key)));
    }
    std::vector<RegionEntry> regions;
    for (const auto& at : regionKeys) regions.push_back({at, RegionFile(at, PlaceholderSha), PlaceholderSha});
    const auto text = IndexText(grid, palette, minimum, maximum, regions, PlaceholderSha, grid.cellSize);
    auto index = VM::ParseVoxelMap(text);
    if (!index) return Engine::Base::Err(Refused("the index", index.error()));
    return std::move(index).value();
}

bool SameRegionKeys(const VM::VoxelMapDocument& a, const VM::VoxelMapDocument& b) {
    return std::equal(a.regions.begin(), a.regions.end(), b.regions.begin(), b.regions.end(),
                      [](const auto& x, const auto& y) { return x.at == y.at; });
}

} // namespace

Result<BakedMap> BakeMap(const GridDocument& grid) {
    ShapeCache shapes(grid);
    std::map<Vec3i, VM::VoxelChunkDocument> chunks;
    for (const auto& [at, cells] : grid.chunks)
        if (auto chunk = BakeChunk(grid, shapes, at)) chunks.emplace(at, std::move(*chunk));
    std::vector<Vec3i> keys;
    for (const auto& [at, chunk] : chunks) keys.push_back(at);
    auto index = PlaceholderIndex(grid, PaletteOf(grid), keys);
    if (!index) return Engine::Base::Err(std::move(index).error());
    for (const auto& [at, chunk] : chunks)
        if (auto valid = VM::ValidateVoxelChunk(chunk, *index); !valid)
            return Engine::Base::Err(Refused("a chunk", valid.error()));
    return BakedMap{std::move(index).value(), std::move(chunks), grid.shapes, BlockShapes(grid)};
}

Result<void> RebakeChunks(const GridDocument& grid, BakedMap& map, const std::span<const Vec3i> keys) {
    const auto palette = PaletteOf(grid);
    if (!SamePalette(palette, map.index.palette) || !SameShapes(grid.shapes, map.shapes) ||
        BlockShapes(grid) != map.blockShapes || grid.cellSize != map.index.cellSize ||
        grid.origin[0] != map.index.origin.x || grid.origin[1] != map.index.origin.y ||
        grid.origin[2] != map.index.origin.z) {
        auto whole = BakeMap(grid);
        if (!whole) return Engine::Base::Err(std::move(whole).error());
        map = std::move(whole).value();
        return {};
    }
    ShapeCache shapes(grid);
    std::map<Vec3i, std::optional<VM::VoxelChunkDocument>> baked;
    for (const auto& key : keys) baked[key] = BakeChunk(grid, shapes, key);

    // The chunks after the change, for the bounds and the regions.
    std::vector<Vec3i> after;
    for (const auto& [at, chunk] : map.chunks)
        if (!baked.contains(at)) after.push_back(at);
    for (const auto& [at, chunk] : baked)
        if (chunk) after.push_back(at);
    std::sort(after.begin(), after.end());
    auto index = PlaceholderIndex(grid, palette, after);
    if (!index) return Engine::Base::Err(std::move(index).error());
    const bool sameIndex = index->boundsMinimum == map.index.boundsMinimum &&
                           index->boundsMaximum == map.index.boundsMaximum && SameRegionKeys(*index, map.index);
    for (const auto& [at, chunk] : baked)
        if (chunk)
            if (auto valid = VM::ValidateVoxelChunk(*chunk, *index); !valid)
                return Engine::Base::Err(Refused("a chunk", valid.error()));
    if (!sameIndex) map.index = std::move(index).value();
    for (auto& [at, chunk] : baked) {
        if (chunk) map.chunks.insert_or_assign(at, std::move(*chunk));
        else map.chunks.erase(at);
    }
    return {};
}

std::set<Vec3i> ChunksTouchedBy(const std::span<const Vec3i> cells) {
    std::set<Vec3i> keys;
    for (const auto& cell : cells) {
        const Vec3i key{FloorDiv(cell[0], ChunkSize), FloorDiv(cell[1], ChunkSize), FloorDiv(cell[2], ChunkSize)};
        keys.insert(key);
        for (int axis = 0; axis < 3; ++axis) {
            const auto local = cell[axis] - key[axis] * ChunkSize;
            Vec3i across = key;
            if (local == 0) {
                across[axis] -= 1;
                keys.insert(across);
            } else if (local == ChunkSize - 1) {
                across[axis] += 1;
                keys.insert(across);
            }
        }
    }
    return keys;
}

Result<MapText> WriteMap(const BakedMap& map, const std::span<const std::byte> sourceBytes) {
    std::map<VM::VoxelRegionKey, std::vector<const VM::VoxelChunkDocument*>> byRegion;
    for (const auto& [at, chunk] : map.chunks) byRegion[VM::RegionOf(chunk.at)].push_back(&chunk);
    MapText text;
    std::vector<RegionEntry> entries;
    for (const auto& [at, chunks] : byRegion) {
        auto regionText = RegionText(at, chunks);
        const auto sha = Sha256Of(regionText);
        entries.push_back({at, RegionFile(at, sha), sha});
        text.regions.push_back({entries.back().file, std::move(regionText)});
    }
    GridDocument header; // only origin is read from it
    header.origin = {map.index.origin.x, map.index.origin.y, map.index.origin.z};
    text.index = IndexText(header, map.index.palette, map.index.boundsMinimum, map.index.boundsMaximum, entries,
                           Engine::Base::Sha256(sourceBytes), map.index.cellSize);

    // One validator (Design Invariant 2): read every text back through Engine
    // and compare it with what was baked.
    auto index = VM::ParseVoxelMap(text.index);
    if (!index) return Engine::Base::Err(Refused("the index", index.error()));
    const auto differs = [](const std::string& what) {
        return Engine::Base::Err(
            VoxelMapError::Make(VoxelMapErrorCode::BuildRefused, "the " + what + " reads back different", ""));
    };
    const auto sameEntries = std::equal(
        index->regions.begin(), index->regions.end(), entries.begin(), entries.end(),
        [](const VM::VoxelMapRegion& read, const RegionEntry& written) {
            return read.at == written.at && read.file == written.file && read.sha256 == written.sha256;
        });
    if (!sameEntries || index->sourceSha256 != Engine::Base::Sha256(sourceBytes) ||
        index->buildVersion != MapBuildVersion || !SamePalette(index->palette, map.index.palette) ||
        index->cellSize != map.index.cellSize ||
        index->boundsMinimum != map.index.boundsMinimum || index->boundsMaximum != map.index.boundsMaximum ||
        index->origin.x != map.index.origin.x || index->origin.y != map.index.origin.y ||
        index->origin.z != map.index.origin.z)
        return differs("index");
    std::size_t compared = 0;
    for (std::size_t r = 0; r < text.regions.size(); ++r) {
        auto region = VM::ParseVoxelRegion(text.regions[r].text, *index, entries[r].at);
        if (!region) return Engine::Base::Err(Refused(text.regions[r].file, region.error()));
        for (const auto& read : region->chunks) {
            const auto baked = map.chunks.find(Vec3i{read.at.x, read.at.y, read.at.z});
            if (baked == map.chunks.end() || !SameChunk(read, baked->second)) return differs(text.regions[r].file);
            ++compared;
        }
    }
    if (compared != map.chunks.size()) return differs("map");
    return text;
}

bool IsRegionFileName(const std::string_view name) noexcept {
    // r_<x>_<z>_<16 lowercase hexadecimal digits>.json, x and z decimal integers.
    if (!name.starts_with("r_") || !name.ends_with(".json")) return false;
    auto rest = name.substr(2, name.size() - 2 - 5);
    const auto integer = [&rest]() {
        std::size_t i = 0;
        if (i < rest.size() && rest[i] == '-') ++i;
        const auto digits = i;
        while (i < rest.size() && rest[i] >= '0' && rest[i] <= '9') ++i;
        if (i == digits || i >= rest.size() || rest[i] != '_') return false;
        rest = rest.substr(i + 1);
        return true;
    };
    if (!integer() || !integer()) return false;
    return rest.size() == 16 &&
           std::all_of(rest.begin(), rest.end(), [](const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

Result<void> WriteRegionFiles(const std::string& directory, const MapText& text) {
    for (const auto& region : text.regions) {
        const auto path = directory + "/" + region.file;
        if (auto existing = ReadFile(path, VM::MaxRegionTextBytes + 1);
            existing && std::string_view(reinterpret_cast<const char*>(existing->data()), existing->size()) == region.text)
            continue;
        if (auto written = WriteFileReplacing(path, region.text); !written) return written;
    }
    return {};
}

Result<void> ReplaceMapIndex(const std::string& directory, const MapText& text) {
    return WriteFileReplacing(directory + "/" + std::string(MapIndexFile), text.index);
}

std::vector<std::string> ListedRegionFiles(const std::string& directory) {
    auto bytes = ReadFile(directory + "/" + std::string(MapIndexFile), VM::MaxMapTextBytes + 1);
    if (!bytes) return {};
    const auto index = VM::ParseVoxelMap({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
    if (!index) return {};
    std::vector<std::string> files;
    for (const auto& region : index->regions) files.push_back(region.file);
    return files;
}

Result<void> RemoveStaleRegionFiles(const std::string& directory, const MapText& text,
                                    const std::span<const std::string> previous) {
    namespace Io = Engine::IO;
    std::set<std::string> listed;
    for (const auto& region : text.regions) listed.insert(region.file);
    const auto prefix = std::string(MapRegionDirectory) + "/";
    Io::FS::NativeFileSystem files;
    for (const auto& file : previous) {
        if (listed.contains(file) || !file.starts_with(prefix) || !IsRegionFileName(std::string_view(file).substr(prefix.size())))
            continue;
        const auto path = directory + "/" + file;
        auto target = Io::FS::NativeFileUri(path);
        if (!target)
            return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::WriteFailed, "cannot remove",
                                                         Engine::Base::CauseDetail(target.error(), path)));
        auto removed = files.Remove(target.value(), {});
        if (!removed && removed.error().code != Io::IoErrorCode::NotFound)
            return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::WriteFailed, "cannot remove",
                                                         Engine::Base::CauseDetail(removed.error(), path)));
    }
    return {};
}

Result<void> SaveMap(const std::string& directory, const MapText& text) {
    const auto previous = ListedRegionFiles(directory);
    if (auto written = WriteRegionFiles(directory, text); !written) return written;
    if (auto replaced = ReplaceMapIndex(directory, text); !replaced) return replaced;
    return RemoveStaleRegionFiles(directory, text, previous);
}

} // namespace Gyo::Tools::VoxelMapEditor
