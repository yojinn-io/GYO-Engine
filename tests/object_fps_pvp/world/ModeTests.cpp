// The game's modes (world/Modes.hpp; mode-config declaration, section 3): the shipped core PvP mode, what a mode file
// may not say, and the rules digest. A Client whose mode is not the Match's: WorldTests.cpp (its Session).
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/character/CharacterRig.hpp"
#include "object_fps_pvp/world/Modes.hpp"


using namespace object_fps_pvp::test;
namespace sim = object_fps_pvp::sim;

namespace {

std::optional<std::vector<PW::ModeContent>> Load(const ModeFolder& folder, std::string& error) {
    const auto& content = *ShippedContent();
    return PW::LoadModes(folder.root.string(), content.arenas, content.combat, *content.character, PW::GameSets(), error);
}

std::string Mode(const std::string& id, const std::string& rules = R"({"movement": "rules/movement_rules.json"})",
                 const std::string& sets = "{}", const std::string& arenas = "[]") {
    return ModeText(id, rules, sets, arenas);
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
    folder.Write("rules/slow.json", R"({"version": 2, "crouch_body_height": 1.2, "crouch_eye_height": 1.0, "crouch_speed_scale": 0.25,)"
                                     R"( "sprint_speed_scale": 1.0, "hold_jump": true})");
    // The same values as core_pvp written otherwise (CRLF, other spaces): the digest does not see the bytes.
    folder.Modes({{"a.json", Mode("core_pvp")},
                  {"b.json", "{\r\n  \"arenas\": [],\r\n \"sets\": {},  \"rules\": {\"movement\": \"rules/movement_rules.json\"},\r\n"
                             "\"version\": 1, \"id\": \"copy\", \"name\": \"N\"}\r\n"},
                  {"c.json", Mode("slow", R"({"movement": "rules/slow.json"})", "{}", R"(["pvp_training_v1"])")}});
    std::string error;
    const auto modes = Load(folder, error);
    REQUIRE_MESSAGE(modes.has_value(), error);
    REQUIRE(modes->size() == 3);
    CHECK(modes->at(0).rulesDigest == ShippedContent()->modes.front().rulesDigest);
    // Another id is another mode: its digest differs (the id is in it), the rule values are the same.
    CHECK(modes->at(1).stance == modes->at(0).stance);
    CHECK(modes->at(1).rulesDigest != modes->at(0).rulesDigest);
    CHECK(modes->at(2).stance.crouchSpeedScale == 0.25F);
    CHECK(modes->at(2).Plays("pvp_training_v1"));
    CHECK_FALSE(modes->at(2).Plays("pvp_corners_v1"));
    CHECK(modes->at(2).rulesDigest != modes->at(0).rulesDigest);
}

TEST_CASE("world: the rules digest follows each rule value: the movement rules, the combat rules and the hit shapes") {
    // One mode id, read with one rule value changed at a time: each gives another digest.
    const auto& content = *ShippedContent();
    const auto digestOf = [&](const std::string& movement, const sim::CombatRules& combat, const object_fps_pvp::character::CharacterRig& rig) {
        ModeFolder folder;
        folder.Write("rules/movement.json", movement);
        folder.Modes({{"a.json", Mode("same", R"({"movement": "rules/movement.json"})")}});
        std::string error;
        const auto modes = PW::LoadModes(folder.root.string(), content.arenas, combat, rig, PW::GameSets(), error);
        REQUIRE_MESSAGE(modes.has_value(), error);
        return modes->front().rulesDigest;
    };
    const std::string shipped = R"({"version": 2, "crouch_body_height": 1.2, "crouch_eye_height": 1.0, "crouch_speed_scale": 0.5,)"
                                R"( "sprint_speed_scale": 1.0, "hold_jump": true})";
    const std::uint64_t base = digestOf(shipped, content.combat, *content.character);
    CHECK(digestOf(shipped, content.combat, *content.character) == base);
    std::string slower = shipped;
    slower.replace(slower.find("0.5"), 3, "0.4");
    CHECK_MESSAGE(digestOf(slower, content.combat, *content.character) != base, "rulesDigest: the movement rules");
    sim::CombatRules combat = content.combat;
    combat.legPermille += 1;
    CHECK_MESSAGE(digestOf(shipped, combat, *content.character) != base, "rulesDigest: the combat rules");
    object_fps_pvp::character::CharacterRig rig = *content.character;
    REQUIRE_FALSE(rig.hitShapes.empty());
    rig.hitShapes.back().radius += 0.01F;
    CHECK_MESSAGE(digestOf(shipped, content.combat, rig) != base, "rulesDigest: the hit shapes");
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
    refused({{"a.json", Mode("a", R"({"movement": "rules/movement_rules.json", "weapons": "rules/movement_rules.json"})")}},
            "no rule kind 'weapons'");
    refused({{"a.json", Mode("a", "{}")}}, "no movement rules");
    refused({{"a.json", Mode("a", R"({"movement": "rules/movement_rules.json"})", R"({"doors": {}})")}}, "no gameplay set 'doors'");
    refused({{"a.json", Mode("a", R"({"movement": "rules/movement_rules.json"})", "{}", R"(["no_such_arena"])")}},
            "no installed arena has the id 'no_such_arena'");
    refused({{"a.json", Mode("same")}, {"b.json", Mode("same")}}, "another installed mode has the id 'same'");
    refused({{"a.json", Mode(std::string(65, 'a'))}}, "id is 1 to 64 characters");
    refused({{"a.json", Mode("a", R"({"movement": "../rules/movement_rules.json"})")}}, "not a path inside the content");
    refused({{"a.json", Mode("a", R"({"movement": "rules/missing.json"})")}}, "missing.json");
}

TEST_CASE("world: which arenas a mode plays is one question to the content: every choice asks GameContent::Plays") {
    ModeFolder folder;
    folder.Modes({{"a.json", Mode("all")}, {"b.json", Mode("one", R"({"movement": "rules/movement_rules.json"})", "{}", R"(["pvp_corners_v1"])")}});
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
