#include "voxel_map_editor/ChunkedMap.hpp"

#include "JsonRules.hpp"

#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

namespace voxel_map_editor {
namespace {

namespace VM = GYO::Engine::VoxelMap;
namespace Base = GYO::Engine::Base;

// The grid's chunks are the map's chunks, in the same units.
static_assert(kChunkSize == VM::kChunkCells);
static_assert(kUnits == VM::kUnitsPerCell);

// The direction brightness the bake multiplies a block's colour by, in percent, in the map's face order
// (+X, -X, +Y, -Y, +Z, -Z).
constexpr std::array<std::uint32_t, VM::kFaceCount> kFaceBrightnessPercent{80, 80, 100, 50, 65, 65};

const std::string kPlaceholderSha(64, '0');

VM::VoxelChunkKey KeyOf(const Vec3i& at) { return {at[0], at[1], at[2]}; }

std::uint8_t Shade(const std::uint8_t channel, const std::uint32_t percent) {
    return static_cast<std::uint8_t>((channel * percent + 50u) / 100u);
}

std::string Ints(const std::initializer_list<std::int64_t> values) {
    std::string out = "[";
    bool first = true;
    for (const auto v : values) {
        if (!first) out += ", ";
        first = false;
        out += std::to_string(v);
    }
    return out + "]";
}

// Whether a shape's boxes cover a whole side of the cell (the map's face order: +X, -X, +Y, -Y, +Z, -Z).
bool CoversSide(const std::vector<Box>& boxes, const int side) {
    const auto axis = static_cast<std::size_t>(side / 2);
    const bool positive = side % 2 == 0;
    const std::size_t u = axis == 0 ? 2 : 0; // the two in-plane axes
    const std::size_t v = axis == 1 ? 2 : 1;
    std::array<bool, kUnits * kUnits> covered{};
    for (const auto& box : boxes) {
        if ((positive ? box.maximum[axis] : box.minimum[axis]) != (positive ? kUnits : 0)) continue;
        for (int a = box.minimum[u]; a < box.maximum[u]; ++a)
            for (int b = box.minimum[v]; b < box.maximum[v]; ++b) covered[static_cast<std::size_t>(a * kUnits + b)] = true;
    }
    return std::all_of(covered.begin(), covered.end(), [](const bool c) { return c; });
}

// A shape turned to one orientation, and whether it covers each whole side of its cell; computed when first asked for.
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
            result.coversSide[static_cast<std::size_t>(side)] = CoversSide(result.boxes, side);
        return turned_.emplace(key, std::move(result)).first->second;
    }

private:
    const GridDocument& grid_;
    std::map<std::pair<std::uint32_t, std::uint8_t>, TurnedShape> turned_;
};

// The in-plane axes of a face direction, as the map's quads use them: X faces (Z, Y), Y faces (X, Z), Z faces (X, Y).
constexpr std::array<std::array<std::size_t, 2>, 3> kInPlane{{{2, 1}, {0, 2}, {0, 1}}};

using Rectangle = std::array<std::int32_t, 4>; // u0, v0, u1, v1

// Merges rectangles of one plane into fewer rectangles with the same union and no overlap. Coordinates are compressed
// to the rectangles' own edges; the scan goes row by row along v, then along u; a rectangle grows along u as far as it
// can, then along v while the whole row below is free. Integers only, so the result is the same on every platform.
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
    const auto axis = static_cast<std::size_t>(dir / 2);
    std::array<std::int32_t, 3> corner{};
    corner[axis] = plane;
    corner[kInPlane[axis][0]] = r[0];
    corner[kInPlane[axis][1]] = r[1];
    return {corner[0], corner[1], corner[2], static_cast<VM::VoxelFace>(dir), r[2] - r[0], r[3] - r[1], palette};
}

using QuadKey = std::array<std::int32_t, 7>;
QuadKey Ordered(const VM::VoxelQuad& q) {
    return {q.x, q.y, q.z, static_cast<std::int32_t>(q.face), q.width, q.height, static_cast<std::int32_t>(q.palette)};
}

