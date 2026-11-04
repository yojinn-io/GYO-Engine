#include <doctest/doctest.h>

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/Edit.hpp"
#include "gyo/audio_editor/PreviewPlayer.hpp"
#include "gyo/audio_editor/ProjectFile.hpp"
#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"
#include "gyo/audio_editor/Waveform.hpp"

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// v8-tool-audio-editor-edit: the judgments 2 to 7 of the pre-declaration
// (docs/tools/plans/v8/assets.md). All are deterministic and run once.

using namespace Gyo::Tools::AudioEditor;

namespace {

namespace Audio = Engine::Audio;

const std::filesystem::path kReference = std::filesystem::path(GYO_AUDIO_EDITOR_FIXTURES) / "reference";
const Engine::Time::TimePoint kAnchor =
    Engine::Time::TimePoint{} + std::chrono::duration_cast<Engine::Time::Duration>(std::chrono::seconds(10));

std::vector<std::byte> ReadBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t index = 0; index < chars.size(); ++index) bytes[index] = static_cast<std::byte>(chars[index]);
    return bytes;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::filesystem::path TemporaryDirectory(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("gyo_audio_editor_edit_tests_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

DecodedWav Decode(const char* name) {
    auto decoded = DecodeWav(ReadBytes(kReference / (std::string(name) + ".wav")));
    REQUIRE(decoded.has_value());
    return std::move(decoded).value();
}

// A synthetic source: channel c, frame f is (f + 1) / 1000 * (c ? -1 : 1).
DecodedWav Synthetic(const std::uint32_t rate, const std::size_t frames, const std::size_t channels = 2) {
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
std::vector<std::int16_t> DataChunk(const std::vector<std::byte>& wav) {
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
PreviewRig MakePreviewRig() {
    auto created = Audio::Mixer::Create(PreviewMixerConfig(), Audio::ClipBank{});
    REQUIRE(created.has_value());
    PreviewRig rig{std::make_unique<Audio::Mixer>(std::move(created).value()), std::nullopt};
    auto submitter = rig.mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    rig.player.emplace(std::move(submitter).value(), rig.mixer.get());
    return rig;
}

std::vector<std::int16_t> RenderBlock(Audio::Mixer& mixer, const std::uint32_t frames,
                                      const Engine::Time::TimePoint anchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * Audio::kChannels, 0x5555);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

} // namespace

TEST_CASE("Default settings render what ConvertToClip converts") {
    for (const char* name :
         {"mono_32000_pcm32", "mono_44100_pcm16", "mono_96000_float32", "stereo_22050_pcm24", "stereo_48000_pcm8"}) {
        CAPTURE(name);
        const DecodedWav source = Decode(name);
        const auto rendered = RenderClip(source, {}, DitherSeed(name));
        REQUIRE(rendered.has_value());
        const ConvertedClip converted = ConvertToClip(source, DitherSeed(name));
        CHECK(rendered->samples == converted.samples);
        CHECK(rendered->saturated == converted.saturated);
    }
}

// Judgment 2.
TEST_CASE("Trimming a 48 kHz source keeps exactly that range, and another rate scales the frames") {
    const DecodedWav source = Synthetic(Audio::kSampleRate, 100);
    const auto planes = RenderPlanes(source, {.trimStart = 7, .trimEnd = 42});
    REQUIRE(planes.has_value());
    REQUIRE(planes->size() == 2);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        CAPTURE(channel);
        const std::vector<double> expected(source.planes[channel].begin() + 7, source.planes[channel].begin() + 42);
        CHECK((*planes)[channel] == expected);
    }
    // No end: to the source's end.
    const auto tail = RenderPlanes(source, {.trimStart = 90});
    REQUIRE(tail.has_value());
    CHECK((*tail)[0] == std::vector<double>(source.planes[0].begin() + 90, source.planes[0].end()));

    const DecodedWav other = Synthetic(22050, 300);
    const auto resampled = RenderClip(other, {.trimStart = 50, .trimEnd = 250}, 1);
    REQUIRE(resampled.has_value());
    CHECK(resampled->Frames() == 200U * 48000U / 22050U); // floor(200 * 48000 / 22050) = 435
    const auto whole = RenderClip(Decode("stereo_22050_pcm24"), {.trimStart = 1, .trimEnd = 299}, 1);
    REQUIRE(whole.has_value());
    CHECK(whole->Frames() == 298U * 48000U / 22050U);
}

TEST_CASE("Settings that keep no frame, end after the source or leave the gain range are refused") {
    const DecodedWav source = Synthetic(Audio::kSampleRate, 10);
    const auto refused = [&source](const EditSettings& settings) {
        const auto rendered = RenderClip(source, settings, 1);
        return !rendered.has_value() && rendered.error().code == AudioEditorErrorCode::InvalidSettings;
    };
    CHECK(refused({.trimStart = 5, .trimEnd = 5}));
    // The window checks a project's settings against its source with ValidateSettings alone.
    CHECK_FALSE(ValidateSettings({.trimStart = 5, .trimEnd = 5}, 10).has_value());
    CHECK(ValidateSettings({.trimStart = 4, .trimEnd = 5}, 10).has_value());
    CHECK(refused({.trimStart = 6, .trimEnd = 5}));
    CHECK(refused({.trimStart = 10}));
    CHECK(refused({.trimStart = 0, .trimEnd = 11}));
    CHECK(refused({.gainDb = kMaxGainDb + 0.5}));
    CHECK(refused({.gainDb = kMinGainDb - 0.5}));
    CHECK(refused({.gainDb = std::numeric_limits<double>::quiet_NaN()}));
    CHECK(RenderClip(source, {.trimStart = 9, .trimEnd = 10, .gainDb = kMaxGainDb}, 1).has_value());
    // One frame of a 96 kHz source resamples to no frame of the clip.
    const DecodedWav fast = Synthetic(96000, 10);
    const auto none = RenderClip(fast, {.trimStart = 3, .trimEnd = 4}, 1);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().code == AudioEditorErrorCode::InvalidSettings);
    CHECK(RenderClip(fast, {.trimStart = 3, .trimEnd = 5}, 1)->Frames() == 1);
    CHECK(RenderClip(source, {.gainDb = kMinGainDb}, 1).has_value());
}

// Judgment 3.
TEST_CASE("Gain scales every sample by 10^(dB/20) and saturation counts the clamped samples") {
    const DecodedWav source = Synthetic(Audio::kSampleRate, 64);
    for (const double db : {-6.5, 3.0, 12.25}) {
        CAPTURE(db);
        const auto planes = RenderPlanes(source, {.gainDb = db});
        REQUIRE(planes.has_value());
        const double factor = std::pow(10.0, db / 20.0);
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t frame = 0; frame < 64; ++frame) {
                CHECK((*planes)[channel][frame] == source.planes[channel][frame] * factor);
            }
        }
    }

    // +6 dB on 0.9 and -0.95 is past full scale whatever the dither; on 0.2 and 0.4 it is not.
    DecodedWav loud;
    loud.sampleRate = Audio::kSampleRate;
    loud.formatTag = 3;
    loud.bits = 32;
    loud.planes = {{0.9, 0.2, -0.95, 0.4, 0.9, -0.95, 0.2}};
    const auto clip = RenderClip(loud, {.gainDb = 6.0}, 3);
    REQUIRE(clip.has_value());
    CHECK(clip->saturated == 4);
    CHECK(clip->samples[0] == 32767);
    CHECK(clip->samples[2] == -32768);
    CHECK(clip->samples[4] == 32767);
    CHECK(clip->samples[5] == -32768);
    for (const std::size_t index : {1U, 3U, 6U}) {
        CHECK(std::abs(clip->samples[index]) < 32767);
    }
}

