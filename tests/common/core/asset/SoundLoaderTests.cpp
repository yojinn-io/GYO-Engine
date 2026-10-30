#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/asset/loaders/SoundLoader.hpp"

using Engine::Asset::AssetErrorCode;
using Engine::Asset::Loaders::SoundAsset;
using Engine::Asset::Loaders::SoundLoader;

namespace {

void PutU16(std::vector<std::byte>& out, const std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFF));
    out.push_back(static_cast<std::byte>(value >> 8));
}

void PutU32(std::vector<std::byte>& out, const std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::byte>((value >> shift) & 0xFF));
}

void PutTag(std::vector<std::byte>& out, const char* tag) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>(tag[i]));
}

struct WavSpec final {
    std::uint16_t format = 1;
    std::uint16_t channels = 1;
    std::uint32_t sampleRate = 48000;
    std::uint16_t bits = 16;
    std::vector<std::int16_t> samples{1, -2, 300};
    bool withData = true;
    bool oddChunkBeforeData = false;
    std::uint32_t dataBytesOverride = 0; // nonzero: written as the data chunk size
};

std::vector<std::byte> MakeWav(const WavSpec& spec) {
    std::vector<std::byte> body;
    PutTag(body, "WAVE");
    PutTag(body, "fmt ");
    PutU32(body, 16);
    PutU16(body, spec.format);
    PutU16(body, spec.channels);
    PutU32(body, spec.sampleRate);
    PutU32(body, spec.sampleRate * spec.channels * spec.bits / 8);
    PutU16(body, static_cast<std::uint16_t>(spec.channels * spec.bits / 8));
    PutU16(body, spec.bits);
    if (spec.oddChunkBeforeData) {
        PutTag(body, "LIST");
        PutU32(body, 3);
        for (int i = 0; i < 4; ++i) body.push_back(std::byte{0}); // 3 bytes plus the pad byte
    }
    if (spec.withData) {
        PutTag(body, "data");
        const auto bytes = static_cast<std::uint32_t>(spec.samples.size() * 2);
        PutU32(body, spec.dataBytesOverride != 0 ? spec.dataBytesOverride : bytes);
        for (const std::int16_t sample : spec.samples) PutU16(body, static_cast<std::uint16_t>(sample));
    }
    std::vector<std::byte> file;
    PutTag(file, "RIFF");
    PutU32(file, static_cast<std::uint32_t>(body.size()));
    file.insert(file.end(), body.begin(), body.end());
    return file;
}

auto Load(const std::vector<std::byte>& bytes) {
    SoundLoader loader;
    Engine::Asset::Loading::LoadContext context;
    context.resolvedPath = "assets/test.wav";
    return loader.Load(Engine::Base::ConstSpan<std::byte>(bytes.data(), bytes.size()), context);
}

void CheckFails(const WavSpec& spec, const AssetErrorCode code) {
    const auto result = Load(MakeWav(spec));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == code);
}

} // namespace

TEST_CASE("SoundLoader reads mono and stereo PCM16 WAV little-endian") {
    WavSpec mono;
    auto loaded = Load(MakeWav(mono));
    REQUIRE(loaded.has_value());
    const SoundAsset* sound = loaded->As<SoundAsset>();
    REQUIRE(sound != nullptr);
    CHECK(sound->sampleRate == 48000);
    CHECK(sound->channels == 1);
    CHECK(sound->pcm16 == std::vector<std::int16_t>{1, -2, 300});

    WavSpec stereo;
    stereo.channels = 2;
    stereo.sampleRate = 44100; // the loader reports the rate; the audio clip bank decides what it accepts
    stereo.samples = {-32768, 32767, 5, -5};
    stereo.oddChunkBeforeData = true; // a padded odd-sized chunk before the data
    loaded = Load(MakeWav(stereo));
    REQUIRE(loaded.has_value());
    sound = loaded->As<SoundAsset>();
    REQUIRE(sound != nullptr);
    CHECK(sound->sampleRate == 44100);
    CHECK(sound->channels == 2);
    CHECK(sound->pcm16 == stereo.samples);
}

TEST_CASE("SoundLoader rejects files it cannot represent as PCM16") {
    const std::vector<std::byte> tiny(8, std::byte{0});
    const auto small = Load(tiny);
    REQUIRE_FALSE(small.has_value());
    CHECK(small.error().code == AssetErrorCode::DecodeFailed);

    auto notRiff = MakeWav({});
    notRiff[0] = std::byte{'X'};
    const auto riff = Load(notRiff);
    REQUIRE_FALSE(riff.has_value());
    CHECK(riff.error().code == AssetErrorCode::UnsupportedFormat);

    WavSpec spec;
    spec.format = 3; // IEEE float
    CheckFails(spec, AssetErrorCode::UnsupportedFormat);
    spec = {};
    spec.bits = 8;
    CheckFails(spec, AssetErrorCode::UnsupportedFormat);
    spec = {};
    spec.channels = 6;
    CheckFails(spec, AssetErrorCode::UnsupportedFormat);
    spec = {};
    spec.withData = false;
    CheckFails(spec, AssetErrorCode::DecodeFailed);
    spec = {};
    spec.samples = {};
    spec.dataBytesOverride = 0;
    CheckFails(spec, AssetErrorCode::DecodeFailed); // an empty data chunk
    spec = {};
    spec.dataBytesOverride = 1000; // claims more bytes than the file has
    CheckFails(spec, AssetErrorCode::DecodeFailed);
}
