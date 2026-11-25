#include "engine/voxel_map/VoxelMap.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/json/Document.hpp"

namespace GYO::Engine::VoxelMap {

namespace {

// The first broken rule: its message (starting with the rule's name) and the path of the offending value.
struct Problem final {
    std::string message;
    std::string path;
};
using Check = std::optional<Problem>;

Check Broken(std::string message, std::string path) { return Problem{std::move(message), std::move(path)}; }

std::string Index(const std::string& path, const std::size_t i) { return path + "[" + std::to_string(i) + "]"; }
std::string Field(const std::string& path, const std::string_view key) {
    return path.empty() ? std::string(key) : path + "." + std::string(key);
}

// --- The JSON layer: kinds, exact fields and the integer range. ---------------------------------------------------

// The object at path with exactly these keys (repeated keys are refused by Json::Parse).
Check ExactObject(const Json::Value& value, const std::initializer_list<std::string_view> keys, const std::string& path,
                  const std::string_view rule, std::optional<Json::ObjectView>& out) {
    const auto object = value.AsObject();
    if (!object) return Broken(std::string(rule) + ": not an object", path);
    if (object->Size() != keys.size()) return Broken(std::string(rule) + ": the fields are not exactly the contract's", path);
    for (const std::string_view key : keys) {
        if (!object->Find(key)) return Broken(std::string(rule) + ": the field " + std::string(key) + " is missing", path);
    }
    out = *object;
    return std::nullopt;
}

Json::Value Get(const Json::ObjectView& object, const std::string_view key) { return *object.Find(key); }

// An integer literal within +-2^30 (the range rule is rangeRule: M9 or R5); kindRule names the field's own rule.
Check Integer(const Json::Value& value, const std::string& path, const std::string_view kindRule,
              const std::string_view rangeRule, std::int64_t& out) {
    if (value.GetKind() != Json::Kind::Number || !value.IsIntegerLiteral()) {
        return Broken(std::string(kindRule) + ": not an integer", path);
    }
    const auto number = value.AsInt64();
    if (!number || *number < -kIntegerLimit || *number > kIntegerLimit) {
        return Broken(std::string(rangeRule) + ": an integer outside +-2^30", path);
    }
    out = *number;
    return std::nullopt;
}

template <std::size_t N>
Check Integers(const Json::Value& value, const std::string& path, const std::string_view kindRule,
               const std::string_view rangeRule, std::array<std::int64_t, N>& out) {
    const auto array = value.AsArray();
    if (!array || array->Size() != N) {
        return Broken(std::string(kindRule) + ": not " + std::to_string(N) + " integers", path);
    }
    std::size_t i = 0;
    for (const Json::Value item : *array) {
        if (Check broken = Integer(item, Index(path, i), kindRule, rangeRule, out[i])) return broken;
        ++i;
    }
    return std::nullopt;
}

Check Array(const Json::Value& value, const std::string& path, const std::string_view rule,
            std::optional<Json::ArrayView>& out) {
    const auto array = value.AsArray();
    if (!array) return Broken(std::string(rule) + ": not an array", path);
    out = *array;
    return std::nullopt;
}

bool IsLowerHex64(const std::string_view text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](const char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

bool IsNameCharacter(const char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'; }

std::int64_t FloorDivide(const std::int64_t value, const std::int64_t divisor) {
    const std::int64_t remainder = ((value % divisor) + divisor) % divisor;
    return (value - remainder) / divisor;
}

// --- Rules on the validated values, shared by every entry point. -------------------------------------------------

// The far corner of a quad: X faces run along Z (width) and Y (height), Y faces along X and Z, Z faces along X and Y.
std::array<std::int64_t, 3> FarCorner(const VoxelQuad& quad) {
    const std::int64_t x = quad.x, y = quad.y, z = quad.z, w = quad.width, h = quad.height;
    switch (quad.face) {
    case VoxelFace::PositiveX:
    case VoxelFace::NegativeX: return {x, y + h, z + w};
    case VoxelFace::PositiveY:
    case VoxelFace::NegativeY: return {x + w, y, z + h};
    case VoxelFace::PositiveZ:
    case VoxelFace::NegativeZ: return {x + w, y + h, z};
    }
    return {x, y, z};
}

bool InChunk(const std::int64_t value) { return value >= 0 && value <= kChunkUnits; }

std::array<std::int64_t, 7> QuadOrder(const VoxelQuad& q) {
    return {q.x, q.y, q.z, static_cast<std::int64_t>(q.face), q.width, q.height, static_cast<std::int64_t>(q.palette)};
}
std::array<std::int64_t, 6> BoxOrder(const VoxelBox& b) {
    return {b.minimum.x, b.minimum.y, b.minimum.z, b.maximum.x, b.maximum.y, b.maximum.z};
}

// C1 (not empty), C2 (direction and sizes), C3, C4 and C5: what a chunk breaks on its own. Paths relative to the chunk.
Check ChunkRules(const VoxelChunkDocument& chunk) {
    if (chunk.quads.empty() && chunk.colliders.empty()) return Broken("C1: a chunk without quads and colliders", "");
    for (std::size_t i = 0; i < chunk.quads.size(); ++i) {
        const VoxelQuad& quad = chunk.quads[i];
        const std::string path = Index("quads", i);
        if (static_cast<std::uint8_t>(quad.face) >= kFaceCount) return Broken("C2: a direction outside 0 to 5", Index(path, 3));
        if (quad.width < 1) return Broken("C2: a width below 1", Index(path, 4));
        if (quad.height < 1) return Broken("C2: a height below 1", Index(path, 5));
        const std::array<std::int64_t, 3> far = FarCorner(quad);
        if (!InChunk(quad.x) || !InChunk(quad.y) || !InChunk(quad.z) || !InChunk(far[0]) || !InChunk(far[1]) ||
            !InChunk(far[2])) {
            return Broken("C4: a corner outside the chunk's [0, 256]", path);
        }
        if (i > 0 && !(QuadOrder(chunk.quads[i - 1]) < QuadOrder(quad))) {
            return Broken("C5: quads not in strictly increasing order", path);
        }
    }
    for (std::size_t i = 0; i < chunk.colliders.size(); ++i) {
        const VoxelBox& box = chunk.colliders[i];
        const std::string path = Index("colliders", i);
        if (!(box.minimum.x < box.maximum.x) || !(box.minimum.y < box.maximum.y) || !(box.minimum.z < box.maximum.z)) {
            return Broken("C3: a collider whose min is not below its max", path);
        }
        if (!InChunk(box.minimum.x) || !InChunk(box.minimum.y) || !InChunk(box.minimum.z) || !InChunk(box.maximum.x) ||
            !InChunk(box.maximum.y) || !InChunk(box.maximum.z)) {
            return Broken("C4: a corner outside the chunk's [0, 256]", path);
        }
        if (i > 0 && !(BoxOrder(chunk.colliders[i - 1]) < BoxOrder(box))) {
            return Broken("C5: colliders not in strictly increasing order", path);
        }
    }
    return std::nullopt;
}

bool InBounds(const VoxelChunkKey key, const VoxelMapDocument& map) {
    return key.x >= map.boundsMinimum.x && key.x <= map.boundsMaximum.x && key.y >= map.boundsMinimum.y &&
           key.y <= map.boundsMaximum.y && key.z >= map.boundsMinimum.z && key.z <= map.boundsMaximum.z;
}

// The chunk rules that need the index: inside the bounds (C1) and palette indices into the palette (C2).
Check ChunkMapRules(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    if (!InBounds(chunk.at, map)) return Broken("C1: a chunk outside the map's bounds", "at");
    for (std::size_t i = 0; i < chunk.quads.size(); ++i) {
        if (chunk.quads[i].palette >= map.palette.size()) {
            return Broken("C2: a palette index outside the palette", Index(Index("quads", i), 6));
        }
    }
    return std::nullopt;
}

std::string Prefixed(const std::string& prefix, const std::string& path) {
    if (path.empty()) return prefix;
    return prefix + "." + path;
}

// --- The index. ----------------------------------------------------------------------------------------------------

Check ReadPalette(const Json::Value& value, std::vector<VoxelPaletteEntry>& out) {
    std::optional<Json::ArrayView> entries;
    if (Check broken = Array(value, "palette", "M7", entries)) return broken;
    if (entries->Size() < 1 || entries->Size() > kMaxPaletteEntries) {
        return Broken("M7: the palette does not have 1 to 256 entries", "palette");
    }
    std::set<std::string, std::less<>> names;
    std::size_t i = 0;
    for (const Json::Value item : *entries) {
        const std::string path = Index("palette", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(item, {"name", "faces"}, path, "M7", object)) return broken;
        const auto name = Get(*object, "name").AsString();
        if (!name || name->empty() || name->size() > kMaxNameLength ||
            !std::all_of(name->begin(), name->end(), IsNameCharacter)) {
            return Broken("M7: a name that is not 1 to 64 characters of a-z, 0-9, _ and .", Field(path, "name"));
        }
        if (!names.emplace(*name).second) return Broken("M7: a name that repeats in the palette", Field(path, "name"));
        std::optional<Json::ArrayView> faces;
        if (Check broken = Array(Get(*object, "faces"), Field(path, "faces"), "M7", faces)) return broken;
        if (faces->Size() != kFaceCount) return Broken("M7: faces are not six colours", Field(path, "faces"));
        VoxelPaletteEntry entry;
        entry.name = std::string(*name);
        std::size_t face = 0;
        for (const Json::Value colour : *faces) {
            const std::string colourPath = Index(Field(path, "faces"), face);
            std::array<std::int64_t, 4> components{};
            if (Check broken = Integers(colour, colourPath, "M7", "M9", components)) return broken;
            for (std::size_t c = 0; c < 4; ++c) {
                if (components[c] < 0 || components[c] > 255) {
                    return Broken("M7: a colour component outside 0 to 255", Index(colourPath, c));
                }
                entry.faces[face][c] = static_cast<std::uint8_t>(components[c]);
            }
            ++face;
        }
        out.push_back(std::move(entry));
    }
    return std::nullopt;
}

bool IsDeviceName(const std::string_view segment) {
    const std::string_view stem = segment.substr(0, segment.find('.'));
    for (const std::string_view name : {"con", "prn", "aux", "nul"}) {
        if (stem == name) return true;
    }
    if (stem.size() == 4 && (stem.substr(0, 3) == "com" || stem.substr(0, 3) == "lpt") && stem[3] >= '1' &&
        stem[3] <= '9') {
        return true;
    }
    return false;
}

bool ValidRegionFile(const std::string_view file) {
    if (file.empty() || file.size() > kMaxRegionFileLength) return false;
    const bool characters = std::all_of(file.begin(), file.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '/';
    });
    if (!characters) return false;
    std::size_t start = 0;
    while (start <= file.size()) {
        const std::size_t end = std::min(file.find('/', start), file.size());
        const std::string_view segment = file.substr(start, end - start);
        // An empty segment (which covers a leading '/'), and one ending in '.' (which covers "." and ".."), name another
        // file on some platform.
        if (segment.empty() || segment.back() == '.' || IsDeviceName(segment)) return false;
        start = end + 1;
    }
    return true;
}

Check ReadRegions(const Json::Value& value, const VoxelMapDocument& map, std::vector<VoxelMapRegion>& out) {
    std::optional<Json::ArrayView> entries;
    if (Check broken = Array(value, "regions", "M8", entries)) return broken;
    if (entries->Size() < 1) return Broken("M8: no regions", "regions");
    const std::int64_t minimumX = FloorDivide(map.boundsMinimum.x, kRegionChunks);
    const std::int64_t maximumX = FloorDivide(map.boundsMaximum.x, kRegionChunks);
    const std::int64_t minimumZ = FloorDivide(map.boundsMinimum.z, kRegionChunks);
    const std::int64_t maximumZ = FloorDivide(map.boundsMaximum.z, kRegionChunks);
    std::set<std::string, std::less<>> files;
    std::size_t i = 0;
    for (const Json::Value item : *entries) {
        const std::string path = Index("regions", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(item, {"at", "file", "sha256"}, path, "M8", object)) return broken;
        std::array<std::int64_t, 2> at{};
        if (Check broken = Integers(Get(*object, "at"), Field(path, "at"), "M8", "M9", at)) return broken;
        if (at[0] < minimumX || at[0] > maximumX || at[1] < minimumZ || at[1] > maximumZ) {
            return Broken("M8: a region outside the bounds", Field(path, "at"));
        }
        VoxelMapRegion region;
        region.at = {static_cast<std::int32_t>(at[0]), static_cast<std::int32_t>(at[1])};
        if (!out.empty() && !(out.back().at < region.at)) {
            return Broken("M8: regions not in strictly increasing order of at", Field(path, "at"));
        }
        const auto file = Get(*object, "file").AsString();
        if (!file || !ValidRegionFile(*file)) return Broken("M8: a file name the contract does not allow", Field(path, "file"));
        if (!files.emplace(*file).second) return Broken("M8: a file that repeats in the list", Field(path, "file"));
        const auto sha = Get(*object, "sha256").AsString();
        if (!sha || !IsLowerHex64(*sha)) return Broken("M8: a sha256 that is not 64 lowercase hex digits", Field(path, "sha256"));
        region.file = std::string(*file);
        region.sha256 = std::string(*sha);
        out.push_back(std::move(region));
    }
    return std::nullopt;
}

Check ReadMap(const std::string_view text, VoxelMapDocument& map) {
    if (text.size() > kMaxMapTextBytes) return Broken("M10: the text is larger than 1 MiB", "");
    auto parsed = Json::Parse(text);
    if (!parsed) return Broken("M1: " + parsed.error().message, "");
    std::optional<Json::ObjectView> root;
    if (Check broken = ExactObject(parsed->Root(),
                                   {"schema", "version", "source_sha256", "build_version", "origin", "cell_size",
                                    "chunk_size", "region_size", "bounds", "palette", "regions"},
                                   "", "M1", root)) {
        return broken;
    }
    const auto schema = Get(*root, "schema").AsString();
    if (!schema || *schema != kMapSchema) return Broken("M2: schema is not gyo.voxel_map", "schema");
    std::int64_t version = 0;
    if (Check broken = Integer(Get(*root, "version"), "version", "M2", "M9", version)) return broken;
    if (version != kMapVersion) return Broken("M2: version is not 1", "version");

    const auto source = Get(*root, "source_sha256").AsString();
    if (!source || !IsLowerHex64(*source)) return Broken("M3: source_sha256 is not 64 lowercase hex digits", "source_sha256");
    const auto build = Get(*root, "build_version").AsString();
    if (!build || build->empty() || build->size() > kMaxBuildVersionLength ||
        !std::all_of(build->begin(), build->end(), [](const char c) { return c >= 0x20 && c <= 0x7E; })) {
        return Broken("M3: build_version is not 1 to 64 printable ASCII characters", "build_version");
    }
    map.sourceSha256 = std::string(*source);
    map.buildVersion = std::string(*build);

    std::array<std::int64_t, 3> origin{};
    if (Check broken = Integers(Get(*root, "origin"), "origin", "M4", "M9", origin)) return broken;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (kUnitsPerCell * origin[axis] < -kCoordinateLimit || kUnitsPerCell * origin[axis] > kCoordinateLimit) {
            return Broken("M4: 16 x origin outside +-2^20 units", Index("origin", axis));
        }
    }
    map.origin = {static_cast<std::int32_t>(origin[0]), static_cast<std::int32_t>(origin[1]),
                  static_cast<std::int32_t>(origin[2])};
    const Json::Value cellValue = Get(*root, "cell_size");
    const auto cell = cellValue.AsDouble();
    if (!cell) return Broken("M4: cell_size is not a number", "cell_size");
    const auto cellSize = static_cast<float>(*cell);
    if (!(cellSize >= std::ldexp(1.0F, -16) && cellSize <= std::ldexp(1.0F, 16))) {
        return Broken("M4: cell_size outside [2^-16, 2^16]", "cell_size");
    }
    map.cellSize = cellSize;

    std::int64_t chunkSize = 0;
    if (Check broken = Integer(Get(*root, "chunk_size"), "chunk_size", "M5", "M9", chunkSize)) return broken;
    if (chunkSize != kChunkCells) return Broken("M5: chunk_size is not 16", "chunk_size");
    std::int64_t regionSize = 0;
    if (Check broken = Integer(Get(*root, "region_size"), "region_size", "M5", "M9", regionSize)) return broken;
    if (regionSize != kRegionChunks) return Broken("M5: region_size is not 8", "region_size");

    std::optional<Json::ObjectView> bounds;
    if (Check broken = ExactObject(Get(*root, "bounds"), {"min", "max"}, "bounds", "M6", bounds)) return broken;
    std::array<std::int64_t, 3> minimum{}, maximum{};
    if (Check broken = Integers(Get(*bounds, "min"), "bounds.min", "M6", "M9", minimum)) return broken;
    if (Check broken = Integers(Get(*bounds, "max"), "bounds.max", "M6", "M9", maximum)) return broken;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (minimum[axis] > maximum[axis]) return Broken("M6: min is above max", Index("bounds.max", axis));
        const std::int64_t low = kUnitsPerCell * origin[axis] + kChunkUnits * minimum[axis];
        const std::int64_t high = kUnitsPerCell * origin[axis] + kChunkUnits * maximum[axis] + kChunkUnits;
        if (low < -kCoordinateLimit || high > kCoordinateLimit) {
            return Broken("M6: a corner of the bounds outside +-2^20 units", "bounds");
        }
    }
    map.boundsMinimum = {static_cast<std::int32_t>(minimum[0]), static_cast<std::int32_t>(minimum[1]),
                         static_cast<std::int32_t>(minimum[2])};
    map.boundsMaximum = {static_cast<std::int32_t>(maximum[0]), static_cast<std::int32_t>(maximum[1]),
                         static_cast<std::int32_t>(maximum[2])};

