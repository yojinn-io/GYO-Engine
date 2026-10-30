#include <doctest/doctest.h>
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <array>
#include <cstdint>

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
