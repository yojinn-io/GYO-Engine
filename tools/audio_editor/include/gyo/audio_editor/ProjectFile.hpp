#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"
#include "gyo/audio_editor/Edit.hpp"
#include "gyo/audio_editor/SoundExport.hpp"

#include <string>
#include <string_view>

namespace Gyo::Tools::AudioEditor {

// The editor's own record of the clips cut from one source (Design
// Invariant 4): where the source is, where the clips go and how each is
// edited. Its format belongs to the editor (docs/audio_editor/architecture.md,
// "Project file"); the game never reads it.
struct AudioProject final {
    ExportBatch batch; // the source, the destination and the regions

    friend bool operator==(const AudioProject& left, const AudioProject& right);
};

inline constexpr std::string_view kProjectFormat = "gyo.audio_editor.project";
// The version the editor writes; it also reads version 1, a project of one region.
inline constexpr int kProjectVersion = 2;

// The file's text; the same project always gives the same text.
[[nodiscard]] std::string ProjectText(const AudioProject& project);
// InvalidProject for text that is not this format's version 1 or 2 with
// exactly its fields, or that has no region; the settings are not checked
// against a source here.
[[nodiscard]] Result<AudioProject, AudioEditorError> ParseProject(std::string_view text);

// Writes the project by atomic replace. InvalidProject when it has no region
// or when path is inside the project's asset root (Design Invariant 5: every file there is registered);
// WriteFailed when it cannot be written. path is UTF-8.
[[nodiscard]] Result<void, AudioEditorError> SaveProject(const std::string& path, const AudioProject& project);
// ReadFailed when the file cannot be read; InvalidProject as ParseProject.
[[nodiscard]] Result<AudioProject, AudioEditorError> LoadProject(const std::string& path);

} // namespace Gyo::Tools::AudioEditor
