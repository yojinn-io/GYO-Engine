#include "gyo/ui_editor/EditorGui.hpp"

#include <iostream>

namespace Gyo::Tools::UiEditor {

int RunEditorGui(const CommandLineOptions&) {
    std::cerr
        << "error: this gyo_ui_editor build has no SDL3/ImGui GUI host; "
           "reconfigure with GYO_UI_EDITOR_BUILD_GUI=ON and provide the required "
           "targets. --validate remains available.\n";
    return 3;
}

} // namespace Gyo::Tools::UiEditor
