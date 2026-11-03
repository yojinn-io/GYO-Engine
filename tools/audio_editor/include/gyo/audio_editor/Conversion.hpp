#pragma once

#include "gyo/audio_editor/WavFile.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Gyo::Tools::AudioEditor {

// Windowed-sinc resampling as convert_wav.py's resample() does it: a Blackman
// window, 32 zero crossings on each side, the cutoff at 0.95 of the lower
// Nyquist frequency, evaluated at each output sample's exact source position.
// The same rate returns the plane unchanged.
[[nodiscard]] std::vector<double> Resample(std::span<const double> plane, std::uint32_t sourceRate,
                                           std::uint32_t targetRate);

struct QuantizedSamples final {
    std::vector<std::int16_t> interleaved;
    std::size_t saturated{}; // samples clamped to [-32768, 32767]
};

// Each sample becomes value * 32767 plus a triangular dither in (-1, 1) from a
// 64-bit Mersenne Twister seeded with seed, rounded half up, then saturated.
// The generator and its conversion to [0, 1) are specified by the standard,
// so the sequence does not depend on the platform.
[[nodiscard]] QuantizedSamples Quantize(const std::vector<std::vector<double>>& planes, std::uint64_t seed);

// The dither seed of an asset: one output always gets the same noise.
[[nodiscard]] std::uint64_t DitherSeed(std::string_view assetId) noexcept;

struct ConvertedClip final {
    std::uint32_t sampleRate{};
    std::uint16_t channels{};
    std::vector<std::int16_t> samples; // interleaved PCM16
    std::size_t saturated{};

    [[nodiscard]] std::size_t Frames() const noexcept { return channels == 0 ? 0 : samples.size() / channels; }
};

// The source as a clip in the format the engine publishes (Design Invariant 2):
// resampled to Engine::Audio::kSampleRate and quantized to PCM16.
[[nodiscard]] ConvertedClip ConvertToClip(const DecodedWav& source, std::uint64_t seed);

} // namespace Gyo::Tools::AudioEditor
