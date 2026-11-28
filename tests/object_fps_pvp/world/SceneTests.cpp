// Scene files (world/Scene.hpp; interact-door declaration section 5; the data contract
// games/object_fps_pvp/docs/scene-format.zh-Hant.md), from outside, with two sets of the test's own that take scene
// objects ("crate" and "shelf"): what a scene file may not say, the entity an object becomes, the scene digest (only the
// objects of the sets the mode turns on), an arena whose scene does not read, and a Client furnished otherwise than the
// Match.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/world/Scene.hpp"

#include <algorithm>

using namespace object_fps_pvp::test;

namespace {

namespace Json = GYO::Engine::Json;

// What a crate or a shelf object puts on its entity: its size, read from its data.
struct Crate final {
    std::uint32_t size = 0;
    friend bool operator==(const Crate&, const Crate&) = default;
};
void HashValue(K::Hasher& h, const Crate& v) { HashValue(h, v.size); }
struct Shelf final {
    std::uint32_t size = 0;
};
void HashValue(K::Hasher& h, const Shelf& v) { HashValue(h, v.size); }

// A set whose scene objects are {"size": a positive whole number}: the size is its digest, and it puts Thing on the
// object's entity.
template <typename Thing>
PW::GameplaySet SizedSet(const std::string& name) {
    PW::GameplaySet set;
    set.name = name;
    set.build = [name](O::WorldSetup& setup) { setup.Registrar().Component<Thing>({.name = name}); };
    set.readScene = [](const K::EntityId entity, const Json::ObjectView& data, std::string& error) -> std::optional<PW::SetData> {
        std::uint64_t size = 0;
        if (const auto value = data.Find("size")) {
            if (const auto number = value->AsUint64()) size = *number;
        }
        if (size == 0) {
            error = "size is not a positive whole number";
            return std::nullopt;
        }
        const auto read = static_cast<std::uint32_t>(size);
        return PW::SetData{read, [entity, read](K::World& world) { world.Insert(entity, Thing{read}); }};
    };
    return set;
}

// The test's sets: crate (slot 0), shelf (slot 1), and one that takes no scene objects (slot 2).
const std::vector<PW::GameplaySet>& Sets() {
    static const std::vector<PW::GameplaySet> sets = [] {
        PW::GameplaySet plain;
        plain.name = "plain";
        plain.build = [](O::WorldSetup&) {};
        return std::vector<PW::GameplaySet>{SizedSet<Crate>("crate"), SizedSet<Shelf>("shelf"), plain};
    }();
    return sets;
}

std::string Object(const std::string& id, const std::string& set, const std::string& data) {
    return R"({"id": )" + id + R"(, "set": ")" + set + R"(", "data": )" + data + "}";
}

std::string SceneText(const std::string& objects, const std::string& id = "room", const std::string& version = "1") {
    return R"({"version": )" + version + R"(, "id": ")" + id + R"(", "objects": [)" + objects + "]}";
}

// The scene a crate of size 3 (id 1) and a shelf of size 2 (id 2) furnish, with the crate's size given.
std::string Room(const std::string& crate = "3", const std::string& shelf = "2") {
    return SceneText(Object("1", "crate", R"({"size": )" + crate + "}") + ", " + Object("2", "shelf", R"({"size": )" + shelf + "}"));
}

PW::SceneContent Parse(const std::string& text) {
    std::string error;
    auto scene = PW::ParseScene(text, Sets(), error);
    REQUIRE_MESSAGE(scene.has_value(), error);
    return std::move(*scene);
}

std::string Refused(const std::string& text) {
    std::string error;
    CHECK_FALSE(PW::ParseScene(text, Sets(), error).has_value());
    return error;
}

PW::SetsOn On(std::initializer_list<std::size_t> slots) {
    PW::SetsOn on;
    for (const std::size_t slot : slots) on.Add(slot);
    return on;
}

// The shipped content with the test's sets, two modes (core_pvp: no set; crates: crate on, shelf off) and arena 0
// furnished by the scene given.
std::shared_ptr<const PW::GameContent> Furnished(const std::string& scene) {
    auto copy = std::make_shared<PW::GameContent>(*ShippedContent());
    copy->sets = Sets();
    ModeFolder folder;
    folder.Modes({{"core.json", ModeText("core_pvp")}, {"crates.json", ModeText("crates", kShippedRules, R"({"crate": {}})")}});
    std::string error;
    auto modes = PW::LoadModes(folder.root.string(), copy->arenas, copy->combat, copy->sets, error);
    REQUIRE_MESSAGE(modes.has_value(), error);
    copy->modes = std::move(*modes);
    copy->scenes.assign(copy->arenas.size(), PW::SceneContent{});
    copy->scenes[0] = Parse(scene);
    return copy;
}

} // namespace

