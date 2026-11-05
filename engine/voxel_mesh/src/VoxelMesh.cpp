#include "engine/voxel_mesh/VoxelMesh.hpp"

#include "engine/base/Assert.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
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

// The DOM keeps the last of repeated keys, so repeats are caught while parsing.
Json ParseWithoutRepeatedKeys(const std::string_view text) {
    std::vector<std::set<std::string>> open;
    Json::parser_callback_t callback = [&open](int, const Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start) {
            open.emplace_back();
        } else if (event == Json::parse_event_t::object_end) {
            open.pop_back();
        } else if (event == Json::parse_event_t::key) {
            const auto key = parsed.get<std::string>();
            if (!open.back().insert(key).second) Fail("repeated key '" + key + "'", "");
        }
        return true;
    };
    try {
        return Json::parse(text.begin(), text.end(), callback);
    } catch (const Json::exception& failure) {
        Fail(std::string("not JSON: ") + failure.what(), "");
    }
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

VoxelQuad ParseQuad(const Json& entry, const std::size_t paletteSize, const std::array<std::int64_t, 3>& offset,
                    const std::string& path) {
    if (!entry.is_array() || entry.size() != 7) Fail("a quad is seven integers", path);
    std::array<std::int64_t, 7> v{};
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = Integer(entry[i], At(path, i));
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
        RequireInRange(v[axis] + offset[axis], At(path, axis));
        RequireInRange(v[axis] + extent[axis] + offset[axis], path);
    }
    return {static_cast<std::int32_t>(v[0]), static_cast<std::int32_t>(v[1]), static_cast<std::int32_t>(v[2]),
            face, static_cast<std::int32_t>(v[4]), static_cast<std::int32_t>(v[5]),
            static_cast<std::uint32_t>(v[6])};
}

VoxelBox ParseCollider(const Json& entry, const std::array<std::int64_t, 3>& offset, const std::string& path) {
    if (!entry.is_object()) Fail("a collider is an object", path);
    RequireExactKeys(entry, {"min", "max"}, path);
    const auto minimum = Triple(entry.at("min"), path + ".min");
    const auto maximum = Triple(entry.at("max"), path + ".max");
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!(minimum[axis] < maximum[axis])) Fail("min is below max on every axis", path);
        RequireInRange(minimum[axis] + offset[axis], At(path + ".min", axis));
        RequireInRange(maximum[axis] + offset[axis], At(path + ".max", axis));
    }
    return {{static_cast<std::int32_t>(minimum[0]), static_cast<std::int32_t>(minimum[1]),
             static_cast<std::int32_t>(minimum[2])},
            {static_cast<std::int32_t>(maximum[0]), static_cast<std::int32_t>(maximum[1]),
             static_cast<std::int32_t>(maximum[2])}};
}

auto QuadKey(const VoxelQuad& q) {
    return std::make_tuple(q.x, q.y, q.z, static_cast<int>(q.face), q.width, q.height, q.palette);
}

auto BoxKey(const VoxelBox& b) {
    return std::make_tuple(b.minimum.x, b.minimum.y, b.minimum.z, b.maximum.x, b.maximum.y, b.maximum.z);
}

