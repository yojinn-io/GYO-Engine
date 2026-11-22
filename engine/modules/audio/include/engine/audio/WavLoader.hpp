#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/audio/Clip.hpp"

// The sound loader: type "sound", format "wav" (version 1), the Data Contract
// in docs/architecture/target/v2/audio.zh-Hant.md section 6. It reads RIFF/WAVE
// with integer PCM of 16 bits and hands the samples to Clip::Make without
// copying them again, so a clip the mixer cannot play (not 48000 Hz, not 1 or 2
// channels) is InvalidClip. No settings, no references. A product registers it
// with AssetLoaderRegistry::Register<WavLoader>().

namespace GYO::Engine::Audio {

// The rules are checked in this order and the first one broken is returned.
enum class WavLoaderCode : std::uint8_t {
    // Fewer than 12 bytes, or not "RIFF" ... "WAVE".
    NotWave = 1,
    // A chunk whose header or body runs past the end of the file.
    Truncated = 2,
    // More than one "fmt " or more than one "data" chunk.
    DuplicateChunk = 3,
    // No "fmt " or no "data" chunk.
    MissingChunk = 4,
    // A "fmt " chunk shorter than 16 bytes, or a block align other than
    // channels times 2.
    BadFormatChunk = 5,
    // A format tag other than 1 (integer PCM), or other than 16 bits per sample.
    NotPcm16 = 6,
    // A "data" chunk of an odd number of bytes.
    PartialSample = 7,
    // The samples break a rule of Clip::Make (channels, rate, empty, whole
    // frames, frame count); the detail is Clip::Make's message.
    InvalidClip = 8,
};

[[nodiscard]] constexpr const char* ToString(const WavLoaderCode code) noexcept {
    switch (code) {
    case WavLoaderCode::NotWave: return "NotWave";
    case WavLoaderCode::Truncated: return "Truncated";
    case WavLoaderCode::DuplicateChunk: return "DuplicateChunk";
    case WavLoaderCode::MissingChunk: return "MissingChunk";
    case WavLoaderCode::BadFormatChunk: return "BadFormatChunk";
    case WavLoaderCode::NotPcm16: return "NotPcm16";
    case WavLoaderCode::PartialSample: return "PartialSample";
    case WavLoaderCode::InvalidClip: return "InvalidClip";
    }
    return "Unknown";
}

struct WavSettings final {
    [[nodiscard]] friend bool operator==(const WavSettings&, const WavSettings&) = default;
};

class WavLoader final {
public:
    using Output = Clip;
    using Settings = WavSettings;
    using Code = WavLoaderCode;

    static constexpr std::string_view kTypeName = "sound";
    static constexpr std::array<std::string_view, 1> kFormats{"wav"};
    // About 5.8 minutes of 48 kHz stereo PCM16; a product may override it.
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;

    // Any settings object is accepted and ignored.
    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    [[nodiscard]] Base::Result<Clip, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                     const Settings& settings,
                                                                     Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<WavLoader>);

} // namespace GYO::Engine::Audio
