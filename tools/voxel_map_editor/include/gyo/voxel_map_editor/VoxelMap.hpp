#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The voxel map editor's core (docs/voxel_map_editor/architecture.md): the
// tool's own grid format gyo.voxel_grid, the 24 orientations of a shape, and
// the build that bakes a grid into Engine's gyo.voxel_mesh. Units are 1/16 of
// a cell throughout, as in the baked file.
namespace Gyo::Tools::VoxelMapEditor {

enum class VoxelMapErrorCode : std::uint8_t {
    InvalidGrid = 1,  // a gyo.voxel_grid text breaks the format
    InvalidBoxes,     // boxes to import are not whole cells
    BuildRefused,     // the baked text did not pass Engine's ParseVoxelMesh
    ReadFailed,
    WriteFailed,
    FileChanged,      // a file changed on disk since the editor opened or saved it
    PreviewFailed,    // the window's preview could not start or draw
    EmptyMap,         // a grid without filled cells has no chunked map
    InvalidMarkers,   // Engine refuses a gyo.map_markers or gyo.marker_kinds text
    UnknownKinds,     // a marker's kind is not in the product's list
};

[[nodiscard]] constexpr const char* ToString(const VoxelMapErrorCode code) noexcept {
    switch (code) {
    case VoxelMapErrorCode::InvalidGrid: return "InvalidGrid";
    case VoxelMapErrorCode::InvalidBoxes: return "InvalidBoxes";
    case VoxelMapErrorCode::BuildRefused: return "BuildRefused";
    case VoxelMapErrorCode::ReadFailed: return "ReadFailed";
    case VoxelMapErrorCode::WriteFailed: return "WriteFailed";
    case VoxelMapErrorCode::FileChanged: return "FileChanged";
    case VoxelMapErrorCode::PreviewFailed: return "PreviewFailed";
    case VoxelMapErrorCode::EmptyMap: return "EmptyMap";
    case VoxelMapErrorCode::InvalidMarkers: return "InvalidMarkers";
    case VoxelMapErrorCode::UnknownKinds: return "UnknownKinds";
    }
    return "Unknown";
}

using VoxelMapError = Engine::Base::Error<VoxelMapErrorCode>;
static_assert(Engine::Base::CodedError<VoxelMapError>);

template <class T>
using Result = Engine::Base::Result<T, VoxelMapError>;

inline constexpr std::string_view GridSchema = "gyo.voxel_grid";
inline constexpr int GridVersion = 1;
inline constexpr std::int32_t ChunkSize = 16;
inline constexpr std::int32_t ChunkCells = ChunkSize * ChunkSize * ChunkSize;
inline constexpr std::int32_t Units = 16; // units per cell
inline constexpr std::size_t MaxShapes = 256;
inline constexpr std::size_t MaxBlocks = 256;
// A cell (after adding origin) lies in [-2^16, 2^16 - 1], so every corner of
// the baked file stays within Engine's +-2^20 units.
inline constexpr std::int32_t CellLimit = 1 << 16;
inline constexpr std::size_t MaxGridTextBytes = std::size_t{16} * 1024 * 1024;
inline constexpr std::string_view BuildVersion = "voxel_map_editor 1";

using Vec3i = std::array<std::int32_t, 3>;

// An axis-aligned box: minimum < maximum on every axis.
struct Box final {
    Vec3i minimum{};
    Vec3i maximum{};
    friend bool operator==(const Box&, const Box&) = default;
    friend auto operator<=>(const Box&, const Box&) = default;
};

struct Shape final {
    std::string name;
    std::vector<Box> boxes; // within [0, 16] on every axis
};

struct Block final {
    std::string name;
    std::uint32_t shape{};
    std::array<std::uint8_t, 4> color{}; // sRGB RGBA8
};

// block 0 is empty; block n is blocks[n - 1].
struct Cell final {
    std::uint16_t block{};
    std::uint8_t orientation{};
    friend bool operator==(const Cell&, const Cell&) = default;
};

// Cells of one chunk, x fastest, then z, then y.
using ChunkCellsArray = std::array<Cell, ChunkCells>;

[[nodiscard]] constexpr std::size_t CellIndex(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    return static_cast<std::size_t>(x + z * ChunkSize + y * ChunkSize * ChunkSize);
}

struct GridDocument final {
    Vec3i origin{};     // in cells; y is 0
    float cellSize{1.0F};
    std::vector<Shape> shapes;
    std::vector<Block> blocks;
    std::map<Vec3i, ChunkCellsArray> chunks; // by chunk coordinate; no chunk is all empty

