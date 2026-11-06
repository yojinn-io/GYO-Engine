#include "engine/voxel_mesh/VoxelMesh.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/base/StrictJson.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <span>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace Engine::VoxelMesh {
namespace {
using Json = nlohmann::json;

// Raw integers are bounded before any arithmetic, so sums cannot overflow.
constexpr std::int64_t RawIntegerLimit = std::int64_t{1} << 30;

// Thrown inside parsing only, and turned into the module's error at its end.
struct Invalid final {
    std::string message;
    std::string path;
};

[[noreturn]] void Fail(std::string message, std::string path) {
    throw Invalid{std::move(message), std::move(path)};
}

std::string At(const std::string& path, const std::size_t index) {
    return path + "[" + std::to_string(index) + "]";
}

void RequireExactKeys(const Json& object, const std::set<std::string>& keys, const std::string& path) {
    for (const auto& key : keys)
        if (!object.contains(key)) Fail("missing field '" + key + "'", path);
    for (const auto& item : object.items())
        if (!keys.contains(item.key())) Fail("unknown field '" + item.key() + "'", path);
}

// The DOM keeps the last of repeated keys, so repeats are caught by a scan
// before the DOM is built (engine/base/StrictJson.hpp).
Json ParseWithoutRepeatedKeys(const std::string_view text) {
    std::string failure;
    auto document = Base::ParseStrictJson<Json>(text, failure);
    if (!document) Fail(failure, "");
    return std::move(*document);
}

std::string Join(const std::string& prefix, const std::string& field) {
    return prefix.empty() ? field : prefix + "." + field;
}

// An integer in [-2^30, 2^30]; a number with a fraction or exponent is not one.
std::int64_t Integer(const Json& value, const std::string& path) {
    if (!value.is_number_integer()) Fail("an integer is expected", path);
    if (value.is_number_unsigned()) {
        const auto unsignedValue = value.get<std::uint64_t>();
        if (unsignedValue > static_cast<std::uint64_t>(RawIntegerLimit)) Fail("the integer is too large", path);
        return static_cast<std::int64_t>(unsignedValue);
    }
    const auto signedValue = value.get<std::int64_t>();
    if (signedValue < -RawIntegerLimit || signedValue > RawIntegerLimit) Fail("the integer is too large", path);
    return signedValue;
}

std::array<std::int64_t, 3> Triple(const Json& value, const std::string& path) {
    if (!value.is_array() || value.size() != 3) Fail("three integers are expected", path);
    return {Integer(value[0], At(path, 0)), Integer(value[1], At(path, 1)), Integer(value[2], At(path, 2))};
}

bool ValidName(const std::string& name) {
    if (name.empty() || name.size() > MaxNameLength) return false;
    return std::all_of(name.begin(), name.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

bool ValidSha256(const std::string& text) {
    if (text.size() != 64) return false;
    return std::all_of(text.begin(), text.end(),
                       [](const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool ValidBuildVersion(const std::string& text) {
    if (text.empty() || text.size() > MaxBuildVersionLength) return false;
    return std::all_of(text.begin(), text.end(), [](const char c) { return c >= 0x20 && c <= 0x7E; });
}

void RequireInRange(const std::int64_t units, const std::string& path) {
    if (units < -CoordinateLimit || units > CoordinateLimit) Fail("a coordinate lies beyond +-2^20 units", path);
}

// Chunk-local coordinates (gyo.voxel_region rule C4).
void RequireInChunk(const std::int64_t units, const std::string& path) {
    if (units < 0 || units > ChunkUnits) Fail("a coordinate lies outside the chunk (0 to 256 units)", path);
}

// How a corner is bounded: version 1 adds 16 * origin and checks +-2^20; a
// chunk adds nothing and checks [0, 256].
using RangeCheck = void (*)(std::int64_t, const std::string&);

VoxelPaletteEntry ParsePaletteEntry(const Json& entry, const std::string& path) {
    if (!entry.is_object()) Fail("a palette entry is an object", path);
    RequireExactKeys(entry, {"name", "faces"}, path);
    const auto& name = entry.at("name");
    if (!name.is_string() || !ValidName(name.get<std::string>()))
        Fail("a palette name is 1 to 64 characters of a-z, 0-9, '_' and '.'", path + ".name");
    const auto& faces = entry.at("faces");
    if (!faces.is_array() || faces.size() != FaceCount) Fail("faces lists six colours", path + ".faces");
    VoxelPaletteEntry result{name.get<std::string>(), {}};
    for (std::size_t face = 0; face < FaceCount; ++face) {
        const auto facePath = At(path + ".faces", face);
        const auto& colour = faces[face];
        if (!colour.is_array() || colour.size() != 4) Fail("a colour is four integers", facePath);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto value = Integer(colour[channel], At(facePath, channel));
            if (value < 0 || value > 255) Fail("a colour channel is 0 to 255", At(facePath, channel));
            result.faces[face][channel] = static_cast<std::uint8_t>(value);
        }
    }
    return result;
}

// The rules of one quad on its seven integers, shared by gyo.voxel_mesh,
// gyo.voxel_region and ValidateVoxelChunk (one rule implementation).
VoxelQuad CheckQuad(const std::array<std::int64_t, 7>& v, const std::size_t paletteSize,
                    const std::array<std::int64_t, 3>& offset, const RangeCheck requireInRange,
                    const std::string& path) {
    if (v[3] < 0 || v[3] >= static_cast<std::int64_t>(FaceCount)) Fail("dir is 0 to 5", At(path, 3));
    if (v[4] < 1) Fail("width is at least 1", At(path, 4));
    if (v[5] < 1) Fail("height is at least 1", At(path, 5));
    if (v[6] < 0 || v[6] >= static_cast<std::int64_t>(paletteSize)) Fail("palette names an entry", At(path, 6));
    // The extent of each axis: the face's plane does not grow along its own axis.
    const auto face = static_cast<VoxelFace>(v[3]);
    std::array<std::int64_t, 3> extent{};
    switch (face) {
    case VoxelFace::PositiveX:
    case VoxelFace::NegativeX: extent = {0, v[5], v[4]}; break;
    case VoxelFace::PositiveY:
    case VoxelFace::NegativeY: extent = {v[4], 0, v[5]}; break;
    case VoxelFace::PositiveZ:
    case VoxelFace::NegativeZ: extent = {v[4], v[5], 0}; break;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        requireInRange(v[axis] + offset[axis], At(path, axis));
        requireInRange(v[axis] + extent[axis] + offset[axis], path);
    }
    return {static_cast<std::int32_t>(v[0]), static_cast<std::int32_t>(v[1]), static_cast<std::int32_t>(v[2]),
            face, static_cast<std::int32_t>(v[4]), static_cast<std::int32_t>(v[5]),
            static_cast<std::uint32_t>(v[6])};
}

VoxelQuad ParseQuad(const Json& entry, const std::size_t paletteSize, const std::array<std::int64_t, 3>& offset,
                    const RangeCheck requireInRange, const std::string& path) {
    if (!entry.is_array() || entry.size() != 7) Fail("a quad is seven integers", path);
    std::array<std::int64_t, 7> v{};
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = Integer(entry[i], At(path, i));
    return CheckQuad(v, paletteSize, offset, requireInRange, path);
}

// The rules of one box, shared like CheckQuad.
VoxelBox CheckBox(const std::array<std::int64_t, 3>& minimum, const std::array<std::int64_t, 3>& maximum,
                  const std::array<std::int64_t, 3>& offset, const RangeCheck requireInRange,
                  const std::string& path) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!(minimum[axis] < maximum[axis])) Fail("min is below max on every axis", path);
        requireInRange(minimum[axis] + offset[axis], At(path + ".min", axis));
        requireInRange(maximum[axis] + offset[axis], At(path + ".max", axis));
    }
    return {{static_cast<std::int32_t>(minimum[0]), static_cast<std::int32_t>(minimum[1]),
             static_cast<std::int32_t>(minimum[2])},
            {static_cast<std::int32_t>(maximum[0]), static_cast<std::int32_t>(maximum[1]),
             static_cast<std::int32_t>(maximum[2])}};
}

VoxelBox ParseCollider(const Json& entry, const std::array<std::int64_t, 3>& offset, const RangeCheck requireInRange,
                       const std::string& path) {
    if (!entry.is_object()) Fail("a collider is an object", path);
    RequireExactKeys(entry, {"min", "max"}, path);
    const auto minimum = Triple(entry.at("min"), path + ".min");
    const auto maximum = Triple(entry.at("max"), path + ".max");
    return CheckBox(minimum, maximum, offset, requireInRange, path);
}

auto QuadKey(const VoxelQuad& q) {
    return std::make_tuple(q.x, q.y, q.z, static_cast<int>(q.face), q.width, q.height, q.palette);
}

auto BoxKey(const VoxelBox& b) {
    return std::make_tuple(b.minimum.x, b.minimum.y, b.minimum.z, b.maximum.x, b.maximum.y, b.maximum.z);
}

// The fields gyo.voxel_mesh and gyo.voxel_map share: provenance, frame and
// palette (gyo.voxel_mesh rules 3 to 5; gyo.voxel_map rules M3, M4 and M7).
struct Common final {
    std::string sourceSha256;
    std::string buildVersion;
    std::array<std::int64_t, 3> origin{};
    float cellSize{};
    std::vector<VoxelPaletteEntry> palette;
};

void RequireSchemaAndVersion(const Json& document, const std::string_view expectedSchema, const int expectedVersion) {
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != expectedSchema)
        Fail("schema is '" + std::string(expectedSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != expectedVersion)
        Fail("version is " + std::to_string(expectedVersion), "version");
}

Common ParseCommon(const Json& document) {
    Common result;
    const auto& sha = document.at("source_sha256");
    if (!sha.is_string() || !ValidSha256(sha.get<std::string>()))
        Fail("source_sha256 is 64 lowercase hexadecimal digits", "source_sha256");
    result.sourceSha256 = sha.get<std::string>();
    const auto& build = document.at("build_version");
    if (!build.is_string() || !ValidBuildVersion(build.get<std::string>()))
        Fail("build_version is 1 to 64 printable ASCII characters", "build_version");
    result.buildVersion = build.get<std::string>();

    result.origin = Triple(document.at("origin"), "origin");
    const auto& cellSize = document.at("cell_size");
    if (!cellSize.is_number()) Fail("cell_size is a number", "cell_size");
    // The decimal goes to the nearest double (nlohmann), then to the nearest float.
    result.cellSize = static_cast<float>(cellSize.get<double>());
    // Within [2^-16, 2^16], units * cell stays a finite normal float for every
    // corner and dividing it by 16 stays exact (no subnormal results).
    if (!(result.cellSize >= MinCellSize) || !(result.cellSize <= MaxCellSize))
        Fail("cell_size is within [2^-16, 2^16]", "cell_size");

    const auto& palette = document.at("palette");
    if (!palette.is_array() || palette.empty() || palette.size() > MaxPaletteEntries)
        Fail("palette lists 1 to 256 entries", "palette");
    std::set<std::string> names;
    for (std::size_t i = 0; i < palette.size(); ++i) {
        auto entry = ParsePaletteEntry(palette[i], At("palette", i));
        if (!names.insert(entry.name).second) Fail("a palette name repeats", At("palette", i) + ".name");
        result.palette.push_back(std::move(entry));
    }
    return result;
}

Math::Vec3i ToVec3i(const std::array<std::int64_t, 3>& v) {
    return {static_cast<std::int32_t>(v[0]), static_cast<std::int32_t>(v[1]), static_cast<std::int32_t>(v[2])};
}

VoxelMeshDocument ParseDocument(const std::string_view text) {
    if (text.size() > MaxTextBytes) Fail("the text exceeds 16 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document,
                     {"schema", "version", "source_sha256", "build_version", "origin", "cell_size", "palette",
                      "quads", "colliders"},
                     "");
    RequireSchemaAndVersion(document, VoxelMeshSchema, VoxelMeshVersion);

    auto common = ParseCommon(document);
    VoxelMeshDocument result;
    result.sourceSha256 = std::move(common.sourceSha256);
    result.buildVersion = std::move(common.buildVersion);
    result.origin = ToVec3i(common.origin);
    result.cellSize = common.cellSize;
    result.palette = std::move(common.palette);
    const std::array<std::int64_t, 3> offset{common.origin[0] * UnitsPerCell, common.origin[1] * UnitsPerCell,
                                             common.origin[2] * UnitsPerCell};

    const auto& quads = document.at("quads");
    if (!quads.is_array() || quads.empty()) Fail("quads lists one or more quads", "quads");
    for (std::size_t i = 0; i < quads.size(); ++i) {
        auto quad = ParseQuad(quads[i], result.palette.size(), offset, RequireInRange, At("quads", i));
        if (!result.quads.empty() && !(QuadKey(result.quads.back()) < QuadKey(quad)))
            Fail("quads are in strictly increasing order", At("quads", i));
        result.quads.push_back(quad);
    }

    const auto& colliders = document.at("colliders");
    if (!colliders.is_array()) Fail("colliders is an array", "colliders");
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        auto box = ParseCollider(colliders[i], offset, RequireInRange, At("colliders", i));
        if (!result.colliders.empty() && !(BoxKey(result.colliders.back()) < BoxKey(box)))
            Fail("colliders are in strictly increasing order", At("colliders", i));
        result.colliders.push_back(box);
    }
    return result;
}

// ---------------------------------------------------------------------------
// gyo.voxel_map and gyo.voxel_region

std::int64_t FloorDivide(const std::int64_t value, const std::int64_t divisor) {
    const auto quotient = value / divisor;
    return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

// A Windows device name, alone or before an extension ("con", "nul.json"):
// such a file name opens the device on Windows.
bool ReservedOnWindows(const std::string& segment) {
    const auto stem = segment.substr(0, segment.find('.'));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return true;
    return stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' &&
           stem[3] <= '9';
}

// A region file name: 1 to 128 characters of a-z 0-9 _ . - /, relative, with
// no empty segment, no segment ending in "." (which also rules out "." and
// ".."; Windows drops a trailing dot, so "a." and "a" would name one file) and
// no Windows device name.
bool ValidRegionFile(const std::string& file) {
    if (file.empty() || file.size() > MaxRegionFileLength) return false;
    const bool characters = std::all_of(file.begin(), file.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '/';
    });
    if (!characters) return false;
    std::size_t start = 0;
    while (true) {
        const auto end = file.find('/', start);
        const auto segment = file.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (segment.empty()) return false;
        if (segment.back() == '.') return false;
        if (ReservedOnWindows(segment)) return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
}

std::array<std::int64_t, 2> Pair(const Json& value, const std::string& path) {
    if (!value.is_array() || value.size() != 2) Fail("two integers are expected", path);
    return {Integer(value[0], At(path, 0)), Integer(value[1], At(path, 1))};
}

VoxelChunkKey ToChunkKey(const std::array<std::int64_t, 3>& v) {
    return {static_cast<std::int32_t>(v[0]), static_cast<std::int32_t>(v[1]), static_cast<std::int32_t>(v[2])};
}

VoxelMapDocument ParseMapDocument(const std::string_view text) {
    if (text.size() > MaxMapTextBytes) Fail("the text exceeds 1 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document,
                     {"schema", "version", "source_sha256", "build_version", "origin", "cell_size", "chunk_size",
                      "region_size", "bounds", "palette", "regions"},
                     "");
    RequireSchemaAndVersion(document, VoxelMapSchema, VoxelMapVersion);
    auto common = ParseCommon(document);
    // Rule M4 (beyond version 1 rule 4): 16 * origin within +-2^20 units, so
    // marker positions within +-2^20 of the origin convert exactly.
    for (std::size_t axis = 0; axis < 3; ++axis)
        RequireInRange(common.origin[axis] * UnitsPerCell, At("origin", axis));

    const auto& chunkSize = document.at("chunk_size");
    if (!chunkSize.is_number_integer() || chunkSize.get<long long>() != ChunkCells)
        Fail("chunk_size is 16", "chunk_size");
    const auto& regionSize = document.at("region_size");
    if (!regionSize.is_number_integer() || regionSize.get<long long>() != RegionChunks)
        Fail("region_size is 8", "region_size");

    const auto& bounds = document.at("bounds");
    if (!bounds.is_object()) Fail("bounds is an object", "bounds");
    RequireExactKeys(bounds, {"min", "max"}, "bounds");
    const auto minimum = Triple(bounds.at("min"), "bounds.min");
    const auto maximum = Triple(bounds.at("max"), "bounds.max");
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!(minimum[axis] <= maximum[axis])) Fail("bounds.min is at most bounds.max on every axis", "bounds");
        // Every corner of every chunk in the bounds, in units from the world
        // origin, lies within +-2^20 (so its conversion to float is exact).
        const auto offset = common.origin[axis] * UnitsPerCell;
        RequireInRange(offset + minimum[axis] * ChunkUnits, At("bounds.min", axis));
        RequireInRange(offset + maximum[axis] * ChunkUnits + ChunkUnits, At("bounds.max", axis));
    }

