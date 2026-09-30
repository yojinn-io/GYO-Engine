#include <doctest/doctest.h>
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include <algorithm>
#include <cmath>

namespace {
using namespace fps::pvp;
Arena LifeArena() {
    Arena a; a.id = "life_prediction"; a.width = a.depth = 20;
    a.spawns = {{{3, 0, 3}, 0}, {{15, 0, 15}, 0}};
    return a;
}
PlayerState LifePlayer() { PlayerState p; p.playerId = 1; p.position = {3, 0, 3}; return p; }
}

TEST_CASE("v5 jump edge survives a substep frame and is consumed once across multiple steps") {
    LocalPlayerPrediction p(LifeArena()); p.Reconcile(LifePlayer(), 1);
    CHECK_FALSE(p.Advance(0, 0, 0, 0, 0, true));
    CHECK(p.Advance(1.0 / 30, 0, 0, 0, 0));
    const auto commands = p.PendingInput().commands;
    REQUIRE(commands.size() == 4);
    CHECK_FALSE(commands[0].jumpRequested); CHECK_FALSE(commands[1].jumpRequested);
    CHECK(commands[2].jumpRequested); CHECK_FALSE(commands[3].jumpRequested);
    CHECK(p.Observation().predictedPosition.y > 0);
}

TEST_CASE("v5 discarded jump edges do not survive lost focus full windows or new epochs") {
    const auto arena = LifeArena();
    for (int mode : {0, 1, 2}) {
        LocalPlayerPrediction p(arena); auto a = LifePlayer(); p.Reconcile(a, 1);
        if (mode == 1) {
            for (int i = 0; i < 12; ++i) static_cast<void>(p.Advance(MovementTickSeconds, 0, 0, 0, 0));
            REQUIRE(p.Observation().frozen);
        }
        static_cast<void>(p.Advance(0, 0, 0, 0, 0, true));
        if (mode == 0) p.ClearJumpRequest();
        if (mode == 1) { a.lastResolvedCommand = p.Observation().latestCommand; p.Reconcile(a, 2); }
        if (mode == 2) { a.movementEpoch = 2; p.Reconcile(a, 2); }
        static_cast<void>(p.Advance(MovementTickSeconds, 0, 0, 0, 0));
        for (const auto& c : p.PendingInput().commands) CHECK_FALSE(c.jumpRequested);
        CHECK(p.Observation().predictedPosition.y == 0);
    }
}

TEST_CASE("v5 prediction and authority agree in three dimensions at 30 60 and 144 FPS") {
    for (const double fps : {30., 60., 144.}) {
        auto arena = LifeArena(); LocalPlayerPrediction p(arena); auto authority = LifePlayer();
        p.Reconcile(authority, 1); std::uint64_t tick = 1, executed = 0; double maximumY = 0;
        for (int frame = 0; frame < static_cast<int>(fps * 2); ++frame) {
            static_cast<void>(p.Advance(1 / fps, 1, 0, 0, 0, frame == 0));
            for (const auto& c : p.PendingInput().commands) {
                if (c.sequence <= executed) continue;
                authority = StepMovement(arena, authority, c); executed = c.sequence;
                maximumY = std::max(maximumY, static_cast<double>(authority.position.y));
            }
            CHECK(p.Observation().predictedPosition.x == doctest::Approx(authority.position.x));
            CHECK(p.Observation().predictedPosition.y == doctest::Approx(authority.position.y));
            CHECK(p.Observation().predictedPosition.z == doctest::Approx(authority.position.z));
            CHECK(p.Observation().verticalVelocity == doctest::Approx(authority.verticalVelocity));
            p.Reconcile(authority, ++tick);
        }
        CHECK(maximumY == doctest::Approx(.6).epsilon(.01));
        CHECK(authority.grounded); CHECK(authority.position.y == 0);
        CHECK(executed >= 120); CHECK(executed <= 122);
    }
}

