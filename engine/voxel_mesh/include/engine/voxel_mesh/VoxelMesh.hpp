#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/VecInt.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::VoxelMesh {

// The baked geometry of a map built on a grid (docs/architecture/voxel-mesh.md).
// A tool decides culling, merging and shading and writes the result; this
// module only validates and expands it. It knows faces, colours, boxes, chunks
// and opaque names, never blocks or products.
//
// Formats: gyo.voxel_map version 1 (an index) with gyo.voxel_region version 1
// (the files of 8 x 8 chunk columns), and the single-file gyo.voxel_mesh
// version 1, which is deprecated: it is read until the products and tools
// have moved to the chunked map, then removed (v8-arch-big-map-engine-format).

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
    // A gyo.voxel_map document breaks its contract.
    InvalidVoxelMap = 2,
    // A gyo.voxel_region document breaks its contract (or does not belong to
    // the map it is read with).
    InvalidVoxelRegion = 3,
    // A chunk checked with ValidateVoxelChunk breaks its contract.
    InvalidVoxelChunk = 4,
};

[[nodiscard]] constexpr const char* ToString(const VoxelMeshErrorCode code) noexcept {
    switch (code) {
    case VoxelMeshErrorCode::InvalidVoxelMesh: return "InvalidVoxelMesh";
    case VoxelMeshErrorCode::InvalidVoxelMap: return "InvalidVoxelMap";
    case VoxelMeshErrorCode::InvalidVoxelRegion: return "InvalidVoxelRegion";
    case VoxelMeshErrorCode::InvalidVoxelChunk: return "InvalidVoxelChunk";
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

// Deprecated with gyo.voxel_mesh version 1 (see the top of this file).
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

// Deprecated with gyo.voxel_mesh version 1 (see the top of this file).
// Expands a document returned by ParseVoxelMesh; it cannot fail. Any other
// document is a programmer error (asserted).
[[nodiscard]] VoxelMeshGeometry ExpandVoxelMesh(const VoxelMeshDocument& document);

// ---------------------------------------------------------------------------
// gyo.voxel_map version 1 and gyo.voxel_region version 1: a map split into
// chunks of 16^3 cells. A chunk is the unit of validation and expansion, a
// region (8 x 8 chunk columns, all heights) the unit of files; how a consumer
// draws or loads them is its own. Every iteration order follows the total
// order of chunk and region keys.

inline constexpr std::string_view VoxelMapSchema = "gyo.voxel_map";
inline constexpr std::string_view VoxelRegionSchema = "gyo.voxel_region";
inline constexpr int VoxelMapVersion = 1;
inline constexpr int VoxelRegionVersion = 1;
// Cells along each edge of a chunk, and chunk columns along each horizontal
// edge of a region (fixed in version 1; written in the index).
inline constexpr std::int64_t ChunkCells = 16;
inline constexpr std::int64_t RegionChunks = 8;
// A chunk's edge in units: chunk-local coordinates lie in [0, ChunkUnits].
inline constexpr std::int64_t ChunkUnits = ChunkCells * UnitsPerCell;
inline constexpr std::size_t MaxMapTextBytes = std::size_t{1} * 1024 * 1024;
inline constexpr std::size_t MaxRegionTextBytes = std::size_t{16} * 1024 * 1024;
inline constexpr std::size_t MaxRegionFileLength = 128;

// A chunk's coordinates in chunks; ordered by (x, y, z).
struct VoxelChunkKey final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    friend auto operator<=>(const VoxelChunkKey&, const VoxelChunkKey&) = default;
};

// A region's coordinates in regions (horizontal only); ordered by (x, z).
struct VoxelRegionKey final {
    std::int32_t x{};
    std::int32_t z{};
    friend auto operator<=>(const VoxelRegionKey&, const VoxelRegionKey&) = default;
};

struct VoxelMapRegion final {
    VoxelRegionKey at{};
    std::string file;   // where the consumer finds the region's text; opaque here
    std::string sha256; // of the region's text, lowercase hexadecimal
};

// A validated index.
struct VoxelMapDocument final {
    std::string sourceSha256;
    std::string buildVersion;
    Math::Vec3i origin{}; // in cells
    float cellSize{};     // world length of one cell
    VoxelChunkKey boundsMinimum{}; // inclusive, in chunks
    VoxelChunkKey boundsMaximum{}; // inclusive, in chunks
    std::vector<VoxelPaletteEntry> palette;
    std::vector<VoxelMapRegion> regions; // nonempty, in key order
};

// One chunk; quads and boxes in units from the chunk's minimum corner
// (0 to ChunkUnits on every axis).
struct VoxelChunkDocument final {
    VoxelChunkKey at{};
    std::vector<VoxelQuad> quads;    // in canonical order
    std::vector<VoxelBox> colliders; // in canonical order
};

// A validated region: its chunks in key order, empty chunks left out.
struct VoxelRegionDocument final {
    VoxelRegionKey at{};
    std::vector<VoxelChunkDocument> chunks;
};

// Checks every rule of gyo.voxel_map version 1 (InvalidVoxelMap with the
// offending path as detail).
[[nodiscard]] VoxelMeshResult<VoxelMapDocument> ParseVoxelMap(std::string_view text);

// Checks every rule of gyo.voxel_region version 1 against the map entry the
// caller read the text for: the text is the region expected names (its at),
// it has that entry's SHA-256, and every chunk lies in the region and the
// map's bounds (InvalidVoxelRegion). So a file that holds another region's
// text is rejected here, not left to each consumer to notice. map must come
// from ParseVoxelMap and list expected (asserted).
[[nodiscard]] VoxelMeshResult<VoxelRegionDocument> ParseVoxelRegion(std::string_view text,
                                                                    const VoxelMapDocument& map,
                                                                    VoxelRegionKey expected);

// Checks one chunk without text, by the same rules (the same code) that
// ParseVoxelRegion applies to each chunk: within the map's bounds, at least
// one quad or box, every quad and box valid and inside the chunk, canonical
// order (InvalidVoxelChunk, detail relative to the chunk such as quads[3][4]).
// map must come from ParseVoxelMap.
[[nodiscard]] VoxelMeshResult<void> ValidateVoxelChunk(const VoxelChunkDocument& chunk,
                                                       const VoxelMapDocument& map);

// World-space geometry of one chunk: as VoxelMeshGeometry without the palette
// image (ExpandVoxelPalette gives the one image of a map). A world coordinate
// is float(local + 256 * key + 16 * origin) * cellSize / 16.
struct VoxelChunkGeometry final {
    std::vector<VoxelMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Math::Aabb> colliders;
};

struct VoxelPaletteImage final {
    // One column per palette entry, one row per VoxelFace; tightly packed
    // sRGB RGBA8.
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::byte> rgba8;
};

// Expands a chunk that ParseVoxelRegion returned or ValidateVoxelChunk
// accepted with the same map; it cannot fail. Any other chunk is a programmer
// error (asserted). uv addresses the map's palette image.
[[nodiscard]] VoxelChunkGeometry ExpandVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map);
[[nodiscard]] VoxelPaletteImage ExpandVoxelPalette(const VoxelMapDocument& map);

// The region a chunk belongs to: floor(x / 8), floor(z / 8).
[[nodiscard]] VoxelRegionKey RegionOf(VoxelChunkKey key) noexcept;
// The world position of a chunk's minimum corner; key must lie in the map's
// bounds (asserted), where the conversion is exact.
[[nodiscard]] Math::Vec3 ChunkWorldOrigin(VoxelChunkKey key, const VoxelMapDocument& map);
// The world position of integer units in 1/16 of a cell from the map's
// origin (gyo.map_markers coordinates, for example): float(units + 16 *
// origin) * cellSize / 16 per axis. Exact for every units within +-2^20:
// rule M4 keeps 16 * origin within +-2^20, so each sum stays within +-2^21.
[[nodiscard]] Math::Vec3 MapUnitsToWorld(Math::Vec3i units, const VoxelMapDocument& map);

} // namespace Engine::VoxelMesh