    VoxelMapDocument result;
    result.sourceSha256 = std::move(common.sourceSha256);
    result.buildVersion = std::move(common.buildVersion);
    result.origin = ToVec3i(common.origin);
    result.cellSize = common.cellSize;
    result.palette = std::move(common.palette);
    result.boundsMinimum = ToChunkKey(minimum);
    result.boundsMaximum = ToChunkKey(maximum);

    const auto& regions = document.at("regions");
    if (!regions.is_array() || regions.empty()) Fail("regions lists one or more regions", "regions");
    std::set<std::string> files;
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const auto path = At("regions", i);
        const auto& entry = regions[i];
        if (!entry.is_object()) Fail("a region entry is an object", path);
        RequireExactKeys(entry, {"at", "file", "sha256"}, path);
        const auto at = Pair(entry.at("at"), path + ".at");
        if (at[0] < FloorDivide(minimum[0], RegionChunks) || at[0] > FloorDivide(maximum[0], RegionChunks) ||
            at[1] < FloorDivide(minimum[2], RegionChunks) || at[1] > FloorDivide(maximum[2], RegionChunks))
            Fail("a region lies within the bounds", path + ".at");
        const auto& file = entry.at("file");
        if (!file.is_string() || !ValidRegionFile(file.get<std::string>()))
            Fail("file is 1 to 128 characters of a-z, 0-9, '_', '.', '-' and '/', relative, without empty, '.' or "
                 "'..' segments",
                 path + ".file");
        if (!files.insert(file.get<std::string>()).second) Fail("a region file repeats", path + ".file");
        const auto& sha = entry.at("sha256");
        if (!sha.is_string() || !ValidSha256(sha.get<std::string>()))
            Fail("sha256 is 64 lowercase hexadecimal digits", path + ".sha256");
        VoxelMapRegion region{{static_cast<std::int32_t>(at[0]), static_cast<std::int32_t>(at[1])},
                              file.get<std::string>(), sha.get<std::string>()};
        if (!result.regions.empty() && !(result.regions.back().at < region.at))
            Fail("regions are in strictly increasing order of at", path + ".at");
        result.regions.push_back(std::move(region));
    }
    return result;
}