TEST_CASE("world: a scene file reads its objects in order, each with its set's reading of its data; an object's id names its entity") {
    const PW::SceneContent scene = Parse(Room());
    CHECK(scene.id == "room");
    REQUIRE(scene.objects.size() == 2);
    CHECK(scene.objects[0].id == 1);
    CHECK(scene.objects[0].slot == 0);
    CHECK(scene.objects[0].data.digest == 3);
    CHECK(scene.objects[1].slot == 1);
    CHECK(PW::SceneEntity(1) == 3'100'001);
    CHECK(PW::SceneEntity(PW::kMaxSceneObjectId) == 3'199'999);
    // Ids up to the last one read.
    CHECK(Parse(SceneText(Object(std::to_string(PW::kMaxSceneObjectId), "crate", R"({"size": 1})"))).objects.front().id == PW::kMaxSceneObjectId);
    CHECK(Parse(SceneText("")).objects.empty());
}

TEST_CASE("world: a scene file that says what it may not does not read, naming the object and what was wrong") {
    const auto says = [](const std::string& error, const std::string& what) {
        CHECK_MESSAGE(error.find(what) != std::string::npos, "[pvp-scene-read] expected '" << what << "', read: " << error);
    };
    says(Refused(SceneText("", "room", "2")), "unsupported scene version");
    says(Refused(SceneText("", "")), "id is 1 to 64 characters");
    says(Refused(SceneText("", std::string(65, 'r'))), "id is 1 to 64 characters");
    says(Refused(SceneText(Object("0", "crate", R"({"size": 1})"))), "object 0: id is not 1 to 99999");
    says(Refused(SceneText(Object(std::to_string(PW::kMaxSceneObjectId + 1), "crate", R"({"size": 1})"))), "id is not 1 to 99999");
    says(Refused(SceneText(Object("4", "crate", R"({"size": 1})") + ", " + Object("4", "shelf", R"({"size": 1})"))),
         "object 1 (id 4): id is another object's");
    says(Refused(SceneText(Object("1", "door", "{}"))), "object 0 (id 1): the game has no gameplay set 'door'");
    says(Refused(SceneText(Object("1", "plain", "{}"))), "the set 'plain' takes no scene objects");
    says(Refused(SceneText(Object("1", "crate", "3"))), "object 0 (id 1): data");
    says(Refused(Room("0")), "object 0 (id 1): set 'crate': size is not a positive whole number");
    says(Refused(R"({"version": 1, "id": "room"})"), "objects");
}

TEST_CASE("world: the scene digest counts the objects of the sets the mode turns on, by id, not the others nor the file's order") {
    const auto digest = [](const std::string& text, const PW::SetsOn& on) { return PW::SceneDigest(Parse(text), on, Sets()); };
    const std::uint64_t base = digest(Room(), On({0}));
    CHECK_MESSAGE(digest(Room("4"), On({0})) != base, "[pvp-scene-digest] an object of a set that is on");
    CHECK_MESSAGE(digest(Room("3", "5"), On({0})) == base, "[pvp-scene-digest-off] an object of a set that is off does not count");
    CHECK_MESSAGE(digest(Room("3", "5"), On({0, 1})) != digest(Room(), On({0, 1})), "[pvp-scene-digest] both on");
    // The same objects in another order are the same scene; another id is another.
    const std::string crate = Object("1", "crate", R"({"size": 3})");
    const std::string shelf = Object("2", "shelf", R"({"size": 2})");
    CHECK(digest(SceneText(shelf + ", " + crate), On({0, 1})) == digest(SceneText(crate + ", " + shelf), On({0, 1})));
    CHECK_MESSAGE(digest(SceneText(Object("7", "crate", R"({"size": 3})")), On({0})) != digest(SceneText(crate), On({0})),
                  "[pvp-scene-digest] an object's id");
    // Nothing on: the empty scene's digest, whatever the scene (the core PvP).
    CHECK(digest(Room("9", "9"), On({})) == PW::SceneDigest(PW::SceneContent{}, On({}), Sets()));
    CHECK(digest(Room(), On({})) != base);
}

