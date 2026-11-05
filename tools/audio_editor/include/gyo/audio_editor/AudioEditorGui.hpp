#pragma once

#include <string>

namespace Gyo::Tools::AudioEditor {

// The editor's window: playback, waveform, trim, gain, the project file and
// export. file is a project (".json") or a source WAV to open, or empty;
// UTF-8. Returns the process exit code.
[[nodiscard]] int RunAudioEditorGui(const std::string& file);

} // namespace Gyo::Tools::AudioEditor
