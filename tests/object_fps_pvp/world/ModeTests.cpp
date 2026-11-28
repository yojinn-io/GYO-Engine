// The game's modes (world/Modes.hpp; mode-config declaration, section 3): the shipped core PvP mode, what a mode file
// may not say, and the rules digest; the character's and the arena's digests, and the arena spawns' room for the
// character (arena-rules-split declaration 3.6, 5). A Client whose mode is not the Match's: WorldTests.cpp (its Session).
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/character/CharacterRig.hpp"
#include "object_fps_pvp/world/Modes.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>


using namespace object_fps_pvp::test;
namespace sim = object_fps_pvp::sim;

namespace {

std::optional<std::vector<PW::ModeContent>> Load(const ModeFolder& folder, std::string& error) {
    const auto& content = *ShippedContent();
    return PW::LoadModes(folder.root.string(), content.arenas, content.combat, PW::GameSets(), error);
}


} // namespace

TEST_CASE("world: the shipped modes: core PvP comes first, plays every arena with the shipped movement rules") {
    const PW::GameContent& content = *ShippedContent();
    REQUIRE_FALSE(content.modes.empty());
    const PW::ModeContent& core = content.modes.front();
    CHECK(core.id == "core_pvp");
    CHECK(core.name == "核心 PvP");
    for (const auto& arena : content.arenas) CHECK(core.Plays(arena.id));
    CHECK(core.rulesDigest != 0);
    CHECK(content.FindMode("core_pvp") == std::optional<std::size_t>{0});
    CHECK_FALSE(content.FindMode("no_such_mode").has_value());
}

TEST_CASE("world: a mode file reads; a second mode with other rules has another digest, the same values the same one") {
    ModeFolder folder;
    folder.Write("rules/slow.json", R"({"version": 3, "movement_speed": 3, "jump_height": 0.6, "gravity": 18, "crouch_speed_scale": 0.25,)"
                                     R"( "sprint_speed_scale": 1.0, "hold_jump": true})");
    // The same values as core_pvp written otherwise (CRLF, other spaces): the digest does not see the bytes.
    folder.Modes({{"a.json", ModeText("core_pvp")},
                  {"b.json", "{\r\n  \"arenas\": [],\r\n \"sets\": {},  \"rules\": {\"damage\": \"rules/damage_rules.json\", \"movement\": \"rules/movement_rules.json\"},\r\n"
                             "\"version\": 1, \"id\": \"copy\", \"name\": \"N\"}\r\n"},
                  {"c.json", ModeText("slow", R"({"movement": "rules/slow.json", "damage": "rules/damage_rules.json"})", "{}", R"(["pvp_training_v1"])")}});
    std::string error;
    const auto modes = Load(folder, error);
    REQUIRE_MESSAGE(modes.has_value(), error);
    REQUIRE(modes->size() == 3);
    CHECK(modes->at(0).rulesDigest == ShippedContent()->modes.front().rulesDigest);
    // Another id is another mode: its digest differs (the id is in it), the rule values are the same.
    CHECK(modes->at(1).movement == modes->at(0).movement);
    CHECK(modes->at(1).damage == modes->at(0).damage);
    CHECK(modes->at(1).rulesDigest != modes->at(0).rulesDigest);
    CHECK(modes->at(2).movement.crouchSpeedScale == 0.25F);
    CHECK(modes->at(2).Plays("pvp_training_v1"));
    CHECK_FALSE(modes->at(2).Plays("pvp_corners_v1"));
    CHECK(modes->at(2).rulesDigest != modes->at(0).rulesDigest);
}