TEST_CASE("v5 authoritative death suppresses pending controlled replay and life change clears correction") {
    auto arena = LifeArena(); LocalPlayerPrediction p(arena); auto a = LifePlayer(); p.Reconcile(a, 1);
    for (int n = 0; n < 6; ++n) static_cast<void>(p.Advance(MovementTickSeconds, 1, 0, 0, 0, n == 0));
    a.lifeState = LifeState::Dead; a.lifeStateTick = 2; a.respawnTick = 182;
    p.Reconcile(a, 2);
    CHECK(p.Observation().predictedPosition.z == a.position.z);
    CHECK(p.Observation().predictedPosition.y == 0);
    static_cast<void>(p.Advance(MovementTickSeconds, 1, 1, 1, 1, true));
    CHECK(p.Observation().predictedPosition.z == a.position.z);
    CHECK_FALSE(p.PendingInput().commands.back().jumpRequested);
    a.lifeState = LifeState::Alive; a.lifeGeneration = 2; a.movementEpoch = 2;
    a.position = {15, 0, 15}; a.lifeStateTick = 182; a.respawnTick = 0;
    p.Reconcile(a, 182);
    CHECK(p.Observation().lifeGeneration == 2); CHECK(p.PendingInput().lifeGeneration == 2);
    CHECK(p.Observation().pendingCommands == InitialCommandLead);
    CHECK(p.Observation().correctionOffset.x == 0); CHECK(p.Observation().correctionOffset.y == 0);
    CHECK(p.Observation().renderPosition.z == 15);
    // A 30 FPS frame after reseeding clamps one step. The diagnostic must
    // identify the new life, just like the generated commands it describes.
    auto trace = std::make_shared<MovementTrace>();
    SetMovementTrace(trace);
    static_cast<void>(p.Advance(1.0 / 30, 0, 0, 0, 0));
    SetMovementTrace({});
    bool sawGap = false;
    for (const auto& event : trace->Drain()) {
        CHECK(event.lifeGeneration == 2);
        if (event.kind == MovementTraceKind::RuntimeGap) {
            sawGap = true;
            CHECK(event.epoch == 2);
            CHECK(event.droppedSeconds == doctest::Approx(MovementTickSeconds));
        }
    }
    CHECK(sawGap);
}

TEST_CASE("v5 render correction sweeps vertically and never displays below the ground") {
    auto a = LifePlayer(); a.position.y = .5F; a.grounded = false;
    LocalPlayerPrediction p(LifeArena()); p.Reconcile(a, 1);
    for (int i = 0; i < 100; ++i) {
        static_cast<void>(p.Advance(MovementTickSeconds, 0, 0, 0, 0));
        auto state = a; state.lastResolvedCommand = p.Observation().latestCommand;
        state.position = p.Observation().predictedPosition;
        state.verticalVelocity = p.Observation().verticalVelocity; state.grounded = p.Observation().grounded;
        if (i == 5) state.position.y = .1F;
        p.Reconcile(state, i + 2);
        CHECK(p.Observation().renderPosition.y >= 0);
    }
}

TEST_CASE("v5 remote timeline does not mix lives or life-state segments with combat") {
    SnapshotTimeline timeline; auto p = LifePlayer();
    const auto now = SnapshotTimeline::Clock::now();
    CombatState c; c.playerId = 1;
    REQUIRE(timeline.Push({1, {p}, {c}}, now));
    p.lifeState = LifeState::Dead; p.lifeStateTick = 2; p.respawnTick = 182; c.hp = 0;
    REQUIRE(timeline.Push({2, {p}, {c}}, now + std::chrono::milliseconds(17)));
    auto selected = timeline.Sample(1, now + std::chrono::milliseconds(18));
    REQUIRE(selected); CHECK(selected->player.lifeState == LifeState::Dead);
    REQUIRE(selected->combat); CHECK(selected->combat->hp == 0); CHECK(selected->lowerTick == 2);
    p.lifeState = LifeState::Alive; p.lifeGeneration = 2; p.movementEpoch = 2;
    p.position = {15, 0, 15}; c.lifeGeneration = 2; c.hp = 100;
    REQUIRE(timeline.Push({182, {p}, {c}}, now + std::chrono::seconds(3)));
    selected = timeline.Sample(1, now + std::chrono::seconds(3)); REQUIRE(selected);
    CHECK(selected->player.lifeGeneration == 2); CHECK(selected->player.position.z == 15);
    REQUIRE(selected->combat); CHECK(selected->combat->lifeGeneration == 2); CHECK(selected->combat->hp == 100);
}
