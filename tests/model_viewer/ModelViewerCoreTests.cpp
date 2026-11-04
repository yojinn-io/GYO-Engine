#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/model_viewer/EventDocument.hpp"
#include "gyo/model_viewer/EventFiles.hpp"
#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/ModelSummary.hpp"

#include "engine/model/Animation.hpp"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

using namespace Gyo::Tools::ModelViewer;
namespace Model = Engine::Model;

namespace {

// A model built in code (no product's file): two nodes, one triangle, and two
// clips whose names sort the other way round from their order in the model.
Model::ModelAsset MakeModel() {
    Model::ModelAsset model;
    model.nodes.push_back({"root", std::nullopt, {{0, 0, 0}, {}, {1, 1, 1}}});
    model.nodes.push_back({"joint", 0, {{0, 1, 0}, {}, {1, 1, 1}}});
    model.materials.push_back({"skin", {0.5F, 0.25F, 1.0F, 1.0F}});
    Model::MeshPart part;
    part.name = "triangle";
    part.joints.push_back({1, {}});
    part.vertices = {{{0, 0, 0}, {0, 1, 0}, {0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
                     {{1, 0, 0}, {0, 1, 0}, {1, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
                     {{0, 0, 1}, {0, 1, 0}, {0, 1}, {0, 0, 0, 0}, {1, 0, 0, 0}}};
    part.indices = {0, 1, 2};
    model.meshes.push_back(part);
    Model::AnimationClip reload;
    reload.name = "Armature|Reload";
    reload.durationSeconds = 2.5;
    Model::NodeTrack track;
    track.nodeIndex = 1;
    track.translations = {{0, {0, 1, 0}}, {2.5, {0, 3, 0}}};
    reload.tracks.push_back(track);
    model.clips.push_back(reload);
    Model::AnimationClip idle;
    idle.name = "Armature|Idle";
    idle.durationSeconds = 1.0;
    model.clips.push_back(idle);
    REQUIRE(Model::ValidateModel(model));
    return model;
}

constexpr const char* kModelId = "game.model.rifle";

std::uint64_t Bits(const double value) { return std::bit_cast<std::uint64_t>(value); }

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

// A fresh empty directory under the system temporary directory, apart from
// other runs on the same host (a mutation run beside a normal build).
std::filesystem::path TemporaryDirectory(const std::string& name) {
    static const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("gyo_model_viewer_tests_" + run + "_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

// The layout the writer gives, written by hand: a file in it reads back byte for byte.
constexpr const char* kHandWritten = R"({
  "schema": "gyo.anim_events",
  "version": 1,
  "model_asset_id": "game.model.rifle",
  "clips": {
    "Armature|Reload": [
      { "seconds": 2.35, "event": "magazine_in" }
    ]
  }
}
)";

Model::AnimationEventSet Parse(const std::string& text) {
    auto parsed = Model::ParseAnimationEvents(text);
    REQUIRE(parsed);
    return std::move(parsed).value();
}

Model::AnimationEventSet OneEvent(const std::string& clip, const double seconds, const std::string& name) {
    return {kModelId, {{clip, {{seconds, name}}}}};
}

std::string RefusalDetail(const Model::AnimationEventSet& set, const Model::ModelAsset& model) {
    auto checked = CheckedText(set, model);
    REQUIRE_FALSE(checked);
    CHECK(checked.error().code == ModelViewerErrorCode::InvalidEvents);
    return checked.error().detail;
}

} // namespace

TEST_CASE("written events read back as the same events, every seconds value bit for bit") {
    const auto model = MakeModel();
    Model::AnimationEventSet set{kModelId, {}};
    // Clips out of name order; the edge moments of a clip; a value that needs 17 digits.
    set.tracks.push_back({"Armature|Reload",
                          {{0.0, "start"},
                           {0.1 + 0.2, "magazine_out"},
                           {0.1 + 0.2, "bolt"},
                           {1.0, "magazine_out"},
                           {2.5, "end"}}});
    set.tracks.push_back({"Armature|Idle", {{std::nextafter(1.0, 0.0), "blink"}}});
    auto text = CheckedText(set, model);
    REQUIRE(text);
    const auto parsed = Parse(*text);
    REQUIRE(parsed.tracks.size() == 2);
    CHECK(parsed.tracks[0].clip == "Armature|Idle");
    CHECK(parsed.tracks[1].clip == "Armature|Reload");
    CHECK(Bits(parsed.tracks[0].events[0].seconds) == Bits(std::nextafter(1.0, 0.0)));
    CHECK(Bits(parsed.tracks[1].events[1].seconds) == Bits(0.1 + 0.2));
    CHECK(parsed.tracks[1].events[2].name == "bolt");
    Model::AnimationEventSet byName{kModelId, {set.tracks[1], set.tracks[0]}};
    CHECK(SameEvents(parsed, byName));
    CHECK(Model::BindAnimationEvents(parsed, model));
}

TEST_CASE("events are the same only field by field, seconds bit for bit") {
    const auto base = OneEvent("Armature|Idle", 0.0, "a");
    CHECK(SameEvents(base, base));
    CHECK_FALSE(SameEvents(base, OneEvent("Armature|Idle", -0.0, "a")));
    CHECK_FALSE(SameEvents(base, OneEvent("Armature|Idle", 0.0, "b")));
    CHECK_FALSE(SameEvents(base, OneEvent("Armature|Reload", 0.0, "a")));
    CHECK_FALSE(SameEvents(base, {"game.model.other", base.tracks}));
    auto more = base;
    more.tracks[0].events.push_back({0.5, "b"});
    CHECK_FALSE(SameEvents(base, more));
    more = base;
    more.tracks.push_back({"Armature|Reload", {{0.0, "a"}}});
    CHECK_FALSE(SameEvents(base, more));
}

TEST_CASE("a hand-written file in the writer's layout is written back byte for byte") {
    const auto model = MakeModel();
    auto text = CheckedText(Parse(kHandWritten), model);
    REQUIRE(text);
    CHECK(*text == kHandWritten);
}

TEST_CASE("negative zero keeps its sign and a value that is not finite is refused by Engine") {
    const auto model = MakeModel();
    auto text = CheckedText(OneEvent("Armature|Idle", -0.0, "zero"), model);
    REQUIRE(text);
    CHECK(Bits(Parse(*text).tracks[0].events[0].seconds) == Bits(-0.0));
    RefusalDetail(OneEvent("Armature|Idle", std::numeric_limits<double>::quiet_NaN(), "nan"), model);
    RefusalDetail(OneEvent("Armature|Idle", std::numeric_limits<double>::infinity(), "inf"), model);
}

TEST_CASE("Engine decides what is refused; its path names the offending value") {
    const auto model = MakeModel();
    CHECK(RefusalDetail(OneEvent("Armature|Idle", 1.5, "late"), model).find("clips.\"Armature|Idle\"") !=
          std::string::npos);
    CHECK(RefusalDetail(OneEvent("Armature|Idle", -0.5, "early"), model).find("clips.\"Armature|Idle\"[0].seconds") !=
          std::string::npos);
    CHECK(RefusalDetail(OneEvent("Armature|Idle", 0.5, "Bad Name"), model).find("clips.\"Armature|Idle\"[0].event") !=
          std::string::npos);
    CHECK(RefusalDetail(OneEvent("Armature|Missing", 0.5, "gone"), model).find("clips.\"Armature|Missing\"") !=
          std::string::npos);
    Model::AnimationEventSet repeated{kModelId, {{"Armature|Idle", {{0.5, "twice"}, {0.5, "twice"}}}}};
    CHECK(RefusalDetail(repeated, model).find("clips.\"Armature|Idle\"[1]") != std::string::npos);
    Model::AnimationEventSet unsorted{kModelId, {{"Armature|Idle", {{0.6, "a"}, {0.5, "b"}}}}};
    CHECK(RefusalDetail(unsorted, model).find("clips.\"Armature|Idle\"[1].seconds") != std::string::npos);
    CHECK(RefusalDetail({kModelId, {}}, model).find("clips") != std::string::npos);
    CHECK(RefusalDetail({kModelId, {{"Armature|Idle", {}}}}, model).find("clips.\"Armature|Idle\"") !=
          std::string::npos);
    CHECK(RefusalDetail({"", {{"Armature|Idle", {{0.5, "a"}}}}}, model).find("model_asset_id") != std::string::npos);
}

TEST_CASE("edits keep a clip's events in order and drop a clip that loses its last event") {
    EventDocument document(OneEvent("Armature|Reload", 1.0, "middle"));
    CHECK(document.Add("Armature|Reload", {1.0, "tie"}) == 1); // after the equal moment
    CHECK(document.Add("Armature|Reload", {0.5, "first"}) == 0);
    CHECK(document.Add("Armature|Idle", {0.2, "blink"}) == 0);
    REQUIRE(document.Track("Armature|Idle"));
    // first 0.5, middle 1.0, tie 1.0 -> move first past both.
    CHECK(document.Move("Armature|Reload", 0, 2.0) == 2);
    const auto* reload = document.Track("Armature|Reload");
    REQUIRE(reload);
    CHECK(reload->events[0].name == "middle");
    CHECK(reload->events[1].name == "tie");
    CHECK(reload->events[2].name == "first");
    CHECK(reload->events[2].seconds == 2.0);
    document.Rename("Armature|Reload", 1, "renamed");
    CHECK(document.Track("Armature|Reload")->events[1].name == "renamed");
    CHECK(document.Move("Armature|Reload", 2, 0.0) == 0);
    CHECK(document.Track("Armature|Reload")->events[0].name == "first");
    const std::string clip = document.Track("Armature|Idle")->clip;
    document.Remove(document.Track("Armature|Idle")->clip, 0); // a view of the emptied track's own name
    CHECK_FALSE(document.Track(clip));
    CHECK(document.Set().tracks.size() == 1);
}

TEST_CASE("saving writes only the opened file and refuses a file changed since it was opened") {
    const auto model = MakeModel();
    const auto root = TemporaryDirectory("save");
    const auto events = root / "weapons" / "rifle.anim_events.json";
    const auto neighbour = root / "weapons" / "rifle.json";
    WriteFile(events, kHandWritten);
    WriteFile(neighbour, "{\"other\": true}\n");
    WriteFile(root / "asset_catalog.json", "{ \"version\": 1, \"assets\": [] }\n");
    const std::string eventsPath = Utf8Path(events);

    auto opened = OpenEvents(eventsPath, kModelId, model);
    REQUIRE(opened);
    opened->document.Move("Armature|Reload", 0, 1.25);
    auto saved = SaveEvents(eventsPath, opened->stamp, opened->document.Set(), model);
    REQUIRE(saved);
    CHECK(Parse(ReadFile(events)).tracks[0].events[0].seconds == 1.25);
    CHECK(ReadFile(neighbour) == "{\"other\": true}\n");
    CHECK(ReadFile(root / "asset_catalog.json") == "{ \"version\": 1, \"assets\": [] }\n");
    CHECK(std::distance(std::filesystem::directory_iterator(root / "weapons"), std::filesystem::directory_iterator{}) ==
          2);

    // What Engine refuses is not saved: the file keeps its bytes.
    const std::string before = ReadFile(events);
    opened->document.Move("Armature|Reload", 0, 9.0);
    auto refused = SaveEvents(eventsPath, *saved, opened->document.Set(), model);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == ModelViewerErrorCode::InvalidEvents);
    CHECK(ReadFile(events) == before);

    // Someone else changes the file: saving would overwrite that, so it is refused.
    opened->document.Move("Armature|Reload", 0, 1.5);
    const std::string external = std::string(kHandWritten).replace(std::string(kHandWritten).find("2.35"), 4, "2.40");
    WriteFile(events, external);
    auto stale = SaveEvents(eventsPath, *saved, opened->document.Set(), model);
    REQUIRE_FALSE(stale);
    CHECK(stale.error().code == ModelViewerErrorCode::ChangedOnDisk);
    CHECK(ReadFile(events) == external);
}

TEST_CASE("opening refuses events of another model and events Engine cannot bind") {
    const auto model = MakeModel();
    const auto root = TemporaryDirectory("open");
    const auto path = root / "events.json";
    WriteFile(path, kHandWritten);
    auto other = OpenEvents(Utf8Path(path), "game.model.pistol", model);
    REQUIRE_FALSE(other);
    CHECK(other.error().code == ModelViewerErrorCode::OtherModel);
    WriteFile(path, std::string(kHandWritten).replace(std::string(kHandWritten).find("2.35"), 4, "3.00"));
    auto beyond = OpenEvents(Utf8Path(path), kModelId, model);
    REQUIRE_FALSE(beyond);
    CHECK(beyond.error().code == ModelViewerErrorCode::InvalidEvents);
    auto missing = OpenEvents(Utf8Path(root / "missing.json"), kModelId, model);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == ModelViewerErrorCode::ReadFailed);
}

constexpr const char* kCatalog = R"({
  "version": 1,
  "assets": [
    { "id": "game.model.rifle", "type": "model", "path": "weapons/rifle.fbx" },
    { "id": "game.text.rifle", "type": "text", "path": "weapons/rifle.json" }
  ]
}
)";