// One chunk's quads and boxes in units from its minimum corner; nullopt when it has no filled cell.
std::optional<VM::VoxelChunkDocument> BakeChunk(const GridDocument& grid, ShapeCache& shapes, const Vec3i& at) {
    const auto found = grid.chunks.find(at);
    if (found == grid.chunks.end()) return std::nullopt;
    const auto& cells = found->second;
    // (dir, plane, palette) -> the faces left after culling, in its plane.
    std::map<std::tuple<int, std::int32_t, std::uint32_t>, std::vector<Rectangle>> faces;
    std::vector<Box> solids;
    for (std::int32_t y = 0; y < kChunkSize; ++y)
        for (std::int32_t z = 0; z < kChunkSize; ++z)
            for (std::int32_t x = 0; x < kChunkSize; ++x) {
                const auto& cell = cells[CellIndex(x, y, z)];
                if (cell.block == 0) continue;
                const Vec3i local{x, y, z};
                const Vec3i position{at[0] * kChunkSize + x, at[1] * kChunkSize + y, at[2] * kChunkSize + z};
                const auto& shape = shapes.Get(cell);
                const auto palette = static_cast<std::uint32_t>(cell.block - 1u);
                for (const auto& part : shape.boxes) {
                    Box box;
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        box.minimum[axis] = local[axis] * kUnits + part.minimum[axis];
                        box.maximum[axis] = local[axis] * kUnits + part.maximum[axis];
                    }
                    solids.push_back(box);
                    for (int dir = 0; dir < 6; ++dir) {
                        const auto axis = static_cast<std::size_t>(dir / 2);
                        const bool positive = dir % 2 == 0;
                        const bool onSide = positive ? part.maximum[axis] == kUnits : part.minimum[axis] == 0;
                        if (onSide) {
                            Vec3i neighbour = position;
                            neighbour[axis] += positive ? 1 : -1;
                            const auto other = grid.At(neighbour);
                            // The neighbour's side that touches this face (cullface).
                            if (other.block != 0 && shapes.Get(other).coversSide[static_cast<std::size_t>(dir ^ 1)])
                                continue;
                        }
                        const auto u = kInPlane[axis][0];
                        const auto v = kInPlane[axis][1];
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
        for (std::size_t dir = 0; dir < VM::kFaceCount; ++dir) {
            const auto percent = kFaceBrightnessPercent[dir];
            entry.faces[dir] = {Shade(block.color[0], percent), Shade(block.color[1], percent),
                                Shade(block.color[2], percent), block.color[3]};
        }
        palette.push_back(std::move(entry));
    }
    return palette;
}

struct RegionEntry final {
    VM::VoxelRegionKey at{};
    std::string file;
    std::string sha256;
};

// The index text. Keys in the contract's order; one line per field, palette entry and region.
std::string IndexText(const GYO::Engine::Math::Vec3i& origin, const float cellSize,
                      const std::vector<VM::VoxelPaletteEntry>& palette, const VM::VoxelChunkKey& minimum,
                      const VM::VoxelChunkKey& maximum, const std::vector<RegionEntry>& regions,
                      const std::string& sourceSha256) {
    std::string out = "{\n  \"schema\": \"" + std::string(VM::kMapSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(VM::kMapVersion) + ",\n";
    out += "  \"source_sha256\": \"" + sourceSha256 + "\",\n";
    out += "  \"build_version\": \"" + std::string(kMapBuildVersion) + "\",\n";
    out += "  \"origin\": " + Ints({origin.x, origin.y, origin.z}) + ",\n";
    out += "  \"cell_size\": " + JsonRules::FloatText(cellSize) + ",\n";
    out += "  \"chunk_size\": " + std::to_string(VM::kChunkCells) + ",\n";
    out += "  \"region_size\": " + std::to_string(VM::kRegionChunks) + ",\n";
    out += "  \"bounds\": { \"min\": " + Ints({minimum.x, minimum.y, minimum.z}) +
           ", \"max\": " + Ints({maximum.x, maximum.y, maximum.z}) + " },\n";
    out += "  \"palette\": [";
    for (std::size_t i = 0; i < palette.size(); ++i) {
        out += i ? ",\n" : "\n";
        out += "    { \"name\": " + JsonRules::Quoted(palette[i].name) + ", \"faces\": [";
        for (std::size_t dir = 0; dir < VM::kFaceCount; ++dir) {
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
    std::string out = "{\n  \"schema\": \"" + std::string(VM::kRegionSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(VM::kRegionVersion) + ",\n";
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
    return std::string(kMapRegionDirectory) + "/r_" + std::to_string(at.x) + "_" + std::to_string(at.z) + "_" +
           sha256.substr(0, 16) + ".json";
}

Error Refused(const std::string& what, const VM::VoxelMapError& error) {
    return Error::Make(ErrorCode::BuildRefused, "the voxel_map Module refuses " + what + ": " + error.message,
                       Base::CauseDetail(error));
}

// The index for a set of chunks, read back through the Module (placeholders for what only WriteMap knows).
Result<VM::VoxelMapDocument> PlaceholderIndex(const GridDocument& grid, const std::vector<VM::VoxelPaletteEntry>& palette,
                                              const std::vector<Vec3i>& keys) {
    if (keys.empty()) return Base::Err(Error::Make(ErrorCode::EmptyMap, "the grid has no filled cell", ""));
    VM::VoxelChunkKey minimum = KeyOf(keys.front()), maximum = minimum;
    std::set<VM::VoxelRegionKey> regionKeys;
    for (const auto& key : keys) {
        minimum = {std::min(minimum.x, key[0]), std::min(minimum.y, key[1]), std::min(minimum.z, key[2])};
        maximum = {std::max(maximum.x, key[0]), std::max(maximum.y, key[1]), std::max(maximum.z, key[2])};
        regionKeys.insert(VM::RegionOf(KeyOf(key)));
    }
    std::vector<RegionEntry> regions;
    for (const auto& at : regionKeys) regions.push_back({at, RegionFile(at, kPlaceholderSha), kPlaceholderSha});
    const auto text = IndexText({grid.origin[0], grid.origin[1], grid.origin[2]}, grid.cellSize, palette, minimum,
                                maximum, regions, kPlaceholderSha);
    auto index = VM::ParseVoxelMap(text);
    if (!index) return Base::Err(Refused("the index", index.error()));
    return std::move(index).value();
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
    if (!index) return Base::Err(std::move(index).error());
    for (const auto& [at, chunk] : chunks)
        if (auto valid = VM::ValidateVoxelChunk(chunk, *index); !valid) return Base::Err(Refused("a chunk", valid.error()));
    return BakedMap{std::move(index).value(), std::move(chunks)};
}

Result<MapText> WriteMap(const BakedMap& map, const std::span<const std::byte> sourceBytes) {
    std::map<VM::VoxelRegionKey, std::vector<const VM::VoxelChunkDocument*>> byRegion;
    for (const auto& [at, chunk] : map.chunks) byRegion[VM::RegionOf(chunk.at)].push_back(&chunk);
    MapText text;
    std::vector<RegionEntry> entries;
    for (const auto& [at, chunks] : byRegion) {
        auto regionText = RegionText(at, chunks);
        const auto sha = Base::Sha256(AsBytes(regionText));
        entries.push_back({at, RegionFile(at, sha), sha});
        text.regions.push_back({entries.back().file, std::move(regionText)});
    }
    const auto sourceSha256 = Base::Sha256(sourceBytes);
    text.index = IndexText(map.index.origin, map.index.cellSize, map.index.palette, map.index.boundsMinimum,
                           map.index.boundsMaximum, entries, sourceSha256);

    // One validator: read every text back through the Module and compare it with what was baked.
    auto index = VM::ParseVoxelMap(text.index);
    if (!index) return Base::Err(Refused("the index", index.error()));
    const auto differs = [](const std::string& what) {
        return Base::Err(Error::Make(ErrorCode::BuildRefused, "the " + what + " reads back different", ""));
    };
    const auto sameEntries = std::equal(index->regions.begin(), index->regions.end(), entries.begin(), entries.end(),
                                        [](const VM::VoxelMapRegion& read, const RegionEntry& written) {
                                            return read.at == written.at && read.file == written.file &&
                                                   read.sha256 == written.sha256;
                                        });
    if (!sameEntries || index->sourceSha256 != sourceSha256 || index->buildVersion != kMapBuildVersion ||
        index->palette != map.index.palette || index->cellSize != map.index.cellSize ||
        index->boundsMinimum != map.index.boundsMinimum || index->boundsMaximum != map.index.boundsMaximum ||
        index->origin != map.index.origin)
        return differs("index");
    std::size_t compared = 0;
    for (std::size_t r = 0; r < text.regions.size(); ++r) {
        auto region = VM::ParseVoxelRegion(text.regions[r].text, *index, entries[r].at);
        if (!region) return Base::Err(Refused(text.regions[r].file, region.error()));
        for (const auto& read : region->chunks) {
            const auto baked = map.chunks.find(Vec3i{read.at.x, read.at.y, read.at.z});
            if (baked == map.chunks.end() || read != baked->second) return differs(text.regions[r].file);
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
    return rest.size() == 16 && std::all_of(rest.begin(), rest.end(), [](const char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

Result<void> WriteRegionFiles(const std::string& directory, const MapText& text) {
    for (const auto& region : text.regions) {
        const auto path = directory + "/" + region.file;
        if (auto existing = ReadFile(path, VM::kMaxRegionTextBytes + 1); existing && AsText(*existing) == region.text)
            continue;
        if (auto written = WriteFileReplacing(path, region.text); !written) return written;
    }
    return {};
}

Result<void> ReplaceMapIndex(const std::string& directory, const MapText& text) {
    return WriteFileReplacing(directory + "/" + std::string(kMapIndexFile), text.index);
}

std::vector<std::string> ListedRegionFiles(const std::string& directory) {
    auto bytes = ReadFile(directory + "/" + std::string(kMapIndexFile), VM::kMaxMapTextBytes + 1);
    if (!bytes) return {};
    const auto index = VM::ParseVoxelMap(AsText(*bytes));
    if (!index) return {};
    std::vector<std::string> files;
    for (const auto& region : index->regions) files.push_back(region.file);
    return files;
}

Result<void> RemoveStaleRegionFiles(const std::string& directory, const MapText& text,
                                    const std::span<const std::string> previous) {
    std::set<std::string> listed;
    for (const auto& region : text.regions) listed.insert(region.file);
    const auto prefix = std::string(kMapRegionDirectory) + "/";
    for (const auto& file : previous) {
        if (listed.contains(file) || !file.starts_with(prefix) ||
            !IsRegionFileName(std::string_view(file).substr(prefix.size())))
            continue;
        if (auto removed = RemoveFileIfPresent(directory + "/" + file); !removed) return removed;
    }
    return {};
}

Result<void> SaveMap(const std::string& directory, const MapText& text) {
    const auto previous = ListedRegionFiles(directory);
    if (auto written = WriteRegionFiles(directory, text); !written) return written;
    if (auto replaced = ReplaceMapIndex(directory, text); !replaced) return replaced;
    return RemoveStaleRegionFiles(directory, text, previous);
}

Result<MapSummary> ReadMapSummary(const std::string& directory) {
    auto bytes = ReadFile(directory + "/" + std::string(kMapIndexFile), VM::kMaxMapTextBytes + 1);
    if (!bytes) return Base::Err(std::move(bytes).error());
    auto index = VM::ParseVoxelMap(AsText(*bytes));
    if (!index) return Base::Err(Refused("the index", index.error()));
    MapSummary summary{std::move(index).value(), 0, 0, 0, bytes->size()};
    for (const auto& entry : summary.index.regions) {
        auto regionBytes = ReadFile(directory + "/" + entry.file, VM::kMaxRegionTextBytes + 1);
        if (!regionBytes) return Base::Err(std::move(regionBytes).error());
        auto region = VM::ParseVoxelRegion(AsText(*regionBytes), summary.index, entry.at);
        if (!region) return Base::Err(Refused(entry.file, region.error()));
        summary.bytes += regionBytes->size();
        for (const auto& chunk : region->chunks) {
            ++summary.chunks;
            summary.quads += chunk.quads.size();
            summary.colliders += chunk.colliders.size();
        }
    }
    return summary;
}

} // namespace voxel_map_editor
