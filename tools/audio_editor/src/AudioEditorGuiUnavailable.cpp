#include "gyo/audio_editor/AudioEditorGui.hpp"

#include <iostream>

namespace Gyo::Tools::AudioEditor {

int RunAudioEditorGui(const std::string&) {
    std::cerr << "error: this gyo_audio_editor build has no window; reconfigure with GYO_TOOLS=audio_editor "
                 "(the gui variant). info and convert remain available.\n";
    return 2;
}

} // namespace Gyo::Tools::AudioEditor
