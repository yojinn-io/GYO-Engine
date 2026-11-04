#include <doctest/doctest.h>

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/Edit.hpp"
#include "gyo/audio_editor/PreviewPlayer.hpp"
#include "gyo/audio_editor/ProjectFile.hpp"
#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "AudioEditorTestSupport.hpp"

#include "engine/os/Files.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

// v8-tool-audio-editor-regions: the judgments 1 to 6 of the pre-declaration
// (docs/tools/plans/v8/assets.md). All are deterministic and run once.

using namespace Gyo::Tools::AudioEditor;
using namespace Gyo::Tools::AudioEditor::TestSupport;

namespace {

// A catalog with one entry the editor does not own, written with a layout and
// a field of its own so any change to it shows in the bytes.
constexpr const char* kCatalog = "{\n  \"version\": 1,\n  \"assets\": [\n"
                                 "    {\"id\": \"game.ui.menu\", \"type\": \"text\", \"path\": \"ui/menu.json\", "
                                 "\"note\": \"kept\"}\n  ]\n}\n";
constexpr const char* kOtherEntry = "{\"id\": \"game.ui.menu\", \"type\": \"text\", \"path\": \"ui/menu.json\", "
                                    "\"note\": \"kept\"}";

struct Root final {
    std::filesystem::path root;
    std::filesystem::path assets;
    std::filesystem::path catalog;
};

Root MakeRoot(const std::string& name) {
    Root made;
    made.root = TemporaryDirectory("regions_" + name);
    made.assets = made.root / "assets";
    made.catalog = made.assets / "asset_catalog.json";
    std::filesystem::create_directories(made.assets);
    std::ofstream(made.catalog, std::ios::binary) << kCatalog;
    return made;
}

// Every file under a directory, relative to it.
std::set<std::string> FilesUnder(const std::filesystem::path& directory) {
    std::set<std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) files.insert(Utf8(std::filesystem::relative(entry.path(), directory)));
    }
    return files;
}

const std::filesystem::path kSource = kReference / "stereo_22050_pcm24.wav";

// Three clips of the reference source (300 frames), as the reload sound is cut into actions.
std::vector<SoundRegion> ThreeRegions() {
    return {
        {{"game.audio.magazine_out", "audio/magazine_out.wav"},
         {.trimStart = 0, .trimEnd = 120, .gainDb = 0.0, .fadeIn = 0, .fadeOut = 20}},
        {{"game.audio.magazine_in", "audio/magazine_in.wav"},
         {.trimStart = 100, .trimEnd = 220, .gainDb = 2.5, .fadeIn = 10, .fadeOut = 15}},
        {{"game.audio.bolt_pull", "audio/bolt_pull.wav"},
         {.trimStart = 200, .trimEnd = std::nullopt, .gainDb = -3.0, .fadeIn = 6, .fadeOut = 0}},
    };
}

ExportBatch BatchIn(const Root& root, std::vector<SoundRegion> regions) {
    return {Utf8(kSource), Utf8(root.assets), Utf8(root.catalog), std::move(regions)};
}

} // namespace

// Judgment 1, the part a unit test holds: a version 1 project is one region
// without fades, and exports what the -edit export of the same request wrote.
TEST_CASE("A version 1 project reads as one region and exports what the single export writes") {
    const Root single = MakeRoot("v1_single");
    const Root project = MakeRoot("v1_project");
    const std::string v1 = nlohmann::ordered_json{
        {"format", "gyo.audio_editor.project"},
        {"version", 1},
        {"source", Utf8(kSource)},
        {"asset_root", Utf8(project.assets)},
        {"asset_catalog", Utf8(project.catalog)},
        {"id", "game.audio.reload"},
        {"path", "audio/reload.wav"},
        {"trim_start", 40},
        {"trim_end", 250},
        {"gain_db", -1.5}}.dump(2);
    const auto parsed = ParseProject(v1);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->batch.regions.size() == 1);
    const SoundRegion& region = parsed->batch.regions[0];
    CHECK(region.registration.id == "game.audio.reload");
    CHECK(region.settings == EditSettings{.trimStart = 40, .trimEnd = 250, .gainDb = -1.5});

    REQUIRE(ExportSounds(parsed->batch).has_value());
    ExportRequest request{Utf8(kSource), Utf8(single.assets), Utf8(single.catalog), region.registration,
                          region.settings};
    REQUIRE(ExportSound(request).has_value());
    CHECK(ReadBytes(project.assets / "audio" / "reload.wav") == ReadBytes(single.assets / "audio" / "reload.wav"));
}

