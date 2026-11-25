#pragma once

#include "voxel_map_editor/Grid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Goxel's own file (.gox, version 2) read into a gyo.voxel_grid. The reader is written from the format's description
// (a "GOX " magic, the version, then chunks of type, length, data and CRC; BL16 a 16^3 block stored as a 64 x 64 RGBA
// PNG with voxel i at pixel i, i = x + 16 y + 256 z; LAYR a layer: its blocks (index, x, y, z, 0) and a DICT), not
// from Goxel's code.
//
// - Layers. The geometry is the visible layers that hold their own blocks, merged in file order, a later layer's voxel
//   over an earlier one's (Goxel's view of the layers). A hidden layer, a layer that is a clone of another (base_id)
//   or a procedural shape (shape), and a layer whose name matches an excluded pattern are left out: scene markers
//   (spawns, monster stand-ins) are not geometry.
// - Voxels. Alpha below 127 is empty, as Goxel's exports have it; any other voxel is solid, its colour the RGB (the
//   alpha is not kept: a block is opaque).
// - Axes. Goxel's z is up and its space right-handed; gyo's y is up and its space left-handed
//   (docs/architecture/math.md). A Goxel voxel (x, y, z) is the gyo cell (x, z, y): swapping two axes is one mirror,
//   which turns the right-handed description into the left-handed one, so the shape is not mirrored. The grid's origin
//   is 0, so cells keep Goxel's coordinates.
// - Blocks. One block per colour, named c_<rrggbb>, in colour order, a cube unless the options give the colour another
//   shape of DefaultShapes (slab, stair; orientation 0). More than 256 colours is InvalidGox (a palette holds 256).
// The CRC of a chunk is not checked (Goxel's reader does not check it either); a chunk of another type is skipped.
namespace voxel_map_editor {

using Rgb = std::array<std::uint8_t, 3>;

struct GoxImportOptions final {
    // Patterns of layer names to leave out: '*' matches any run of characters, '?' one character, case-sensitive.
    std::vector<std::string> excludeLayers;
    // A shape name of DefaultShapes for a colour; the other colours are cubes.
    std::map<Rgb, std::string> blockShapes;
    float cellSize = 0.5F;
};

enum class GoxLayerUse : std::uint8_t {
    Geometry = 1,
    Hidden,
    Excluded,
    NoBlocks, // a clone (base_id) or a procedural shape: no blocks of its own
};

[[nodiscard]] constexpr const char* ToString(const GoxLayerUse use) noexcept {
    switch (use) {
    case GoxLayerUse::Geometry: return "geometry";
    case GoxLayerUse::Hidden: return "hidden";
    case GoxLayerUse::Excluded: return "excluded";
    case GoxLayerUse::NoBlocks: return "no blocks";
    }
    return "unknown";
}

struct GoxLayerReport final {
    std::string name;
    GoxLayerUse use{};
    std::size_t voxels{}; // solid voxels of the layer (before later layers cover any)
};

struct GoxImport final {
    GridDocument grid;
    std::vector<GoxLayerReport> layers; // in file order
    std::size_t cells{};                // filled cells of the grid
    std::vector<Rgb> unusedShapeColours; // colours the options give a shape that no geometry voxel has
};

// InvalidGox: not a .gox of version 2, a chunk that does not fit, a BL16 that is not a 64 x 64 PNG, a layer naming a
// block that is not there, a shape name DefaultShapes has not, a cell beyond the grid's range, no voxel at all, or more
// than 256 colours. The grid read back with ParseGrid(WriteGrid(grid)) (InvalidGox with its message otherwise).
[[nodiscard]] Result<GoxImport> ImportGox(std::span<const std::byte> bytes, const GoxImportOptions& options);

// Whether a layer name matches a pattern ('*' any run, '?' one character).
[[nodiscard]] bool MatchesPattern(std::string_view pattern, std::string_view name) noexcept;
// "RRGGBB" (hexadecimal, either case) as a colour.
[[nodiscard]] std::optional<Rgb> ParseRgb(std::string_view text) noexcept;
// The block name of a colour: c_ and its six lowercase hexadecimal digits.
[[nodiscard]] std::string ColourBlockName(const Rgb& colour);

} // namespace voxel_map_editor