TEST_CASE("world: the rules digest follows each rule value: the movement, damage and combat rules") {
    // One mode id, read with one rule value changed at a time: each gives another digest.
    const auto& content = *ShippedContent();
    const std::string shippedMovement = R"({"version": 3, "movement_speed": 3, "jump_height": 0.6, "gravity": 18, "crouch_speed_scale": 0.5,)"
                                        R"( "sprint_speed_scale": 1.0, "hold_jump": true})";
    const std::string shippedDamage = R"({"version": 1, "fall_damage": {"safe_height": 4.0, "points_per_metre": 15}})";
    const auto digestOf = [&](const std::string& movement, const std::optional<std::string>& damage, const sim::CombatRules& combat) {
        ModeFolder folder;
        folder.Write("rules/movement.json", movement);
        std::string rules = R"({"movement": "rules/movement.json")";
        if (damage) {
            folder.Write("rules/damage.json", *damage);
            rules += R"(, "damage": "rules/damage.json")";
        }
        folder.Modes({{"a.json", ModeText("same", rules + "}")}});
        std::string error;
        const auto modes = PW::LoadModes(folder.root.string(), content.arenas, combat, PW::GameSets(), error);
        REQUIRE_MESSAGE(modes.has_value(), error);
        return modes->front().rulesDigest;
    };
    const auto with = [](std::string text, const std::string& from, const std::string& to) {
        text.replace(text.find(from), from.size(), to);
        return text;
    };
    const std::uint64_t base = digestOf(shippedMovement, shippedDamage, content.combat);
    CHECK(digestOf(shippedMovement, shippedDamage, content.combat) == base);
    CHECK_MESSAGE(digestOf(with(shippedMovement, "\"crouch_speed_scale\": 0.5", "\"crouch_speed_scale\": 0.4"), shippedDamage, content.combat) != base,
                  "rulesDigest: the movement rules' crouch speed scale");
    CHECK_MESSAGE(digestOf(with(shippedMovement, "\"movement_speed\": 3", "\"movement_speed\": 3.5"), shippedDamage, content.combat) != base,
                  "[pvp-rules-digest-speed] rulesDigest: the movement speed");
    CHECK_MESSAGE(digestOf(with(shippedMovement, "\"jump_height\": 0.6", "\"jump_height\": 0.7"), shippedDamage, content.combat) != base,
                  "[pvp-rules-digest-jump] rulesDigest: the jump height");
    CHECK_MESSAGE(digestOf(with(shippedMovement, "\"gravity\": 18", "\"gravity\": 17"), shippedDamage, content.combat) != base,
                  "[pvp-rules-digest-gravity] rulesDigest: the gravity");
    CHECK_MESSAGE(digestOf(shippedMovement, with(shippedDamage, "4.0", "4.5"), content.combat) != base,
                  "[pvp-rules-digest-fall] rulesDigest: the fall damage's safe height");
    CHECK_MESSAGE(digestOf(shippedMovement, with(shippedDamage, "15}", "16}"), content.combat) != base,
                  "[pvp-rules-digest-fall] rulesDigest: the fall damage's points");
    CHECK_MESSAGE(digestOf(shippedMovement, std::nullopt, content.combat) != base, "[pvp-rules-digest-fall] rulesDigest: no damage rules");
    sim::CombatRules combat = content.combat;
    combat.legPermille += 1;
    CHECK_MESSAGE(digestOf(shippedMovement, shippedDamage, combat) != base, "rulesDigest: the combat rules");
    // The shipped core PvP is these tables.
    std::string error;
    const auto movement = sim::ParseMovementRules(shippedMovement, error);
    REQUIRE_MESSAGE(movement.has_value(), error);
    CHECK(content.modes.front().movement == *movement);
}

TEST_CASE("world: a mode without damage rules plays with none; a mode's damage rules are read from their file") {
    ModeFolder folder;
    folder.Write("rules/soft.json", R"({"version": 1, "fall_damage": {"safe_height": 2.5, "points_per_metre": 40}})");
    folder.Modes({{"a.json", ModeText("none", R"({"movement": "rules/movement_rules.json"})")},
                  {"b.json", ModeText("soft", R"({"movement": "rules/movement_rules.json", "damage": "rules/soft.json"})")}});
    std::string error;
    const auto modes = Load(folder, error);
    REQUIRE_MESSAGE(modes.has_value(), error);
    CHECK_MESSAGE(!modes->at(0).damage.fall.has_value(), "[pvp-mode-no-damage] no damage rules: a landing never hurts");
    REQUIRE_MESSAGE(modes->at(1).damage.fall.has_value(), "[pvp-mode-damage-read]");
    CHECK(modes->at(1).damage.fall->safeHeight == 2.5F);
    CHECK(modes->at(1).damage.fall->milliPointsPerMetre == 40'000U);
    // The shipped core PvP's: as every arena of version 4 carried.
    REQUIRE(ShippedContent()->modes.front().damage.fall.has_value());
    CHECK(ShippedContent()->modes.front().damage.fall->safeHeight == 4.0F);
}

TEST_CASE("world: the character digest follows the body model's id, the body, the skeleton, the clips, the animation rules and the hit shapes") {
    // The user's decision U1 (mode-config declaration 11.7). One value changed at a time: each gives another digest.
    const auto& shipped = *ShippedContent()->character;
    const std::uint64_t base = PW::CharacterDigest(shipped);
    CHECK(base == ShippedContent()->characterDigest);
    CHECK(PW::CharacterDigest(shipped) == base);
    const auto changed = [&](const auto& change) {
        object_fps_pvp::character::CharacterRig rig = shipped;
        change(rig);
        return PW::CharacterDigest(rig);
    };
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodyAssetId += "_other"; }) != base, "characterDigest: the body model's id");
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodySize.height += 0.01F; }) != base, "[pvp-character-digest-body] the body's height");
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodySize.radius += 0.01F; }) != base, "[pvp-character-digest-body] the body's radius");
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodySize.eyeHeight += 0.01F; }) != base, "[pvp-character-digest-body] the eye's height");
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodySize.crouchHeight += 0.01F; }) != base, "[pvp-character-digest-body] the crouched body");
    CHECK_MESSAGE(changed([](auto& rig) { rig.bodySize.crouchEyeHeight += 0.01F; }) != base, "[pvp-character-digest-body] the crouched eye");
    CHECK_MESSAGE(changed([](auto& rig) {
                      auto body = std::make_shared<GYO::Engine::Model::ModelData>(*rig.body);
                      body->nodes.back().rest.translation.y += 0.01F;
                      rig.body = body;
                  }) != base,
                  "characterDigest: the skeleton");
    CHECK_MESSAGE(changed([](auto& rig) {
                      for (auto& clip : rig.clips) {
                          for (auto& track : clip.tracks) {
                              if (!track.rotation.values.empty()) {
                                  track.rotation.values.front().x += 0.01F;
                                  return;
                              }
                          }
                      }
                  }) != base,
                  "characterDigest: the clips");
    CHECK_MESSAGE(changed([](auto& rig) { rig.rules.walkCycleDistance += 0.1; }) != base, "characterDigest: the animation rules");
    CHECK_MESSAGE(changed([](auto& rig) { rig.hitShapes.back().radius += 0.01F; }) != base, "characterDigest: the hit shapes");
}

