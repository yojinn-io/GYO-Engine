#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/audio_editor/CatalogRegistration.hpp"
#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "engine/asset/loaders/SoundLoader.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/ClipBank.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Gyo::Tools::AudioEditor;

namespace {

const std::filesystem::path kReference = std::filesystem::path(GYO_AUDIO_EDITOR_FIXTURES) / "reference";

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

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

// A fresh empty directory under the system temporary directory.
std::filesystem::path TemporaryDirectory(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / ("gyo_audio_editor_tests_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

constexpr const char* kCatalog = R"({
  "version": 1,
  "assets": [
    { "id": "game.text.settings", "type": "text", "path": "settings.json" },
    { "id": "game.audio.shot",
      "type": "sound", "path": "audio/shot.wav" },

    { "id": "game.font.ui", "type": "font", "path": "fonts/ui.ttf" }
  ]
}
)";

} // namespace

TEST_CASE("Resampling matches convert_wav.py's resample() within 1e-9") {
    // v8-tool-audio-editor-acceptance: the reference is the script's own output
    // (tests/audio_editor/fixtures/make_reference.py).
    const nlohmann::json reference = nlohmann::json::parse(ReadText(kReference / "resample.json"));
    REQUIRE(reference.size() == 5);
    for (const auto& [name, expected] : reference.items()) {
        CAPTURE(name);
        const auto decoded = DecodeWav(ReadBytes(kReference / (name + ".wav")));
        REQUIRE(decoded.has_value());
        CHECK(decoded->sampleRate == expected["source_rate"].get<std::uint32_t>());
        REQUIRE(decoded->planes.size() == expected["channels"].get<std::size_t>());
        for (std::size_t channel = 0; channel < decoded->planes.size(); ++channel) {
            const std::vector<double> resampled =
                Resample(decoded->planes[channel], decoded->sampleRate, Engine::Audio::kSampleRate);
            const auto& plane = expected["planes"][channel];
            REQUIRE(resampled.size() == plane.size());
            double worst = 0.0;
            for (std::size_t index = 0; index < resampled.size(); ++index) {
                worst = std::max(worst, std::abs(resampled[index] - plane[index].get<double>()));
            }
            CHECK(worst <= 1e-9);
        }
    }
}

TEST_CASE("Quantizing is reproducible and dithers within one step") {
    const std::vector<std::vector<double>> planes{{0.0, 0.25, -0.5, 0.999, -1.0, 0.1234}, {0.0, -0.25, 0.5, 0.0, 0.3, 0.7}};
    const QuantizedSamples first = Quantize(planes, 42);
    const QuantizedSamples again = Quantize(planes, 42);
    CHECK(first.interleaved == again.interleaved);
    CHECK(first.saturated == 0);
    for (std::size_t index = 0; index < first.interleaved.size(); ++index) {
        const double value = planes[index % 2][index / 2] * 32767.0;
        // a dither in (-1, 1), then rounding by at most a half
        CHECK(std::abs(first.interleaved[index] - value) < 1.5);
    }

    const std::vector<std::vector<double>> silence{std::vector<double>(4096, 0.0)};
    const QuantizedSamples noise = Quantize(silence, 7);
    int nonzero = 0;
    for (const std::int16_t sample : noise.interleaved) {
        CHECK((sample >= -1 && sample <= 1));
        nonzero += sample != 0 ? 1 : 0;
    }
    CHECK(nonzero > 0);
    CHECK(Quantize(silence, 8).interleaved != noise.interleaved);

    // Half a step: a dither within (-1, 1) rounds only to 0 or 1, a wider one
    // also to -1 or 2 (v8-tool-audio-editor-acceptance: a dither of ±1 LSB).
    const std::vector<std::vector<double>> half{std::vector<double>(4096, 0.5 / 32767.0)};
    int ones = 0;
    for (const std::int16_t sample : Quantize(half, 9).interleaved) {
        CHECK((sample == 0 || sample == 1));
        ones += sample;
    }
    CHECK(ones > 0);
    CHECK(ones < 4096);
}

TEST_CASE("Quantizing counts and clamps saturated samples") {
    const QuantizedSamples loud = Quantize({{1.5, -1.5, 0.0, 2.0}}, 1);
    CHECK(loud.saturated == 3);
    CHECK(loud.interleaved[0] == 32767);
    CHECK(loud.interleaved[1] == -32768);
    CHECK(loud.interleaved[3] == 32767);
}

