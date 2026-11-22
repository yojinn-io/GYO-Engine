#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace GYO::Engine::Audio {

enum class AudioErrorCode : std::uint16_t {
    // A clip that is empty, has a channel count other than 1 or 2, a sample
    // count that is not a whole number of frames, or a rate other than kSampleRate.
    InvalidClip = 1,
    // A mixer configuration outside its documented limits.
    InvalidConfig = 2,
    // Every submitter slot of the mixer is taken.
    TooManySubmitters = 3,
    // 4 was DeviceUnavailable, an output backend's error; it moved out with the
    // backend (v8-arch-gyo2-audio-review). Values are not renumbered.
    // Every clip slot of the submitter is loaded or not yet handed back.
    NoClipSlot = 5,
};

[[nodiscard]] constexpr const char* ToString(const AudioErrorCode code) noexcept {
    switch (code) {
    case AudioErrorCode::InvalidClip:   return "InvalidClip";
    case AudioErrorCode::InvalidConfig: return "InvalidConfig";
    case AudioErrorCode::TooManySubmitters: return "TooManySubmitters";
    case AudioErrorCode::NoClipSlot: return "NoClipSlot";
    }
    return "Unknown";
}

using AudioError = Base::Error<AudioErrorCode>;
static_assert(Base::CodedError<AudioError>);

} // namespace GYO::Engine::Audio
