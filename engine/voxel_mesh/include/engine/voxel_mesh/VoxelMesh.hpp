#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/VecInt.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::VoxelMesh {

// gyo.voxel_mesh version 1: the baked geometry of a map built on a grid
// (docs/architecture/voxel-mesh.md). A tool decides culling, merging and
// shading and writes the result; this module only validates and expands it.
// It knows faces, colours, boxes and opaque names, never blocks or products.

inline constexpr std::string_view VoxelMeshSchema = "gyo.voxel_mesh";
inline constexpr int VoxelMeshVersion = 1;
// Coordinates are integers in 1/16 of a cell.
inline constexpr std::int64_t UnitsPerCell = 16;
// Every corner, after adding 16 * origin, lies within +-2^20 units, where the
// conversion to float is exact.
inline constexpr std::int64_t CoordinateLimit = std::int64_t{1} << 20;
inline constexpr std::size_t MaxTextBytes = std::size_t{16} * 1024 * 1024;
inline constexpr std::size_t MaxPaletteEntries = 256;
inline constexpr std::size_t MaxNameLength = 64;
inline constexpr std::size_t MaxBuildVersionLength = 64;
// cell_size is within [2^-16, 2^16].
inline constexpr float MinCellSize = 1.0F / 65536.0F;
inline constexpr float MaxCellSize = 65536.0F;

// Failure classes of the module. Zero is not a valid code; the numeric values
// are not a data contract.
enum class VoxelMeshErrorCode : std::uint8_t {
    // A gyo.voxel_mesh document breaks its contract.
    InvalidVoxelMesh = 1,
};

[[nodiscard]] constexpr const char* ToString(const VoxelMeshErrorCode code) noexcept {
    switch (code) {
    case VoxelMeshErrorCode::InvalidVoxelMesh: return "InvalidVoxelMesh";
    }
    return "Unknown";
}

using VoxelMeshError = Base::Error<VoxelMeshErrorCode>;
static_assert(Base::CodedError<VoxelMeshError>);

template <class T>
using VoxelMeshResult = Base::Result<T, VoxelMeshError>;

// The direction a face looks at; also its row in the palette image.
enum class VoxelFace : std::uint8_t {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};
inline constexpr std::size_t FaceCount = 6;

struct VoxelPaletteEntry final {
    std::string name; // opaque: what it means belongs to the tool
    // sRGB 8-bit RGBA per face direction, in VoxelFace order.
    std::array<std::array<std::uint8_t, 4>, FaceCount> faces{};
};

// One axis-aligned rectangle. (x, y, z) is its minimum corner, in units from
// origin; width and height run along the face's two in-plane axes:
// X faces (Z, Y), Y faces (X, Z), Z faces (X, Y).
struct VoxelQuad final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    VoxelFace face{VoxelFace::PositiveX};
    std::int32_t width{};
    std::int32_t height{};
    std::uint32_t palette{};
};

// A collision box in units from origin; minimum < maximum on every axis.
struct VoxelBox final {
    Math::Vec3i minimum{};
    Math::Vec3i maximum{};
};

// A validated document, still in integers.
struct VoxelMeshDocument final {
    std::string sourceSha256;
    std::string buildVersion;
    Math::Vec3i origin{}; // in cells
    float cellSize{};     // world length of one cell
    std::vector<VoxelPaletteEntry> palette;
    std::vector<VoxelQuad> quads;      // nonempty, in canonical order
    std::vector<VoxelBox> colliders;   // in canonical order, may be empty
};

// Checks every rule of version 1. Anything the contract does not allow is an
// InvalidVoxelMesh error with the offending path as its detail; nothing is
// ignored or repaired.
[[nodiscard]] VoxelMeshResult<VoxelMeshDocument> ParseVoxelMesh(std::string_view text);

struct VoxelMeshVertex final {
    Math::Vec3 position{};
    Math::Vec2 uv{};
};

// World-space geometry of a document. Every quad gives four vertices and six
// indices in quad order, clockwise seen from outside (Engine's front face).
// A world coordinate is float(units + 16 * origin) * cellSize / 16; every
// vertex of a quad samples the centre of its texel in the palette image.
struct VoxelMeshGeometry final {
    std::vector<VoxelMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    // One column per palette entry, one row per VoxelFace; tightly packed
    // sRGB RGBA8.
    std::uint32_t paletteWidth{};
    std::uint32_t paletteHeight{};
    std::vector<std::byte> paletteRgba8;
    std::vector<Math::Aabb> colliders;
};

// Expands a document returned by ParseVoxelMesh; it cannot fail. Any other
// document is a programmer error (asserted).
[[nodiscard]] VoxelMeshGeometry ExpandVoxelMesh(const VoxelMeshDocument& document);

} // namespace Engine::VoxelMesh
