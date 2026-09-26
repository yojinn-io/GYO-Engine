#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <thread>

namespace {
fps::pvp::Arena TestArena() {
    fps::pvp::Arena arena;
    arena.id = "synthetic_two_player_arena";
    arena.width = arena.depth = 12;
    arena.walls = {
        {{-1, 0, -1}, {0, 3, 13}}, {{12, 0, -1}, {13, 3, 13}},
        {{0, 0, -1}, {12, 3, 0}}, {{0, 0, 12}, {12, 3, 13}},
        {{5, 0, 4}, {6, 3, 8}},
    };
    arena.spawns = {{{2, 0, 2}, 0}, {{9, 0, 2}, 0}};
    return arena;
}

fps::pvp::PlayerInput Input(fps::pvp::PlayerId playerId, std::uint64_t sequence,
    float forward = 1, float right = 0, float yaw = 0, float pitch = 0) {
    return {playerId, {{sequence, forward, right, yaw, pitch}}};
}

fps::pvp::PlayerInput Window(fps::pvp::PlayerId playerId, std::uint64_t first,
    std::uint64_t last, float forward = 1, float right = 0) {
    fps::pvp::PlayerInput result{playerId, {}};
    for (auto sequence = first; sequence <= last; ++sequence)
        result.commands.push_back({sequence, forward, right, 0, 0});
    return result;
}

void Step(fps::pvp::PvpMatch& match, unsigned count = 1) {
    for (unsigned index = 0; index < count; ++index)
        match.Tick({match.TickCount() + 1, 1.0 / 60});
}
}

TEST_CASE("PvP authority joins at distinct valid spawns and owns membership") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    CHECK_FALSE(match.Join(0, error));
    REQUIRE(match.Join(7, error));
    REQUIRE(match.Join(8, error));
    CHECK(match.Join(7, error));
    CHECK_FALSE(match.Join(9, error));
    CHECK(error == "match_full");
    const auto snapshot = match.Snapshot();
    REQUIRE(snapshot.players.size() == 2);
    CHECK(snapshot.players[0].position.x == 2);
    CHECK(snapshot.players[1].position.x == 9);
    CHECK(match.Leave(7));
    CHECK_FALSE(match.Leave(7));
    CHECK(match.Join(9, error));
}

TEST_CASE("PvP backlog rotates epoch on the next boundary without extra movement or world ticks") {
    using namespace fps::pvp;
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } scope;
    PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    for (unsigned tick = 1; tick <= MovementBacklogSampleTicks; ++tick) {
        const auto state = match.Snapshot().players.front();
        auto input = Window(1, state.lastResolvedCommand + 1, state.lastResolvedCommand + 5);
        input.movementEpoch = state.movementEpoch;
        REQUIRE(match.SubmitInput(input));
        REQUIRE(match.SubmitInput(Input(2, tick, 0, -1)));
        Step(match);
        const auto snapshot = match.Snapshot();
        CHECK(snapshot.players.front().movementEpoch == 1);
        CHECK(snapshot.players.front().lastResolvedCommand == tick);
        CHECK(snapshot.players.front().contiguousPendingCommands == 4);
    }
    const auto before = match.Snapshot();
    REQUIRE(match.SubmitInput(Input(2, 31, 0, -1)));
    Step(match);
    const auto reset = match.Snapshot();
    CHECK(reset.tick == before.tick + 1);
    CHECK(reset.players.front().movementEpoch == 2);
    CHECK(reset.players.front().lastResolvedCommand == 0);
    CHECK(reset.players.front().contiguousPendingCommands == 0);
    CHECK(reset.players.front().position.x == before.players.front().position.x);
    CHECK(reset.players.front().position.z == before.players.front().position.z);
    CHECK(reset.players.back().lastResolvedCommand == 31);
    CHECK(reset.players.back().position.x < before.players.back().position.x);
    CHECK_FALSE(match.SubmitInput(Input(1, 31)));
    auto future = Input(1, 1);
    future.movementEpoch = 3;
    CHECK_FALSE(match.SubmitInput(future));
    auto second = Input(1, 2, 0);
    second.movementEpoch = 2;
    REQUIRE(match.SubmitInput(second));
    Step(match, 3); // A lost reset snapshot does not lose the epoch transition.
    const auto waiting = match.Snapshot().players.front();
    CHECK(waiting.movementEpoch == 2);
    CHECK(waiting.lastResolvedCommand == 0);
    CHECK(waiting.position.z == before.players.front().position.z);
    auto first = Input(1, 1, 0);
    first.movementEpoch = 2;
    REQUIRE(match.SubmitInput(first));
    Step(match, 3);
    const auto resumed = match.Snapshot().players.front();
    CHECK(resumed.lastResolvedCommand == 3);
    CHECK(resumed.position.z == waiting.position.z); // Old held input was cleared.
    unsigned resets{};
    for (const auto& event : scope.trace->Drain()) {
        if (event.kind != MovementTraceKind::Reset || event.playerId != 1) continue;
        ++resets;
        CHECK(event.resetReason == MovementResetReason::Backlog);
        CHECK(event.authorityTick == 31);
        CHECK(TraceResetReasonName(event.resetReason) == "backlog");
    }
    CHECK(resets == 1);
}

