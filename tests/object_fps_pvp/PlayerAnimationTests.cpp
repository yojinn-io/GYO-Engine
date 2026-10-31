#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerAnimation.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// v8-product-anim-authority A1 (E2): the Match's integer animation state
// machine, rule by rule (protocol-v9 §4.4), with synthetic rules.
namespace {
using namespace fps::pvp;

PlayerAnimationSource Source() {
    PlayerAnimationSource source;
    source.referenceSpeed = 3;
    source.maxFrameDeltaSeconds = 0.1; // teleport above 0.3 m in one tick
    source.walkNativeSpeed = 1;
    source.jogNativeSpeed = 5;
    source.walkClipSeconds = 1;  // a 1 m walk cycle
    source.jogClipSeconds = 0.8; // a 4 m jog cycle
    source.transitionSeconds = 0.1;
    source.jumpStartSeconds = 0.1;
    source.jumpLandSeconds = 0.1;
    return source;
}

PlayerAnimationRules Rules() {
    std::string error;
    const auto rules = MakePlayerAnimationRules(Source(), error);
    REQUIRE_MESSAGE(rules, error);
    return *rules;
}

PlayerState Player(float z = 0, bool grounded = true, float verticalVelocity = 0) {
    PlayerState player;
    player.playerId = 1;
    player.position = {0, 0, z};
    player.grounded = grounded;
    player.verticalVelocity = verticalVelocity;
    return player;
}

// Drives one player through authority states, one per tick from tick 1.
struct Track final {
    PlayerAnimationRules rules{Rules()};
    std::optional<PlayerAnimation> value;
    std::optional<PlayerState> before;
    std::uint64_t tick{};
    const PlayerAnimation& Step(const PlayerState& now) {
        value = AdvancePlayerAnimation(rules, value, before ? &*before : nullptr, now, ++tick);
        before = now;
        return *value;
    }
};

std::int32_t PhaseDifference(std::uint32_t later, std::uint32_t earlier) {
    return static_cast<std::int32_t>(later - earlier);
}
} // namespace

TEST_CASE("PvP animation rules come from whole-tick spans and the clip cycle lengths") {
    const auto rules = Rules();
    CHECK(rules.walkCycleDistance == doctest::Approx(1.0));
    CHECK(rules.jogCycleDistance == doctest::Approx(4.0));
    CHECK(rules.teleportDistance == doctest::Approx(0.3));
    CHECK(rules.transitionTicks == 6);
    CHECK(rules.jumpStartTicks == 6);
    CHECK(rules.jumpLandTicks == 6);
    const auto rejected = [](auto edit) {
        auto source = Source();
        edit(source);
        std::string error;
        return !MakePlayerAnimationRules(source, error) && !error.empty();
    };
    CHECK(rejected([](auto& s) { s.transitionSeconds = 0.105; }));  // 6.3 ticks
    CHECK(rejected([](auto& s) { s.jumpStartSeconds = 0.01; }));    // 0.6 ticks
    CHECK(rejected([](auto& s) { s.jumpLandSeconds = 0; }));
    CHECK(rejected([](auto& s) { s.jogNativeSpeed = 1; }));         // jog not faster than walk
    CHECK(rejected([](auto& s) { s.walkClipSeconds = 0; }));
    CHECK(rejected([](auto& s) { s.jogClipSeconds = -1; }));
    CHECK(rejected([](auto& s) { s.referenceSpeed = std::numeric_limits<double>::quiet_NaN(); }));
    CHECK_FALSE(rejected([](auto& s) { s.transitionSeconds = 0.2; })); // 12 ticks
}

TEST_CASE("PvP animation phase follows the walked distance over the cycle length") {
    Track track;
    track.Step(Player(0));
    // 1 m/s is the walk native speed: jog 0, a 1 m cycle, 1/60 cycle a tick.
    for (int i = 1; i <= 60; ++i) track.Step(Player(static_cast<float>(i) / 60));
    CHECK(track.value->jogWeightPermille == 0);
    CHECK(track.value->gaitPhaseQ16 == 60 * 1092); // round(65536 / 60) each tick
    // 3.75 m/s (a step exact in float): ratio 0.6875, weight 687, a 3.0625 m cycle.
    const auto start = track.value->gaitPhaseQ16;
    track.Step(Player(1 + 0.0625F));
    CHECK(track.value->jogWeightPermille == 687);
    CHECK(PhaseDifference(track.value->gaitPhaseQ16, start) == 1337); // round(0.0625 / 3.0625 x 65536)
}