TEST_CASE("a new events file is written and registered as text; a refused one writes nothing") {
    const auto model = MakeModel();
    const auto root = TemporaryDirectory("create");
    const auto catalog = root / "asset_catalog.json";
    WriteFile(catalog, kCatalog);
    WriteFile(root / "weapons" / "rifle.json", "{}\n");
    const auto events = OneEvent("Armature|Reload", 2.35, "magazine_in");
    NewEventsRequest request{Utf8Path(root), Utf8Path(catalog), "game.anim_events.rifle",
                             "weapons/rifle.anim_events.json"};

    // Refused before anything is written: an id already registered, a taken file, events Engine refuses.
    for (const auto& [id, path] : {std::pair{"game.text.rifle", "weapons/new.json"},
                                   std::pair{"game.anim_events.other", "weapons/rifle.json"}}) {
        auto refused = CreateEvents({request.assetRoot, request.catalogPath, id, path}, kModelId, events, model);
        REQUIRE_FALSE(refused);
        CHECK(ReadFile(catalog) == kCatalog);
    }
    auto taken = CreateEvents({request.assetRoot, request.catalogPath, "game.anim_events.other", "weapons/rifle.json"},
                              kModelId, events, model);
    CHECK(taken.error().code == ModelViewerErrorCode::InvalidRegistration); // the catalog already names that path
    auto invalid = CreateEvents(request, kModelId, OneEvent("Armature|Reload", 9.0, "late"), model);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == ModelViewerErrorCode::InvalidEvents);
    CHECK_FALSE(std::filesystem::exists(root / "weapons" / "rifle.anim_events.json"));
    CHECK(ReadFile(catalog) == kCatalog);
    // An unregistered file already at the path is not replaced.
    WriteFile(root / "weapons" / "stray.json", "stray\n");
    auto stray = CreateEvents({request.assetRoot, request.catalogPath, "game.anim_events.stray", "weapons/stray.json"},
                              kModelId, events, model);
    REQUIRE_FALSE(stray);
    CHECK(stray.error().code == ModelViewerErrorCode::AlreadyExists);
    CHECK(ReadFile(root / "weapons" / "stray.json") == "stray\n");
    CHECK(ReadFile(catalog) == kCatalog);

    auto otherModel = CreateEvents(request, "game.model.pistol", events, model);
    REQUIRE_FALSE(otherModel);
    CHECK(otherModel.error().code == ModelViewerErrorCode::OtherModel);
    CHECK(ReadFile(catalog) == kCatalog);

    auto created = CreateEvents(request, kModelId, events, model);
    REQUIRE(created);
    CHECK(Parse(ReadFile(root / "weapons" / "rifle.anim_events.json")).tracks[0].events[0].name == "magazine_in");
    const std::string after = ReadFile(catalog);
    CHECK(after.find("\"id\": \"game.anim_events.rifle\", \"type\": \"text\", \"path\": "
                     "\"weapons/rifle.anim_events.json\"") != std::string::npos);
    CHECK(after.find("{ \"id\": \"game.text.rifle\", \"type\": \"text\", \"path\": \"weapons/rifle.json\" }") !=
          std::string::npos);
    CHECK(ReadFile(root / "weapons" / "rifle.json") == "{}\n");
}