// Judgment 4: the samples the player loads are what export writes; at gain 1
// the mixer renders them unchanged, so its output is that data chunk too.
TEST_CASE("The preview plays the bytes export writes") {
    for (const char* name : {"stereo_22050_pcm24", "mono_44100_pcm16"}) {
        CAPTURE(name);
        const auto root = TemporaryDirectory(std::string("preview_") + name);
        const auto assets = root / "assets";
        std::filesystem::create_directories(assets);
        std::ofstream(assets / "asset_catalog.json", std::ios::binary) << "{\n  \"version\": 1,\n  \"assets\": []\n}\n";
        ExportRequest request{Utf8(kReference / (std::string(name) + ".wav")), Utf8(assets),
                              Utf8(assets / "asset_catalog.json"),
                              {"game.audio.preview", "audio/preview.wav"},
                              {.trimStart = 20, .trimEnd = 250, .gainDb = 3.0}};
        REQUIRE(ExportSound(request).has_value());
        const std::vector<std::int16_t> written = DataChunk(ReadBytes(assets / "audio" / "preview.wav"));

        const DecodedWav source = Decode(name);
        auto clip = RenderClip(source, request.settings, DitherSeed(request.registration.id));
        REQUIRE(clip.has_value());
        CHECK(clip->samples == written);

        PreviewRig rig = MakePreviewRig();
        const std::size_t frames = clip->Frames();
        const std::uint16_t channels = clip->channels;
        rig.player->Play(std::move(clip).value(), kAnchor);
        REQUIRE_FALSE(rig.player->Waiting());
        const std::vector<std::int16_t> out = RenderBlock(*rig.mixer, static_cast<std::uint32_t>(frames), kAnchor);
        std::vector<std::int16_t> heard;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            heard.push_back(out[frame * 2]);
            if (channels == 2) heard.push_back(out[frame * 2 + 1]);
            else CHECK(out[frame * 2 + 1] == out[frame * 2]);
        }
        CHECK(heard == written);
    }
}