    // The cell at a cell coordinate (relative to origin), empty outside chunks.
    [[nodiscard]] Cell At(const Vec3i& cell) const;
    // Sets a cell; an empty cell may drop its chunk.
    void Set(const Vec3i& cell, Cell value);
};

// The default shape library of a new grid: cube, slab (lower half) and stair.
[[nodiscard]] std::vector<Shape> DefaultShapes();

// Reads every rule of gyo.voxel_grid version 1; InvalidGrid with the path of
// the offending value otherwise. Nothing is ignored or repaired.
[[nodiscard]] Result<GridDocument> ParseGrid(std::string_view text);
// The one text of a document: ParseGrid(WriteGrid(d)) == d, and a text the
// writer produced reads back and writes again byte for byte.
[[nodiscard]] std::string WriteGrid(const GridDocument& document);

// The 24 rotations of a cell: up = (+Y, -Y, +X, -X, +Z, -Z)[o / 4], then
// (o % 4) quarter turns about that up axis. Row-major integer matrices.
inline constexpr std::size_t OrientationCount = 24;
using Matrix3i = std::array<std::array<std::int32_t, 3>, 3>;
[[nodiscard]] const Matrix3i& OrientationMatrix(std::size_t orientation);
// A box of a shape (cell-local units) turned about the cell's centre (8, 8, 8).
[[nodiscard]] Box RotateBox(const Box& box, std::size_t orientation);

// The direction brightness the build multiplies a block's colour by, in
// percent, in Engine's face order (+X, -X, +Y, -Y, +Z, -Z).
inline constexpr std::array<std::uint32_t, 6> FaceBrightnessPercent{80, 80, 100, 50, 65, 65};

struct BuildStats final {
    std::size_t cells{};
    std::size_t quads{};
    std::size_t colliders{};
};

struct BuildOutput final {
    std::string text; // a gyo.voxel_mesh that ParseVoxelMesh accepts
    BuildStats stats;
};

// Bakes a grid into gyo.voxel_mesh text. sourceBytes are the grid file's
// bytes, for source_sha256. BuildRefused when Engine's ParseVoxelMesh does
// not accept the result (nothing is returned to write).
[[nodiscard]] Result<BuildOutput> BuildMesh(const GridDocument& document, std::span<const std::byte> sourceBytes);

// The collision boxes of a set of boxes: disjoint, with the same union.
// Exposed for tests; BuildMesh uses it.
[[nodiscard]] std::vector<Box> MergeBoxes(std::span<const Box> boxes);

// A grid of cubes from world boxes in a JSON document: the array at a JSON
// pointer (for example "/walls"), each item {"min": [x, y, z], "max": [x, y, z]},
// every coordinate a whole number of cells (cell size 1). The caller says
// where the boxes are; the tool knows no product's format.
[[nodiscard]] Result<GridDocument> ImportBoxes(std::string_view text, std::string_view pointer,
                                               const std::array<std::uint8_t, 4>& color);

// Files through engine/io. ReadFailed / WriteFailed.
[[nodiscard]] Result<std::vector<std::byte>> ReadFile(const std::string& path, std::size_t maxBytes);
[[nodiscard]] Result<void> WriteFileReplacing(const std::string& path, std::string_view text);
// Whether a file is there; ReadFailed when that cannot be told.
[[nodiscard]] Result<bool> FileExists(const std::string& path);

} // namespace Gyo::Tools::VoxelMapEditor