    if (Check broken = ReadPalette(Get(*root, "palette"), map.palette)) return broken;
    return ReadRegions(Get(*root, "regions"), map, map.regions);
}

// --- A region's text. ----------------------------------------------------------------------------------------------

Check ReadQuads(const Json::Value& value, const std::string& path, std::vector<VoxelQuad>& out) {
    std::optional<Json::ArrayView> quads;
    if (Check broken = Array(value, path, "C2", quads)) return broken;
    std::size_t i = 0;
    for (const Json::Value item : *quads) {
        const std::string quadPath = Index(path, i++);
        std::array<std::int64_t, 7> v{};
        if (Check broken = Integers(item, quadPath, "C2", "R5", v)) return broken;
        // Representable here; whether 0 to 5 and whether inside the palette are the shared rules' (ChunkRules,
        // ChunkMapRules), so a chunk that never was text is judged the same.
        if (v[3] < 0 || v[3] > 255) return Broken("C2: a direction outside 0 to 5", Index(quadPath, 3));
        if (v[6] < 0) return Broken("C2: a palette index outside the palette", Index(quadPath, 6));
        out.push_back({static_cast<std::int32_t>(v[0]), static_cast<std::int32_t>(v[1]), static_cast<std::int32_t>(v[2]),
                       static_cast<VoxelFace>(v[3]), static_cast<std::int32_t>(v[4]), static_cast<std::int32_t>(v[5]),
                       static_cast<std::uint32_t>(v[6])});
    }
    return std::nullopt;
}

Check ReadColliders(const Json::Value& value, const std::string& path, std::vector<VoxelBox>& out) {
    std::optional<Json::ArrayView> boxes;
    if (Check broken = Array(value, path, "C3", boxes)) return broken;
    std::size_t i = 0;
    for (const Json::Value item : *boxes) {
        const std::string boxPath = Index(path, i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(item, {"min", "max"}, boxPath, "C3", object)) return broken;
        std::array<std::int64_t, 3> minimum{}, maximum{};
        if (Check broken = Integers(Get(*object, "min"), Field(boxPath, "min"), "C3", "R5", minimum)) return broken;
        if (Check broken = Integers(Get(*object, "max"), Field(boxPath, "max"), "C3", "R5", maximum)) return broken;
        out.push_back({{static_cast<std::int32_t>(minimum[0]), static_cast<std::int32_t>(minimum[1]),
                        static_cast<std::int32_t>(minimum[2])},
                       {static_cast<std::int32_t>(maximum[0]), static_cast<std::int32_t>(maximum[1]),
                        static_cast<std::int32_t>(maximum[2])}});
    }
    return std::nullopt;
}

Check ReadRegion(const std::string_view text, VoxelRegionDocument& region) {
    if (text.size() > kMaxRegionTextBytes) return Broken("R5: the text is larger than 16 MiB", "");
    auto parsed = Json::Parse(text);
    if (!parsed) return Broken("R1: " + parsed.error().message, "");
    std::optional<Json::ObjectView> root;
    if (Check broken = ExactObject(parsed->Root(), {"schema", "version", "at", "chunks"}, "", "R1", root)) return broken;
    const auto schema = Get(*root, "schema").AsString();
    if (!schema || *schema != kRegionSchema) return Broken("R2: schema is not gyo.voxel_region", "schema");
    std::int64_t version = 0;
    if (Check broken = Integer(Get(*root, "version"), "version", "R2", "R5", version)) return broken;
    if (version != kRegionVersion) return Broken("R2: version is not 1", "version");
    std::array<std::int64_t, 2> at{};
    if (Check broken = Integers(Get(*root, "at"), "at", "R3", "R5", at)) return broken;
    region.at = {static_cast<std::int32_t>(at[0]), static_cast<std::int32_t>(at[1])};

    std::optional<Json::ArrayView> chunks;
    if (Check broken = Array(Get(*root, "chunks"), "chunks", "R4", chunks)) return broken;
    if (chunks->Size() < 1) return Broken("R4: no chunks", "chunks");
    std::size_t i = 0;
    for (const Json::Value item : *chunks) {
        const std::string path = Index("chunks", i++);
        std::optional<Json::ObjectView> object;
        if (Check broken = ExactObject(item, {"at", "quads", "colliders"}, path, "R4", object)) return broken;
        std::array<std::int64_t, 3> key{};
        if (Check broken = Integers(Get(*object, "at"), Field(path, "at"), "R4", "R5", key)) return broken;
        VoxelChunkDocument chunk;
        chunk.at = {static_cast<std::int32_t>(key[0]), static_cast<std::int32_t>(key[1]), static_cast<std::int32_t>(key[2])};
        if (RegionOf(chunk.at) != region.at) return Broken("R4: a chunk outside this region", Field(path, "at"));
        if (!region.chunks.empty() && !(region.chunks.back().at < chunk.at)) {
            return Broken("R4: chunks not in strictly increasing key order", Field(path, "at"));
        }
        if (Check broken = ReadQuads(Get(*object, "quads"), Field(path, "quads"), chunk.quads)) return broken;
        if (Check broken = ReadColliders(Get(*object, "colliders"), Field(path, "colliders"), chunk.colliders)) {
            return broken;
        }
        if (Check broken = ChunkRules(chunk)) return Broken(std::move(broken->message), Prefixed(path, broken->path));
        region.chunks.push_back(std::move(chunk));
    }
    region.textSha256 = Base::Sha256(std::as_bytes(std::span<const char>(text.data(), text.size())));
    return std::nullopt;
}

template <class T>
VoxelMapResult<T> Fail(const VoxelMapErrorCode code, Problem problem) {
    return Base::Err(VoxelMapError::Make(code, std::move(problem.message), std::move(problem.path)));
}

// --- Expansion. ----------------------------------------------------------------------------------------------------

// float(units) x cellSize / 16, with units counted from the world origin (exact as an integer; within +-2^20 for a
// validated map, so the conversion is exact and the product rounds once).
float ToWorld(const std::int64_t units, const float cellSize) {
    return static_cast<float>(units) * cellSize / static_cast<float>(kUnitsPerCell);
}

Math::Vec3 WorldOf(const std::int64_t x, const std::int64_t y, const std::int64_t z, const VoxelMapDocument& map) {
    return {ToWorld(x + kUnitsPerCell * map.origin.x, map.cellSize), ToWorld(y + kUnitsPerCell * map.origin.y, map.cellSize),
            ToWorld(z + kUnitsPerCell * map.origin.z, map.cellSize)};
}

} // namespace