VoxelMeshDocument ParseDocument(const std::string_view text) {
    if (text.size() > MaxTextBytes) Fail("the text exceeds 16 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document,
                     {"schema", "version", "source_sha256", "build_version", "origin", "cell_size", "palette",
                      "quads", "colliders"},
                     "");
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != VoxelMeshSchema)
        Fail("schema is '" + std::string(VoxelMeshSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != VoxelMeshVersion)
        Fail("version is " + std::to_string(VoxelMeshVersion), "version");

    VoxelMeshDocument result;
    const auto& sha = document.at("source_sha256");
    if (!sha.is_string() || !ValidSha256(sha.get<std::string>()))
        Fail("source_sha256 is 64 lowercase hexadecimal digits", "source_sha256");
    result.sourceSha256 = sha.get<std::string>();
    const auto& build = document.at("build_version");
    if (!build.is_string() || !ValidBuildVersion(build.get<std::string>()))
        Fail("build_version is 1 to 64 printable ASCII characters", "build_version");
    result.buildVersion = build.get<std::string>();

    const auto origin = Triple(document.at("origin"), "origin");
    result.origin = {static_cast<std::int32_t>(origin[0]), static_cast<std::int32_t>(origin[1]),
                     static_cast<std::int32_t>(origin[2])};
    const std::array<std::int64_t, 3> offset{origin[0] * UnitsPerCell, origin[1] * UnitsPerCell,
                                             origin[2] * UnitsPerCell};
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

    const auto& quads = document.at("quads");
    if (!quads.is_array() || quads.empty()) Fail("quads lists one or more quads", "quads");
    for (std::size_t i = 0; i < quads.size(); ++i) {
        auto quad = ParseQuad(quads[i], result.palette.size(), offset, At("quads", i));
        if (!result.quads.empty() && !(QuadKey(result.quads.back()) < QuadKey(quad)))
            Fail("quads are in strictly increasing order", At("quads", i));
        result.quads.push_back(quad);
    }

    const auto& colliders = document.at("colliders");
    if (!colliders.is_array()) Fail("colliders is an array", "colliders");
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        auto box = ParseCollider(colliders[i], offset, At("colliders", i));
        if (!result.colliders.empty() && !(BoxKey(result.colliders.back()) < BoxKey(box)))
            Fail("colliders are in strictly increasing order", At("colliders", i));
        result.colliders.push_back(box);
    }
    return result;
}

VoxelMeshError MakeError(std::string message, std::string path) {
    return VoxelMeshError::Make(VoxelMeshErrorCode::InvalidVoxelMesh, "gyo.voxel_mesh: " + std::move(message),
                                std::move(path));
}

// float(units) is exact within 2^24; the product rounds once; dividing by 16
// is exact. So every platform gives the same bits.
float ToWorld(const std::int64_t units, const float cellSize) {
    return static_cast<float>(units) * cellSize / static_cast<float>(UnitsPerCell);
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
    const float cell = document.cellSize;
    const auto world = [&](const std::int64_t x, const std::int64_t y, const std::int64_t z) {
        return Math::Vec3{ToWorld(x + ox, cell), ToWorld(y + oy, cell), ToWorld(z + oz, cell)};
    };

    geometry.paletteWidth = static_cast<std::uint32_t>(document.palette.size());
    geometry.paletteHeight = static_cast<std::uint32_t>(FaceCount);
    geometry.paletteRgba8.resize(std::size_t{geometry.paletteWidth} * geometry.paletteHeight * 4);
    for (std::size_t column = 0; column < document.palette.size(); ++column) {
        for (std::size_t row = 0; row < FaceCount; ++row) {
            const auto base = (row * geometry.paletteWidth + column) * 4;
            for (std::size_t channel = 0; channel < 4; ++channel)
                geometry.paletteRgba8[base + channel] = static_cast<std::byte>(document.palette[column].faces[row][channel]);
        }
    }

    geometry.vertices.reserve(document.quads.size() * 4);
    geometry.indices.reserve(document.quads.size() * 6);
    const float width = static_cast<float>(geometry.paletteWidth);
    const float height = static_cast<float>(geometry.paletteHeight);
    for (const auto& quad : document.quads) {
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
        const auto first = static_cast<std::uint32_t>(geometry.vertices.size());
        for (const auto& corner : corners) geometry.vertices.push_back({corner, uv});
        geometry.indices.insert(geometry.indices.end(),
                                {first, first + 1, first + 2, first, first + 2, first + 3});
    }

    geometry.colliders.reserve(document.colliders.size());
    for (const auto& box : document.colliders) {
        geometry.colliders.push_back({world(box.minimum.x, box.minimum.y, box.minimum.z),
                                      world(box.maximum.x, box.maximum.y, box.maximum.z)});
    }
    return geometry;
}

} // namespace Engine::VoxelMesh