// A chunk as integers, before any rule: the one input of the chunk rules,
// whether it came from text (ParseVoxelRegion) or from a document
// (ValidateVoxelChunk).
struct RawChunk final {
    std::array<std::int64_t, 3> at{};
    std::vector<std::array<std::int64_t, 7>> quads;
    std::vector<std::array<std::int64_t, 6>> colliders;
};

// The chunk rules (gyo.voxel_region C1 to C5 and the bounds of R4), the one
// implementation behind ParseVoxelRegion and ValidateVoxelChunk. prefix is
// prepended to every path ("" or "chunks[2]").
VoxelChunkDocument CheckChunk(const RawChunk& raw, const VoxelMapDocument& map, const std::string& prefix) {
    const std::array<std::int64_t, 3> low{map.boundsMinimum.x, map.boundsMinimum.y, map.boundsMinimum.z};
    const std::array<std::int64_t, 3> high{map.boundsMaximum.x, map.boundsMaximum.y, map.boundsMaximum.z};
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (raw.at[axis] < low[axis] || raw.at[axis] > high[axis])
            Fail("a chunk lies within the map's bounds", Join(prefix, "at"));
    if (raw.quads.empty() && raw.colliders.empty()) Fail("a chunk has a quad or a collider", prefix);

    VoxelChunkDocument result;
    result.at = ToChunkKey(raw.at);
    constexpr std::array<std::int64_t, 3> noOffset{};
    for (std::size_t i = 0; i < raw.quads.size(); ++i) {
        const auto path = At(Join(prefix, "quads"), i);
        auto quad = CheckQuad(raw.quads[i], map.palette.size(), noOffset, RequireInChunk, path);
        if (!result.quads.empty() && !(QuadKey(result.quads.back()) < QuadKey(quad)))
            Fail("quads are in strictly increasing order", path);
        result.quads.push_back(quad);
    }
    for (std::size_t i = 0; i < raw.colliders.size(); ++i) {
        const auto path = At(Join(prefix, "colliders"), i);
        const auto& c = raw.colliders[i];
        auto box = CheckBox({c[0], c[1], c[2]}, {c[3], c[4], c[5]}, noOffset, RequireInChunk, path);
        if (!result.colliders.empty() && !(BoxKey(result.colliders.back()) < BoxKey(box)))
            Fail("colliders are in strictly increasing order", path);
        result.colliders.push_back(box);
    }
    return result;
}

