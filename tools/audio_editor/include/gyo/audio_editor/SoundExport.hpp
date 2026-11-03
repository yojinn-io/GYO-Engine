#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace Gyo::Tools::AudioEditor {

// The sound's entry in the product's catalog; its type is always "sound".
struct SoundRegistration final {
    std::string id;   // the catalog id, for example "product.audio.reload"
    std::string path; // relative to the asset root, '/' separated
};

// UTF-8 paths, as GYO::Os takes them.
struct ExportRequest final {
    std::string sourcePath;  // the WAV to convert; only read (Design Invariant 4)
    std::string assetRoot;   // the mounted product's asset root
    std::string catalogPath; // that product's catalog
    SoundRegistration registration;
};

struct ExportReport final {
    std::string outputPath; // assetRoot joined with registration.path
    std::uint32_t sourceRate{};
    std::uint16_t sourceFormatTag{};
    std::uint16_t sourceBits{};
    std::uint16_t channels{};
    std::uint32_t outputRate{};
    std::size_t frames{};
    std::size_t saturated{};
    bool added{};
    std::optional<std::string> previousPath;
};

// Converts the source, writes the clip into the asset root and registers it in
// the catalog. Both files are written by atomic replace, the clip first; the
// catalog is not touched when the clip fails. The clip's path may not be the
// source itself, so a source is never overwritten.
[[nodiscard]] Result<ExportReport, AudioEditorError> ExportSound(const ExportRequest& request);

} // namespace Gyo::Tools::AudioEditor
