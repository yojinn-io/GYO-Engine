#pragma once

#include <string>
#include <vector>

namespace Gyo::Tools::VoxelMapEditor {

// measure-map: the measurement of v8-arch-big-map-tool-chunked. Writes the
// building-like synthetic grid and a per-chunk CSV into outputDirectory and
// prints a summary per map (the synthetic map, one checkerboard chunk, then
// each grid file given). Exit status as the other commands.
int MeasureMap(const std::string& outputDirectory, const std::vector<std::string>& gridPaths);

} // namespace Gyo::Tools::VoxelMapEditor
