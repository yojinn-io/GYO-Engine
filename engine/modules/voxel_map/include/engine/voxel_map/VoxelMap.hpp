#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/VecInt.hpp"

// A map built on a grid, baked by a tool and split into chunks (docs/architecture/target/v2/voxel-map.zh-Hant.md):
// gyo.voxel_map version 1 (the index) and gyo.voxel_region version 1 (the files of 8 x 8 chunk columns). A tool
// decides culling, merging and colours and writes them; this Module reads, validates and expands what it wrote. It
// knows faces, colours, boxes, chunks, regions and opaque names, nothing a game means by them, and it opens no file:
// a region's `file` is a name the reader resolves.
//
// A chunk is the unit of validation and expansion, a region the unit of files. Every iteration order follows the
// total order of keys: chunk keys (x, y, z) and region keys (x, z), lexicographic.

namespace GYO::Engine::VoxelMap {

inline constexpr std::string_view kMapSchema = "gyo.voxel_map";
inline constexpr std::string_view kRegionSchema = "gyo.voxel_region";
inline constexpr std::int64_t kMapVersion = 1;
inline constexpr std::int64_t kRegionVersion = 1;

// Coordinates are integers in 1/16 of a cell.
inline constexpr std::int64_t kUnitsPerCell = 16;
// Cells along each edge of a chunk, and chunk columns along each horizontal edge of a region (fixed in version 1;
// written in the index).
inline constexpr std::int64_t kChunkCells = 16;
inline constexpr std::int64_t kRegionChunks = 8;
// A chunk's edge in units: chunk-local coordinates lie in [0, kChunkUnits].
inline constexpr std::int64_t kChunkUnits = kChunkCells * kUnitsPerCell;
// Every integer lies within +-2^30, so a sum of two cannot overflow 32 bits.
inline constexpr std::int64_t kIntegerLimit = std::int64_t{1} << 30;
// Every world corner, in units from the world origin, lies within +-2^20, where converting it to float is exact.
inline constexpr std::int64_t kCoordinateLimit = std::int64_t{1} << 20;
inline constexpr std::size_t kMaxMapTextBytes = std::size_t{1} * 1024 * 1024;
inline constexpr std::size_t kMaxRegionTextBytes = std::size_t{16} * 1024 * 1024;
inline constexpr std::size_t kMaxPaletteEntries = 256;
inline constexpr std::size_t kMaxNameLength = 64;
inline constexpr std::size_t kMaxBuildVersionLength = 64;
inline constexpr std::size_t kMaxRegionFileLength = 128;

// Failure classes of the Module. Zero is not a valid code; the numeric values are not a data contract.
enum class VoxelMapErrorCode : std::uint8_t {
    // A gyo.voxel_map text breaks its contract.
    InvalidVoxelMap = 1,
    // A gyo.voxel_region text breaks its contract, or the region does not belong to the map it is checked with.
    InvalidVoxelRegion,
    // A chunk checked with ValidateVoxelChunk breaks its contract.
    InvalidVoxelChunk,
};

[[nodiscard]] constexpr const char* ToString(const VoxelMapErrorCode code) noexcept {
    switch (code) {
    case VoxelMapErrorCode::InvalidVoxelMap: return "InvalidVoxelMap";
    case VoxelMapErrorCode::InvalidVoxelRegion: return "InvalidVoxelRegion";
    case VoxelMapErrorCode::InvalidVoxelChunk: return "InvalidVoxelChunk";
    }
    return "Unknown";
}

// message names the rule (M1 to M10, R1 to R5, C1 to C5) and what broke it; detail is the path of the offending
// value (for example "chunks[2].quads[3][4]"), empty for the whole text.
using VoxelMapError = Base::Error<VoxelMapErrorCode>;
static_assert(Base::CodedError<VoxelMapError>);

template <class T>
using VoxelMapResult = Base::Result<T, VoxelMapError>;

// The direction a face looks at; also its row in the palette image.
enum class VoxelFace : std::uint8_t {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};
inline constexpr std::size_t kFaceCount = 6;

// sRGB RGBA8.
using VoxelColor = std::array<std::uint8_t, 4>;

struct VoxelPaletteEntry final {
    // Opaque: what it means is the tool's.
    std::string name;
    // One colour per face direction, in VoxelFace order.
    std::array<VoxelColor, kFaceCount> faces{};

