#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "engine/asset/AssetTypeName.hpp"
#include "engine/audio/AudioError.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace GYO::Engine::Audio {

// A clip's samples as the mixer reads them.
struct ClipView final {
    Base::ConstSpan<std::int16_t> samples; // interleaved, channels per frame
    std::uint16_t channels = 0;            // 1 or 2
    std::uint32_t frames = 0;
};

// An immutable PCM16 clip whose samples are shared: copying a Clip adds a
// reference, never copies the samples, so the asset store, the author and the
// mixer all hold the same samples. Read it from any thread and let it go on any
// thread but the audio thread (the mixer never lets go of one there; Design
// Invariant 7). Its type name for the catalog is "sound".
class Clip final {
public:
    // An empty clip; playing it is a Programmer Error.
    Clip() = default;

    // Copies the samples once. InvalidClip for an empty clip, channels other
    // than 1 or 2, a sample count that is not a multiple of channels, a
    // sampleRate other than kSampleRate or more than 2^32 - 1 frames.
    [[nodiscard]] static Base::Result<Clip, AudioError> Make(Base::ConstSpan<std::int16_t> interleaved,
                                                             std::uint16_t channels, std::uint32_t sampleRate);
    // The same rules; takes the samples without copying them.
    [[nodiscard]] static Base::Result<Clip, AudioError> Make(std::vector<std::int16_t>&& interleaved,
                                                             std::uint16_t channels, std::uint32_t sampleRate);

    [[nodiscard]] const ClipView& View() const noexcept { return view_; }
    [[nodiscard]] explicit operator bool() const noexcept { return samples_ != nullptr; }

private:
    Clip(std::shared_ptr<const std::vector<std::int16_t>> samples, std::uint16_t channels) noexcept;

    std::shared_ptr<const std::vector<std::int16_t>> samples_;
    ClipView view_;
};

} // namespace GYO::Engine::Audio

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::Audio::Clip> {
    static constexpr std::string_view value = "sound";
};