TEST_CASE("PvP backlog fuse tolerates normal phase queues and bounds repeated resets") {
    using namespace fps::pvp;
    for (const unsigned futureCount : {2U, 3U, 4U}) {
        PvpMatch match(TestArena());
        std::string error;
        REQUIRE(match.Join(1, error));
        for (unsigned tick = 1; tick <= 180; ++tick) {
            const auto state = match.Snapshot().players.front();
            auto input = Window(1, state.lastResolvedCommand + 1,
                state.lastResolvedCommand + futureCount, 0);
            input.movementEpoch = state.movementEpoch;
            REQUIRE(match.SubmitInput(input));
            Step(match);
            CHECK(match.Snapshot().players.front().movementEpoch == 1);
        }
    }
    PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    std::vector<std::uint64_t> resetTicks;
    for (unsigned tick = 1; tick <= 160; ++tick) {
        const auto before = match.Snapshot().players.front();
        auto input = Window(1, before.lastResolvedCommand + 1, before.lastResolvedCommand + 5, 0);
        input.movementEpoch = before.movementEpoch;
        REQUIRE(match.SubmitInput(input));
        Step(match);
        const auto after = match.Snapshot().players.front();
        if (after.movementEpoch != before.movementEpoch) {
            resetTicks.push_back(tick);
            CHECK(after.lastResolvedCommand == 0);
        }
    }
    REQUIRE(resetTicks.size() == 3);
    CHECK(resetTicks[0] == 31);
    CHECK(resetTicks[1] == 92);
    CHECK(resetTicks[2] == 153);
}

TEST_CASE("PvP fallback reuses executed input rather than an accepted future control") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 1)));
    REQUIRE(match.SubmitInput(Input(1, 3, 0)));
    CHECK(match.Snapshot().players.front().contiguousPendingCommands == 1);
    Step(match, 2);
    CHECK(match.Snapshot().players.front().position.z == doctest::Approx(2.1));
    CHECK(match.Snapshot().players.front().lastResolvedCommand == 2);
    Step(match);
    CHECK(match.Snapshot().players.front().position.z == doctest::Approx(2.1));
}

TEST_CASE("PvP host traces each accepted tuple once and classifies actual held and neutral resolution") {
    using namespace fps::pvp;
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } scope;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(MovementTickSeconds));
    REQUIRE(host.SubmitInput(Input(1, 1)));
    REQUIRE(host.SubmitInput(Input(1, 1)));
    for (unsigned tick = 0; tick < 18; ++tick) {
        REQUIRE(host.SubmitInput(Input(1, 1)));
        static_cast<void>(host.Advance(MovementTickSeconds));
    }
    REQUIRE(host.SubmitInput(Input(1, 19, 0)));
    static_cast<void>(host.Advance(MovementTickSeconds));
    unsigned accepted{}, actual{}, held{}, neutral{};
    for (const auto& event : scope.trace->Drain()) {
        if (event.kind == MovementTraceKind::HostAccepted) ++accepted;
        if (event.kind != MovementTraceKind::Resolved) continue;
        if (event.source == MovementInputSource::Actual) ++actual;
        if (event.source == MovementInputSource::Held) ++held;
        if (event.source == MovementInputSource::Neutral) ++neutral;
    }
    CHECK(accepted == 2);
    CHECK(actual == 2);
    CHECK(held == InputHoldTicks);
    CHECK(neutral == 2);
    CHECK(scope.trace->Dropped() == 0);
}