RawChunk ParseRawChunk(const Json& entry, const std::string& path) {
    if (!entry.is_object()) Fail("a chunk is an object", path);
    RequireExactKeys(entry, {"at", "quads", "colliders"}, path);
    RawChunk raw;
    raw.at = Triple(entry.at("at"), path + ".at");
    const auto& quads = entry.at("quads");
    if (!quads.is_array()) Fail("quads is an array", path + ".quads");
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const auto quadPath = At(path + ".quads", i);
        const auto& quad = quads[i];
        if (!quad.is_array() || quad.size() != 7) Fail("a quad is seven integers", quadPath);
        std::array<std::int64_t, 7> v{};
        for (std::size_t k = 0; k < v.size(); ++k) v[k] = Integer(quad[k], At(quadPath, k));
        raw.quads.push_back(v);
    }
    const auto& colliders = entry.at("colliders");
    if (!colliders.is_array()) Fail("colliders is an array", path + ".colliders");
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto boxPath = At(path + ".colliders", i);
        const auto& box = colliders[i];
        if (!box.is_object()) Fail("a collider is an object", boxPath);
        RequireExactKeys(box, {"min", "max"}, boxPath);
        const auto minimum = Triple(box.at("min"), boxPath + ".min");
        const auto maximum = Triple(box.at("max"), boxPath + ".max");
        raw.colliders.push_back({minimum[0], minimum[1], minimum[2], maximum[0], maximum[1], maximum[2]});
    }
    return raw;
}