TEST_CASE("The preview volume changes what is heard, not the clip") {
    PreviewRig rig = MakePreviewRig();
    ConvertedClip clip{Audio::kSampleRate, 1, std::vector<std::int16_t>(480, 1000), 0};
    rig.player->SetVolume(0.5f);
    rig.player->Play(clip, kAnchor);
    const auto out = RenderBlock(*rig.mixer, 480, kAnchor);
    CHECK(out[0] == 500);
    CHECK(out[958] == 500);
}

// Judgment 7.
// The name has no comma: doctest's -tc filter splits at commas.
TEST_CASE("The preview player plays and stops sounds and swaps clips on two slots") {
    PreviewRig rig = MakePreviewRig();
    ConvertedClip clip{Audio::kSampleRate, 1, std::vector<std::int16_t>(4800, 100), 0};
    rig.player->Play(clip, kAnchor);
    CHECK(rig.player->Position().status == Audio::Playhead::Status::NotPlaying); // not taken yet
    (void)RenderBlock(*rig.mixer, 480, kAnchor);
    CHECK(rig.player->Position().status == Audio::Playhead::Status::Playing);
    CHECK(rig.player->Position().clipFrame == 480);
    (void)RenderBlock(*rig.mixer, 480, kAnchor);
    CHECK(rig.player->Position().clipFrame == 960);

    rig.player->Stop(kAnchor);
    const auto stopped = RenderBlock(*rig.mixer, 480, kAnchor);
    CHECK(rig.player->Position().status == Audio::Playhead::Status::NotPlaying);
    for (const std::int16_t sample : stopped) CHECK(sample == 0);
    for (int block = 0; block < 3; ++block) {
        for (const std::int16_t sample : RenderBlock(*rig.mixer, 480, kAnchor)) CHECK(sample == 0);
    }

    // Ten swaps in one frame: the slots are taken, so the later clips wait and
    // only the last is kept; one block hands both slots back and it starts.
    const Audio::SoundHandle first = rig.player->Current();
    std::vector<Audio::SoundHandle> started;
    for (int swap = 0; swap < 10; ++swap) {
        rig.player->Play({Audio::kSampleRate, 1, std::vector<std::int16_t>(4800, static_cast<std::int16_t>(swap + 1)), 0},
                         kAnchor);
        if (rig.player->Current().Valid() && (started.empty() || started.back() != rig.player->Current())) {
            started.push_back(rig.player->Current());
        }
    }
    CHECK(rig.player->Waiting());
    (void)RenderBlock(*rig.mixer, 480, kAnchor);
    rig.player->Update(kAnchor);
    CHECK_FALSE(rig.player->Waiting());
    CHECK_FALSE(rig.player->Problem().has_value());
    const auto last = RenderBlock(*rig.mixer, 480, kAnchor);
    CHECK(last[0] == 10);
    CHECK(rig.mixer->PlayheadOf(first).status == Audio::Playhead::Status::NotPlaying);
    for (const Audio::SoundHandle sound : started) {
        CHECK(rig.mixer->PlayheadOf(sound).status == Audio::Playhead::Status::NotPlaying);
    }

    // Ten frames with two swaps each: the second clip finds both slots taken
    // (one plays, one waits for its unload to be taken), waits, and starts
    // after the next block; every earlier sound is stopped.
    std::vector<Audio::SoundHandle> earlier{rig.player->Current()};
    for (int swap = 0; swap < 10; ++swap) {
        CAPTURE(swap);
        rig.player->Play({Audio::kSampleRate, 1, std::vector<std::int16_t>(4800, static_cast<std::int16_t>(swap + 100)), 0},
                         kAnchor);
        earlier.push_back(rig.player->Current());
        rig.player->Play({Audio::kSampleRate, 1, std::vector<std::int16_t>(4800, static_cast<std::int16_t>(swap + 20)), 0},
                         kAnchor);
        CHECK(rig.player->Waiting());
        (void)RenderBlock(*rig.mixer, 480, kAnchor);
        rig.player->Update(kAnchor);
        CHECK_FALSE(rig.player->Waiting());
        CHECK_FALSE(rig.player->Problem().has_value());
        earlier.push_back(rig.player->Current());
        const auto heard = RenderBlock(*rig.mixer, 480, kAnchor);
        CHECK(heard[0] == swap + 20);
        // The editor calls Update every frame; it leaves the sound playing.
        rig.player->Update(kAnchor);
        CHECK(RenderBlock(*rig.mixer, 480, kAnchor)[0] == swap + 20);
        CHECK(rig.player->Position().status == Audio::Playhead::Status::Playing);
        for (std::size_t index = 0; index + 1 < earlier.size(); ++index) {
            CHECK(rig.mixer->PlayheadOf(earlier[index]).status == Audio::Playhead::Status::NotPlaying);
        }
    }
}

