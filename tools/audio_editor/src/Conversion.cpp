#include "gyo/audio_editor/Conversion.hpp"

#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Fnv1a.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace Gyo::Tools::AudioEditor {
namespace {

// convert_wav.py's filter: these three numbers are its specification.
constexpr double kZeroCrossings = 32.0;
constexpr double kCutoff = 0.95;

// [0, 1) from the top 53 bits; std::uniform_real_distribution is not
// specified exactly, the generator's raw output is.
double Unit(std::mt19937_64& generator) {
    return static_cast<double>(generator() >> 11) * 0x1.0p-53;
}

} // namespace

std::vector<double> Resample(const std::span<const double> plane, const std::uint32_t sourceRate,
                             const std::uint32_t targetRate) {
    if (sourceRate == targetRate) {
        return {plane.begin(), plane.end()};
    }
    const double ratio = static_cast<double>(targetRate) / static_cast<double>(sourceRate);
    // Cut off below the lower of the two Nyquist frequencies, in source samples.
    const double cutoff = kCutoff * std::min(1.0, ratio);
    const double half = kZeroCrossings / cutoff;
    const auto outputs = static_cast<std::size_t>(std::floor(static_cast<double>(plane.size()) * ratio));
    const auto lastIndex = static_cast<double>(plane.size()) - 1.0;
    std::vector<double> out;
    out.reserve(outputs);
    for (std::size_t j = 0; j < outputs; ++j) {
        const double x = static_cast<double>(j) / ratio; // this output sample's position in source samples
        const double first = std::max(0.0, std::ceil(x - half));
        const double last = std::min(lastIndex, std::floor(x + half));
        double acc = 0.0;
        for (double i = first; i <= last; i += 1.0) {
            const double t = i - x;
            const double arg = cutoff * t;
            const double sinc = arg == 0.0 ? 1.0 : std::sin(std::numbers::pi * arg) / (std::numbers::pi * arg);
            const double w = 0.42 + 0.5 * std::cos(std::numbers::pi * t / half) +
                             0.08 * std::cos(2 * std::numbers::pi * t / half);
            acc += plane[static_cast<std::size_t>(i)] * cutoff * sinc * w;
        }
        out.push_back(acc);
    }
    return out;
}

QuantizedSamples Quantize(const std::vector<std::vector<double>>& planes, const std::uint64_t seed) {
    QuantizedSamples result;
    if (planes.empty()) {
        return result;
    }
    const std::size_t frames = planes.front().size();
    result.interleaved.reserve(frames * planes.size());
    std::mt19937_64 generator(seed);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (const std::vector<double>& plane : planes) {
            const double first = Unit(generator);
            const double dithered = plane[frame] * 32767.0 + (first - Unit(generator));
            double sample = std::floor(dithered + 0.5);
            if (sample > 32767.0 || sample < -32768.0) {
                ++result.saturated;
                sample = std::clamp(sample, -32768.0, 32767.0);
            }
            result.interleaved.push_back(static_cast<std::int16_t>(sample));
        }
    }
    return result;
}

std::uint64_t DitherSeed(const std::string_view assetId) noexcept {
    return Engine::Base::Fnv1a64(assetId);
}

ConvertedClip ConvertToClip(const DecodedWav& source, const std::uint64_t seed) {
    std::vector<std::vector<double>> resampled;
    resampled.reserve(source.planes.size());
    for (const std::vector<double>& plane : source.planes) {
        resampled.push_back(Resample(plane, source.sampleRate, Engine::Audio::kSampleRate));
    }
    QuantizedSamples quantized = Quantize(resampled, seed);
    ConvertedClip clip;
    clip.sampleRate = Engine::Audio::kSampleRate;
    clip.channels = static_cast<std::uint16_t>(source.planes.size());
    clip.samples = std::move(quantized.interleaved);
    clip.saturated = quantized.saturated;
    return clip;
}

} // namespace Gyo::Tools::AudioEditor