VoxelRegionDocument ParseRegionDocument(const std::string_view text, const VoxelMapDocument& map,
                                        const VoxelRegionKey expected) {
    const auto listed = std::lower_bound(map.regions.begin(), map.regions.end(), expected,
                                         [](const VoxelMapRegion& region, const VoxelRegionKey& k) { return region.at < k; });
    // The caller names an entry of the map; any other key is a programmer error.
    GYO_ASSERT(listed != map.regions.end() && listed->at == expected);
    if (text.size() > MaxRegionTextBytes) Fail("the text exceeds 16 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document, {"schema", "version", "at", "chunks"}, "");
    RequireSchemaAndVersion(document, VoxelRegionSchema, VoxelRegionVersion);

    const auto at = Pair(document.at("at"), "at");
    const VoxelRegionKey key{static_cast<std::int32_t>(at[0]), static_cast<std::int32_t>(at[1])};
    if (key != expected) Fail("the text is the region of the map entry it was read for", "at");
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    if (Base::Sha256(bytes) != listed->sha256) Fail("the text's SHA-256 differs from the map's entry", "");

    const auto& chunks = document.at("chunks");
    if (!chunks.is_array() || chunks.empty()) Fail("chunks lists one or more chunks", "chunks");
    VoxelRegionDocument result;
    result.at = key;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        const auto path = At("chunks", i);
        const auto raw = ParseRawChunk(chunks[i], path);
        if (FloorDivide(raw.at[0], RegionChunks) != at[0] || FloorDivide(raw.at[2], RegionChunks) != at[1])
            Fail("a chunk lies in its region", path + ".at");
        auto chunk = CheckChunk(raw, map, path);
        if (!result.chunks.empty() && !(result.chunks.back().at < chunk.at))
            Fail("chunks are in strictly increasing order of at", path + ".at");
        result.chunks.push_back(std::move(chunk));
    }
    return result;
}

