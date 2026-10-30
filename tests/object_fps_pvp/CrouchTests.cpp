#include <doctest/doctest.h>
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementRulesFile.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

// v8-product-crouch (protocol-v8 §4, §5): stance rules shared by prediction,
// replay and authority, and the Match's substitution of missing commands.
namespace {
using namespace fps::pvp;

// An open floor with a low slab over x in [6, 8]: 1.5 above the floor, so a
// 1.2 crouched body fits under it and a 1.8 standing body does not.
Arena CrouchArena() {
    Arena arena;
    arena.id = "synthetic_crouch_arena";
    arena.width = arena.depth = 12;
    arena.walls = {
        {{-1, 0, -1}, {0, 3, 13}}, {{12, 0, -1}, {13, 3, 13}},
        {{0, 0, -1}, {12, 3, 0}}, {{0, 0, 12}, {12, 3, 13}},
        {{6, 1.5F, 0}, {8, 3, 12}},
    };
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 9}, 0}};
    return arena;
}

MovementCommand Command(std::uint64_t sequence, bool crouch, bool jump = false, float forward = 0) {
    return {sequence, forward, 0, 0, 0, jump, crouch};
}

PlayerState Under() { return {1, {7, 0, 6}, 0, 0, 0, 1, 0, 0, true}; }
} // namespace

TEST_CASE("PvP crouch holds the stance, lowers speed and stands only with room") {
    const auto arena = CrouchArena();
    PlayerState state{1, {2, 0, 2}};
    state = StepMovement(arena, state, Command(1, true));
    CHECK(state.crouched);
    CHECK_FALSE(state.sprinting);
    // Crouched speed is the arena speed times crouchSpeedScale.
    const auto before = state.position;
    state = StepMovement(arena, state, Command(2, true, false, 1));
    const auto crouchedStep = state.position.z - before.z;
    PlayerState standing{1, {2, 0, 2}};
    const auto standingStep = StepMovement(arena, standing, Command(1, false, false, 1)).position.z - 2;
    CHECK(crouchedStep == doctest::Approx(standingStep * DefaultStanceRules.crouchSpeedScale));
    state = StepMovement(arena, state, Command(3, false));
    CHECK_FALSE(state.crouched);

    // Under the slab a release keeps the crouch; leaving it stands up.
    auto under = Under();
    under.crouched = true;
    under = StepMovement(arena, under, Command(1, false));
    CHECK(under.crouched);
    PlayerState outside = Under();
    outside.crouched = true;
    outside.position.x = 4;
    CHECK_FALSE(StepMovement(arena, outside, Command(1, false)).crouched);
}

TEST_CASE("PvP crouch jump: a grounded jump stands up first and never jumps from under a slab") {
    const auto arena = CrouchArena();
    PlayerState state{1, {2, 0, 2}};
    state.crouched = true;
    // Holding crouch and jumping: the jump wins on the ground.
    state = StepMovement(arena, state, Command(1, true, true));
    CHECK_FALSE(state.crouched);
    CHECK_FALSE(state.grounded);
    CHECK(state.position.y > 0);
    // In the air the held crouch takes effect: the crouch jump.
    const auto feet = state.position.y;
    state = StepMovement(arena, state, Command(2, true));
    CHECK(state.crouched);
    CHECK(state.position.y > 0);
    CHECK(state.position.y < feet + 0.2F);

    // Under the slab the stand-up fails, so the jump does not happen.
    auto under = Under();
    under.crouched = true;
    under = StepMovement(arena, under, Command(1, false, true));
    CHECK(under.crouched);
    CHECK(under.grounded);
    CHECK(under.position.y == 0);
}

TEST_CASE("PvP crouch in the air stands up only with room and freezes while dead") {
    const auto arena = CrouchArena();
    PlayerState state{1, {2, 0, 2}};
    state = StepMovement(arena, state, Command(1, false, true));
    REQUIRE_FALSE(state.grounded);
    state = StepMovement(arena, state, Command(2, true));
    CHECK(state.crouched);
    state = StepMovement(arena, state, Command(3, false));
    CHECK_FALSE(state.crouched);

    PlayerState dead{1, {2, 0, 2}};
    dead.lifeState = LifeState::Dead;
    dead.crouched = true;
    dead = StepMovement(arena, dead, Command(1, false));
    CHECK(dead.crouched);
    CHECK_FALSE(dead.sprinting);
}

TEST_CASE("PvP crouch changes the shot eye and the target capsule") {
    const auto arena = CrouchArena();
    PlayerState shooter{1, {2, 0, 2}};
    // A level shot from a standing eye (1.6) passes above a crouched target (1.2).
    std::array targets{PlayerState{2, {2, 0, 5}}};
    CHECK(QueryShot(arena, shooter, 0, 0, targets, 100).kind == ShotHitKind::Player);
    targets[0].crouched = true;
    CHECK(QueryShot(arena, shooter, 0, 0, targets, 100).kind != ShotHitKind::Player);
    // A crouched shooter's eye (1.0) is inside the crouched target's height.
    shooter.crouched = true;
    CHECK(QueryShot(arena, shooter, 0, 0, targets, 100).kind == ShotHitKind::Player);
}