TEST_CASE("the description lists what Engine loaded") {
    CHECK(DescribeModel(MakeModel()) ==
          "nodes: 2\n"
          "  root\n"
          "    joint\n"
          "meshes: 1\n"
          "  triangle: 3 vertices, 1 triangles, 1 joints, node root, material skin\n"
          "materials: 1\n"
          "  skin: base color 0.500 0.250 1.000 1.000\n"
          "clips: 2\n"
          "  Armature|Reload: 2.500 s, 1 tracks\n"
          "  Armature|Idle: 1.000 s, 0 tracks\n");
}

TEST_CASE("mounted content lists models and the events files that name them") {
    const auto root = TemporaryDirectory("content");
    const auto catalog = root / "asset_catalog.json";
    WriteFile(catalog, R"({
  "version": 1,
  "assets": [
    { "id": "game.model.rifle", "type": "model", "path": "rifle.fbx" },
    { "id": "game.anim_events.rifle", "type": "text", "path": "rifle.anim_events.json" },
    { "id": "game.anim_events.pistol", "type": "text", "path": "pistol.anim_events.json" },
    { "id": "game.anim_events.broken", "type": "text", "path": "broken.anim_events.json" },
    { "id": "game.text.notes", "type": "text", "path": "notes.json" }
  ]
}
)");
    WriteFile(root / "rifle.fbx", "not an fbx");
    WriteFile(root / "rifle.anim_events.json", kHandWritten);
    WriteFile(root / "pistol.anim_events.json",
              std::string(kHandWritten).replace(std::string(kHandWritten).find("rifle"), 5, "pistol"));
    WriteFile(root / "broken.anim_events.json", "{ \"schema\": \"gyo.anim_events\", \"version\": 2 }\n");
    WriteFile(root / "notes.json", "{}\n");
    auto content = ModelContent::Mount(root, catalog);
    REQUIRE(content);
    REQUIRE(content->Models().size() == 1);
    CHECK(content->Models()[0]->id == "game.model.rifle");

    auto candidates = content->EventsFor("game.model.rifle");
    REQUIRE(candidates);
    REQUIRE(candidates->size() == 2);
    // In id order; the broken file is listed with Engine's reason, the pistol's is not listed.
    CHECK((*candidates)[0].assetId == "game.anim_events.broken");
    CHECK_FALSE((*candidates)[0].events);
    CHECK((*candidates)[0].refusal.find("InvalidAnimationEvents") != std::string::npos);
    CHECK((*candidates)[1].assetId == "game.anim_events.rifle");
    CHECK((*candidates)[1].events);

    auto model = content->LoadModel("game.model.rifle");
    REQUIRE_FALSE(model);
    CHECK(model.error().code == ModelViewerErrorCode::InvalidModel);
    auto notModel = content->LoadModel("game.text.notes");
    REQUIRE_FALSE(notModel);
    CHECK(notModel.error().code == ModelViewerErrorCode::UnknownAsset);
    auto unknown = content->ReadText("game.model.rifle");
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error().code == ModelViewerErrorCode::UnknownAsset);

    auto missing = ModelContent::Mount(root / "nowhere", std::nullopt);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == ModelViewerErrorCode::InvalidCatalog);
}