// Judgment 2.
TEST_CASE("Fades ramp the samples linearly over the source frames and must fit the trim") {
    const DecodedWav source = Synthetic(Audio::kSampleRate, 100);
    const EditSettings settings{.trimStart = 10, .trimEnd = 60, .gainDb = 4.0, .fadeIn = 8, .fadeOut = 5};
    const auto planes = RenderPlanes(source, settings);
    REQUIRE(planes.has_value());
    const double g = std::pow(10.0, 4.0 / 20.0);
    const std::size_t n = 50;
    for (std::size_t channel = 0; channel < 2; ++channel) {
        REQUIRE((*planes)[channel].size() == n);
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            double expected = source.planes[channel][10 + i] * g;
            if (i < 8) expected *= static_cast<double>(i) / 8.0;
            if (n - 1 - i < 5) expected *= static_cast<double>(n - 1 - i) / 5.0;
            CHECK((*planes)[channel][i] == expected);
        }
        CHECK((*planes)[channel].front() == 0.0);
        CHECK((*planes)[channel].back() == 0.0);
        CHECK((*planes)[channel][8] == source.planes[channel][18] * g); // past the fade in
    }

    // At another rate the fades act on the source's frames, before resampling.
    const DecodedWav slow = Synthetic(24000, 100, 1);
    const auto resampled = RenderPlanes(slow, {.trimStart = 10, .trimEnd = 60, .fadeIn = 8, .fadeOut = 5});
    REQUIRE(resampled.has_value());
    std::vector<double> faded(slow.planes[0].begin() + 10, slow.planes[0].begin() + 60);
    for (std::size_t i = 0; i < n; ++i) {
        if (i < 8) faded[i] *= static_cast<double>(i) / 8.0;
        if (n - 1 - i < 5) faded[i] *= static_cast<double>(n - 1 - i) / 5.0;
    }
    CHECK((*resampled)[0] == Resample(faded, 24000, Audio::kSampleRate));

    // Fades of 0 frames are the settings without fades.
    const auto none = RenderPlanes(source, {.trimStart = 10, .trimEnd = 60, .gainDb = 4.0});
    const auto zero = RenderPlanes(source, {.trimStart = 10, .trimEnd = 60, .gainDb = 4.0, .fadeIn = 0, .fadeOut = 0});
    REQUIRE(none.has_value());
    CHECK(*none == *zero);

    // The whole trim may fade; one frame more may not.
    CHECK(ValidateSettings({.trimStart = 10, .trimEnd = 60, .fadeIn = 20, .fadeOut = 30}, 100).has_value());
    for (const EditSettings& tooLong :
         {EditSettings{.trimStart = 10, .trimEnd = 60, .fadeIn = 21, .fadeOut = 30},
          EditSettings{.trimStart = 10, .trimEnd = 60, .fadeIn = 51},
          EditSettings{.trimStart = 10, .trimEnd = 60, .fadeOut = 51},
          EditSettings{.trimStart = 10, .trimEnd = 60, .fadeIn = SIZE_MAX, .fadeOut = 2}}) {
        const auto refused = ValidateSettings(tooLong, 100);
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().code == AudioEditorErrorCode::InvalidSettings);
        CHECK(RenderClip(source, tooLong, 1).error().code == AudioEditorErrorCode::InvalidSettings);
    }
}

