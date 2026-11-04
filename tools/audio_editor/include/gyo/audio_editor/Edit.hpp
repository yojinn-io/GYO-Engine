#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"
#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Gyo::Tools::AudioEditor {

// The gain range the editor accepts, in dB.
inline constexpr double kMinGainDb = -60.0;
inline constexpr double kMaxGainDb = 24.0;

// What the editor changes in a source (Design Invariant 4: the source itself
// is only read). Frames are the source's own, before resampling.
struct EditSettings final {
    std::size_t trimStart{};            // the first source frame kept
    std::optional<std::size_t> trimEnd; // one past the last frame kept; none: the source's end
    double gainDb{};                    // applied to every sample before resampling

    friend bool operator==(const EditSettings&, const EditSettings&) = default;
};

// InvalidSettings unless trimStart < end <= frames (end: trimEnd or frames)
// and gainDb is within [kMinGainDb, kMaxGainDb].
[[nodiscard]] Result<void, AudioEditorError> ValidateSettings(const EditSettings& settings, std::size_t frames);

// The trimmed range scaled by 10^(gainDb / 20), then resampled to
// Engine::Audio::kSampleRate: the clip before quantizing. InvalidSettings as
// ValidateSettings, and when the trim resamples to no frame.
[[nodiscard]] Result<std::vector<std::vector<double>>, AudioEditorError> RenderPlanes(const DecodedWav& source,
                                                                                     const EditSettings& settings);

// The one function preview and export both use (Design Invariant 6): trim,
// gain, resample, quantize. Default settings give ConvertToClip's bytes.
[[nodiscard]] Result<ConvertedClip, AudioEditorError> RenderClip(const DecodedWav& source,
                                                                 const EditSettings& settings, std::uint64_t seed);

} // namespace Gyo::Tools::AudioEditor
