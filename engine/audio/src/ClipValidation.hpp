#pragma once

#include <cstdint>

#include "engine/audio/AudioError.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace Engine::Audio::Detail {

// The clip rules shared by ClipBank::Add and Submitter::LoadClip: InvalidClip
// for an empty clip, channels other than 1 or 2, a sample count that is not a
// multiple of channels, or sampleRate != kSampleRate.
[[nodiscard]] Base::Result<void, AudioError> ValidateClip(Base::ConstSpan<std::int16_t> interleaved,
                                                          std::uint16_t channels, std::uint32_t sampleRate);

} // namespace Engine::Audio::Detail
