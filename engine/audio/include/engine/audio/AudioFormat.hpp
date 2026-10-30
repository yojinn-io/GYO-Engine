#pragma once

#include <cstdint>

namespace Engine::Audio {

// The one output format of the mixer: interleaved stereo PCM16 at 48 kHz.
// Converting to the device's format belongs to the output backend.
inline constexpr std::uint32_t kSampleRate = 48000; // frames per second
inline constexpr std::uint16_t kChannels = 2;

// Largest gain a voice or the master may have; gains are in [0, kMaxGain].
inline constexpr float kMaxGain = 8.0f;

} // namespace Engine::Audio