TEST_CASE("PvP packet count cannot accelerate a player's command clock") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    for (std::uint64_t sequence = 1; sequence <= fps::pvp::MaxFutureCommands; ++sequence)
        REQUIRE(match.SubmitInput(Input(1, sequence, 1, 1)));
    for (unsigned repeat = 0; repeat < 100; ++repeat)
        REQUIRE(match.SubmitInput(Input(1, 1, 1, 1)));
    CHECK_FALSE(match.SubmitInput(Input(1, 33)));
    CHECK(match.Snapshot().players[0].position.x == 2);
    Step(match);
    const auto player = match.Snapshot().players[0];
    CHECK(std::hypot(player.position.x - 2, player.position.z - 2) == doctest::Approx(3.0 / 60));
    CHECK(player.lastResolvedCommand == 1);
    CHECK(match.Snapshot().tick == 1);
    REQUIRE(match.SubmitInput(Input(1, 33)));
}

TEST_CASE("PvP Join normalizes any finite content spawn yaw for the movement wire contract") {
    auto arena = TestArena();
    arena.spawns[0].yaw = 10000000.0F;
    std::string error;
    REQUIRE(arena.Validate(error));
    fps::pvp::PvpMatch match(arena);
    REQUIRE(match.Join(1, error));
    const auto state = match.Snapshot().players[0];
    CHECK(state.yaw == std::remainder(arena.spawns[0].yaw, 2 * std::numbers::pi_v<float>));
    CHECK(fps::pvp::ValidMovementCommand({1, 0, 0, state.yaw, state.pitch}));
    CHECK(state.lastResolvedCommand == 0);
}

TEST_CASE("PvP buffers reordered commands but only starts from sequence one") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 2, 0, 1)));
    Step(match, 5);
    CHECK(match.Snapshot().players[0].lastResolvedCommand == 0);
    CHECK(match.Snapshot().players[0].position.x == 2);
    REQUIRE(match.SubmitInput(Input(1, 1)));
    Step(match, 2);
    const auto player = match.Snapshot().players[0];
    CHECK(player.position.z == doctest::Approx(2.05));
    CHECK(player.position.x == doctest::Approx(2.05));
    CHECK(player.lastResolvedCommand == 2);
    CHECK(match.Snapshot().tick == 7);
}

TEST_CASE("PvP duplicate active Join preserves pose cursor and input lifetime") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(7, error));
    REQUIRE(match.SubmitInput(Input(7, 1, 1, 0, 0, 0.2F)));
    Step(match, 5);
    const auto before = match.Snapshot();
    REQUIRE(match.Join(7, error));
    const auto after = match.Snapshot();
    REQUIRE(after.players.size() == 1);
    CHECK(after.tick == before.tick);
    CHECK(after.players[0].position.z == before.players[0].position.z);
    CHECK(after.players[0].pitch == before.players[0].pitch);
    CHECK(after.players[0].lastResolvedCommand == 5);
    REQUIRE(match.SubmitInput(Input(7, 1, -1))); // Already resolved: ignored.
    Step(match, 11); // Actual command plus fifteen missing commands.
    const auto stopped = match.Snapshot().players[0];
    CHECK(stopped.position.z == doctest::Approx(2.8));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == stopped.position.z);
}

