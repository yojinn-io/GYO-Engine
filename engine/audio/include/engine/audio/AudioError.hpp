#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace Engine::Audio {

enum class AudioErrorCode : std::uint16_t {
    // A clip that is empty, has a channel count other than 1 or 2, a sample
    // count that is not a whole number of frames, or a rate other than kSampleRate.
    InvalidClip = 1,
    // A mixer configuration outside its documented limits.
    InvalidConfig = 2,
    // Every submitter slot of the mixer is taken.
    TooManySubmitters = 3,
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

} // namespace Engine::Audio