VoxelMapResult<VoxelMapDocument> ParseVoxelMap(const std::string_view text) {
    VoxelMapDocument map;
    if (Check broken = ReadMap(text, map)) return Fail<VoxelMapDocument>(VoxelMapErrorCode::InvalidVoxelMap, std::move(*broken));
    return map;
}

VoxelMapResult<VoxelRegionDocument> ParseVoxelRegion(const std::string_view text) {
    VoxelRegionDocument region;
    if (Check broken = ReadRegion(text, region)) {
        return Fail<VoxelRegionDocument>(VoxelMapErrorCode::InvalidVoxelRegion, std::move(*broken));
    }
    return region;
}

VoxelMapResult<void> CheckVoxelRegion(const VoxelRegionDocument& region, const VoxelMapDocument& map,
                                      const VoxelRegionKey expected) {
    const auto entry = std::find_if(map.regions.begin(), map.regions.end(),
                                    [expected](const VoxelMapRegion& r) { return r.at == expected; });
    GYO_ASSERT(entry != map.regions.end() && "CheckVoxelRegion: expected is not a region of the map");
    if (region.at != expected) {
        return Fail<void>(VoxelMapErrorCode::InvalidVoxelRegion, {"R3: at is not the region the index entry names", "at"});
    }
    if (region.textSha256 != entry->sha256) {
        return Fail<void>(VoxelMapErrorCode::InvalidVoxelRegion, {"R3: the text's SHA-256 is not the index entry's", ""});
    }
    for (std::size_t i = 0; i < region.chunks.size(); ++i) {
        if (Check broken = ChunkMapRules(region.chunks[i], map)) {
            return Fail<void>(VoxelMapErrorCode::InvalidVoxelRegion,
                              {std::move(broken->message), Prefixed(Index("chunks", i), broken->path)});
        }
    }
    return {};
}