RawChunk ToRawChunk(const VoxelChunkDocument& chunk) {
    RawChunk raw;
    raw.at = {chunk.at.x, chunk.at.y, chunk.at.z};
    raw.quads.reserve(chunk.quads.size());
    for (const auto& q : chunk.quads)
        raw.quads.push_back({q.x, q.y, q.z, static_cast<std::int64_t>(q.face), q.width, q.height, q.palette});
    raw.colliders.reserve(chunk.colliders.size());
    for (const auto& b : chunk.colliders)
        raw.colliders.push_back({b.minimum.x, b.minimum.y, b.minimum.z, b.maximum.x, b.maximum.y, b.maximum.z});
    return raw;
}

VoxelMeshError MakeError(std::string message, std::string path) {
    return VoxelMeshError::Make(VoxelMeshErrorCode::InvalidVoxelMesh, "gyo.voxel_mesh: " + std::move(message),
                                std::move(path));
}

VoxelMeshError MakeError(const VoxelMeshErrorCode code, const std::string_view schema, Invalid& invalid) {
    return VoxelMeshError::Make(code, std::string(schema) + ": " + std::move(invalid.message), std::move(invalid.path));
}

// float(units) is exact within 2^24; the product rounds once; dividing by 16
// is exact. So every platform gives the same bits.
float ToWorld(const std::int64_t units, const float cellSize) {
    return static_cast<float>(units) * cellSize / static_cast<float>(UnitsPerCell);
}

// One column per palette entry, one row per face direction.
void FillPalette(const std::vector<VoxelPaletteEntry>& palette, std::uint32_t& width, std::uint32_t& height,
                 std::vector<std::byte>& rgba8) {
    width = static_cast<std::uint32_t>(palette.size());
    height = static_cast<std::uint32_t>(FaceCount);
    rgba8.resize(std::size_t{width} * height * 4);
    for (std::size_t column = 0; column < palette.size(); ++column) {
        for (std::size_t row = 0; row < FaceCount; ++row) {
            const auto base = (row * width + column) * 4;
            for (std::size_t channel = 0; channel < 4; ++channel)
                rgba8[base + channel] = static_cast<std::byte>(palette[column].faces[row][channel]);
        }
    }
}