TEST_CASE("an edit changes only the event it edits") {
    const auto model = MakeModel();
    const auto root = TemporaryDirectory("one_edit");
    const auto path = root / "events.json";
    Model::AnimationEventSet set{kModelId, {}};
    set.tracks.push_back({"Armature|Idle", {{0.1, "blink"}, {1.0 / 3.0, "look"}}});
    set.tracks.push_back({"Armature|Reload", {{0.1 + 0.2, "magazine_out"}, {2.35, "magazine_in"}, {2.5, "end"}}});
    auto text = CheckedText(set, model);
    REQUIRE(text);
    WriteFile(path, *text);
    auto opened = OpenEvents(Utf8Path(path), kModelId, model);
    REQUIRE(opened);
    opened->document.Move("Armature|Reload", 1, 2.0);
    REQUIRE(SaveEvents(Utf8Path(path), opened->stamp, opened->document.Set(), model));
    auto expected = set;
    expected.tracks[1].events[1].seconds = 2.0;
    CHECK(SameEvents(Parse(ReadFile(path)), expected));
}

TEST_CASE("a catalog that cannot be written leaves the new file unregistered and named") {
    const auto model = MakeModel();
    const auto root = TemporaryDirectory("catalog_write");
    const auto catalog = root / "asset_catalog.json";
    WriteFile(catalog, kCatalog);
    std::filesystem::create_directories(root / "weapons");
    // The new file goes into weapons/; the catalog's directory takes no new file, so its replace fails.
    std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    // Where a read-only directory still takes files (Windows ignores these bits; so does a root user), the
    // failure cannot be made here: say so instead of passing silently.
    if (std::ofstream(root / "probe.tmp")) {
        std::filesystem::permissions(root, std::filesystem::perms::owner_all);
        MESSAGE("skipped: this host writes into a read-only directory");
        return;
    }
    auto created = CreateEvents({Utf8Path(root), Utf8Path(catalog), "game.anim_events.rifle", "weapons/new.json"},
                                kModelId, OneEvent("Armature|Reload", 2.35, "magazine_in"), model);
    std::filesystem::permissions(root, std::filesystem::perms::owner_all);
    REQUIRE_FALSE(created);
    CHECK(created.error().code == ModelViewerErrorCode::WriteFailed);
    CHECK(created.error().detail.find("written but not registered") != std::string::npos);
    CHECK(created.error().detail.find("weapons/new.json") != std::string::npos);
    CHECK(std::filesystem::exists(root / "weapons" / "new.json"));
    CHECK(ReadFile(catalog) == kCatalog);
}

TEST_CASE("a model loads through Engine's ufbx loader from the mounted content") {
    const auto root = TemporaryDirectory("fixture");
    std::filesystem::copy_file(std::filesystem::path(GYO_MODEL_VIEWER_FIXTURES) / "animated_triangle.fbx",
                               root / "triangle.fbx");
    WriteFile(root / "asset_catalog.json",
              "{ \"version\": 1, \"assets\": [ { \"id\": \"game.model.triangle\", \"type\": \"model\", "
              "\"path\": \"triangle.fbx\" } ] }\n");
    auto content = ModelContent::Mount(root, root / "asset_catalog.json");
    REQUIRE(content);
    auto loaded = content->LoadModel("game.model.triangle");
    REQUIRE(loaded);
    REQUIRE(loaded->model);
    REQUIRE(loaded->model->clips.size() == 1);
    CHECK(loaded->model->clips[0].name == "Take");
    CHECK(loaded->model->clips[0].durationSeconds == doctest::Approx(2.0 / 30));
    CHECK(DescribeModel(*loaded->model).find("  Take: 0.067 s, 1 tracks\n") != std::string::npos);
}
