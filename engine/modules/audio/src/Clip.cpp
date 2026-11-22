#include "engine/audio/Clip.hpp"

#include <limits>
#include <string>
#include <utility>

#include "ClipValidation.hpp"

#include "engine/audio/AudioFormat.hpp"

namespace GYO::Engine::Audio {

Base::Result<void, AudioError> Detail::ValidateClip(const Base::ConstSpan<std::int16_t> interleaved,
                                                   const std::uint16_t channels, const std::uint32_t sampleRate) {
    if (channels != 1 && channels != 2) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidClip, "a clip has 1 or 2 channels",
                                          "channels=" + std::to_string(channels)));
    }
    if (sampleRate != kSampleRate) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidClip, "a clip is 48000 Hz; resample it beforehand",
                                          "sampleRate=" + std::to_string(sampleRate)));
    }
    if (interleaved.empty()) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidClip, "a clip has at least one frame"));
    }
    if (interleaved.size() % channels != 0) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidClip, "a clip's samples are whole frames",
                                          "samples=" + std::to_string(interleaved.size())));
    }
    // ClipView::frames and a voice's position are 32 bits.
    if (interleaved.size() / channels > std::numeric_limits<std::uint32_t>::max()) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidClip, "a clip has at most 2^32 - 1 frames",
                                          "samples=" + std::to_string(interleaved.size())));
    }
    return {};
}

Clip::Clip(std::shared_ptr<const std::vector<std::int16_t>> samples, const std::uint16_t channels) noexcept
    : samples_(std::move(samples)),
      view_{Base::ConstSpan<std::int16_t>(samples_->data(), samples_->size()), channels,
            static_cast<std::uint32_t>(samples_->size() / channels)} {}

Base::Result<Clip, AudioError> Clip::Make(const Base::ConstSpan<std::int16_t> interleaved, const std::uint16_t channels,
                                          const std::uint32_t sampleRate) {
    if (auto valid = Detail::ValidateClip(interleaved, channels, sampleRate); !valid) {
        return Base::Err(std::move(valid).error());
    }
    return Clip(std::make_shared<const std::vector<std::int16_t>>(interleaved.begin(), interleaved.end()), channels);
}

Base::Result<Clip, AudioError> Clip::Make(std::vector<std::int16_t>&& interleaved, const std::uint16_t channels,
                                          const std::uint32_t sampleRate) {
    if (auto valid = Detail::ValidateClip(Base::ConstSpan<std::int16_t>(interleaved.data(), interleaved.size()),
                                          channels, sampleRate);
        !valid) {
        return Base::Err(std::move(valid).error());
    }
    return Clip(std::make_shared<const std::vector<std::int16_t>>(std::move(interleaved)), channels);
}

} // namespace GYO::Engine::Audio