TEST_CASE("PvP crouch survives held and neutral substitution and an epoch rotation") {
    PvpMatch match(CrouchArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, {Command(1, true)}}));
    match.Tick({match.TickCount() + 1, 1.0 / 60});
    REQUIRE(match.Snapshot().players.front().crouched);
    // Held, then neutral (InputHoldTicks later), both keep the crouch.
    for (std::uint32_t index = 0; index < InputHoldTicks + 2; ++index)
        match.Tick({match.TickCount() + 1, 1.0 / 60});
    CHECK(match.Snapshot().players.front().crouched);
    // A Resync epoch keeps the pose; the Match does not step the player until
    // the new epoch's first command arrives, so no cleared command substitutes.
    REQUIRE(match.RequestMovementResync(1));
    for (int index = 0; index < 3; ++index) match.Tick({match.TickCount() + 1, 1.0 / 60});
    const auto after = match.Snapshot().players.front();
    CHECK(after.movementEpoch > 1);
    CHECK(after.crouched);
}

TEST_CASE("PvP crouch seeds a crouched lead from a crouched authority") {
    LocalPlayerPrediction client(CrouchArena());
    PlayerState authority{1, {2, 0, 2}};
    authority.crouched = true;
    client.Reconcile(authority, 1);
    const auto input = client.PendingInput();
    REQUIRE_FALSE(input.commands.empty());
    for (const auto& command : input.commands) CHECK(command.crouchHeld);
}

// W2 (v8-product-crouch wire): sprint_held has no behaviour in pv8. The same
// commands with and without it leave the Match in the same state, and
// sprinting stays false.
TEST_CASE("PvP sprint_held changes nothing in pv8") {
    const auto run = [](bool sprint) {
        PvpMatch match(CrouchArena());
        std::string error;
        REQUIRE(match.Join(1, error));
        for (std::uint64_t sequence = 1; sequence <= 90; ++sequence) {
            auto command = Command(sequence, sequence > 30 && sequence < 60, sequence == 70, 1);
            command.sprintHeld = sprint;
            REQUIRE(match.SubmitInput({1, {command}}));
            match.Tick({match.TickCount() + 1, 1.0 / 60});
            CHECK_FALSE(match.Snapshot().players.front().sprinting);
        }
        return match.Snapshot().players.front();
    };
    const auto held = run(true), released = run(false);
    CHECK(held.position.x == released.position.x);
    CHECK(held.position.y == released.position.y);
    CHECK(held.position.z == released.position.z);
    CHECK(held.verticalVelocity == released.verticalVelocity);
    CHECK(held.lastResolvedCommand == released.lastResolvedCommand);
    CHECK(held.crouched == released.crouched);
}

// W4 (v8-product-crouch wire, protocol-v8 section 3): the Match's movement rules
// data. The release file loads for the release arenas; every bound, a missing
// or extra key and another version are refused, never replaced by defaults.
TEST_CASE("PvP movement rules data loads the release file and refuses every violation") {
    const auto assets = std::filesystem::path(PVP_PRODUCT_ARENA_PATH).parent_path();
    std::string error;
    for (const char* name : {"pvp_arena.json", "pvp_corners.json"}) {
        const auto arena = Arena::Load(assets / name, error);
        REQUIRE_MESSAGE(arena, error);
        const auto rules = LoadStanceRules(assets / "movement_rules.json", *arena, error);
        REQUIRE_MESSAGE(rules, error);
        CHECK(*rules == DefaultStanceRules);
    }
    const auto arena = CrouchArena();
    const auto path = std::filesystem::temp_directory_path() / "gyo_pvp_movement_rules_test.json";
    const auto load = [&](const std::string& text) {
        { std::ofstream file(path, std::ios::binary | std::ios::trunc); file << text; }
        return LoadStanceRules(path, arena, error);
    };
    const auto rules = [](const std::string& body, const std::string& eye, const std::string& crouch,
                          const std::string& sprint) {
        return R"({"version": 1, "crouch_body_height": )" + body + R"(, "crouch_eye_height": )" + eye +
            R"(, "crouch_speed_scale": )" + crouch + R"(, "sprint_speed_scale": )" + sprint + "}";
    };
    const auto valid = load(rules("1.2", "1.0", "0.5", "1.0"));
    REQUIRE_MESSAGE(valid, error);
    CHECK(*valid == DefaultStanceRules);
    for (const auto& text : {
             rules("1.2", "1.3", "0.5", "1.0"),   // eye above the crouched body
             rules("1.2", "0", "0.5", "1.0"),     // no eye height
             rules("1.8", "1.0", "0.5", "1.0"),   // not below the standing body
             rules("0.4", "0.3", "0.5", "1.0"),   // below two radii
             rules("1.2", "1.0", "0", "1.0"),     // no crouch speed
             rules("1.2", "1.0", "1.5", "1.0"),   // faster than standing
             rules("1.2", "1.0", "0.5", "0"),     // no sprint speed
             rules("1.2", "1.0", "0.5", "3.5"),   // beyond the sprint bound
             rules("1e40", "1.0", "0.5", "1.0"),  // not a finite float
             rules("\"1.2\"", "1.0", "0.5", "1.0"), // not a number
             std::string(R"({"version": 2, "crouch_body_height": 1.2, "crouch_eye_height": 1.0, "crouch_speed_scale": 0.5, "sprint_speed_scale": 1.0})"),
             std::string(R"({"version": 1, "crouch_body_height": 1.2, "crouch_eye_height": 1.0, "crouch_speed_scale": 0.5})"),
             std::string(R"({"version": 1, "crouch_body_height": 1.2, "crouch_eye_height": 1.0, "crouch_speed_scale": 0.5, "sprint_speed_scale": 1.0, "extra": 1})"),
             std::string("not json")}) {
        CAPTURE(text);
        CHECK_FALSE(load(text));
        CHECK_FALSE(error.empty());
    }
    std::filesystem::remove(path);
    CHECK_FALSE(LoadStanceRules(path, arena, error));
}
