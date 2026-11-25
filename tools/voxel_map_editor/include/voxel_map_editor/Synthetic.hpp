#pragma once

#include "voxel_map_editor/Grid.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

// Synthetic maps for the measurements, made in memory (a km^2 map's grid text can pass the 16 MiB a grid file may
// hold, so it does not go through a file). Each is the same on every platform: integer rules and std::minstd_rand.
// The maps are master's (tools/voxel_map_editor/src/Synthetic.cpp), carried so their bakes can be compared.
namespace voxel_map_editor {

// 1 km at 0.5 m a cell: the default size of the km2 maps.
inline constexpr std::int32_t kKm2Cells = 2000;
// The size a km2 map may be given: whole chunks, up to 16 km at 0.5 m a cell.
inline constexpr std::int32_t kMaxSyntheticCells = 32768;

// minstd_rand(20261106), 256 x 48 x 256 cells (768 chunks). A one-cell floor every 8 cells, a wall every 16 cells
// along x and z with random door holes, and per room random stairs and slabs.
[[nodiscard]] GridDocument BuildingGrid();
// size x size cells of ground at y = 0, and a wall one cell wide and two high every 64 cells along x and z.
[[nodiscard]] GridDocument GroundGrid(std::int32_t size = kKm2Cells);
// BuildingGrid tiled over size x size cells (the same building in every 256 x 256 tile); size is whole chunks.
[[nodiscard]] GridDocument TiledBuildingGrid(std::int32_t size = kKm2Cells);
// building (its size is fixed), km2-ground or km2-building; nullopt for another name, or a size that is not a whole
// number of chunks from 16 to kMaxSyntheticCells.
[[nodiscard]] std::optional<GridDocument> SyntheticByName(std::string_view name, std::int32_t size = kKm2Cells);

} // namespace voxel_map_editor