// Judgment 6. On rising samples a column's minimum is its first sample and
// its maximum its last, so a boundary one sample off changes the result; the
// scrambled samples put the extremes inside the columns.
TEST_CASE("Each waveform column has the extremes of its samples") {
    std::vector<double> rising(100);
    std::vector<double> scrambled(100);
    for (std::size_t index = 0; index < 100; ++index) {
        rising[index] = static_cast<double>(index) / 100.0 - 0.5;
        scrambled[index] = static_cast<double>((index * 37) % 100) / 100.0 - 0.5;
    }
    for (const std::vector<double>* samples : {&rising, &scrambled}) {
        for (const std::size_t columns : {std::size_t{7}, std::size_t{33}}) {
            CAPTURE(columns);
            const auto result = Waveform(*samples, columns);
            REQUIRE(result.size() == columns);
            for (std::size_t column = 0; column < columns; ++column) {
                const std::size_t first = column * 100 / columns;
                const std::size_t last = (column + 1) * 100 / columns;
                double low = (*samples)[first];
                double high = (*samples)[first];
                for (std::size_t index = first; index < last; ++index) {
                    low = std::min(low, (*samples)[index]);
                    high = std::max(high, (*samples)[index]);
                }
                CAPTURE(column);
                CHECK(result[column] == WaveColumn{low, high});
            }
        }
    }
}

TEST_CASE("A waveform wider than its samples repeats them, and no samples give no columns") {
    const std::vector<double> three{-0.25, 0.5, 0.75};
    const auto wide = Waveform(three, 6);
    REQUIRE(wide.size() == 6);
    CHECK(wide[0] == WaveColumn{-0.25, -0.25});
    CHECK(wide[1] == WaveColumn{-0.25, -0.25});
    CHECK(wide[2] == WaveColumn{0.5, 0.5});
    CHECK(wide[5] == WaveColumn{0.75, 0.75});
    CHECK(Waveform({}, 4).empty());
}

// Judgment 5.
TEST_CASE("A project reads back as saved, and is refused inside the asset root or with another version") {
    const auto root = TemporaryDirectory("project");
    AudioProject project;
    project.request = {Utf8(root / "sources" / std::filesystem::path(u8"シャッター.wav")), Utf8(root / "assets"),
                       Utf8(root / "assets" / "asset_catalog.json"), {"game.audio.shutter", "audio/shutter.wav"},
                       {.trimStart = 12, .trimEnd = 3400, .gainDb = -4.7}};
    const std::string path = Utf8(root / "projects" / "shutter.audio.json");
    REQUIRE(SaveProject(path, project).has_value());
    const auto loaded = LoadProject(path);
    REQUIRE(loaded.has_value());
    CHECK(*loaded == project);
    CHECK(ProjectText(*loaded) == ReadText(root / "projects" / "shutter.audio.json"));

    AudioProject untrimmed = project;
    untrimmed.request.settings = {.gainDb = 0.1 + 0.2}; // a double that needs all its digits
    REQUIRE(SaveProject(path, untrimmed).has_value());
    const auto again = LoadProject(path);
    REQUIRE(again.has_value());
    CHECK(*again == untrimmed);
    CHECK_FALSE(again->request.settings.trimEnd.has_value());

    std::filesystem::create_directories(root / "assets");
    const std::string inside = Utf8(root / "assets" / "audio" / "shutter.audio.json");
    const auto refused = SaveProject(inside, project);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == AudioEditorErrorCode::InvalidProject);
    CHECK_FALSE(std::filesystem::exists(root / "assets" / "audio"));

    std::string text = ProjectText(project);
    const auto at = text.find("\"version\": 1");
    REQUIRE(at != std::string::npos);
    text.replace(at, 12, "\"version\": 2");
    const auto newer = ParseProject(text);
    REQUIRE_FALSE(newer.has_value());
    CHECK(newer.error().code == AudioEditorErrorCode::InvalidProject);

    for (const std::string& broken :
         {std::string("[]"), std::string("not json"), std::string(R"({"format": "gyo.audio_editor.project"})"),
          ProjectText(project).replace(ProjectText(project).find("\"gain_db\""), 9, "\"gain\"")}) {
        CAPTURE(broken);
        const auto parsed = ParseProject(broken);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == AudioEditorErrorCode::InvalidProject);
    }
}