VoxelMapResult<void> CheckVoxelMap(const VoxelMapDocument& map, const std::span<const VoxelRegionDocument* const> regions) {
    GYO_ASSERT(regions.size() == map.regions.size() && "CheckVoxelMap: one region per index entry, in index order");
    for (std::size_t i = 0; i < regions.size(); ++i) {
        GYO_ASSERT(regions[i] != nullptr && "CheckVoxelMap: a region that was not read");
        auto checked = CheckVoxelRegion(*regions[i], map, map.regions[i].at);
        if (!checked) {
            VoxelMapError error = std::move(checked).error();
            return Fail<void>(VoxelMapErrorCode::InvalidVoxelRegion,
                              {std::move(error.message), Prefixed(Index("regions", i), error.detail)});
        }
    }
    return {};
}

VoxelMapResult<VoxelRegionDocument> ParseVoxelRegion(const std::string_view text, const VoxelMapDocument& map,
                                                     const VoxelRegionKey expected) {
    auto region = ParseVoxelRegion(text);
    if (!region) return region;
    if (auto checked = CheckVoxelRegion(*region, map, expected); !checked) return Base::Err(std::move(checked).error());
    return region;
}

VoxelMapResult<void> ValidateVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    if (Check broken = ChunkRules(chunk)) return Fail<void>(VoxelMapErrorCode::InvalidVoxelChunk, std::move(*broken));
    if (Check broken = ChunkMapRules(chunk, map)) return Fail<void>(VoxelMapErrorCode::InvalidVoxelChunk, std::move(*broken));
    return {};
}

