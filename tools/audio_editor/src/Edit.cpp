#include "gyo/audio_editor/Edit.hpp"

#include "engine/audio/AudioFormat.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace Gyo::Tools::AudioEditor {

Result<void, AudioEditorError> ValidateSettings(const EditSettings& settings, const std::size_t frames) {
    const std::size_t end = settings.trimEnd.value_or(frames);
    if (end > frames || settings.trimStart >= end) {
        return Engine::Base::Err(AudioEditorError::Make(
            AudioEditorErrorCode::InvalidSettings, "the trim keeps no frame or ends after the source",
            std::to_string(settings.trimStart) + ".." + std::to_string(end) + " of " + std::to_string(frames)));
    }
    if (!std::isfinite(settings.gainDb) || settings.gainDb < kMinGainDb || settings.gainDb > kMaxGainDb) {
        return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidSettings,
                                                        "the gain is outside the editor's range",
                                                        std::to_string(settings.gainDb) + " dB"));
    }
    return {};
}

Result<std::vector<std::vector<double>>, AudioEditorError> RenderPlanes(const DecodedWav& source,
                                                                       const EditSettings& settings) {
    if (auto valid = ValidateSettings(settings, source.Frames()); !valid) {
        return Engine::Base::Err(std::move(valid).error());
    }
    const std::size_t end = settings.trimEnd.value_or(source.Frames());
    // 0 dB is exactly 1, so the default settings leave every sample as it is.
    const double factor = std::pow(10.0, settings.gainDb / 20.0);
    std::vector<std::vector<double>> planes;
    planes.reserve(source.planes.size());
    for (const std::vector<double>& plane : source.planes) {
        std::vector<double> edited(plane.begin() + static_cast<std::ptrdiff_t>(settings.trimStart),
                                   plane.begin() + static_cast<std::ptrdiff_t>(end));
        for (double& sample : edited) sample *= factor;
        planes.push_back(Resample(edited, source.sampleRate, Engine::Audio::kSampleRate));
    }
    // A trim shorter than one output frame (a source above kSampleRate)
    // resamples to nothing, which ClipBank refuses.
    if (!planes.empty() && planes.front().empty()) {
        return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidSettings,
                                                        "the trim is shorter than one frame of the clip",
                                                        std::to_string(end - settings.trimStart) + " source frames"));
    }
    return planes;
}

Result<ConvertedClip, AudioEditorError> RenderClip(const DecodedWav& source, const EditSettings& settings,
                                                   const std::uint64_t seed) {
    auto planes = RenderPlanes(source, settings);
    if (!planes) return Engine::Base::Err(std::move(planes).error());
    QuantizedSamples quantized = Quantize(*planes, seed);
    ConvertedClip clip;
    clip.sampleRate = Engine::Audio::kSampleRate;
    clip.channels = static_cast<std::uint16_t>(source.planes.size());
    clip.samples = std::move(quantized.interleaved);
    clip.saturated = quantized.saturated;
    return clip;
}

} // namespace Gyo::Tools::AudioEditor