    friend bool operator==(const VoxelPaletteEntry&, const VoxelPaletteEntry&) = default;
};

// One axis-aligned rectangle: its minimum corner (x, y, z) on the face's plane, the direction it looks at, its two
// in-plane sizes (X faces run along Z and Y, Y faces along X and Z, Z faces along X and Y) and its palette entry.
struct VoxelQuad final {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    VoxelFace face = VoxelFace::PositiveX;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t palette = 0;

    friend bool operator==(const VoxelQuad&, const VoxelQuad&) = default;
};

// A collision box; minimum < maximum on every axis.
struct VoxelBox final {
    Math::Vec3i minimum{};
    Math::Vec3i maximum{};

    friend bool operator==(const VoxelBox&, const VoxelBox&) = default;
};

// A chunk's coordinates in chunks.
struct VoxelChunkKey final {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend auto operator<=>(const VoxelChunkKey&, const VoxelChunkKey&) = default;
};

// A region's coordinates in regions (horizontal only).
struct VoxelRegionKey final {
    std::int32_t x = 0;
    std::int32_t z = 0;

    friend auto operator<=>(const VoxelRegionKey&, const VoxelRegionKey&) = default;
};

struct VoxelMapRegion final {
    VoxelRegionKey at{};
    // Where the reader finds the region's text, relative to the index's directory; opaque here.
    std::string file;
    // SHA-256 of the region's text, 64 lowercase hexadecimal digits.
    std::string sha256;

    friend bool operator==(const VoxelMapRegion&, const VoxelMapRegion&) = default;
};

// A validated index.
struct VoxelMapDocument final {
    std::string sourceSha256;
    std::string buildVersion;
    // In cells.
    Math::Vec3i origin{};
    // The world length of one cell.
    float cellSize = 0.0F;
    // In chunks, both ends included.
    VoxelChunkKey boundsMinimum{};
    VoxelChunkKey boundsMaximum{};
    std::vector<VoxelPaletteEntry> palette;
    // Nonempty, in key order.
    std::vector<VoxelMapRegion> regions;

    friend bool operator==(const VoxelMapDocument&, const VoxelMapDocument&) = default;
};

// One chunk: quads and boxes in units from the chunk's minimum corner (0 to kChunkUnits on every axis).
struct VoxelChunkDocument final {
    VoxelChunkKey at{};
    // Each in strictly increasing order: quads by their seven integers, boxes by (minimum, maximum).
    std::vector<VoxelQuad> quads;
    std::vector<VoxelBox> colliders;

    friend bool operator==(const VoxelChunkDocument&, const VoxelChunkDocument&) = default;
};

// A region read from its text: its chunks in key order (empty chunks left out) and the text's SHA-256, which
// CheckVoxelRegion compares with the index.
struct VoxelRegionDocument final {
    VoxelRegionKey at{};
    std::vector<VoxelChunkDocument> chunks;
    std::string textSha256;

