#pragma once

#include "gyo/audio_editor/AudioEditorError.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Gyo::Tools::AudioEditor {

// A source WAV as samples in [-1, 1): one plane per channel.
struct DecodedWav final {
    std::uint32_t sampleRate{};
    std::uint16_t formatTag{}; // 1 integer PCM, 3 IEEE float (WAVE_FORMAT_EXTENSIBLE resolved to its subformat)
    std::uint16_t bits{};
    std::vector<std::vector<double>> planes;

    [[nodiscard]] std::size_t Frames() const noexcept { return planes.empty() ? 0 : planes.front().size(); }
};

// Reads RIFF/WAVE: integer PCM of 8, 16, 24 or 32 bits or 32-bit float, 1 or
// 2 channels, any sample rate. Samples are scaled as convert_wav.py scales
// them (8 bits: (b - 128) / 128; n bits: s / 2^(n-1)); a trailing partial
// frame is dropped.
[[nodiscard]] Result<DecodedWav, AudioEditorError> DecodeWav(std::span<const std::byte> bytes);

// A canonical 44-byte-header PCM16 WAV of interleaved samples.
[[nodiscard]] std::vector<std::byte> EncodePcm16Wav(std::uint16_t channels, std::uint32_t sampleRate,
                                                    std::span<const std::int16_t> interleaved);

} // namespace Gyo::Tools::AudioEditor
