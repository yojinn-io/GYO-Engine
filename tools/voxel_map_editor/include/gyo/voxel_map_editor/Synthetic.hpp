#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include <optional>
#include <string_view>

// Synthetic maps for the measurements, made in memory (a km^2 map's grid text
// can pass the 16 MiB a grid file may hold, so it does not go through a file).
// Each is the same on every platform: integer rules and std::minstd_rand.
namespace Gyo::Tools::VoxelMapEditor {

inline constexpr std::int32_t Km2Cells = 2000; // 1 km at 0.5 m a cell

// v8-arch-big-map-tool-chunked: minstd_rand(20261106), 256 x 48 x 256 cells (768 chunks). A one-cell floor every 8
// cells, a wall every 16 cells along x and z with random door holes, and per room random stairs and slabs.
[[nodiscard]] GridDocument BuildingGrid();
// v8-tool-voxel-editor-measure: 2000 x 2000 cells of ground at y = 0, and a wall one cell wide and two high every
// 64 cells along x and z.
[[nodiscard]] GridDocument Km2GroundGrid();
// v8-tool-voxel-editor-measure: BuildingGrid tiled over 2000 x 2000 cells (the same building in every 256 x 256 tile).
[[nodiscard]] GridDocument Km2BuildingGrid();
// building, km2-ground or km2-building.
[[nodiscard]] std::optional<GridDocument> SyntheticByName(std::string_view name);

} // namespace Gyo::Tools::VoxelMapEditor