TEST_CASE("PvP animation phase decreases backward and its signed difference survives wrapping") {
    Track track;
    track.Step(Player(0));
    track.Step(Player(-1.0F / 60)); // yaw 0 faces +z; this is backward
    CHECK(PhaseDifference(track.value->gaitPhaseQ16, 0) == -1092);
    CHECK(track.value->gaitPhaseQ16 == 0x100000000ULL - 1092);
    // Forward across 2^32 again.
    PlayerAnimation near = *track.value;
    near.gaitPhaseQ16 = 0xFFFFFF00U;
    track.value = near;
    const auto before = track.value->gaitPhaseQ16;
    track.Step(Player(0));
    CHECK(track.value->gaitPhaseQ16 < before); // wrapped
    CHECK(PhaseDifference(track.value->gaitPhaseQ16, before) == 1092);
}

TEST_CASE("PvP animation move weight steps through the transition in whole permille") {
    Track track;
    track.Step(Player(0));
    std::vector<std::uint32_t> up, down;
    for (int i = 1; i <= 8; ++i) up.push_back(track.Step(Player(static_cast<float>(i) / 60)).moveWeightPermille);
    CHECK(up == std::vector<std::uint32_t>{166, 333, 500, 666, 833, 1000, 1000, 1000});
    for (int i = 0; i < 7; ++i) down.push_back(track.Step(Player(8.0F / 60)).moveWeightPermille);
    CHECK(down == std::vector<std::uint32_t>{833, 666, 500, 333, 166, 0, 0});
}

TEST_CASE("PvP animation jog weight follows the speed and holds while still") {
    Track track;
    track.Step(Player(0));
    track.Step(Player(0.0625F)); // 3.75 m/s
    CHECK(track.value->jogWeightPermille == 687);
    track.Step(Player(0.0625F));
    CHECK(track.value->jogWeightPermille == 687); // still: kept
    track.Step(Player(0.0625F + 0.125F)); // 7.5 m/s, above jog: clamped
    CHECK(track.value->jogWeightPermille == 1000);
    const auto phase = track.value->gaitPhaseQ16;
    track.Step(Player(0.1875F));
    CHECK(track.value->gaitPhaseQ16 == phase); // a still tick adds no phase
}

TEST_CASE("PvP animation jump phases start from their clip time zero points") {
    Track track;
    track.Step(Player(0));
    track.Step(Player(0, false, 4)); // liftoff while rising at tick 2
    CHECK(track.value->jumpPhase == JumpPhase::Start);
    CHECK(track.value->jumpPhaseTick == 2);
    for (int i = 0; i < 5; ++i) track.Step(Player(0, false, 2));
    CHECK(track.value->jumpPhase == JumpPhase::Start); // tick 7: 5 ticks in
    track.Step(Player(0, false, 1));
    CHECK(track.value->jumpPhase == JumpPhase::Airborne); // tick 8: start + 6
    CHECK(track.value->jumpPhaseTick == 8);
    track.Step(Player(0, true)); // landing at tick 9
    CHECK(track.value->jumpPhase == JumpPhase::Land);
    CHECK(track.value->jumpPhaseTick == 9);
    for (int i = 0; i < 6; ++i) track.Step(Player(0, true));
    CHECK(track.value->jumpPhase == JumpPhase::Grounded); // tick 15: land + 6
    CHECK(track.value->jumpPhaseTick == 15);
    track.Step(Player(0, false, -1)); // walking off a ledge goes straight to the loop
    CHECK(track.value->jumpPhase == JumpPhase::Airborne);
    CHECK(track.value->jumpPhaseTick == 16);
    track.Step(Player(0, true));
    track.Step(Player(0, false, 3)); // a hop inside Land starts again
    CHECK(track.value->jumpPhase == JumpPhase::Start);
    CHECK(track.value->jumpPhaseTick == 18);
}