TEST_CASE("world: a mode that names an unknown rule kind, set or arena, no movement rules, or another mode's id does not load") {
    const auto refused = [](const std::vector<std::pair<std::string, std::string>>& files, const std::string& expected) {
        ModeFolder folder;
        folder.Modes(files);
        std::string error;
        const auto modes = Load(folder, error);
        CHECK_FALSE_MESSAGE(modes.has_value(), "refused: " << expected);
        CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
    };
    refused({{"a.json", ModeText("a", R"({"movement": "rules/movement_rules.json", "weapons": "rules/movement_rules.json"})")}},
            "no rule kind 'weapons'");
    refused({{"a.json", ModeText("a", "{}")}}, "no movement rules");
    refused({{"a.json", ModeText("a", R"({"movement": "rules/movement_rules.json"})", R"({"doors": {}})")}}, "no gameplay set 'doors'");
    refused({{"a.json", ModeText("a", R"({"movement": "rules/movement_rules.json"})", "{}", R"(["no_such_arena"])")}},
            "no installed arena has the id 'no_such_arena'");
    refused({{"a.json", ModeText("same")}, {"b.json", ModeText("same")}}, "another installed mode has the id 'same'");
    refused({{"a.json", ModeText(std::string(65, 'a'))}}, "id is 1 to 64 characters");
    refused({{"a.json", ModeText("a", R"({"movement": "../rules/movement_rules.json"})")}}, "not a path inside the content");
    refused({{"a.json", ModeText("a", R"({"movement": "rules/missing.json"})")}}, "missing.json");
    refused({{"a.json", ModeText("a", R"({"movement": "rules/movement_rules.json", "damage": "rules/movement_rules.json"})")}},
            "rules/movement_rules.json: unsupported version");
}

TEST_CASE("world: which arenas a mode plays is one question to the content: every choice asks GameContent::Plays") {
    ModeFolder folder;
    folder.Modes({{"a.json", ModeText("all")}, {"b.json", ModeText("one", R"({"movement": "rules/movement_rules.json"})", "{}", R"(["pvp_corners_v1"])")}});
    std::string error;
    auto modes = Load(folder, error);
    REQUIRE_MESSAGE(modes.has_value(), error);
    auto content = std::make_shared<PW::GameContent>(*ShippedContent());
    content->modes = std::move(*modes);
    const auto training = content->FindArena("pvp_training_v1");
    const auto corners = content->FindArena("pvp_corners_v1");
    REQUIRE((training && corners));
    CHECK(content->Plays(0, *training));
    CHECK(content->Plays(0, *corners));
    CHECK_FALSE_MESSAGE(content->Plays(1, *training), "[pvp-mode-plays] a mode that lists its arenas plays only them");
    CHECK(content->Plays(1, *corners));
    CHECK_FALSE(content->FindArena("no_such_arena").has_value());
}