// Appends the four vertices and six indices of every quad and the world box of
// every collider; units are offset by (ox, oy, oz) before conversion. The same
// code expands gyo.voxel_mesh and a chunk, so both give the same bits.
void AppendGeometry(const std::vector<VoxelQuad>& quads, const std::vector<VoxelBox>& boxes, const std::int64_t ox,
                    const std::int64_t oy, const std::int64_t oz, const float cell, const std::size_t paletteSize,
                    std::vector<VoxelMeshVertex>& vertices, std::vector<std::uint32_t>& indices,
                    std::vector<Math::Aabb>& colliders) {
    const auto world = [&](const std::int64_t x, const std::int64_t y, const std::int64_t z) {
        return Math::Vec3{ToWorld(x + ox, cell), ToWorld(y + oy, cell), ToWorld(z + oz, cell)};
    };
    vertices.reserve(vertices.size() + quads.size() * 4);
    indices.reserve(indices.size() + quads.size() * 6);
    const float width = static_cast<float>(paletteSize);
    const float height = static_cast<float>(FaceCount);
    for (const auto& quad : quads) {
        const std::int64_t x0 = quad.x;
        const std::int64_t y0 = quad.y;
        const std::int64_t z0 = quad.z;
        // Corners in the order of Render's unit cube faces (PrimitiveMesh.cpp,
        // MakeUnitCube): bottom-left, bottom-right, top-right, top-left seen
        // from outside, which Engine draws clockwise as the front face.
        std::array<Math::Vec3, 4> corners{};
        switch (quad.face) {
        case VoxelFace::PositiveX: {
            const auto z1 = z0 + quad.width, y1 = y0 + quad.height;
            corners = {world(x0, y0, z1), world(x0, y0, z0), world(x0, y1, z0), world(x0, y1, z1)};
            break;
        }
        case VoxelFace::NegativeX: {
            const auto z1 = z0 + quad.width, y1 = y0 + quad.height;
            corners = {world(x0, y0, z0), world(x0, y0, z1), world(x0, y1, z1), world(x0, y1, z0)};
            break;
        }
        case VoxelFace::PositiveY: {
            const auto x1 = x0 + quad.width, z1 = z0 + quad.height;
            corners = {world(x0, y0, z1), world(x1, y0, z1), world(x1, y0, z0), world(x0, y0, z0)};
            break;
        }
        case VoxelFace::NegativeY: {
            const auto x1 = x0 + quad.width, z1 = z0 + quad.height;
            corners = {world(x0, y0, z0), world(x1, y0, z0), world(x1, y0, z1), world(x0, y0, z1)};
            break;
        }
        case VoxelFace::PositiveZ: {
            const auto x1 = x0 + quad.width, y1 = y0 + quad.height;
            corners = {world(x0, y0, z0), world(x1, y0, z0), world(x1, y1, z0), world(x0, y1, z0)};
            break;
        }
        case VoxelFace::NegativeZ: {
            const auto x1 = x0 + quad.width, y1 = y0 + quad.height;
            corners = {world(x1, y0, z0), world(x0, y0, z0), world(x0, y1, z0), world(x1, y1, z0)};
            break;
        }
        }
        // The centre of the quad's texel: the column of its palette entry and
        // the row of its direction (Render samples linearly, without mipmaps).
        const Math::Vec2 uv{(static_cast<float>(quad.palette) + 0.5F) / width,
                            (static_cast<float>(static_cast<std::uint8_t>(quad.face)) + 0.5F) / height};
        const auto first = static_cast<std::uint32_t>(vertices.size());
        for (const auto& corner : corners) vertices.push_back({corner, uv});
        indices.insert(indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }

    colliders.reserve(colliders.size() + boxes.size());
    for (const auto& box : boxes) {
        colliders.push_back({world(box.minimum.x, box.minimum.y, box.minimum.z),
                             world(box.maximum.x, box.maximum.y, box.maximum.z)});
    }
}

bool InBounds(const VoxelChunkKey key, const VoxelMapDocument& map) {
    return key.x >= map.boundsMinimum.x && key.x <= map.boundsMaximum.x && key.y >= map.boundsMinimum.y &&
           key.y <= map.boundsMaximum.y && key.z >= map.boundsMinimum.z && key.z <= map.boundsMaximum.z;
}

} // namespace

VoxelMeshResult<VoxelMeshDocument> ParseVoxelMesh(const std::string_view text) {
    try {
        return ParseDocument(text);
    } catch (Invalid& invalid) {
        return Base::Err(MakeError(std::move(invalid.message), std::move(invalid.path)));
    }
}

VoxelMeshGeometry ExpandVoxelMesh(const VoxelMeshDocument& document) {
    // Only ParseVoxelMesh's result is valid input (Design Invariant 3); these
    // are the preconditions the expansion itself relies on.
    GYO_ASSERT(!document.palette.empty() && document.palette.size() <= MaxPaletteEntries);
    GYO_ASSERT(document.cellSize >= MinCellSize && document.cellSize <= MaxCellSize);
    GYO_ASSERT(std::all_of(document.quads.begin(), document.quads.end(),
                           [&](const VoxelQuad& q) { return q.palette < document.palette.size(); }));
    VoxelMeshGeometry geometry;
    const std::int64_t ox = std::int64_t{document.origin.x} * UnitsPerCell;
    const std::int64_t oy = std::int64_t{document.origin.y} * UnitsPerCell;
    const std::int64_t oz = std::int64_t{document.origin.z} * UnitsPerCell;
    FillPalette(document.palette, geometry.paletteWidth, geometry.paletteHeight, geometry.paletteRgba8);
    AppendGeometry(document.quads, document.colliders, ox, oy, oz, document.cellSize, document.palette.size(),
                   geometry.vertices, geometry.indices, geometry.colliders);
    return geometry;
}