void AppendVoxelChunk(VoxelGeometry& geometry, const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    // The preconditions the expansion itself relies on (the switch on the face, the uv's column, exact world
    // coordinates); the rest of the rules is the readers' to have checked (ValidateVoxelChunk, CheckVoxelMap).
    GYO_ASSERT(InBounds(chunk.at, map) && "AppendVoxelChunk: a chunk outside the map's bounds");
    for (const VoxelQuad& quad : chunk.quads) {
        GYO_ASSERT(static_cast<std::uint8_t>(quad.face) < kFaceCount && quad.palette < map.palette.size() &&
                   "AppendVoxelChunk: a quad's direction or palette index the map's rules refuse");
    }
    GYO_ASSERT(geometry.positions.size() + 4 * chunk.quads.size() <= std::numeric_limits<std::uint32_t>::max() &&
               "AppendVoxelChunk: more vertices than 32-bit indices address");
    const std::int64_t baseX = kChunkUnits * chunk.at.x;
    const std::int64_t baseY = kChunkUnits * chunk.at.y;
    const std::int64_t baseZ = kChunkUnits * chunk.at.z;
    const float columns = static_cast<float>(map.palette.size());
    for (const VoxelQuad& quad : chunk.quads) {
        const std::int64_t x0 = baseX + quad.x, y0 = baseY + quad.y, z0 = baseZ + quad.z;
        const std::array<std::int64_t, 3> far = FarCorner(quad);
        const std::int64_t x1 = baseX + far[0], y1 = baseY + far[1], z1 = baseZ + far[2];
        const auto at = [&map](const std::int64_t x, const std::int64_t y, const std::int64_t z) { return WorldOf(x, y, z, map); };
        // Counter-clockwise on the screen seen from the side the face looks at (render's front face). The space is
        // left-handed (docs/architecture/math.md), so (v1 - v0) x (v2 - v0) points against the normal.
        std::array<Math::Vec3, 4> corners{};
        Math::Vec3 normal{};
        switch (quad.face) {
        case VoxelFace::PositiveX:
            corners = {at(x0, y0, z1), at(x0, y1, z1), at(x0, y1, z0), at(x0, y0, z0)};
            normal = {1.0F, 0.0F, 0.0F};
            break;
        case VoxelFace::NegativeX:
            corners = {at(x0, y0, z0), at(x0, y1, z0), at(x0, y1, z1), at(x0, y0, z1)};
            normal = {-1.0F, 0.0F, 0.0F};
            break;
        case VoxelFace::PositiveY:
            corners = {at(x0, y0, z1), at(x0, y0, z0), at(x1, y0, z0), at(x1, y0, z1)};
            normal = {0.0F, 1.0F, 0.0F};
            break;
        case VoxelFace::NegativeY:
            corners = {at(x0, y0, z0), at(x0, y0, z1), at(x1, y0, z1), at(x1, y0, z0)};
            normal = {0.0F, -1.0F, 0.0F};
            break;
        case VoxelFace::PositiveZ:
            corners = {at(x0, y0, z0), at(x0, y1, z0), at(x1, y1, z0), at(x1, y0, z0)};
            normal = {0.0F, 0.0F, 1.0F};
            break;
        case VoxelFace::NegativeZ:
            corners = {at(x1, y0, z0), at(x1, y1, z0), at(x0, y1, z0), at(x0, y0, z0)};
            normal = {0.0F, 0.0F, -1.0F};
            break;
        }
        const auto base = static_cast<std::uint32_t>(geometry.positions.size());
        const Math::Vec2 uv{(static_cast<float>(quad.palette) + 0.5F) / columns,
                            (static_cast<float>(quad.face) + 0.5F) / static_cast<float>(kFaceCount)};
        for (const Math::Vec3& corner : corners) {
            geometry.positions.push_back(corner);
            geometry.normals.push_back(normal);
            geometry.uvs.push_back(uv);
        }
        for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) geometry.indices.push_back(base + i);
    }
    for (const VoxelBox& box : chunk.colliders) {
        geometry.colliders.push_back({WorldOf(baseX + box.minimum.x, baseY + box.minimum.y, baseZ + box.minimum.z, map),
                                      WorldOf(baseX + box.maximum.x, baseY + box.maximum.y, baseZ + box.maximum.z, map)});
    }
}