TEST_CASE("A converted clip loads through SoundLoader and ClipBank (Design Invariant 2)") {
    for (const char* name : {"stereo_22050_pcm24", "mono_96000_float32", "stereo_48000_pcm8"}) {
        CAPTURE(name);
        const auto decoded = DecodeWav(ReadBytes(kReference / (std::string(name) + ".wav")));
        REQUIRE(decoded.has_value());
        const ConvertedClip clip = ConvertToClip(*decoded, DitherSeed(name));
        CHECK(clip.sampleRate == Engine::Audio::kSampleRate);
        const std::vector<std::byte> wav = EncodePcm16Wav(clip.channels, clip.sampleRate, clip.samples);

        Engine::Asset::Loaders::SoundLoader loader;
        Engine::Asset::Loading::LoadContext context;
        context.resolvedPath = "assets/converted.wav";
        const auto loaded = loader.Load(Engine::Base::ConstSpan<std::byte>(wav.data(), wav.size()), context);
        REQUIRE(loaded.has_value());
        const auto* sound = loaded->As<Engine::Asset::Loaders::SoundAsset>();
        REQUIRE(sound != nullptr);
        CHECK(sound->pcm16 == clip.samples);
        Engine::Audio::ClipBank bank;
        CHECK(bank.Add(Engine::Base::ConstSpan<std::int16_t>(sound->pcm16.data(), sound->pcm16.size()),
                       sound->channels, sound->sampleRate)
                  .has_value());
    }
}

TEST_CASE("DecodeWav rejects what it cannot convert") {
    const auto stereo = ReadBytes(kReference / "stereo_22050_pcm24.wav");
    CHECK(DecodeWav(std::vector<std::byte>(stereo.begin(), stereo.begin() + 8)).error().code ==
          AudioEditorErrorCode::InvalidWav);
    auto threeChannels = stereo;
    threeChannels[22] = std::byte{3}; // fmt channels
    CHECK(DecodeWav(threeChannels).error().code == AudioEditorErrorCode::UnsupportedFormat);
    auto twelveBits = stereo;
    twelveBits[34] = std::byte{12}; // fmt bits per sample
    CHECK(DecodeWav(twelveBits).error().code == AudioEditorErrorCode::UnsupportedFormat);
}

TEST_CASE("Registering a new sound appends one line and keeps every other byte") {
    const auto update = RegisterSound(kCatalog, {"game.audio.reload", "audio/reload.wav"});
    REQUIRE(update.has_value());
    CHECK(update->added);
    const std::string catalog = kCatalog;
    const std::size_t last = catalog.find("\"fonts/ui.ttf\" }") + std::string_view("\"fonts/ui.ttf\" }").size();
    CHECK(update->text == catalog.substr(0, last) +
                              ",\n    { \"id\": \"game.audio.reload\", \"type\": \"sound\", \"path\": \"audio/reload.wav\" }" +
                              catalog.substr(last));
}

TEST_CASE("Registering an existing sound rewrites only its entry") {
    const auto update = RegisterSound(kCatalog, {"game.audio.shot", "audio/shot_v2.wav"});
    REQUIRE(update.has_value());
    CHECK_FALSE(update->added);
    REQUIRE(update->previousPath.has_value());
    CHECK(*update->previousPath == "audio/shot.wav");
    std::string expected = kCatalog;
    const std::size_t begin = expected.find("{ \"id\": \"game.audio.shot\"");
    const std::size_t end = expected.find('}', begin) + 1;
    expected.replace(begin, end - begin, R"({ "id": "game.audio.shot", "type": "sound", "path": "audio/shot_v2.wav" })");
    CHECK(update->text == expected);

    const auto same = RegisterSound(kCatalog, {"game.audio.shot", "audio/shot.wav"});
    REQUIRE(same.has_value());
    CHECK_FALSE(same->previousPath.has_value());
}

TEST_CASE("Registering into an empty assets array") {
    const auto update = RegisterSound("{\n  \"version\": 1,\n  \"assets\": []\n}\n", {"a.b", "b.wav"});
    REQUIRE(update.has_value());
    CHECK(update->text ==
          "{\n  \"version\": 1,\n  \"assets\": [\n    { \"id\": \"a.b\", \"type\": \"sound\", \"path\": \"b.wav\" }\n  ]\n}\n");
}