// Judgment 3.
TEST_CASE("Three regions export as three clips that equal each region exported alone") {
    const Root batch = MakeRoot("three");
    const auto reports = ExportSounds(BatchIn(batch, ThreeRegions()));
    REQUIRE(reports.has_value());
    REQUIRE(reports->size() == 3);

    const std::string catalog = ReadText(batch.catalog);
    CHECK(catalog.find(kOtherEntry) != std::string::npos);
    const auto json = nlohmann::json::parse(catalog);
    std::set<std::string> ids;
    for (const auto& entry : json.at("assets")) ids.insert(entry.at("id").get<std::string>());
    CHECK(ids == std::set<std::string>{"game.ui.menu", "game.audio.magazine_out", "game.audio.magazine_in",
                                       "game.audio.bolt_pull"});
    // Each id is registered with its own region's path, and each report is its region's.
    for (std::size_t index = 0; index < 3; ++index) {
        const SoundRegion region = ThreeRegions()[index];
        CAPTURE(region.registration.id);
        bool found = false;
        for (const auto& entry : json.at("assets")) {
            if (entry.at("id") != region.registration.id) continue;
            found = true;
            CHECK(entry.at("type") == "sound");
            CHECK(entry.at("path") == region.registration.path);
        }
        CHECK(found);
        const ExportReport& report = (*reports)[index];
        CHECK(report.outputPath == Utf8(batch.assets) + "/" + region.registration.path);
        const auto clip = RenderClip(Decode("stereo_22050_pcm24"), region.settings, DitherSeed(region.registration.id));
        REQUIRE(clip.has_value());
        CHECK(report.frames == clip->Frames());
        CHECK_FALSE(report.previousPath.has_value());
    }

    for (std::size_t index = 0; index < 3; ++index) {
        const SoundRegion region = ThreeRegions()[index];
        CAPTURE(region.registration.id);
        CHECK((*reports)[index].added);
        const Root alone = MakeRoot("alone_" + std::to_string(index));
        REQUIRE(ExportSounds(BatchIn(alone, {region})).has_value());
        CHECK(ReadBytes(batch.assets / region.registration.path) == ReadBytes(alone.assets / region.registration.path));
    }

    // Exported again, every entry is an update and the other entry still has its bytes.
    const auto again = ExportSounds(BatchIn(batch, ThreeRegions()));
    REQUIRE(again.has_value());
    for (const ExportReport& report : *again) CHECK_FALSE(report.added);
    CHECK(ReadText(batch.catalog) == catalog);
}

// Judgment 4.
TEST_CASE("An export with a refused region writes nothing") {
    const DecodedWav source = Decode("stereo_22050_pcm24");
    std::vector<std::vector<SoundRegion>> refused;
    {
        auto regions = ThreeRegions();
        regions[2].registration.id = regions[0].registration.id; // one id twice
        refused.push_back(regions);
    }
    {
        auto regions = ThreeRegions();
        regions[2].registration.path = regions[1].registration.path; // one path twice
        refused.push_back(regions);
    }
    {
        auto regions = ThreeRegions();
        regions[1].registration.path = "./" + regions[0].registration.path; // not a content path (the catalog's rule)
        refused.push_back(regions);
    }
    {
        auto regions = ThreeRegions();
        regions[2].settings.trimEnd = source.Frames() + 1; // the last region does not fit the source
        refused.push_back(regions);
    }
    {
        auto regions = ThreeRegions();
        regions[2].settings.fadeIn = 1000000; // the last region's fades do not fit its trim
        refused.push_back(regions);
    }
    refused.push_back({}); // no region
    for (std::size_t index = 0; index < refused.size(); ++index) {
        CAPTURE(index);
        const Root root = MakeRoot("refused_" + std::to_string(index));
        const auto before = FilesUnder(root.assets);
        const auto catalog = ReadBytes(root.catalog);
        const auto exported = ExportSounds(BatchIn(root, refused[index]));
        REQUIRE_FALSE(exported.has_value());
        CHECK(FilesUnder(root.assets) == before);
        CHECK(ReadBytes(root.catalog) == catalog);
    }

    // Two paths the catalog takes as different but that name one file, through
    // a directory link: only the editor's own check sees it. Creating a link
    // may need a privilege (Windows), so without one this part is skipped.
    {
        const Root linked = MakeRoot("linked_path");
        std::filesystem::create_directories(linked.assets / "audio");
        std::error_code linkError;
        std::filesystem::create_directory_symlink(linked.assets / "audio", linked.assets / "alias", linkError);
        if (linkError) {
            MESSAGE("no directory link here, the linked-path part is skipped: " << linkError.message());
        } else {
            auto regions = ThreeRegions();
            regions[2].registration.path = "alias/magazine_out.wav"; // audio/magazine_out.wav through the link
            const auto catalog = ReadBytes(linked.catalog);
            const auto exported = ExportSounds(BatchIn(linked, regions));
            REQUIRE_FALSE(exported.has_value());
            CHECK(exported.error().message == "two regions write the same file");
            CHECK_FALSE(std::filesystem::exists(linked.assets / "audio" / "magazine_out.wav"));
            CHECK(ReadBytes(linked.catalog) == catalog);
        }
    }

    // Two paths that differ only in case: one file where the OS layer compares
    // names ignoring case (Windows). Elsewhere it compares bytes, so these are
    // two files there (on macOS's default volume, one: a known limit).
    {
        const Root cased = MakeRoot("case_path");
        auto regions = ThreeRegions();
        regions[2].registration.path = "audio/Magazine_Out.wav";
        const auto catalog = ReadBytes(cased.catalog);
        const auto exported = ExportSounds(BatchIn(cased, regions));
        const auto ignoresCase = Engine::Os::Files::SamePathComponent("Magazine_Out.wav", "magazine_out.wav");
        REQUIRE(ignoresCase.has_value());
        if (*ignoresCase) {
            REQUIRE_FALSE(exported.has_value());
            CHECK(exported.error().message == "two regions write the same file");
            CHECK(ReadBytes(cased.catalog) == catalog);
        } else {
            MESSAGE("case-only paths are two files under this platform's comparison");
        }
    }

    // A clip path that is the source: the source is never overwritten.
    const Root root = MakeRoot("source_path");
    const auto copy = root.assets / "source.wav";
    std::filesystem::copy_file(kSource, copy);
    auto regions = ThreeRegions();
    regions[2].registration.path = "source.wav";
    const auto catalog = ReadBytes(root.catalog);
    const auto exported = ExportSounds({Utf8(copy), Utf8(root.assets), Utf8(root.catalog), regions});
    REQUIRE_FALSE(exported.has_value());
    CHECK(exported.error().code == AudioEditorErrorCode::InvalidRegistration);
    CHECK(FilesUnder(root.assets) == std::set<std::string>{"asset_catalog.json", "source.wav"});
    CHECK(ReadBytes(root.catalog) == catalog);
    CHECK(ReadBytes(copy) == ReadBytes(kSource));
}

