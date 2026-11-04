#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

// The editor's file access through engine/io, shared by export and the
// project file. Paths are UTF-8.
namespace Gyo::Tools::AudioEditor::Detail {

// Larger sources, catalogs or project files are refused rather than read
// whole: about four minutes of 48 kHz 24-bit stereo.
inline constexpr std::size_t kMaxReadBytes = 64U * 1024U * 1024U;

// ReadFailed when the file cannot be opened or read, or is larger than kMaxReadBytes.
[[nodiscard]] Result<std::vector<std::byte>, AudioEditorError> ReadAll(const std::string& path);
// engine/io writes beside the target, then renames over it: a reader sees the
// old file or the new one. Missing folders above the target are created.
// WriteFailed when it cannot.
[[nodiscard]] Result<void, AudioEditorError> WriteReplacing(const std::string& path, std::span<const std::byte> bytes);

} // namespace Gyo::Tools::AudioEditor::Detail
