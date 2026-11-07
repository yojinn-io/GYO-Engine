#include "window/EditorWindow.hpp"

#include <iostream>

namespace Gyo::Tools::VoxelMapEditor {

int RunEditorWindow(const WindowOptions&) {
    std::cerr << "error: this gyo_voxel_map_editor build has no window; build its gui variant. "
                 "build, verify, inspect and import-boxes remain available.\n";
    return 2;
}

int RunPreviewMeasure(const MeasureOptions&) {
    std::cerr << "error: this gyo_voxel_map_editor build has no preview; build its gui variant.\n";
    return 2;
}

} // namespace Gyo::Tools::VoxelMapEditor