// Judgment 5.
TEST_CASE("A version 2 project keeps its regions and refuses other fields") {
    const auto root = TemporaryDirectory("regions_project");
    AudioProject project;
    project.batch = {Utf8(root / "sources" / "reload.wav"), Utf8(root / "assets"),
                     Utf8(root / "assets" / "asset_catalog.json"), ThreeRegions()};
    const std::string path = Utf8(root / "projects" / "reload.audio.json");
    REQUIRE(SaveProject(path, project).has_value());
    const auto loaded = LoadProject(path);
    REQUIRE(loaded.has_value());
    CHECK(*loaded == project);
    const std::string text = ReadText(root / "projects" / "reload.audio.json");
    CHECK(ProjectText(*loaded) == text);

    const auto json = nlohmann::ordered_json::parse(text);
    CHECK(json.at("version") == 2);
    const auto variant = [&json](auto change) {
        nlohmann::ordered_json copy = json;
        change(copy);
        return copy.dump(2);
    };
    const std::vector<std::string> broken{
        variant([](auto& j) { j["version"] = 3; }),
        variant([](auto& j) { j["extra"] = 1; }),
        variant([](auto& j) { j.erase("regions"); }),
        variant([](auto& j) { j["regions"] = nlohmann::ordered_json::array(); }),
        variant([](auto& j) { j["regions"] = nlohmann::ordered_json::object(); }),
        variant([](auto& j) { j["regions"][1]["extra"] = 1; }),
        variant([](auto& j) { j["regions"][1].erase("fade_out"); }),
        variant([](auto& j) { j["regions"][2]["fade_in"] = -1; }),
        variant([](auto& j) { j["regions"][0]["id"] = 7; }),
        variant([](auto& j) { j["id"] = "game.audio.reload"; }), // a version 1 field in version 2
    };
    // Version 1 has exactly its own fields too.
    const nlohmann::ordered_json v1{{"format", "gyo.audio_editor.project"},
                                    {"version", 1},
                                    {"source", "a.wav"},
                                    {"asset_root", "assets"},
                                    {"asset_catalog", "assets/asset_catalog.json"},
                                    {"id", "game.audio.a"},
                                    {"path", "audio/a.wav"},
                                    {"trim_start", 0},
                                    {"trim_end", nullptr},
                                    {"gain_db", 0.0}};
    REQUIRE(ParseProject(v1.dump()).has_value());
    std::vector<std::string> brokenV1;
    {
        auto extra = v1;
        extra["fade_in"] = 0;
        brokenV1.push_back(extra.dump());
        auto missing = v1;
        missing.erase("gain_db");
        brokenV1.push_back(missing.dump());
        auto regions = v1;
        regions["regions"] = nlohmann::ordered_json::array();
        brokenV1.push_back(regions.dump());
    }
    for (const std::string& candidate : broken) brokenV1.push_back(candidate);
    for (const std::string& candidate : brokenV1) {
        CAPTURE(candidate);
        const auto parsed = ParseProject(candidate);
        REQUIRE_FALSE(parsed.has_value());
        CHECK(parsed.error().code == AudioEditorErrorCode::InvalidProject);
    }

    // A project without a region is not written, since it would not be read.
    AudioProject empty = project;
    empty.batch.regions.clear();
    const std::string emptyPath = Utf8(root / "projects" / "empty.audio.json");
    const auto refused = SaveProject(emptyPath, empty);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == AudioEditorErrorCode::InvalidProject);
    CHECK_FALSE(std::filesystem::exists(root / "projects" / "empty.audio.json"));
}

