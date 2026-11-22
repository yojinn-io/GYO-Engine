#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace GYO::Engine::Audio {

enum class AudioErrorCode : std::uint16_t {
    // A clip that is empty, has a channel count other than 1 or 2, a sample
    // count that is not a whole number of frames, a rate other than kSampleRate
    // or more than 2^32 - 1 frames.
    InvalidClip = 1,
    // A mixer configuration outside its documented limits.
    InvalidConfig = 2,
    // Every submitter slot of the mixer is taken.
    TooManySubmitters = 3,
    // 4 was DeviceUnavailable, an output backend's error; it moved out with the
    // backend (v8-arch-gyo2-audio-review). 5 was NoClipSlot; clip slots went
    // with shared clips (v8-arch-gyo2-audio-clip-loader). Values are not
    // renumbered.
};

[[nodiscard]] constexpr const char* ToString(const AudioErrorCode code) noexcept {
    switch (code) {
    case AudioErrorCode::InvalidClip:   return "InvalidClip";
    case AudioErrorCode::InvalidConfig: return "InvalidConfig";
    case AudioErrorCode::TooManySubmitters: return "TooManySubmitters";
    }
    return "Unknown";
}

using AudioError = Base::Error<AudioErrorCode>;
static_assert(Base::CodedError<AudioError>);

} // namespace GYO::Engine::Audio