TEST_CASE("world: an arena whose scene does not read or parse does not load; its scene's objects are read with the sets") {
    ContentFolder folder;
    const std::string shipped = ContentFolder::ReadText(std::filesystem::path(PVP_CONTENT_ROOT) / "maps" / "pvp_arena.json");
    const auto withScene = [&](const std::string& path) {
        std::string text = shipped;
        const std::size_t at = text.find("\"map\":");
        REQUIRE(at != std::string::npos);
        text.insert(at, "\"scene\": \"" + path + "\", ");
        folder.Arenas({{"pvp_arena.json", text}});
    };
    std::filesystem::create_directories(folder.root / "scenes");
    std::string error;
    withScene("scenes/room.json");
    CHECK_FALSE_MESSAGE(PW::LoadGameContent(folder.root.string(), error, Sets()).has_value(), "[pvp-scene-missing] a scene that is not there");
    CHECK_MESSAGE(error.find("pvp_arena.json: its scene:") != std::string::npos, error);
    folder.Write("scenes/room.json", Room("0"));
    CHECK_FALSE(PW::LoadGameContent(folder.root.string(), error, Sets()).has_value());
    CHECK_MESSAGE(error.find("scenes/room.json: object 0 (id 1): set 'crate': size is not a positive whole number") != std::string::npos, error);
    folder.Write("scenes/room.json", Room());
    const auto content = PW::LoadGameContent(folder.root.string(), error, Sets());
    REQUIRE_MESSAGE(content.has_value(), error);
    REQUIRE(content->scenes.size() == content->arenas.size());
    CHECK(content->scenes[0].id == "room");
    CHECK(content->scenes[0].objects.size() == 2);
    // An arena without a scene has none.
    folder.Arenas({{"pvp_arena.json", shipped}});
    const auto bare = PW::LoadGameContent(folder.root.string(), error, Sets());
    REQUIRE_MESSAGE(bare.has_value(), error);
    CHECK(bare->scenes[0].objects.empty());
}

TEST_CASE("world: a scene's objects are at every end with the same ids, before the first step, for the sets the mode turns on") {
    const auto content = Furnished(Room());
    Session crates(1, 4, 0, content, {}, 60, std::nullopt, content, 1);
    Session core(1, 4, 0, content, {}, 60, std::nullopt, content, 0);
    crates.Frames(60);
    core.Frames(60);
    for (K::World* world : {&crates.Match(), &crates.Client(0)}) {
        REQUIRE_MESSAGE(world->Get<Crate>(PW::SceneEntity(1)) != nullptr, "[pvp-scene-put] the crate at every end");
        CHECK(world->Get<Crate>(PW::SceneEntity(1))->size == 3);
        CHECK_MESSAGE(world->Get<Shelf>(PW::SceneEntity(2)) == nullptr, "[pvp-scene-put-off] the shelf's set is off");
    }
    CHECK_MESSAGE(crates.Match().Resource<PW::MatchInfo>()->sceneDigest == content->SceneDigest(1, 0), "[pvp-scene-info] the Match says its scene digest");
    CHECK(crates.Client(0).Resource<PW::ContentMismatch>()->kind == PW::MismatchKind::None);
    // The core PvP: no set on, nothing put, the empty scene's digest.
    CHECK(core.Match().Get<Crate>(PW::SceneEntity(1)) == nullptr);
    CHECK(core.Client(0).Get<Crate>(PW::SceneEntity(1)) == nullptr);
    CHECK_MESSAGE(core.Match().Resource<PW::MatchInfo>()->sceneDigest == PW::SceneDigest(PW::SceneContent{}, On({}), Sets()), "[pvp-scene-info] the empty one's");
}

TEST_CASE("world: a Client furnished otherwise than the Match, in a set the mode turns on, is a content mismatch (scene)") {
    const auto match = Furnished(Room());
    Session other(1, 4, 0, Furnished(Room("4")), {}, 60, std::nullopt, match, 1);
    other.Frames(90);
    const PW::ContentMismatch& mismatch = *other.Client(0).Resource<PW::ContentMismatch>();
    CHECK_MESSAGE(mismatch.kind == PW::MismatchKind::Scene, "[pvp-scene-mismatch] " << mismatch.reason);
    CHECK_MESSAGE(mismatch.reason.find("is furnished otherwise than the Match's (scene digest)") != std::string::npos, mismatch.reason);
    CHECK(mismatch.matchArena == match->arenas[0].id);
    // The shelf's set is off: its object differing is not another match.
    Session shelf(1, 4, 0, Furnished(Room("3", "5")), {}, 60, std::nullopt, match, 1);
    shelf.Frames(90);
    CHECK_MESSAGE(shelf.Client(0).Resource<PW::ContentMismatch>()->kind == PW::MismatchKind::None,
                  "[pvp-scene-digest-off] " << shelf.Client(0).Resource<PW::ContentMismatch>()->reason);
}