TEST_CASE("PvP rejects invalid batches and immutable command conflicts atomically") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 1)));
    CHECK_FALSE(match.SubmitInput({1, {{1, -1, 0, 0, 0}, {2, 0, 1, 0, 0}}}));
    CHECK_FALSE(match.SubmitInput(Input(2, 1)));
    CHECK_FALSE(match.SubmitInput(Input(1, 2, 2)));
    CHECK_FALSE(match.SubmitInput(Input(1, 2, 1, 0, std::numeric_limits<float>::quiet_NaN())));
    CHECK_FALSE(match.SubmitInput(Input(1, 2, 1, 0, 0, std::numeric_limits<float>::infinity())));
    CHECK_FALSE(match.SubmitInput(Input(1, 2, 1, 0, 1.0e6F + 1)));
    CHECK_FALSE(match.SubmitInput(Input(1, 2, 1, 0, 0, std::numbers::pi_v<float>)));
    CHECK_FALSE(match.SubmitInput({1, {}}));
    CHECK_FALSE(match.SubmitInput({1, {{2, 1, 0, 0, 0}, {1, 1, 0, 0, 0}}}));
    CHECK_FALSE(match.SubmitInput({1, {{1, 1, 0, 0, 0}, {1, 1, 0, 0, 0}}}));
    CHECK_FALSE(match.SubmitInput(Input(1, 0)));
    CHECK_FALSE(match.SubmitInput(Window(1, 1, 13)));
    CHECK_FALSE(match.SubmitInput({1, {{2, 0, 1, 0, 0}, {33, 0, 1, 0, 0}}}));
    Step(match, 2);
    const auto player = match.Snapshot().players[0];
    CHECK(player.position.z == doctest::Approx(2.1));
    CHECK(player.position.x == 2); // No command from either rejected batch leaked.
    CHECK(player.lastResolvedCommand == 2);
}

TEST_CASE("PvP absolute aim is not reapplied as a mouse delta and authority clamps pitch") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 1, 1, 0, std::numbers::pi_v<float> / 2, std::numbers::pi_v<float> / 2)));
    Step(match, 3);
    const auto state = match.Snapshot().players[0];
    CHECK(state.position.x == doctest::Approx(2.15));
    CHECK(state.position.z == doctest::Approx(2.0));
    CHECK(state.yaw == doctest::Approx(std::numbers::pi_v<float> / 2));
    CHECK(state.pitch == doctest::Approx(89 * std::numbers::pi_v<float> / 180));
}

TEST_CASE("PvP authority rejects arbitrary simulation deltas without changing its clock") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 1)));
    CHECK_THROWS_AS(match.Tick({1, 0.1}), std::invalid_argument);
    CHECK_THROWS_AS(match.Tick({1, 0.0}), std::invalid_argument);
    CHECK_THROWS_AS(match.Tick({1, std::numeric_limits<double>::infinity()}), std::invalid_argument);
    CHECK_THROWS_AS(match.Tick({2, fps::pvp::MovementTickSeconds}), std::invalid_argument);
    CHECK(match.TickCount() == 0);
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == doctest::Approx(2.05));
}

TEST_CASE("PvP duplicates cannot refresh hold time and exhausted lead waits for a fresh epoch") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Input(1, 1)));
    for (unsigned tick = 0; tick < 30; ++tick) {
        REQUIRE(match.SubmitInput(Input(1, 1)));
        Step(match);
    }
    const auto stopped = match.Snapshot().players[0];
    CHECK(stopped.position.z == doctest::Approx(2.8));
    CHECK(stopped.lastResolvedCommand == 30);
    REQUIRE(match.SubmitInput(Input(1, 30, -1)));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == stopped.position.z);
    CHECK(match.Snapshot().players[0].movementEpoch == 2);
    CHECK(match.Snapshot().players[0].lastResolvedCommand == 0);
    CHECK_FALSE(match.SubmitInput(Input(1, 31)));
    Step(match, 30);
    CHECK(match.Snapshot().players[0].lastResolvedCommand == 0);
    auto resumed = Input(1, 1);
    resumed.movementEpoch = 2;
    REQUIRE(match.SubmitInput(resumed));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == doctest::Approx(2.85));
    auto neutral = Input(1, 2, 0);
    neutral.movementEpoch = 2;
    REQUIRE(match.SubmitInput(neutral));
    Step(match, fps::pvp::InputHoldTicks + 1);
    CHECK(match.Snapshot().players[0].position.z == doctest::Approx(2.85));
    CHECK(match.Snapshot().players[0].lastResolvedCommand == 17);
}