VoxelMeshResult<VoxelMapDocument> ParseVoxelMap(const std::string_view text) {
    try {
        return ParseMapDocument(text);
    } catch (Invalid& invalid) {
        return Base::Err(MakeError(VoxelMeshErrorCode::InvalidVoxelMap, VoxelMapSchema, invalid));
    }
}

VoxelMeshResult<VoxelRegionDocument> ParseVoxelRegion(const std::string_view text, const VoxelMapDocument& map,
                                                      const VoxelRegionKey expected) {
    try {
        return ParseRegionDocument(text, map, expected);
    } catch (Invalid& invalid) {
        return Base::Err(MakeError(VoxelMeshErrorCode::InvalidVoxelRegion, VoxelRegionSchema, invalid));
    }
}

VoxelMeshResult<void> ValidateVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    try {
        static_cast<void>(CheckChunk(ToRawChunk(chunk), map, ""));
        return {};
    } catch (Invalid& invalid) {
        return Base::Err(MakeError(VoxelMeshErrorCode::InvalidVoxelChunk, VoxelRegionSchema, invalid));
    }
}

VoxelChunkGeometry ExpandVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map) {
    // Only a chunk the rules accepted with this map is valid input; these are
    // the preconditions the expansion itself relies on.
    GYO_ASSERT(!map.palette.empty() && map.palette.size() <= MaxPaletteEntries);
    GYO_ASSERT(map.cellSize >= MinCellSize && map.cellSize <= MaxCellSize);
    GYO_ASSERT(InBounds(chunk.at, map));
    GYO_ASSERT(std::all_of(chunk.quads.begin(), chunk.quads.end(), [&](const VoxelQuad& q) {
        return q.palette < map.palette.size() && static_cast<std::size_t>(q.face) < FaceCount;
    }));
    VoxelChunkGeometry geometry;
    const std::int64_t ox = std::int64_t{map.origin.x} * UnitsPerCell + std::int64_t{chunk.at.x} * ChunkUnits;
    const std::int64_t oy = std::int64_t{map.origin.y} * UnitsPerCell + std::int64_t{chunk.at.y} * ChunkUnits;
    const std::int64_t oz = std::int64_t{map.origin.z} * UnitsPerCell + std::int64_t{chunk.at.z} * ChunkUnits;
    AppendGeometry(chunk.quads, chunk.colliders, ox, oy, oz, map.cellSize, map.palette.size(), geometry.vertices,
                   geometry.indices, geometry.colliders);
    return geometry;
}

VoxelPaletteImage ExpandVoxelPalette(const VoxelMapDocument& map) {
    GYO_ASSERT(!map.palette.empty() && map.palette.size() <= MaxPaletteEntries);
    VoxelPaletteImage image;
    FillPalette(map.palette, image.width, image.height, image.rgba8);
    return image;
}

VoxelRegionKey RegionOf(const VoxelChunkKey key) noexcept {
    return {static_cast<std::int32_t>(FloorDivide(key.x, RegionChunks)),
            static_cast<std::int32_t>(FloorDivide(key.z, RegionChunks))};
}

Math::Vec3 ChunkWorldOrigin(const VoxelChunkKey key, const VoxelMapDocument& map) {
    GYO_ASSERT(InBounds(key, map));
    return {ToWorld(std::int64_t{map.origin.x} * UnitsPerCell + std::int64_t{key.x} * ChunkUnits, map.cellSize),
            ToWorld(std::int64_t{map.origin.y} * UnitsPerCell + std::int64_t{key.y} * ChunkUnits, map.cellSize),
            ToWorld(std::int64_t{map.origin.z} * UnitsPerCell + std::int64_t{key.z} * ChunkUnits, map.cellSize)};
}

Math::Vec3 MapUnitsToWorld(const Math::Vec3i units, const VoxelMapDocument& map) {
    return {ToWorld(std::int64_t{units.x} + std::int64_t{map.origin.x} * UnitsPerCell, map.cellSize),
            ToWorld(std::int64_t{units.y} + std::int64_t{map.origin.y} * UnitsPerCell, map.cellSize),
            ToWorld(std::int64_t{units.z} + std::int64_t{map.origin.z} * UnitsPerCell, map.cellSize)};
}

} // namespace Engine::VoxelMesh