TEST_CASE("world: the arena digest follows the arena's own values -- map, bounds, floor, kill height, spawns -- not its id or scene") {
    const auto& content = *ShippedContent();
    const std::uint64_t base = PW::ArenaDigest(content.arenas[0]);
    const auto changed = [&](const auto& change) {
        sim::ArenaContent copy = content.arenas[0];
        change(copy);
        return PW::ArenaDigest(copy);
    };
    CHECK(changed([](sim::ArenaContent&) {}) == base);
    CHECK(PW::ArenaDigest(content.arenas[1]) != base);
    CHECK_MESSAGE(changed([](auto& a) { a.map += "_other"; }) != base, "[pvp-arena-digest] the map");
    CHECK_MESSAGE(changed([](auto& a) { a.width += 1.0F; }) != base, "[pvp-arena-digest] the width");
    CHECK_MESSAGE(changed([](auto& a) { a.depth += 1.0F; }) != base, "[pvp-arena-digest] the depth");
    CHECK_MESSAGE(changed([](auto& a) { a.floor.reset(); }) != base, "[pvp-arena-digest] no floor");
    CHECK_MESSAGE(changed([](auto& a) { a.floor = 0.5F; }) != base, "[pvp-arena-digest] the floor's height");
    CHECK_MESSAGE(changed([](auto& a) { a.killHeight -= 1.0F; }) != base, "[pvp-arena-digest] the kill height");
    CHECK_MESSAGE(changed([](auto& a) { a.spawns[1].position.z += 1.0F; }) != base, "[pvp-arena-digest] a spawn's position");
    CHECK_MESSAGE(changed([](auto& a) { a.spawns[1].yaw += 0.1F; }) != base, "[pvp-arena-digest] a spawn's yaw");
    CHECK_MESSAGE(changed([](auto& a) { a.spawns.pop_back(); }) != base, "[pvp-arena-digest] the spawns");
    // Its id is compared on its own; its scene's content is (the scene's own comparison).
    CHECK(changed([](auto& a) { a.id += "_other"; }) == base);
    CHECK(changed([](auto& a) { a.scene = "scenes/other.json"; }) == base);
}

TEST_CASE("world: a Client whose arena has another spawn than the Match's, with the same id and walls, is a content mismatch (arena data)") {
    auto other = std::make_shared<PW::GameContent>(*ShippedContent());
    other->arenas[0].spawns[1].position.z += 1.0F;
    Session session(1, 4, 0, other);
    session.Frames(90);
    const PW::ContentMismatch& mismatch = *session.Client(0).Resource<PW::ContentMismatch>();
    CHECK_MESSAGE(mismatch.kind == PW::MismatchKind::ArenaData, "[pvp-arena-data-mismatch] " << mismatch.reason);
    CHECK_MESSAGE(mismatch.reason.find("arena 'pvp_training_v1' differs from the Match's (arena digest") != std::string::npos, mismatch.reason);
    CHECK(mismatch.matchArena == "pvp_training_v1");
}

namespace {

// A content root of a test's own: the shipped character (its files and models), the shipped rules, one mode of its own
// and the arenas it writes (none of their maps: LoadGameContent does not read them).
struct ContentFolder {
    std::filesystem::path root;
    ContentFolder() {
        std::random_device random;
        root = std::filesystem::temp_directory_path() / ("gyo_pvp_content_" + std::to_string(random()));
        const std::filesystem::path shipped(PVP_CONTENT_ROOT);
        std::filesystem::create_directories(root / "maps");
        std::filesystem::create_directories(root / "modes");
        for (const char* folder : {"characters", "animations", "rules"}) {
            std::filesystem::copy(shipped / folder, root / folder, std::filesystem::copy_options::recursive);
        }
        std::filesystem::copy_file(shipped / "asset_catalog.json", root / "asset_catalog.json");
        Write("modes/modes.json", R"({"version": 1, "modes": ["a.json"]})");
        Write("modes/a.json", ModeText("a"));
        Arenas({{"pvp_arena.json", ReadText(shipped / "maps" / "pvp_arena.json")}});
    }
    ~ContentFolder() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    ContentFolder(const ContentFolder&) = delete;
    ContentFolder& operator=(const ContentFolder&) = delete;
    static std::string ReadText(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    }
    void Write(const std::string& path, const std::string& text) const {
        std::ofstream out(root / path, std::ios::binary);
        out << text;
    }
    // maps/arenas.json listing the files, each written as given.
    void Arenas(const std::vector<std::pair<std::string, std::string>>& files) const {
        std::string list = R"({"version": 1, "arenas": [)";
        for (std::size_t i = 0; i < files.size(); ++i) {
            list += (i == 0 ? "\"" : ", \"") + files[i].first + "\"";
            Write("maps/" + files[i].first, files[i].second);
        }
        Write("maps/arenas.json", list + "]}");
    }
    // characters/player/character.json with its body (and version) replaced.
    void Character(const std::string& version, const std::string& body) const {
        std::string text = ReadText(root / "characters" / "player" / "character.json");
        text.replace(text.find("\"version\": 2"), std::string("\"version\": 2").size(), "\"version\": " + version);
        const std::size_t at = text.find("\"body\":");
        text.replace(at, text.find('}', at) + 1 - at, "\"body\": " + body);
        Write("characters/player/character.json", text);
    }
};

} // namespace