// Judgment 6: the second of three regions, as the preview plays it, is the
// data chunk export wrote for it.
TEST_CASE("The preview of one region plays the bytes export writes for it") {
    const Root root = MakeRoot("preview");
    REQUIRE(ExportSounds(BatchIn(root, ThreeRegions())).has_value());
    const SoundRegion region = ThreeRegions()[1];
    const std::vector<std::int16_t> written = DataChunk(ReadBytes(root.assets / region.registration.path));

    const DecodedWav source = Decode("stereo_22050_pcm24");
    auto clip = RenderClip(source, region.settings, DitherSeed(region.registration.id));
    REQUIRE(clip.has_value());
    CHECK(clip->samples == written);

    PreviewRig rig = MakePreviewRig();
    const std::size_t frames = clip->Frames();
    rig.player->Play(std::move(clip).value(), kAnchor);
    REQUIRE_FALSE(rig.player->Waiting());
    const std::vector<std::int16_t> heard = RenderBlock(*rig.mixer, static_cast<std::uint32_t>(frames), kAnchor);
    CHECK(std::vector<std::int16_t>(heard.begin(), heard.begin() + static_cast<std::ptrdiff_t>(frames * 2)) ==
          written);
}

// Judgment 4, the write that fails: the clips written before it stay in the
// asset root, unregistered, and the error names them; the catalog is untouched.
TEST_CASE("A clip that cannot be written leaves the catalog untouched and names the clips written before it") {
    const Root root = MakeRoot("write_fails");
    std::filesystem::create_directories(root.assets / "audio" / "magazine_in.wav"); // a directory where clip 2 goes
    const auto catalog = ReadBytes(root.catalog);
    const auto exported = ExportSounds(BatchIn(root, ThreeRegions()));
    REQUIRE_FALSE(exported.has_value());
    CHECK(exported.error().code == AudioEditorErrorCode::WriteFailed);
    CHECK(exported.error().detail.find("written but not registered: " + Utf8(root.assets) + "/audio/magazine_out.wav") !=
          std::string::npos);
    CHECK(exported.error().detail.find("bolt_pull") == std::string::npos);
    CHECK(std::filesystem::exists(root.assets / "audio" / "magazine_out.wav"));
    CHECK_FALSE(std::filesystem::exists(root.assets / "audio" / "bolt_pull.wav"));
    CHECK(ReadBytes(root.catalog) == catalog);
}

TEST_CASE("A path one region gives up and another registers is not reported as left behind") {
    const Root root = MakeRoot("handover");
    // First magazine_out has x.wav; then magazine_out moves to y.wav and bolt_pull takes x.wav.
    REQUIRE(ExportSounds(BatchIn(root, {{{"game.audio.magazine_out", "audio/x.wav"}, {}}})).has_value());
    const auto moved = ExportSounds(BatchIn(root, {{{"game.audio.magazine_out", "audio/y.wav"}, {}},
                                                   {{"game.audio.bolt_pull", "audio/x.wav"}, {}}}));
    REQUIRE(moved.has_value());
    CHECK_FALSE((*moved)[0].previousPath.has_value());

    // Moved with nothing taking the old path, it is reported.
    const auto again = ExportSounds(BatchIn(root, {{{"game.audio.bolt_pull", "audio/z.wav"}, {}}}));
    REQUIRE(again.has_value());
    REQUIRE((*again)[0].previousPath.has_value());
    CHECK(*(*again)[0].previousPath == "audio/x.wav");
}
