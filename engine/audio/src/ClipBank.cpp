#include "engine/audio/ClipBank.hpp"

#include <string>

#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace Engine::Audio {

Base::Result<ClipId, AudioError> ClipBank::Add(const Base::ConstSpan<std::int16_t> interleaved,
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
    clips_.push_back(Clip{std::vector<std::int16_t>(interleaved.begin(), interleaved.end()), channels});
    return ClipId{static_cast<std::uint32_t>(clips_.size())};
}

ClipView ClipBank::View(const ClipId id) const {
    GYO_ASSERT(Contains(id));
    const Clip& clip = clips_[id.value - 1];
    return ClipView{Base::ConstSpan<std::int16_t>(clip.samples.data(), clip.samples.size()), clip.channels,
                    static_cast<std::uint32_t>(clip.samples.size() / clip.channels)};
}

} // namespace Engine::Audio