TEST_CASE("PvP animation resets on a new player, life, epoch or teleport") {
    const auto walked = [] {
        Track track;
        track.Step(Player(0));
        for (int i = 1; i <= 10; ++i) track.Step(Player(static_cast<float>(i) / 60));
        REQUIRE(track.value->gaitPhaseQ16 != 0);
        REQUIRE(track.value->moveWeightPermille == 1000);
        return track;
    };
    const auto reset = [](const PlayerAnimation& value, bool grounded, std::uint64_t tick) {
        CHECK(value.gaitPhaseQ16 == 0);
        CHECK(value.moveWeightPermille == 0);
        CHECK(value.jumpPhase == (grounded ? JumpPhase::Grounded : JumpPhase::Airborne));
        CHECK(value.jumpPhaseTick == tick);
    };
    {
        Track first;
        reset(first.Step(Player(0, false)), false, 1); // seen for the first time
    }
    {
        auto track = walked();
        auto other = Player(11.0F / 60);
        other.playerId = 2;
        reset(track.Step(other), true, 12);
    }
    {
        auto track = walked();
        auto next = Player(11.0F / 60);
        next.lifeGeneration = 2;
        reset(track.Step(next), true, 12);
        CHECK(track.value->lifeGeneration == 2);
    }
    {
        auto track = walked();
        auto next = Player(11.0F / 60, false);
        next.movementEpoch = 2;
        reset(track.Step(next), false, 12); // v8-product-anim-epoch-rule (a)
    }
    {
        auto track = walked();
        const auto& value = track.Step(Player(10.0F / 60 + 0.31F)); // 18.6 m/s
        reset(value, true, 12);
        CHECK(value.jogWeightPermille == 1000); // from this tick's speed
    }
    {
        auto track = walked();
        const auto& value = track.Step(Player(10.0F / 60 + 0.29F));
        CHECK(value.moveWeightPermille == 1000); // below the bound: walked, not reset
    }
}

TEST_CASE("PvP animation freezes through death and restarts with the new life") {
    Track track;
    track.Step(Player(0));
    for (int i = 1; i <= 3; ++i) track.Step(Player(static_cast<float>(i) / 60));
    const auto alive = *track.value;
    auto dead = Player(1);
    dead.lifeState = LifeState::Dead;
    dead.lifeStateTick = 5;
    CHECK(track.Step(dead) == alive);
    CHECK(track.Step(dead) == alive);
    auto respawned = Player(5);
    respawned.lifeGeneration = 2;
    respawned.lifeStateTick = 7;
    const auto& value = track.Step(respawned);
    CHECK(value.lifeGeneration == 2);
    CHECK(value.gaitPhaseQ16 == 0);
    CHECK(value.jumpPhaseTick == 7);
}

TEST_CASE("PvP animation keeps the sprint weight at zero before sprint presentation") {
    Track track;
    track.Step(Player(0));
    for (int i = 1; i <= 30; ++i) {
        auto player = Player(static_cast<float>(i) * 0.1F); // 6 m/s
        player.sprinting = true;
        CHECK(track.Step(player).sprintWeightPermille == 0);
    }
}

// xhigh review of A1 (C1-C3, S1, S2) and the gaps it found; each failed
// before its fix (AGENTS §14.3).
TEST_CASE("PvP animation reset weights come from the new life's own speed, not across the boundary") {
    Track track;
    track.Step(Player(0));
    auto dead = Player(1);
    dead.lifeState = LifeState::Dead;
    track.Step(dead);
    auto respawned = Player(5); // 4 m away, standing still
    respawned.lifeGeneration = 2;
    CHECK(track.Step(respawned).jogWeightPermille == 0);
    auto other = Player(9);
    other.playerId = 2;
    CHECK(track.Step(other).jogWeightPermille == 0);
    auto epoch = Player(13);
    epoch.playerId = 2;
    epoch.movementEpoch = 2;
    CHECK(track.Step(epoch).jogWeightPermille == 0);
}

TEST_CASE("PvP animation rules bound the spans and the derived distances") {
    const auto rejected = [](auto edit) {
        auto source = Source();
        edit(source);
        std::string error;
        return !MakePlayerAnimationRules(source, error);
    };
    CHECK(rejected([](auto& s) { s.transitionSeconds = 1001.0 / 60; })); // the step recovery holds to 1000
    CHECK_FALSE(rejected([](auto& s) { s.transitionSeconds = 1000.0 / 60; }));
    CHECK(rejected([](auto& s) { s.jumpStartSeconds = 1e12; }));
    CHECK(rejected([](auto& s) { s.walkNativeSpeed = 1e200; s.jogNativeSpeed = 2e200; s.walkClipSeconds = 1e200; }));
    CHECK(rejected([](auto& s) { s.referenceSpeed = 1e-200; s.maxFrameDeltaSeconds = 1e-200; }));
}

TEST_CASE("PvP animation treats a non-finite step as a reset at rest") {
    Track track;
    track.Step(Player(0));
    track.Step(Player(1.0F / 60));
    const auto& value = track.Step(Player(std::numeric_limits<float>::infinity()));
    CHECK(value.jogWeightPermille == 0);
    CHECK(value.gaitPhaseQ16 == 0);
    CHECK(value.moveWeightPermille == 0);
}