TEST_CASE("PvP authority and shared movement agree through turns stops walls and corners") {
    const auto arena = TestArena();
    fps::pvp::PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    auto predicted = match.Snapshot().players[0];
    for (std::uint64_t sequence = 1; sequence <= 600; ++sequence) {
        const auto phase = sequence / 100;
        const auto input = Input(1, sequence, phase == 5 ? 0.0F : 1.0F,
            phase == 1 || phase == 4 ? 1.0F : 0.0F,
            phase >= 3 ? std::numbers::pi_v<float> / 2 : 0.0F);
        predicted = fps::pvp::StepMovement(arena, predicted, input.commands.front());
        REQUIRE(match.SubmitInput(input));
        Step(match);
        const auto actual = match.Snapshot().players[0];
        CHECK(actual.position.x == predicted.position.x);
        CHECK(actual.position.y == predicted.position.y);
        CHECK(actual.position.z == predicted.position.z);
        CHECK(actual.yaw == predicted.yaw);
        CHECK(actual.lastResolvedCommand == sequence);
        CHECK(actual.position.x >= arena.radius - 0.0001F);
        CHECK(actual.position.x <= arena.width - arena.radius + 0.0001F);
        CHECK(actual.position.z >= arena.radius - 0.0001F);
        CHECK(actual.position.z <= arena.depth - arena.radius + 0.0001F);
    }
    CHECK_THROWS_AS(static_cast<void>(fps::pvp::StepMovement(arena, predicted, {})), std::invalid_argument);
}

TEST_CASE("PvP pure collision stops at walls while players pass through each other") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    for (std::uint64_t tick = 1; tick <= 140; ++tick) {
        REQUIRE(match.SubmitInput(Input(1, tick, 0, 1)));
        REQUIRE(match.SubmitInput(Input(2, tick, 0, -1)));
        Step(match);
    }
    const auto players = match.Snapshot().players;
    CHECK(players[0].position.x == doctest::Approx(9).epsilon(0.0001));
    CHECK(players[1].position.x == doctest::Approx(2).epsilon(0.0001));
    CHECK(players[0].position.y == 0);
    for (std::uint64_t tick = 141; tick <= 300; ++tick) {
        REQUIRE(match.SubmitInput(Input(1, tick, 0, 1)));
        Step(match);
    }
    CHECK(match.Snapshot().players[0].position.x <= 11.7501F);
    CHECK(match.Snapshot().players[0].position.x > 11.7F);
}

TEST_CASE("PvP shared movement stops at an interior wall and slides around its corner") {
    const auto arena = TestArena();
    fps::pvp::PlayerState state{1, {4, 0, 5}, 0, 0, 0};
    for (std::uint64_t sequence = 1; sequence <= 60; ++sequence)
        state = fps::pvp::StepMovement(arena, state, {sequence, 0, 1, 0, 0});
    CHECK(state.position.x == doctest::Approx(4.75).epsilon(0.0001));
    CHECK(state.position.z == doctest::Approx(5));
    for (std::uint64_t sequence = 61; sequence <= 150; ++sequence)
        state = fps::pvp::StepMovement(arena, state, {sequence, -1, 1, 0, 0});
    CHECK(state.position.x > 6);
    CHECK(state.position.z < 3);
    CHECK(state.position.y == 0);
}

