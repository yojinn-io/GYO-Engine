#pragma once

#include <cstdint>
#include <vector>

#include "engine/audio/AudioError.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace Engine::Audio {

// Names a clip of one ClipBank. Zero is never a valid clip.
struct ClipId final {
    std::uint32_t value = 0;
    friend constexpr bool operator==(ClipId, ClipId) = default;
};

// A clip's samples as the mixer reads them.
struct ClipView final {
    Base::ConstSpan<std::int16_t> samples; // interleaved, channels per frame
    std::uint16_t channels = 0;            // 1 or 2
    std::uint32_t frames = 0;
};

// Immutable PCM16 clips, registered before output starts and owned by the mixer
// afterwards. Add copies the samples, so the caller's buffer (for example a
// loaded sound asset) need not outlive the bank.
class ClipBank final {
public:
    // Fails with InvalidClip for an empty clip, channels other than 1 or 2, a
    // sample count that is not a multiple of channels, or sampleRate != kSampleRate.
    [[nodiscard]] Base::Result<ClipId, AudioError> Add(Base::ConstSpan<std::int16_t> interleaved,
                                                       std::uint16_t channels, std::uint32_t sampleRate);

    [[nodiscard]] std::uint32_t Count() const noexcept { return static_cast<std::uint32_t>(clips_.size()); }
    [[nodiscard]] bool Contains(ClipId id) const noexcept { return id.value != 0 && id.value <= clips_.size(); }
    // A clip that is not in this bank is a Programmer Error.
    [[nodiscard]] ClipView View(ClipId id) const;

private:
    struct Clip final {
        std::vector<std::int16_t> samples;
        std::uint16_t channels = 0;
    };
    std::vector<Clip> clips_;
};

} // namespace Engine::Audio