    friend bool operator==(const VoxelRegionDocument&, const VoxelRegionDocument&) = default;
};

// Every rule of gyo.voxel_map version 1 (M1 to M10); InvalidVoxelMap otherwise.
[[nodiscard]] VoxelMapResult<VoxelMapDocument> ParseVoxelMap(std::string_view text);

// The rules of gyo.voxel_region version 1 that need no index: R1, R2, R4 (fields, chunks in key order and inside the
// region `at` names), R5, and for each chunk C1 (not empty), C2 (except the palette range), C3, C4 and C5;
// InvalidVoxelRegion otherwise. The rest needs the index: CheckVoxelRegion. A loader has the bytes only, so a region
// is read in two steps; both run the same chunk rules as ValidateVoxelChunk.
[[nodiscard]] VoxelMapResult<VoxelRegionDocument> ParseVoxelRegion(std::string_view text);

// The rules of a region that need the index: R3 (its `at` is expected and its text has that entry's sha256), and
// every chunk inside the map's bounds (R4, C1) with palette indices into the map's palette (C2); InvalidVoxelRegion
// otherwise. expected is the key of the index entry whose file the reader read.
[[nodiscard]] VoxelMapResult<void> CheckVoxelRegion(const VoxelRegionDocument& region, const VoxelMapDocument& map,
                                                    VoxelRegionKey expected);

// The whole map, the one answer to "is this map valid": CheckVoxelRegion for every index entry, in index order
// (regions[i] read for map.regions[i]; both are the caller's to line up, asserted by size). An asset reader calls it
// once every region is Loaded (a Loaded region has passed the first step only); the paths start with "regions[i]".
[[nodiscard]] VoxelMapResult<void> CheckVoxelMap(const VoxelMapDocument& map,
                                                 std::span<const VoxelRegionDocument* const> regions);

// Both steps on one text: ParseVoxelRegion, then CheckVoxelRegion.
[[nodiscard]] VoxelMapResult<VoxelRegionDocument> ParseVoxelRegion(std::string_view text, const VoxelMapDocument& map,
                                                                   VoxelRegionKey expected);

// The chunk rules (C1 to C5, inside the map's bounds, palette indices into its palette) on a chunk that never was
// text, such as a tool's working copy; InvalidVoxelChunk with paths relative to the chunk ("quads[3][4]").
[[nodiscard]] VoxelMapResult<void> ValidateVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map);

// World-space geometry, laid out as a model mesh's: four vertices and six indices per quad, in quad order; each
// triangle's corners go counter-clockwise on the screen seen from the side its face looks at (render's front face; in
// the left-handed space (v1 - v0) x (v2 - v0) points against the normal); every vertex has its face's normal
// and the uv of the centre of its colour's texel in ExpandVoxelPalette's image. colliders in chunk then box order.
struct VoxelGeometry final {
    std::vector<Math::Vec3> positions;
    std::vector<Math::Vec3> normals;
    std::vector<Math::Vec2> uvs;
    std::vector<std::uint32_t> indices;
    std::vector<Math::Aabb> colliders;
};

// Appends one chunk to geometry (indices continue from its vertices). The chunk must be one the map's rules accepted
// (CheckVoxelMap or CheckVoxelRegion after ParseVoxelRegion, or ValidateVoxelChunk) with the same map. What the
// expansion relies on is asserted (the chunk in the bounds, each quad's direction and palette index); the rest of a
// chunk the rules refuse gives geometry the contract does not describe. A world coordinate is
// float(local + 256 x key + 16 x origin) x cellSize / 16.
void AppendVoxelChunk(VoxelGeometry& geometry, const VoxelChunkDocument& chunk, const VoxelMapDocument& map);
// One chunk on its own.
[[nodiscard]] VoxelGeometry ExpandVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map);

// The palette as RGBA8 pixels, top row first: one column per entry, one row per VoxelFace.
struct VoxelPaletteImage final {
    std::uint32_t width = 0;
    std::uint32_t height = static_cast<std::uint32_t>(kFaceCount);
    std::vector<std::uint8_t> pixels;
};
[[nodiscard]] VoxelPaletteImage ExpandVoxelPalette(const VoxelMapDocument& map);

// The region a chunk belongs to: floor(x / 8), floor(z / 8).
[[nodiscard]] VoxelRegionKey RegionOf(VoxelChunkKey key) noexcept;
// The world position of a chunk's minimum corner; key must lie in the map's bounds (asserted).
[[nodiscard]] Math::Vec3 ChunkWorldOrigin(VoxelChunkKey key, const VoxelMapDocument& map);
// Integer units from the map's origin in world coordinates: float(units + 16 x origin) x cellSize / 16 per axis,
// exact for units within +-2^20 (rule M4 keeps 16 x origin within +-2^20).
[[nodiscard]] Math::Vec3 MapUnitsToWorld(Math::Vec3i units, const VoxelMapDocument& map);
// The world box of a chunk (its whole 16^3 cells), for culling and bounds; key must lie in the map's bounds.
[[nodiscard]] Math::Aabb ChunkWorldBounds(VoxelChunkKey key, const VoxelMapDocument& map);

} // namespace GYO::Engine::VoxelMap