TEST_CASE("PvP host orders controls and preserves complete immutable input windows") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 7));
    REQUIRE(host.QueueLeave(2, 7));
    REQUIRE(host.QueueJoin(3, 8));
    CHECK_FALSE(host.SubmitInput(Input(7, 1)));
    CHECK(host.Advance(1.0 / 60).steps == 1);
    const auto results = host.TakeControlResults();
    REQUIRE(results.size() == 3);
    CHECK(results[0].requestId == 1);
    CHECK(results[1].kind == fps::pvp::ControlKind::Leave);
    CHECK(results[2].accepted);
    CHECK_FALSE(host.SubmitInput(Input(7, 1)));
    REQUIRE(host.SubmitInput(Input(8, 1)));
    REQUIRE(host.SubmitInput(Input(8, 2, 0, 1)));
    REQUIRE(host.SubmitInput(Input(8, 1)));
    CHECK_FALSE(host.SubmitInput({8, {{1, -1, 0, 0, 0}, {3, 0, -1, 0, 0}}}));
    static_cast<void>(host.Advance(2.0 / 60));
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 3);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 8);
    CHECK(snapshot->players[0].position.x == doctest::Approx(2.05));
    CHECK(snapshot->players[0].position.z == doctest::Approx(2.05));
    CHECK(snapshot->players[0].lastResolvedCommand == 2);
    for (std::uint64_t index = 1; index <= 64; ++index) CHECK(host.QueueLeave(index, 8));
    CHECK_FALSE(host.QueueLeave(65, 8));
    static_cast<void>(host.Advance(1.0 / 60));
    CHECK_FALSE(host.QueueLeave(66, 8)); // Unread results also count against the bound.
    CHECK(host.TakeControlResults().size() == 64);
    CHECK(host.QueueLeave(67, 8));
}

TEST_CASE("PvP host bounds merged ingress and checks commands already queued in authority") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(1.0 / 60));
    REQUIRE(host.SubmitInput(Window(1, 1, 12)));
    REQUIRE(host.SubmitInput(Window(1, 13, 24)));
    REQUIRE(host.SubmitInput(Window(1, 25, 32)));
    CHECK_FALSE(host.SubmitInput(Input(1, 33)));
    CHECK_FALSE(host.SubmitInput(Input(1, 12, -1)));
    static_cast<void>(host.Advance(1.0 / 60));
    REQUIRE(host.SubmitInput(Input(1, 12)));
    CHECK_FALSE(host.SubmitInput(Input(1, 12, -1)));
    REQUIRE(host.SubmitInput(Input(1, 33)));
    for (unsigned step = 0; step < 6; ++step) static_cast<void>(host.Advance(5.0 / 60));
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->players[0].movementEpoch == 2);
    CHECK(snapshot->players[0].lastResolvedCommand == 0);
    CHECK(snapshot->players[0].position.z == doctest::Approx(3.5));
}

TEST_CASE("PvP host publishes exact cadence and catch-up simulates one command per tick") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(2.0 / 60));
    const auto initial = host.TakeSnapshot();
    REQUIRE(initial);
    CHECK(initial->tick == 2);
    REQUIRE(host.SubmitInput(Window(1, 1, 12)));
    const auto advance = host.Advance(1.0);
    CHECK(advance.steps == 5);
    CHECK(advance.droppedSeconds > 0.9);
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 7); // Latest state after all five bounded catch-up steps.
    CHECK(snapshot->players[0].position.z == doctest::Approx(2.25));
    CHECK(snapshot->players[0].lastResolvedCommand == 5);
    CHECK_FALSE(host.TakeSnapshot());
}

TEST_CASE("PvP host Leave discards accepted commands before same-id rejoin") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 7));
    static_cast<void>(host.Advance(1.0 / 60));
    const auto joined = host.TakeControlResults();
    REQUIRE(joined.size() == 1);
    REQUIRE(joined[0].accepted);
    REQUIRE(host.SubmitInput(Window(7, 1, 12, 0, 1)));
    REQUIRE(host.QueueLeave(2, 7));
    REQUIRE(host.QueueJoin(3, 7));
    static_cast<void>(host.Advance(2.0 / 60));
    const auto controls = host.TakeControlResults();
    REQUIRE(controls.size() == 2);
    CHECK(controls[0].kind == fps::pvp::ControlKind::Leave);
    CHECK(controls[0].accepted);
    REQUIRE(controls[1].accepted);
    const auto fresh = host.TakeSnapshot();
    REQUIRE(fresh);
    REQUIRE(fresh->players.size() == 1);
    CHECK(fresh->players[0].position.x == 2);
    CHECK(fresh->players[0].position.z == 2);
    CHECK(fresh->players[0].lastResolvedCommand == 0);
    REQUIRE(host.SubmitInput(Input(7, 1)));
    static_cast<void>(host.Advance(3.0 / 60));
    const auto moved = host.TakeSnapshot();
    REQUIRE(moved);
    CHECK(moved->players[0].position.z == doctest::Approx(2.15));
    CHECK(moved->players[0].lastResolvedCommand == 3);
}