TEST_CASE("world: the character's body is read from its file into the rig: the body, the model's scale; version 1 is refused") {
    ContentFolder folder;
    std::string error;
    const auto shipped = PW::LoadGameContent(folder.root.string(), error);
    REQUIRE_MESSAGE(shipped.has_value(), error);
    CHECK(shipped->character->bodySize == ShippedContent()->character->bodySize);
    CHECK(shipped->character->scale == ShippedContent()->character->scale);
    folder.Character("2", R"({"height": 2.7, "radius": 0.3, "eye_height": 2.4, "crouch_height": 1.5, "crouch_eye_height": 1.3})");
    const auto tall = PW::LoadGameContent(folder.root.string(), error);
    REQUIRE_MESSAGE(tall.has_value(), error);
    const object_fps_pvp::sim::BodySize& body = tall->character->bodySize;
    CHECK_MESSAGE(body.height == 2.7F, "[pvp-character-body-read] " << body.height);
    CHECK(body.radius == 0.3F);
    CHECK(body.eyeHeight == 2.4F);
    CHECK(body.crouchHeight == 1.5F);
    CHECK(body.crouchEyeHeight == 1.3F);
    // The model is scaled to the body's height: 1.5 times the shipped scale.
    CHECK_MESSAGE(tall->character->scale == doctest::Approx(ShippedContent()->character->scale * 1.5F), "[pvp-character-scale] " << tall->character->scale);
    CHECK(tall->characterDigest != ShippedContent()->characterDigest);
    // A body the reader refuses, and a character file of version 1 (no body), do not load.
    folder.Character("2", R"({"eye_height": 2})");
    CHECK_FALSE(PW::LoadGameContent(folder.root.string(), error).has_value());
    CHECK_MESSAGE(error.find("an eye is above the top of its body") != std::string::npos, error);
    folder.Character("1", R"({})");
    CHECK_FALSE_MESSAGE(PW::LoadGameContent(folder.root.string(), error).has_value(), "[pvp-character-version] version 1 loaded");
    CHECK_MESSAGE(error.find("character.json: unsupported version") != std::string::npos, "[pvp-character-version] " << error);
}

TEST_CASE("world: content whose arena has a spawn closer to the bounds than the character's radius does not load") {
    ContentFolder folder;
    const auto arena = [](const std::string& x) {
        return R"({"version": 5, "id": "edge", "width": 10, "depth": 10, "map": "object_fps_pvp.map.pvp_arena", "kill_height": -10,)"
               R"( "spawns": [{"position": [)" + x + R"(, 0, 5], "yaw": 0}, {"position": [5, 0, 5], "yaw": 0}]})";
    };
    const float radius = ShippedContent()->character->bodySize.radius;
    std::string error;
    // Exactly the radius from the bound: room enough.
    folder.Arenas({{"edge.json", arena(std::to_string(radius))}});
    const auto fits = PW::LoadGameContent(folder.root.string(), error);
    REQUIRE_MESSAGE(fits.has_value(), error);
    CHECK(fits->arenas.front().spawns.front().position.x == radius);
    // Inside the arena (the arena file reads) but closer than the radius: the content does not load.
    folder.Arenas({{"edge.json", arena("0.1")}});
    REQUIRE(sim::ParseArena(ContentFolder::ReadText(folder.root / "maps" / "edge.json"), error).has_value());
    CHECK_FALSE_MESSAGE(PW::LoadGameContent(folder.root.string(), error).has_value(), "[pvp-spawn-radius-unchecked]");
    CHECK_MESSAGE(error.find("edge.json: spawn 0 of arena 'edge' is outside the bounds for the character's radius") != std::string::npos, error);
}
