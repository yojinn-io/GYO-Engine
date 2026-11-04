#pragma once

#include <doctest/doctest.h>

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/PreviewPlayer.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Helpers the edit and region tests share; each test file includes this once.
namespace Gyo::Tools::AudioEditor::TestSupport {

namespace Audio = Engine::Audio;

inline const std::filesystem::path kReference = std::filesystem::path(GYO_AUDIO_EDITOR_FIXTURES) / "reference";
inline const Engine::Time::TimePoint kAnchor =
    Engine::Time::TimePoint{} + std::chrono::duration_cast<Engine::Time::Duration>(std::chrono::seconds(10));

inline std::vector<std::byte> ReadBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t index = 0; index < chars.size(); ++index) bytes[index] = static_cast<std::byte>(chars[index]);
    return bytes;
}

inline std::string ReadText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

inline std::filesystem::path TemporaryDirectory(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("gyo_audio_editor_edit_tests_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

inline DecodedWav Decode(const char* name) {
    auto decoded = DecodeWav(ReadBytes(kReference / (std::string(name) + ".wav")));
    REQUIRE(decoded.has_value());
    return std::move(decoded).value();
}

// A synthetic source: channel c, frame f is (f + 1) / 1000 * (c ? -1 : 1).
inline DecodedWav Synthetic(const std::uint32_t rate, const std::size_t frames, const std::size_t channels = 2) {
    DecodedWav source;
    source.sampleRate = rate;
    source.formatTag = 3;
    source.bits = 32;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        std::vector<double> plane(frames);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            plane[frame] = static_cast<double>(frame + 1) / 1000.0 * (channel == 0 ? 1.0 : -1.0);
        }
        source.planes.push_back(std::move(plane));
    }
    return source;
}

// The interleaved samples of a canonical 44-byte-header PCM16 WAV (EncodePcm16Wav).
inline std::vector<std::int16_t> DataChunk(const std::vector<std::byte>& wav) {
    REQUIRE(wav.size() >= 44);
    std::vector<std::int16_t> samples((wav.size() - 44) / 2);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto low = std::to_integer<std::uint16_t>(wav[44 + index * 2]);
        const auto high = std::to_integer<std::uint16_t>(wav[45 + index * 2]);
        samples[index] = static_cast<std::int16_t>(static_cast<std::uint16_t>(low | (high << 8)));
    }
    return samples;
}

struct PreviewRig final {
    std::unique_ptr<Audio::Mixer> mixer; // the player keeps its address
    std::optional<PreviewPlayer> player;
};

// The preview's own mixer configuration, rendered by the test's thread.
inline PreviewRig MakePreviewRig() {
    auto created = Audio::Mixer::Create(PreviewMixerConfig(), Audio::ClipBank{});
    REQUIRE(created.has_value());
    PreviewRig rig{std::make_unique<Audio::Mixer>(std::move(created).value()), std::nullopt};
    auto submitter = rig.mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    rig.player.emplace(std::move(submitter).value(), rig.mixer.get());
    return rig;
}

inline std::vector<std::int16_t> RenderBlock(Audio::Mixer& mixer, const std::uint32_t frames,
                                      const Engine::Time::TimePoint anchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * Audio::kChannels, 0x5555);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

} // namespace Gyo::Tools::AudioEditor::TestSupport