TEST_CASE("PvP host disconnect reset is acknowledged before a new session") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(3.0 / 60));
    REQUIRE(host.SubmitInput(Window(1, 1, 12)));
    REQUIRE(host.QueueJoin(2, 2));
    auto reset = host.RequestReset();
    CHECK(reset.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
    CHECK_FALSE(host.QueueJoin(3, 3));
    CHECK_FALSE(host.SubmitInput(Input(1, 2)));
    CHECK(host.Advance(1).steps == 0);
    CHECK(reset.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    CHECK_NOTHROW(reset.get());
    CHECK(host.TakeControlResults().empty());
    CHECK_FALSE(host.TakeSnapshot());
    REQUIRE(host.QueueJoin(4, 1));
    static_cast<void>(host.Advance(3.0 / 60));
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 3);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 1);
    CHECK(snapshot->players[0].position.z == 2);
    CHECK(snapshot->players[0].lastResolvedCommand == 0);
}

TEST_CASE("PvP arena validates content without campaign or rendering dependencies") {
    auto arena = TestArena();
    std::string error;
    CHECK(arena.Validate(error));
    arena.version = 2;
    CHECK_FALSE(arena.Validate(error));
    arena.version = 1;
    arena.spawns[1] = arena.spawns[0];
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.spawns[0].position.y = 1;
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.spawns[0].position = {5.5F, 0, 5};
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.walls[0].maximum.x = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(arena.Validate(error));
}

