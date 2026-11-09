#pragma once

#include <string>
#include <vector>

namespace Gyo::Tools::VoxelMapEditor {

// measure-map: the measurement of v8-arch-big-map-tool-chunked. Writes the
// building-like synthetic grid and a per-chunk CSV into outputDirectory and
// prints a summary per map (the synthetic map, one checkerboard chunk, then
// each grid file given). Exit status as the other commands.
int MeasureMap(const std::string& outputDirectory, const std::vector<std::string>& gridPaths);

// measure-map --scale: SM1 to SM5 and SM7 of v8-tool-voxel-editor-measure on a
// synthetic map made in memory (km2-ground or km2-building): the grid text's
// size, memory, the whole-document copy a drag starts with, a palette undo
// step, the whole bake and write, and the pick reach. Each timing 5 times.
// Writes scale-<map>.txt into outputDirectory and prints it.
int MeasureScale(const std::string& outputDirectory, const std::string& map);

} // namespace Gyo::Tools::VoxelMapEditor
