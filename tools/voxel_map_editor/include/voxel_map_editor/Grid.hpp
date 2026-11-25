#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The voxel map editor's grid: the tool's own format gyo.voxel_grid version 1, the 24 orientations of a shape, the
// collision box merge and the import of boxes. Units are 1/16 of a cell throughout, as in the baked map.
namespace voxel_map_editor {

enum class ErrorCode : std::uint8_t {
    InvalidGrid = 1, // a gyo.voxel_grid text breaks the format
    InvalidBoxes,    // boxes to import are not whole cells
    InvalidGox,      // a .gox file breaks its format, or its voxels cannot become a grid
    BuildRefused,    // the baked text did not pass the voxel_map Module's rules
    EmptyMap,        // a grid without filled cells has no chunked map
    ReadFailed,
    WriteFailed,
};

[[nodiscard]] constexpr const char* ToString(const ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidGrid: return "InvalidGrid";
    case ErrorCode::InvalidBoxes: return "InvalidBoxes";
    case ErrorCode::InvalidGox: return "InvalidGox";
    case ErrorCode::BuildRefused: return "BuildRefused";
    case ErrorCode::EmptyMap: return "EmptyMap";
    case ErrorCode::ReadFailed: return "ReadFailed";
    case ErrorCode::WriteFailed: return "WriteFailed";
    }
    return "Unknown";
}

using Error = GYO::Engine::Base::Error<ErrorCode>;
static_assert(GYO::Engine::Base::CodedError<Error>);

template <class T>
using Result = GYO::Engine::Base::Result<T, Error>;

inline constexpr std::string_view kGridSchema = "gyo.voxel_grid";
inline constexpr std::int64_t kGridVersion = 1;
inline constexpr std::int32_t kChunkSize = 16;
inline constexpr std::int32_t kChunkCells = kChunkSize * kChunkSize * kChunkSize;
inline constexpr std::int32_t kUnits = 16; // units per cell
inline constexpr std::size_t kMaxShapes = 256;
inline constexpr std::size_t kMaxBlocks = 256;
// A cell (after adding origin) lies in [-2^16, 2^16 - 1], so every corner of the baked map stays within +-2^20 units.
inline constexpr std::int32_t kCellLimit = 1 << 16;
inline constexpr std::size_t kMaxGridTextBytes = std::size_t{16} * 1024 * 1024;
inline constexpr float kMinCellSize = 1.0F / 65536.0F;
inline constexpr float kMaxCellSize = 65536.0F;

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
    friend bool operator==(const Shape&, const Shape&) = default;
};

struct Block final {
    std::string name;
    std::uint32_t shape{};
    std::array<std::uint8_t, 4> color{}; // sRGB RGBA8
    friend bool operator==(const Block&, const Block&) = default;
};

// block 0 is empty; block n is blocks[n - 1].
struct Cell final {
    std::uint16_t block{};
    std::uint8_t orientation{};
    friend bool operator==(const Cell&, const Cell&) = default;
};

// Cells of one chunk, x fastest, then z, then y.
using ChunkCellsArray = std::array<Cell, kChunkCells>;

[[nodiscard]] constexpr std::size_t CellIndex(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    return static_cast<std::size_t>(x + z * kChunkSize + y * kChunkSize * kChunkSize);
}

struct GridDocument final {
    Vec3i origin{}; // in cells; y is 0
    float cellSize{1.0F};
    std::vector<Shape> shapes;
    std::vector<Block> blocks;
    std::map<Vec3i, ChunkCellsArray> chunks; // by chunk coordinate; no chunk is all empty

    // The cell at a cell coordinate (relative to origin), empty outside chunks.
    [[nodiscard]] Cell At(const Vec3i& cell) const;
    // Sets a cell; an empty cell may drop its chunk.
    void Set(const Vec3i& cell, Cell value);

    friend bool operator==(const GridDocument&, const GridDocument&) = default;
};

// The default shape library of a new grid: cube, slab (lower half) and stair.
[[nodiscard]] std::vector<Shape> DefaultShapes();

// Reads every rule of gyo.voxel_grid version 1; InvalidGrid with the path of the offending value otherwise. Nothing
// is ignored or repaired.
[[nodiscard]] Result<GridDocument> ParseGrid(std::string_view text);
// The one text of a document: ParseGrid(WriteGrid(d)) == d, and a text the writer produced reads back and writes
// again byte for byte.
[[nodiscard]] std::string WriteGrid(const GridDocument& document);

// The 24 rotations of a cell: up = (+Y, -Y, +X, -X, +Z, -Z)[o / 4], then (o % 4) quarter turns about that up axis.
// Row-major integer matrices.
inline constexpr std::size_t kOrientationCount = 24;
using Matrix3i = std::array<std::array<std::int32_t, 3>, 3>;
[[nodiscard]] const Matrix3i& OrientationMatrix(std::size_t orientation);
// A box of a shape (cell-local units) turned about the cell's centre (8, 8, 8).
[[nodiscard]] Box RotateBox(const Box& box, std::size_t orientation);

// The collision boxes of a set of boxes: disjoint, with the same union, sorted.
[[nodiscard]] std::vector<Box> MergeBoxes(std::span<const Box> boxes);

// A grid of cubes from world boxes in a JSON document: the array at a JSON pointer (for example "/walls"), each item
// {"min": [x, y, z], "max": [x, y, z]}, every coordinate a whole number of cells (cell size 1). The caller says where
// the boxes are; the tool knows no product's format.
[[nodiscard]] Result<GridDocument> ImportBoxes(std::string_view text, std::string_view pointer,
                                               const std::array<std::uint8_t, 4>& color);

// Files through engine/io. ReadFailed / WriteFailed.
[[nodiscard]] Result<std::vector<std::byte>> ReadFile(const std::string& path, std::size_t maxBytes);
// Writes beside the target, then renames over it (creating the missing directories): a reader sees the old file or
// the new one.
[[nodiscard]] Result<void> WriteFileReplacing(const std::string& path, std::string_view text);
// Removes a file; a file already gone is fine.
[[nodiscard]] Result<void> RemoveFileIfPresent(const std::string& path);

[[nodiscard]] inline std::string_view AsText(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
[[nodiscard]] inline std::span<const std::byte> AsBytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

} // namespace voxel_map_editor