TEST_CASE("PvP animation resets when its previous value belongs to another life than the state before") {
    // A skipped respawn tick: the frozen airborne value of life 1 must not be
    // carried into life 2, whose states before and now are both grounded.
    const auto rules = Rules();
    PlayerAnimation frozen;
    frozen.playerId = 1;
    frozen.lifeGeneration = 1;
    frozen.jumpPhase = JumpPhase::Airborne;
    frozen.jumpPhaseTick = 3;
    auto respawned = Player(0);
    respawned.lifeGeneration = 2;
    const auto value = AdvancePlayerAnimation(rules, frozen, &respawned, respawned, 20);
    CHECK(value.jumpPhase == JumpPhase::Grounded);
    CHECK(value.jumpPhaseTick == 20);
    CHECK(value.lifeGeneration == 2);
}

TEST_CASE("PvP animation resets when the tick runs behind its zero point") {
    const auto rules = Rules();
    PlayerAnimation value;
    value.playerId = 1;
    value.lifeGeneration = 1;
    value.jumpPhase = JumpPhase::Land;
    value.jumpPhaseTick = 50;
    const auto player = Player(0);
    const auto next = AdvancePlayerAnimation(rules, value, &player, player, 10); // a reset match clock
    CHECK(next.jumpPhase == JumpPhase::Grounded);
    CHECK(next.jumpPhaseTick == 10);
}

TEST_CASE("PvP animation walking off an edge at zero vertical speed goes straight to the loop") {
    Track track;
    track.Step(Player(0));
    CHECK(track.Step(Player(0, false, 0)).jumpPhase == JumpPhase::Airborne); // Movement.cpp: vv stays 0 that tick
}

TEST_CASE("PvP animation uses each span on its own phase") {
    auto source = Source();
    source.jumpStartSeconds = 4.0 / 60;
    source.jumpLandSeconds = 8.0 / 60;
    source.transitionSeconds = 12.0 / 60;
    std::string error;
    const auto rules = MakePlayerAnimationRules(source, error);
    REQUIRE(rules);
    CHECK(rules->transitionTicks == 12);
    Track track;
    track.rules = *rules;
    track.Step(Player(0));
    std::vector<std::uint32_t> up;
    for (int i = 1; i <= 12; ++i) up.push_back(track.Step(Player(static_cast<float>(i) / 60)).moveWeightPermille);
    CHECK(up == std::vector<std::uint32_t>{83, 166, 250, 333, 416, 500, 583, 666, 750, 833, 916, 1000});
    track.Step(Player(0.2F, false, 3)); // tick 14
    for (int i = 0; i < 3; ++i) track.Step(Player(0.2F, false, 2));
    CHECK(track.value->jumpPhase == JumpPhase::Start); // tick 17: 3 in
    track.Step(Player(0.2F, false, 1));
    CHECK(track.value->jumpPhase == JumpPhase::Airborne); // tick 18: start + 4
    track.Step(Player(0.2F, true)); // land at tick 19
    for (int i = 0; i < 7; ++i) track.Step(Player(0.2F, true));
    CHECK(track.value->jumpPhase == JumpPhase::Land); // tick 26: 7 in
    track.Step(Player(0.2F, true));
    CHECK(track.value->jumpPhase == JumpPhase::Grounded); // tick 27: land + 8
    CHECK(track.value->jumpPhaseTick == 27);
}

TEST_CASE("PvP animation separates still, tiny and perpendicular steps") {
    Track track;
    track.Step(Player(0));
    for (int i = 1; i <= 6; ++i) track.Step(Player(static_cast<float>(i) / 60));
    const auto phase = track.value->gaitPhaseQ16;
    auto tiny = Player(6.0F / 60);
    tiny.position.x = 0.0000005F; // below the still threshold
    track.Step(tiny);
    CHECK(track.value->gaitPhaseQ16 == phase);
    CHECK(track.value->moveWeightPermille == 833); // a still tick
    auto across = tiny;
    across.position.x += 0.05F; // perpendicular to yaw 0: forward, not backward
    track.Step(across);
    CHECK(PhaseDifference(track.value->gaitPhaseQ16, phase) > 0);
}

TEST_CASE("PvP animation resets when only the life state changes") {
    Track track;
    auto dead = Player(0);
    dead.lifeState = LifeState::Dead;
    track.Step(dead); // first seen while dead
    auto alive = Player(0, false);
    track.Step(alive); // same generation, alive again
    CHECK(track.value->jumpPhase == JumpPhase::Airborne);
    CHECK(track.value->jumpPhaseTick == 2);
}