TEST_CASE("Registration refuses what the content contract or another entry forbids") {
    const auto code = [](const std::string_view text, const SoundRegistration& registration) {
        const auto result = RegisterSound(text, registration);
        REQUIRE_FALSE(result.has_value());
        return result.error().code;
    };
    CHECK(code(kCatalog, {"game.text.settings", "x.wav"}) == AudioEditorErrorCode::InvalidRegistration); // another type
    CHECK(code(kCatalog, {"game.audio.new", "fonts/ui.ttf"}) == AudioEditorErrorCode::InvalidRegistration); // taken path
    CHECK(code(kCatalog, {"", "x.wav"}) == AudioEditorErrorCode::InvalidRegistration);
    for (const char* path : {"", "/abs.wav", "a/../b.wav", "./a.wav", "a//b.wav", "a\\b.wav", "C:/a.wav", "content.json"}) {
        CAPTURE(path);
        CHECK(code(kCatalog, {"game.audio.new", path}) == AudioEditorErrorCode::InvalidRegistration);
    }
    CHECK(code(R"({"version": 2, "assets": []})", {"a", "a.wav"}) == AudioEditorErrorCode::InvalidCatalog);
    CHECK(code(R"({"version": 1})", {"a", "a.wav"}) == AudioEditorErrorCode::InvalidCatalog);
    CHECK(code(R"({"version": 1, "assets": [{"id": "a", "type": "text", "path": "a"}, {"id": "a", "type": "text", "path": "b"}]})",
               {"c", "c.wav"}) == AudioEditorErrorCode::InvalidCatalog);
    CHECK(code("not json", {"a", "a.wav"}) == AudioEditorErrorCode::InvalidCatalog);
}

TEST_CASE("ExportSound writes the clip and its entry and never touches the source") {
    const auto root = TemporaryDirectory("export");
    const auto source = root / "outside" / "reload_raw.wav";
    std::filesystem::create_directories(source.parent_path());
    std::filesystem::copy_file(kReference / "stereo_22050_pcm24.wav", source);
    const auto sourceBytes = ReadBytes(source);
    const auto assets = root / "assets";
    std::filesystem::create_directories(assets);
    WriteText(assets / "asset_catalog.json", kCatalog);

    ExportRequest request{Utf8(source), Utf8(assets), Utf8(assets / "asset_catalog.json"),
                          {"game.audio.reload", "audio/reload.wav"}};
    const auto report = ExportSound(request);
    REQUIRE(report.has_value());
    CHECK(report->added);
    CHECK(report->outputRate == Engine::Audio::kSampleRate);
    CHECK(report->channels == 2);
    CHECK(report->frames == 653); // floor(300 * 48000 / 22050), as the script's reference has it
    CHECK(ReadBytes(source) == sourceBytes);
    CHECK(std::filesystem::exists(assets / "audio" / "reload.wav"));
    CHECK_FALSE(std::filesystem::exists(assets / "audio" / "reload.wav.gyo-tmp"));
    CHECK(ReadText(assets / "asset_catalog.json").find("\"path\": \"audio/reload.wav\"") != std::string::npos);

    // The same request again gives the same bytes (Design Invariant 7) and updates in place.
    const auto first = ReadBytes(assets / "audio" / "reload.wav");
    const auto second = ExportSound(request);
    REQUIRE(second.has_value());
    CHECK_FALSE(second->added);
    CHECK(ReadBytes(assets / "audio" / "reload.wav") == first);
}

TEST_CASE("ExportSound refuses to overwrite its source or the catalog, and writes nothing for a bad source") {
    const auto root = TemporaryDirectory("refuse");
    const auto assets = root / "assets";
    std::filesystem::create_directories(assets / "audio");
    WriteText(assets / "asset_catalog.json", kCatalog);
    std::filesystem::copy_file(kReference / "mono_44100_pcm16.wav", assets / "audio" / "raw.wav");

    ExportRequest request{Utf8(assets / "audio" / "raw.wav"), Utf8(assets), Utf8(assets / "asset_catalog.json"),
                          {"game.audio.raw", "audio/raw.wav"}};
    auto refused = ExportSound(request);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == AudioEditorErrorCode::InvalidRegistration);

    request.registration = {"game.audio.raw", "asset_catalog.json"};
    refused = ExportSound(request);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == AudioEditorErrorCode::InvalidRegistration);

    WriteText(root / "bad.wav", "RIFF....WAVEjunk");
    request.sourcePath = Utf8(root / "bad.wav");
    request.registration = {"game.audio.bad", "audio/bad.wav"};
    refused = ExportSound(request);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == AudioEditorErrorCode::InvalidWav);
    CHECK_FALSE(std::filesystem::exists(assets / "audio" / "bad.wav"));
    CHECK(ReadText(assets / "asset_catalog.json") == kCatalog);
}