TEST_CASE("PvP arena loads its versioned standalone content contract") {
#ifdef PVP_DOMAIN_TEST_ARENA_PATH
    const std::filesystem::path fixture = PVP_DOMAIN_TEST_ARENA_PATH;
#else
    const auto fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures/arena.json";
#endif
    std::string error;
    const auto arena = fps::pvp::Arena::Load(fixture, error);
    INFO(error);
    REQUIRE(arena);
    CHECK(arena->id == "synthetic_contract_arena");
    CHECK(arena->version == 1);
    CHECK(arena->walls.size() == 4);
    CHECK(arena->spawns.size() == 2);
    CHECK(arena->movementSpeed == 3);
    CHECK_FALSE(fps::pvp::Arena::Load(fixture.parent_path() / "missing-arena.json", error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("PvP host owns its clock and reset completes independently of I/O") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    std::jthread simulation([&](std::stop_token stop) { host.Run(stop); });
    auto reset = host.RequestReset();
    REQUIRE(reset.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK_NOTHROW(reset.get());
    REQUIRE(host.QueueJoin(1, 1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::optional<fps::pvp::WorldSnapshot> snapshot;
    while (!snapshot && std::chrono::steady_clock::now() < deadline) {
        auto candidate = host.TakeSnapshot();
        if (candidate && !candidate->players.empty()) snapshot = std::move(candidate);
        if (!snapshot) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(snapshot);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 1);
    simulation.request_stop();
}

TEST_CASE("PvP exhausted lead rotates at the next boundary and awaits a new epoch without looping") {
    using namespace fps::pvp;
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } scope;
    PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    for (unsigned tick = 1; tick <= MovementBacklogSampleTicks; ++tick) {
        REQUIRE(match.SubmitInput(Input(1, 1))); // Late copies cannot restore executed lead.
        REQUIRE(match.SubmitInput(Input(2, tick, 0)));
        Step(match);
        CHECK(match.Snapshot().players.front().movementEpoch == 1);
        CHECK(match.Snapshot().players.front().lastResolvedCommand == tick);
    }
    const auto before = match.Snapshot();
    REQUIRE(match.SubmitInput(Input(2, 31, 0)));
    Step(match);
    const auto after = match.Snapshot();
    CHECK(after.tick == before.tick + 1);
    CHECK(after.players.front().movementEpoch == 2);
    CHECK(after.players.front().lastResolvedCommand == 0);
    CHECK(after.players.front().position.z == before.players.front().position.z);
    CHECK(after.players.back().lastResolvedCommand == before.players.back().lastResolvedCommand + 1);
    CHECK_FALSE(match.SubmitInput(Input(1, 1)));
    Step(match, 600);
    const auto awaiting = match.Snapshot().players.front();
    CHECK(awaiting.movementEpoch == 2);
    CHECK(awaiting.lastResolvedCommand == 0);
    CHECK(awaiting.position.z == before.players.front().position.z);
    auto resumed = Input(1, 1);
    resumed.movementEpoch = 2;
    REQUIRE(match.SubmitInput(resumed));
    Step(match);
    CHECK(match.Snapshot().players.front().lastResolvedCommand == 1);
    unsigned resets{};
    for (const auto& event : scope.trace->Drain()) {
        if (event.kind != MovementTraceKind::Reset || event.playerId != 1) continue;
        ++resets;
        CHECK(event.resetReason == MovementResetReason::Starvation);
        CHECK(event.authorityTick == 31);
        CHECK(TraceResetReasonName(event.resetReason) == "starvation");
    }
    CHECK(resets == 1);
}

TEST_CASE("PvP lead fuse permits all Actual zero queues and expires correlated fallback samples") {
    using namespace fps::pvp;
    for (const bool oneEarlyFallback : {false, true}) {
        PvpMatch match(TestArena());
        std::string error;
        REQUIRE(match.Join(1, error));
        for (unsigned tick = 1; tick <= 180; ++tick) {
            if (!(oneEarlyFallback && tick == 2)) REQUIRE(match.SubmitInput(Input(1, tick)));
            // This positive queue sample prevents the early fallback from
            // triggering a reset; both must age out of the same 30-tick window.
            if (oneEarlyFallback && tick == 3) REQUIRE(match.SubmitInput(Input(1, 4)));
            Step(match);
            CHECK(match.Snapshot().players.front().movementEpoch == 1);
        }
    }
}

TEST_CASE("PvP exhausted lead respects the complete future map and the existing reset cooldown") {
    using namespace fps::pvp;
    SUBCASE("Sparse future commands prevent an empty-buffer reset") {
        PvpMatch match(TestArena());
        std::string error;
        REQUIRE(match.Join(1, error));
        REQUIRE(match.SubmitInput(Input(1, 1)));
        Step(match);
        REQUIRE(match.SubmitInput(Input(1, 33)));
        for (unsigned tick = 2; tick <= 33; ++tick) {
            Step(match);
            CHECK(match.Snapshot().players.front().movementEpoch == 1);
        }
        CHECK(match.Snapshot().players.front().lastResolvedCommand == 33);
    }
    SUBCASE("Repeated running starvation waits for the original cooldown") {
        PvpMatch match(TestArena());
        std::string error;
        REQUIRE(match.Join(1, error));
        REQUIRE(match.SubmitInput(Input(1, 1)));
        Step(match, 31);
        const auto resetTick = match.TickCount();
        REQUIRE(match.Snapshot().players.front().movementEpoch == 2);
        auto resumed = Input(1, 1);
        resumed.movementEpoch = 2;
        REQUIRE(match.SubmitInput(resumed));
        Step(match);
        while (match.TickCount() < resetTick + MovementResetCooldownTicks) {
            Step(match);
            CHECK(match.Snapshot().players.front().movementEpoch == 2);
        }
        Step(match);
        CHECK(match.Snapshot().players.front().movementEpoch == 3);
        CHECK(match.TickCount() == resetTick + MovementResetCooldownTicks + 1);
    }
}
