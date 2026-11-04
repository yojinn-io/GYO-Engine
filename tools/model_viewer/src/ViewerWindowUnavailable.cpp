#include "ViewerWindow.hpp"

#include <iostream>

namespace Gyo::Tools::ModelViewer {

int RunViewerWindow(const WindowOptions&) {
    std::cerr << "error: this gyo_model_viewer build has no window; build the gui variant "
                 "(GYO_TOOLS=model_viewer with its gui variant). The other commands remain available.\n";
    return 3;
}

int RunPreviewMeasure(const MeasureOptions&) {
    std::cerr << "error: this gyo_model_viewer build has no preview; build the gui variant.\n";
    return 3;
}

} // namespace Gyo::Tools::ModelViewer