VoxelGeometry ExpandVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    VoxelGeometry geometry;
    AppendVoxelChunk(geometry, chunk, map);
    return geometry;
}

VoxelPaletteImage ExpandVoxelPalette(const VoxelMapDocument& map) {
    VoxelPaletteImage image;
    image.width = static_cast<std::uint32_t>(map.palette.size());
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    for (std::uint32_t row = 0; row < image.height; ++row) {
        for (std::uint32_t column = 0; column < image.width; ++column) {
            const VoxelColor& colour = map.palette[column].faces[row];
            const std::size_t at = (static_cast<std::size_t>(row) * image.width + column) * 4;
            std::copy(colour.begin(), colour.end(), image.pixels.begin() + static_cast<std::ptrdiff_t>(at));
        }
    }
    return image;
}

VoxelRegionKey RegionOf(const VoxelChunkKey key) noexcept {
    return {static_cast<std::int32_t>(FloorDivide(key.x, kRegionChunks)),
            static_cast<std::int32_t>(FloorDivide(key.z, kRegionChunks))};
}

Math::Vec3 ChunkWorldOrigin(const VoxelChunkKey key, const VoxelMapDocument& map) {
    GYO_ASSERT(InBounds(key, map) && "ChunkWorldOrigin: a chunk outside the map's bounds");
    return WorldOf(kChunkUnits * key.x, kChunkUnits * key.y, kChunkUnits * key.z, map);
}

Math::Vec3 MapUnitsToWorld(const Math::Vec3i units, const VoxelMapDocument& map) {
    return WorldOf(units.x, units.y, units.z, map);
}

Math::Aabb ChunkWorldBounds(const VoxelChunkKey key, const VoxelMapDocument& map) {
    GYO_ASSERT(InBounds(key, map) && "ChunkWorldBounds: a chunk outside the map's bounds");
    const std::int64_t x = kChunkUnits * key.x, y = kChunkUnits * key.y, z = kChunkUnits * key.z;
    return {WorldOf(x, y, z, map), WorldOf(x + kChunkUnits, y + kChunkUnits, z + kChunkUnits, map)};
}

} // namespace GYO::Engine::VoxelMap
