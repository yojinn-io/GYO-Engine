#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"
#include "gyo/audio_editor/Edit.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    EditSettings settings; // the trim and gain of the output; the default converts the whole source
};

// One clip cut from a source: its entry and its settings.
struct SoundRegion final {
    SoundRegistration registration;
    EditSettings settings;

    friend bool operator==(const SoundRegion& left, const SoundRegion& right) {
        return left.registration.id == right.registration.id && left.registration.path == right.registration.path &&
               left.settings == right.settings;
    }
};

// Several clips cut from one source into one asset root and catalog.
struct ExportBatch final {
    std::string sourcePath;  // the WAV to convert; only read (Design Invariant 4)
    std::string assetRoot;   // the mounted product's asset root
    std::string catalogPath; // that product's catalog
    std::vector<SoundRegion> regions;
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

// Renders every region of the source (RenderClip, as the preview does),
// writes each clip into the asset root and registers each in the catalog, one
// report per region in their order. Everything is checked and rendered before
// anything is written: no region, two regions with one id or one path, a clip
// path that is the source or the catalog, a region the catalog or its settings
// refuse, and nothing is written. Two paths are one file when the OS layer
// compares their names equal (Files::SamePathComponent: ignoring case on
// Windows, byte for byte elsewhere). Then the clips are written by atomic
// replace in order and the catalog once, last; when a write fails the catalog
// is not touched, and the clips already written stay, unregistered, named in
// the error's detail. A previous path that another region of the export
// registers is not reported.
[[nodiscard]] Result<std::vector<ExportReport>, AudioEditorError> ExportSounds(const ExportBatch& batch);

// ExportSounds with the request's one region.
[[nodiscard]] Result<ExportReport, AudioEditorError> ExportSound(const ExportRequest& request);

} // namespace Gyo::Tools::AudioEditor
