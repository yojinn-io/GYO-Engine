#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <map>
#include <numbers>
#include <optional>
#include <thread>
#include <variant>
#include <vector>

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

template <class Values, class Project>
std::uint64_t SumOf(const Values& values, Project project) {
    std::uint64_t sum{};
    for (const auto& value : values) sum += project(value);
    return sum;
}

// I2 and its input and action counterparts for one bucket of the host's counts.
void CheckIngressConservation(const fps::pvp::MatchIngressCounts& counts) {
    const auto hostClassifiedCommands = SumOf(counts.classified, [](auto value) { return value; }) +
        SumOf(counts.rejected, [](const auto& value) { return value.commands; });
    const auto hostReceivedCommands = counts.received.commands;
    CHECK(hostClassifiedCommands == hostReceivedCommands);
    const auto hostSettledInputs = counts.acceptedInputs + SumOf(counts.rejected, [](const auto& value) { return value.inputs; });
    CHECK(hostSettledInputs == counts.received.inputs);
    const auto hostSettledBatches = counts.acceptedActions.batches +
        SumOf(counts.rejectedActions, [](const auto& value) { return value.batches; });
    CHECK(hostSettledBatches == counts.receivedActions.batches);
    const auto hostSettledShots = counts.acceptedActions.shots +
        SumOf(counts.rejectedActions, [](const auto& value) { return value.shots; });
    CHECK(hostSettledShots == counts.receivedActions.shots);
}

// J5a for one bucket whose substitution records are all closed: every
// substituted sequence either arrived once (late_first) or closed unarrived.
void CheckSubstitutionConservation(const fps::pvp::MatchIngressCounts& counts) {
    const auto substitutedRecords = counts.substitutedCommands;
    const auto lateFirstAndUnarrived = counts.classified[fps::pvp::IngressIndex(fps::pvp::IngressCommandClass::LateFirst)] +
        SumOf(counts.unarrived, [](auto value) { return value; });
    CHECK(substitutedRecords == lateFirstAndUnarrived);
}

fps::pvp::PlayerInput EpochInput(fps::pvp::PlayerId playerId, std::uint64_t first, std::uint64_t last,
    std::uint64_t epoch, std::uint64_t life = 1, float forward = 1) {
    auto result = Window(playerId, first, last, forward);
    result.movementEpoch = epoch;
    result.lifeGeneration = life;
    return result;
}
}

TEST_CASE("PvP authority joins at distinct valid spawns and owns membership") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    CHECK_FALSE(match.Join(0, error));
    REQUIRE(match.Join(7, error));
    REQUIRE(match.Join(8, error));
    CHECK(match.Join(7, error));
    // Two spawns, both occupied: the third player cannot be placed (the room holds four).
    CHECK_FALSE(match.Join(9, error));
    CHECK(error == "spawn_blocked");
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

TEST_CASE("PvP host reports movement slack from first receipt and late arrivals as negative") {
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now{};
    fps::pvp::MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK_FALSE(snapshot->players[0].movementSlackSequence);
    CHECK_FALSE(snapshot->players[0].movementSlackMicros);
    now += 3ms;
    REQUIRE(host.SubmitInput(Window(5, 1, 3)));
    now += 4ms;
    REQUIRE(host.SubmitInput(Window(5, 1, 4))); // A retransmission keeps the first receipt.
    now += 7ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].lastResolvedCommand == 1);
    CHECK(snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{1});
    CHECK(snapshot->players[0].movementSlackMicros == std::optional<std::int32_t>{11000});
    // Each published snapshot carries the smallest sample since the previous one.
    now += 17ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{2});
    CHECK(snapshot->players[0].movementSlackMicros == std::optional<std::int32_t>{28000});
    // Catch-up ticks inside one Advance publish once, with the smaller sample.
    now += 34ms;
    REQUIRE(host.Advance(2.0 / 60).steps == 2);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].lastResolvedCommand == 4);
    CHECK(snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{4});
    CHECK(snapshot->players[0].movementSlackMicros == std::optional<std::int32_t>{58000});
    // Sequence 5 never arrived in time: substituted, no sample.
    now += 17ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].lastResolvedCommand == 5);
    CHECK_FALSE(snapshot->players[0].movementSlackSequence);
    // Its late arrival reports how late it was, once.
    now += 8ms;
    REQUIRE(host.SubmitInput(Window(5, 5, 6)));
    REQUIRE(host.SubmitInput(Window(5, 5, 6)));
    now += 9ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].lastResolvedCommand == 6);
    CHECK(snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{5});
    CHECK(snapshot->players[0].movementSlackMicros == std::optional<std::int32_t>{-8000});
    now += 17ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK_FALSE(snapshot->players[0].movementSlackSequence); // sequence 7 substituted

    REQUIRE(host.QueueLeave(2, 5));
    REQUIRE(host.QueueJoin(3, 6));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 6);
    CHECK_FALSE(snapshot->players[0].movementSlackSequence);
}

TEST_CASE("PvP match counts resolved, substituted and reset movement for connection quality") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    CHECK_FALSE(match.GetMovementQuality(2));
    REQUIRE(match.GetMovementQuality(1));
    CHECK(match.GetMovementQuality(1)->resolved == 0);
    REQUIRE(match.SubmitInput(Window(1, 1, 2)));
    Step(match, 2);
    auto quality = *match.GetMovementQuality(1);
    CHECK(quality.resolved == 2);
    CHECK(quality.substituted == 0);
    // Without input the cursor runs on Held and then Neutral steps until the
    // Starvation fuse resets the epoch; the reset counts, the respawn would not.
    Step(match, 40);
    quality = *match.GetMovementQuality(1);
    CHECK(quality.resets == 1);
    CHECK(quality.substituted + 2 == quality.resolved);
    CHECK(quality.substituted >= 28);
}

TEST_CASE("PvP host evicts a player after three failed connection-quality windows") {
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now{};
    fps::pvp::MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.QueueJoin(2, 6));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // The joins happen at this tick.
    REQUIRE(host.TakeSnapshot());
    std::map<fps::pvp::PlayerId, std::uint64_t> next{{5, 1}, {6, 1}};
    std::uint64_t latestTick{1};
    std::map<std::uint64_t, std::uint32_t> failuresAt;
    std::optional<std::uint64_t> evictedAt;
    std::vector<fps::pvp::Eviction> evictions;
    // Both players deliver every command in time. Player 6 reports the latest
    // snapshot; player 5 one twelve ticks (200 ms) old, as with a high ping.
    const auto windowTicks = fps::pvp::ConnectionQualityWindowTicks;
    for (std::uint64_t step = 1; step < 4 * windowTicks + 30; ++step) {
        for (auto& [player, sequence] : next) {
            if (evictedAt && player == 5) continue;
            auto window = Window(player, sequence, sequence, 0);
            window.observedAuthorityTick = player == 5 ? (latestTick > 12 ? latestTick - 12 : 0) : latestTick;
            if (window.observedAuthorityTick == 0) window.observedAuthorityTick = latestTick;
            REQUIRE(host.SubmitInput(window));
            ++sequence;
        }
        now += std::chrono::nanoseconds(16'666'667);
        REQUIRE(host.Advance(1.0 / 60).steps == 1);
        for (const auto& eviction : host.TakeEvictions()) {
            evictions.push_back(eviction);
            evictedAt = latestTick + 1;
        }
        const auto snapshot = host.TakeSnapshot();
        REQUIRE(snapshot);
        latestTick = snapshot->tick;
        for (const auto& player : snapshot->players) {
            if (player.playerId == 6) CHECK(player.connectionQualityFailures == 0);
            if (player.playerId == 5) failuresAt[snapshot->tick] = player.connectionQualityFailures;
        }
        if (evictedAt) CHECK(std::none_of(snapshot->players.begin(), snapshot->players.end(),
            [](const auto& player) { return player.playerId == 5; }));
    }
    // The first window after the join is not judged; each later one fails.
    CHECK(failuresAt[windowTicks + 10] == 0);
    CHECK(failuresAt[2 * windowTicks + 10] == 1);
    CHECK(failuresAt[3 * windowTicks + 10] == 2);
    REQUIRE(evictions.size() == 1);
    CHECK(evictions[0].playerId == 5);
    CHECK(evictions[0].reason == fps::pvp::EvictionReason::HighLatency);
    CHECK(evictions[0].referenceAgeMillis >= 199);
    CHECK(evictions[0].referenceAgeMillis <= 201);
    CHECK(evictions[0].substitutedPermille == 0);
    CHECK(evictions[0].movementResets == 0);
    REQUIRE(evictedAt);
    CHECK(*evictedAt == 4 * windowTicks);
    CHECK_FALSE(host.SubmitInput(Window(5, next[5], next[5])));
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
    // A zero-thickness wall is valid geometry but an empty wall: content error.
    arena = TestArena();
    arena.walls[4].maximum.x = arena.walls[4].minimum.x;
    CHECK_FALSE(arena.Validate(error));
    CHECK(error == "Arena wall must be a finite non-empty AABB");
    arena = TestArena();
    std::swap(arena.walls[4].minimum.z, arena.walls[4].maximum.z);
    CHECK_FALSE(arena.Validate(error));
    // A body exactly two radii high (a sphere) is valid, one representable value less is not.
    arena = TestArena();
    arena.bodyHeight = arena.eyeHeight = 2 * arena.radius;
    CHECK(arena.Validate(error));
    arena.bodyHeight = arena.eyeHeight = std::nextafter(2 * arena.radius, 0.0F);
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
    std::string error;
    REQUIRE(host.Start(error));
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
    host.Stop();
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

TEST_CASE("PvP fixed jump follows analytic height and consumes midair edges without a landing queue") {
    using namespace fps::pvp;
    const auto arena = TestArena();
    PlayerState state{1, {2, 0, 2}};
    float maximumHeight{};
    unsigned landingTick{};
    for (std::uint64_t sequence = 1; sequence <= 70; ++sequence) {
        state = StepMovement(arena, state, {sequence, 0, 0, 0, 0, sequence == 1 || sequence == 8});
        maximumHeight = (std::max)(maximumHeight, state.position.y);
        CHECK(state.position.y >= 0);
        if (sequence == 1) {
            const float launch = std::sqrt(2 * arena.gravity * arena.jumpHeight);
            CHECK(state.position.y == doctest::Approx(launch / 60 - arena.gravity / 7200));
            CHECK(state.verticalVelocity == doctest::Approx(launch - arena.gravity / 60));
            CHECK_FALSE(state.grounded);
        }
        if (state.grounded && landingTick == 0) landingTick = static_cast<unsigned>(sequence);
        if (landingTick != 0) {
            CHECK(state.position.y == 0);
            CHECK(state.verticalVelocity == 0);
        }
    }
    CHECK(maximumHeight == doctest::Approx(0.6).epsilon(0.002));
    CHECK(landingTick == 31);
    state = StepMovement(arena, state, {71, 0, 0, 0, 0, true});
    CHECK(state.position.y > 0);
    CHECK_FALSE(state.grounded);
}

TEST_CASE("PvP actual jump is never repeated by held or neutral command substitution") {
    using namespace fps::pvp;
    auto arena = TestArena();
    arena.walls.push_back({{0, 2, 0}, {12, 2.2F, 12}});
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, {{1, 0, 0, 0, 0, true}}}));
    Step(match);
    REQUIRE(match.Snapshot().players.front().position.y > 0);
    bool landed{};
    for (unsigned tick = 2; tick <= 25; ++tick) {
        Step(match);
        const auto state = match.Snapshot().players.front();
        CHECK(state.position.y <= 0.2001F);
        if (state.grounded) landed = true;
        if (landed) CHECK(state.position.y == 0);
        CHECK(state.lastResolvedCommand == tick);
        CHECK(state.movementEpoch == 1);
    }
    REQUIRE(landed); // Ceiling shortens the arc into the held-input interval.
}

TEST_CASE("PvP jump sweeps the full capsule against ceiling walls and corners") {
    using namespace fps::pvp;
    auto arena = TestArena();
    arena.walls.push_back({{0, 2, 0}, {12, 2.2F, 12}});
    PlayerState state{1, {4.6F, 0, 5}};
    bool touchedCeiling{};
    for (std::uint64_t sequence = 1; sequence <= 90; ++sequence) {
        state = StepMovement(arena, state, {sequence, sequence < 40 ? 0.0F : -1.0F,
            1, 0, 0, sequence == 1});
        CHECK(fps::CanPlaceCharacterBody({{state.position.x, state.position.y, state.position.z},
            arena.bodyHeight, arena.radius}, arena.walls, {}));
        CHECK(state.position.y >= 0);
        CHECK(state.position.y <= 0.2001F);
        if (state.position.y > 0.19F && state.verticalVelocity == 0) {
            touchedCeiling = true;
            CHECK_FALSE(state.grounded);
        }
        if (sequence < 35) CHECK(state.position.x <= 4.7501F);
    }
    CHECK(touchedCeiling);
    CHECK(state.grounded);
    CHECK(state.position.y == 0);
    CHECK(state.position.x > 5); // Sliding clears the wall corner after landing.
}

TEST_CASE("PvP capsule lands on a raised wall top and falls when walking off its support") {
    using namespace fps::pvp;
    auto arena = TestArena();
    arena.walls.push_back({{1, 0, 3}, {4, 0.3F, 4}});
    PlayerState state{1, {2, 0, 2.5F}};
    bool landedOnTop{};
    for (std::uint64_t sequence = 1; sequence <= 75; ++sequence) {
        state = StepMovement(arena, state, {sequence, 1, 0, 0, 0, sequence == 1});
        CHECK(fps::CanPlaceCharacterBody({{state.position.x, state.position.y, state.position.z},
            arena.bodyHeight, arena.radius}, arena.walls, {}));
        if (state.grounded && state.position.y > 0.29F) {
            landedOnTop = true;
            CHECK(state.verticalVelocity == 0);
        }
    }
    CHECK(landedOnTop);
    CHECK(state.position.z > 5.9F);
    CHECK(state.position.y == 0);
    CHECK(state.grounded);
    CHECK(state.verticalVelocity == 0);
}

TEST_CASE("PvP authority replays the same vertical state and binds commands to life generation") {
    using namespace fps::pvp;
    const auto arena = TestArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    auto predicted = match.Snapshot().players.front();
    for (std::uint64_t sequence = 1; sequence <= 70; ++sequence) {
        const MovementCommand command{sequence, 1, 0, 0, 0.6F, sequence == 1 || sequence == 8};
        REQUIRE(match.SubmitInput({1, {command}, 1, 1}));
        predicted = StepMovement(arena, predicted, command);
        Step(match);
        const auto state = match.Snapshot().players.front();
        CHECK(state.position.x == predicted.position.x);
        CHECK(state.position.y == predicted.position.y);
        CHECK(state.position.z == predicted.position.z);
        CHECK(state.verticalVelocity == predicted.verticalVelocity);
        CHECK(state.grounded == predicted.grounded);
        CHECK(state.lifeGeneration == 1);
        CHECK(state.lifeState == LifeState::Alive);
    }
    CHECK_FALSE(match.SubmitInput({1, {{71, 0, 0, 0, 0}}, 1, 0}));
    CHECK_FALSE(match.SubmitInput({1, {{71, 0, 0, 0, 0}}, 1, 2}));
    CHECK(match.SubmitInput({1, {{71, 0, 0, 0, 0, true}}, 1, 1}));
    CHECK_FALSE(match.SubmitInput({1, {{71, 0, 0, 0, 0, false}}, 1, 1}));
}

TEST_CASE("PvP arena rejects invalid jump settings and loads their authoritative defaults") {
    using namespace fps::pvp;
    std::string error;
    for (const float bad : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
        auto arena = TestArena();
        arena.jumpHeight = bad;
        CHECK_FALSE(arena.Validate(error));
        arena = TestArena();
        arena.gravity = bad;
        CHECK_FALSE(arena.Validate(error));
    }
    auto arena = TestArena();
    arena.jumpHeight = arena.gravity = std::numeric_limits<float>::max();
    CHECK_FALSE(arena.Validate(error));
    const auto loaded = Arena::Load(PVP_DOMAIN_TEST_ARENA_PATH, error);
    REQUIRE(loaded);
    CHECK(loaded->jumpHeight == 0.6F);
    CHECK(loaded->gravity == 18);
}

TEST_CASE("PvP host counts every input it refuses by reason, in inputs and in commands") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 7));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    // A standing backlog of four rotates the epoch while five commands wait.
    for (std::uint64_t sequence = 1; sequence <= MovementBacklogSampleTicks + 1; ++sequence) {
        REQUIRE(host.SubmitInput(Window(7, sequence, sequence + 4)));
        REQUIRE(host.Advance(1.0 / 60).steps == 1);
    }
    const auto rotated = host.TakeSnapshot();
    REQUIRE(rotated);
    REQUIRE(rotated->players[0].movementEpoch == 2);
    REQUIRE(rotated->players[0].lastResolvedCommand == 0);
    auto backlog = host.TakeIngressStatistics();
    REQUIRE(backlog.size() == 2); // player 0 and player 7
    const auto& backlogCounts = backlog.at(7);
    const auto rotationDiscarded = backlogCounts.discarded[IngressIndex(IngressStagedDiscard::RotationDiscarded)];
    CHECK(rotationDiscarded == 5);
    CHECK(backlogCounts.classified[IngressIndex(IngressCommandClass::AcceptedNew)] == MovementBacklogSampleTicks + 5);
    CheckIngressConservation(backlogCounts);

    CHECK_FALSE(host.SubmitInput(EpochInput(7, 1, 1, 1)));
    CHECK_FALSE(host.SubmitInput(EpochInput(7, 1, 2, 3)));
    CHECK_FALSE(host.SubmitInput(EpochInput(7, 1, 1, 2, 2)));
    CHECK_FALSE(host.SubmitInput(EpochInput(7, MaxFutureCommands + 1, MaxFutureCommands + 2, 2)));
    CHECK_FALSE(host.SubmitInput({7, {}, 2, 1}));
    CHECK_FALSE(host.SubmitInput(EpochInput(7, 1, MaxPendingCommands + 1, 2)));
    CHECK_FALSE(host.SubmitInput(Input(9, 1)));
    REQUIRE(host.SubmitInput(EpochInput(7, 1, 3, 2)));
    CHECK_FALSE(host.SubmitInput(EpochInput(7, 2, 2, 2, 1, -1))); // differs from the staged command
    REQUIRE(host.SubmitInput(EpochInput(7, 1, 3, 2)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // resolves 1; 2 and 3 wait in the Match
    CHECK_FALSE(host.SubmitInput(EpochInput(7, 2, 2, 2, 1, -1))); // differs from the queued command
    REQUIRE(host.SubmitInput(EpochInput(7, 1, 3, 2)));
    for (unsigned step = 0; step < 3; ++step) REQUIRE(host.Advance(1.0 / 60).steps == 1); // 2, 3, then 4 substituted
    REQUIRE(host.SubmitInput(EpochInput(7, 4, 4, 2)));
    REQUIRE(host.SubmitInput(EpochInput(7, 4, 4, 2)));

    const auto taken = host.TakeIngressStatistics();
    REQUIRE(taken.size() == 2);
    const auto& counts = taken.at(7);
    const auto rejected = [&](IngressInputRejection reason) { return counts.rejected[IngressIndex(reason)]; };
    const auto classified = [&](IngressCommandClass value) { return counts.classified[IngressIndex(value)]; };
    const auto epochOldInputs = rejected(IngressInputRejection::EpochOld).inputs;
    CHECK(epochOldInputs == 1);
    CHECK(rejected(IngressInputRejection::EpochOld).commands == 1);
    const auto epochFutureInputs = rejected(IngressInputRejection::EpochFuture).inputs;
    CHECK(epochFutureInputs == 1);
    const auto epochFutureCommands = rejected(IngressInputRejection::EpochFuture).commands;
    CHECK(epochFutureCommands == 2);
    const auto lifeFutureInputs = rejected(IngressInputRejection::LifeFuture).inputs;
    CHECK(lifeFutureInputs == 1);
    const auto beyondWindowCommands = rejected(IngressInputRejection::BeyondWindow).commands;
    CHECK(beyondWindowCommands == 2);
    const auto malformedInputs = rejected(IngressInputRejection::Malformed).inputs;
    CHECK(malformedInputs == 2);
    const auto malformedCommands = rejected(IngressInputRejection::Malformed).commands;
    CHECK(malformedCommands == MaxPendingCommands + 1);
    const auto stagedConflicts = rejected(IngressInputRejection::ConflictStaged).inputs;
    CHECK(stagedConflicts == 1);
    const auto queuedConflicts = rejected(IngressInputRejection::ConflictQueued).inputs;
    CHECK(queuedConflicts == 1);
    CHECK(rejected(IngressInputRejection::LifeOld).inputs == 0);
    CHECK(rejected(IngressInputRejection::Resetting).inputs == 0);
    CHECK(rejected(IngressInputRejection::UnknownPlayer).inputs == 0);
    const auto stagedOverWindowInputs = rejected(IngressInputRejection::StagedOverWindow).inputs;
    CHECK(stagedOverWindowInputs == 0); // unreachable by construction
    const auto acceptedNewCommands = classified(IngressCommandClass::AcceptedNew);
    CHECK(acceptedNewCommands == 3);
    const auto pendingCopies = classified(IngressCommandClass::PendingCopy);
    CHECK(pendingCopies == 5);
    const auto lateFirstCommands = classified(IngressCommandClass::LateFirst);
    CHECK(lateFirstCommands == 1);
    // Sequence 1 was executed: its copy is resolved_copy. The second copy of
    // the substituted 4 is late_copy.
    const auto resolvedUntracked = classified(IngressCommandClass::ResolvedUntracked);
    CHECK(resolvedUntracked == 0);
    CHECK(classified(IngressCommandClass::ResolvedCopy) == 1);
    CHECK(classified(IngressCommandClass::LateCopy) == 1);
    CHECK(counts.lateOnlyInputs == 2);
    CHECK(counts.acceptedInputs == 5);
    CHECK(counts.discarded[IngressIndex(IngressStagedDiscard::HandoffRejected)] == 0);
    CheckIngressConservation(counts);
    // An unknown player's input counts under player 0, never under the id it claims.
    const auto& unknown = taken.at(0);
    const auto unknownPlayerInputs = unknown.rejected[IngressIndex(IngressInputRejection::UnknownPlayer)].inputs;
    CHECK(unknownPlayerInputs == 1);
    CHECK(unknown.received.commands == 1);
    CheckIngressConservation(unknown);
}

TEST_CASE("PvP host counts action batches by outcome in batches and shots") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitActionBatch({1, {{1, 1, 0, 0}}}, 0) == ActionAdmission::Accepted);
    CHECK(host.SubmitActionBatch({1, {{1, 1, 0.5F, 0}}}, 0) == ActionAdmission::Conflict);
    CHECK(host.SubmitActionBatch({9, {{1, 1, 0, 0}}}, 0) == ActionAdmission::InvalidPlayer);
    CHECK(host.SubmitActionBatch({1, {{2, 1, 0, 0}}}, 5) == ActionAdmission::InvalidBatch);
    CHECK(host.SubmitActions({1, {}}) == ActionAdmission::InvalidBatch);
    CHECK(host.SubmitActions({9, {}}) == ActionAdmission::InvalidBatch);
    CHECK(host.SubmitActionBatch({1, {{MaxActionWindow + 1, 1, 0, 0}}}, 0) == ActionAdmission::OutsideWindow);
    host.NoteWireRejection(1, IngressActionRejection::OverBatch, MaxActionBatch + 1);
    host.NoteWireRejection(9, IngressActionRejection::Malformed, 2);
    CHECK_THROWS_AS(host.NoteWireRejection(1, IngressActionRejection::Conflict, 1), std::logic_error);

    const auto taken = host.TakeIngressStatistics();
    CHECK(taken.size() == 2);
    const auto& counts = taken.at(1);
    const auto refused = [&](IngressActionRejection reason) { return counts.rejectedActions[IngressIndex(reason)]; };
    const auto hostAcceptedBatches = counts.acceptedActions.batches;
    CHECK(hostAcceptedBatches == 1);
    const auto hostConflictBatches = refused(IngressActionRejection::Conflict).batches;
    CHECK(hostConflictBatches == 1);
    const auto hostInvalidBatches = refused(IngressActionRejection::InvalidBatch).batches;
    CHECK(hostInvalidBatches == 2);
    CHECK(refused(IngressActionRejection::InvalidBatch).shots == 1);
    const auto hostOutsideWindowShots = refused(IngressActionRejection::OutsideWindow).shots;
    CHECK(hostOutsideWindowShots == 1);
    const auto hostOverBatchShots = refused(IngressActionRejection::OverBatch).shots;
    CHECK(hostOverBatchShots == MaxActionBatch + 1);
    CheckIngressConservation(counts);
    const auto& unknown = taken.at(0);
    const auto hostInvalidPlayerBatches = unknown.rejectedActions[IngressIndex(IngressActionRejection::InvalidPlayer)].batches;
    CHECK(hostInvalidPlayerBatches == 1);
    CHECK(unknown.rejectedActions[IngressIndex(IngressActionRejection::Malformed)].shots == 2);
    const auto unknownEmptyBatches = unknown.rejectedActions[IngressIndex(IngressActionRejection::InvalidBatch)].batches;
    CHECK(unknownEmptyBatches == 1);
    CheckIngressConservation(unknown);
}

TEST_CASE("PvP host ingress counts survive Leave and reset until they are taken") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Window(1, 1, 3)));
    REQUIRE(host.QueueLeave(2, 1));
    REQUIRE(host.QueueJoin(3, 2));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // Leave discards the three staged commands
    auto taken = host.TakeIngressStatistics();
    CHECK(taken.size() == 3); // player 0, the departed 1 and the current 2
    const auto countsAfterLeave = taken[1].classified[IngressIndex(IngressCommandClass::AcceptedNew)];
    CHECK(countsAfterLeave == 3);
    const auto leaveDiscarded = taken[1].discarded[IngressIndex(IngressStagedDiscard::LeaveDiscarded)];
    CHECK(leaveDiscarded == 3);
    CHECK(taken[2].received.inputs == 0);

    REQUIRE(host.SubmitInput(Window(2, 1, 3)));
    auto reset = host.RequestReset();
    CHECK_FALSE(host.SubmitInput(Input(2, 4)));
    CHECK(host.SubmitActionBatch({2, {{1, 1, 0, 0}}}, 0) == ActionAdmission::InvalidPlayer);
    CHECK(host.Advance(1).steps == 0); // the reset clears the match
    REQUIRE(reset.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    taken = host.TakeIngressStatistics();
    CHECK(taken.size() == 2); // player 0 and the reset 2
    const auto countsAfterReset = taken[2].classified[IngressIndex(IngressCommandClass::AcceptedNew)];
    CHECK(countsAfterReset == 3);
    const auto resettingInputs = taken[2].rejected[IngressIndex(IngressInputRejection::Resetting)].inputs;
    CHECK(resettingInputs == 1);
    const auto resettingActionBatches = taken[2].rejectedActions[IngressIndex(IngressActionRejection::Resetting)].batches;
    CHECK(resettingActionBatches == 1);
    CheckIngressConservation(taken[2]);

    // Taken once: the next window starts from zero and only holds player 0.
    taken = host.TakeIngressStatistics();
    REQUIRE(taken.size() == 1);
    CHECK(taken.at(0).received.inputs == 0);
    CHECK(taken.at(0).receivedActions.batches == 0);
}

TEST_CASE("PvP host counts only the unresolved staged commands of an evicted player as discarded") {
    using namespace fps::pvp;
    std::chrono::steady_clock::time_point now{};
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    auto latest = host.TakeSnapshot();
    REQUIRE(latest);
    // Every tick sends [n, n + 1] in time while reporting a snapshot 15 ticks
    // (250 ms) old: the player is evicted for latency with one command ahead.
    std::uint64_t next = 1;
    std::optional<std::map<PlayerId, MatchIngressCounts>> atEviction;
    for (std::uint64_t step = 0; step < 5 * ConnectionQualityWindowTicks && !atEviction; ++step) {
        REQUIRE(!latest->players.empty());
        PlayerInput window{1, {{next, 1, 0, 0, 0}, {next + 1, 1, 0, 0, 0}}, latest->players[0].movementEpoch, 1,
            latest->tick > 15 ? latest->tick - 15 : latest->tick};
        REQUIRE(host.SubmitInput(window));
        ++next;
        now += std::chrono::nanoseconds(1'000'000'000 / AuthorityTickRate);
        REQUIRE(host.Advance(1.0 / 60).steps == 1);
        if (!host.TakeEvictions().empty()) atEviction = host.TakeIngressStatistics();
        if (auto snapshot = host.TakeSnapshot()) latest = std::move(snapshot);
    }
    REQUIRE(atEviction);
    const auto& counts = atEviction->at(1);
    const auto evictedLeaveDiscarded = counts.discarded[IngressIndex(IngressStagedDiscard::LeaveDiscarded)];
    CHECK(evictedLeaveDiscarded == 1);
    CHECK(counts.classified[IngressIndex(IngressCommandClass::AcceptedNew)] == next);
    CheckIngressConservation(counts);
}

TEST_CASE("PvP host ledger classifies late copies and copies of executed commands, reading the clock only for a late first") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now{};
    std::size_t reads{};
    MatchRuntimeHost host(TestArena(), [&] { ++reads; return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Window(5, 1, 2)));
    REQUIRE(host.Advance(4.0 / 60).steps == 4); // 1 and 2 executed, 3 and 4 substituted
    now += 8ms;
    auto readsBefore = reads;
    REQUIRE(host.SubmitInput(Window(5, 1, 2))); // copies of executed commands
    const auto copyClockReads = reads - readsBefore;
    CHECK(copyClockReads == 0);
    readsBefore = reads;
    REQUIRE(host.SubmitInput(Window(5, 3, 3))); // the first copy of a substituted sequence
    const auto lateFirstClockReads = reads - readsBefore;
    CHECK(lateFirstClockReads == 1);
    readsBefore = reads;
    REQUIRE(host.SubmitInput(Window(5, 3, 3)));
    REQUIRE(host.SubmitInput(Window(5, 3, 3)));
    const auto lateCopyClockReads = reads - readsBefore;
    CHECK(lateCopyClockReads == 0);
    REQUIRE(host.SubmitInput(Window(5, 3, 4))); // 3 again, 4 for the first time
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 5 substituted
    REQUIRE(host.QueueLeave(2, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // Leave closes 3, 4 and the never-arrived 5

    const auto taken = host.TakeIngressStatistics();
    const auto& counts = taken.at(5);
    const auto classified = [&](IngressCommandClass value) { return counts.classified[IngressIndex(value)]; };
    const auto lateFirst = classified(IngressCommandClass::LateFirst);
    CHECK(lateFirst == 2);
    const auto lateCopies = classified(IngressCommandClass::LateCopy);
    CHECK(lateCopies == 3);
    const auto resolvedCopies = classified(IngressCommandClass::ResolvedCopy);
    CHECK(resolvedCopies == 2);
    CHECK(classified(IngressCommandClass::ResolvedUntracked) == 0);
    const auto lateOnlyInputs = counts.lateOnlyInputs;
    CHECK(lateOnlyInputs == 4);
    const auto substitutedSequences = counts.substitutedCommands;
    CHECK(substitutedSequences == 3);
    const auto unarrivedOnLeave = counts.unarrived[IngressIndex(IngressSubstitutionClose::Removed)];
    CHECK(unarrivedOnLeave == 1);
    CheckIngressConservation(counts);
    CheckSubstitutionConservation(counts);
}

TEST_CASE("PvP host ledger keeps late arrivals beyond the slack sample bound without publishing them as samples") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now{};
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(5, 1)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    // Every even sequence is substituted while the odd one after it waits:
    // 70 substitutions, more than the 64 the slack track keeps, no reset.
    for (std::uint64_t k = 1; k <= 70; ++k) {
        REQUIRE(host.SubmitInput(Input(5, 2 * k + 1)));
        now += 17ms;
        REQUIRE(host.Advance(2.0 / 60).steps == 2);
    }
    auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].movementEpoch == 1);
    REQUIRE(snapshot->players[0].lastResolvedCommand == 141);
    now += 5ms;
    REQUIRE(host.SubmitInput(Input(5, 2))); // beyond the sample track, within the ledger
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    const bool beyondSampleSequence = snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{2};
    CHECK_FALSE(beyondSampleSequence);
    REQUIRE(host.SubmitInput(Input(5, 140))); // within both
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    const auto withinSampleSequence = snapshot->players[0].movementSlackSequence;
    CHECK(withinSampleSequence == std::optional<std::uint64_t>{140});

    auto taken = host.TakeIngressStatistics();
    const auto beyondSampleLate = taken.at(5).classified[IngressIndex(IngressCommandClass::LateFirst)];
    CHECK(beyondSampleLate == 2);
    // The last window closes every record still open as end.
    const auto finalWindow = host.TakeIngressStatistics(true);
    auto total = taken.at(5);
    const auto& last = finalWindow.at(5);
    total.unarrived[IngressIndex(IngressSubstitutionClose::End)] += last.unarrived[IngressIndex(IngressSubstitutionClose::End)];
    const auto endClosedUnarrived = last.unarrived[IngressIndex(IngressSubstitutionClose::End)];
    CHECK(endClosedUnarrived == 70);
    CheckSubstitutionConservation(total);
}

TEST_CASE("PvP host ledger closes the records of a rotated epoch") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(5, 1)));
    std::uint64_t epoch = 1;
    for (unsigned step = 0; step < 4 * MovementBacklogSampleTicks && epoch == 1; ++step) {
        REQUIRE(host.Advance(1.0 / 60).steps == 1); // starved after 1: substituted until it rotates
        const auto snapshot = host.TakeSnapshot();
        REQUIRE(snapshot);
        epoch = snapshot->players[0].movementEpoch;
    }
    REQUIRE(epoch == 2);
    const auto taken = host.TakeIngressStatistics();
    const auto& counts = taken.at(5);
    const auto rotationSubstituted = counts.substitutedCommands;
    CHECK(rotationSubstituted > 0);
    const auto rotationClosedUnarrived = counts.unarrived[IngressIndex(IngressSubstitutionClose::Epoch)];
    CHECK(rotationClosedUnarrived == rotationSubstituted);
    CHECK(counts.unarrived[IngressIndex(IngressSubstitutionClose::Life)] == 0);
    CheckSubstitutionConservation(counts);
}

TEST_CASE("PvP host counts what Leave and reset discard, and closes their records, across the reset") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.QueueJoin(2, 2));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(1, 1)));
    REQUIRE(host.SubmitInput(Input(2, 1)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // both execute 1
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // both substitute 2
    REQUIRE(host.SubmitActionBatch({1, {{1, 1, 0, 0}}}, 0) == ActionAdmission::Accepted);
    REQUIRE(host.QueueLeave(3, 1));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // Leave drops the staged shot; 2 substitutes 3
    REQUIRE(host.SubmitInput(Window(2, 5, 7)));
    REQUIRE(host.SubmitActionBatch({2, {{1, 1, 0, 0}}}, 0) == ActionAdmission::Accepted);
    REQUIRE(host.QueueActionAcknowledgement(2, 0));
    auto reset = host.RequestReset();
    CHECK(host.Advance(1).steps == 0);
    REQUIRE(reset.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

    const auto taken = host.TakeIngressStatistics();
    const auto& departed = taken.at(1);
    const auto leaveDiscardedShots = departed.leaveDiscardedActionShots;
    CHECK(leaveDiscardedShots == 1);
    const auto unarrivedLeft = departed.unarrived[IngressIndex(IngressSubstitutionClose::Removed)];
    CHECK(unarrivedLeft == 1);
    CheckSubstitutionConservation(departed);
    const auto& reset2 = taken.at(2);
    const auto resetDiscardedCommands = reset2.resetDiscardedCommands;
    CHECK(resetDiscardedCommands == 3);
    const auto resetDiscardedShots = reset2.resetDiscardedActionShots;
    CHECK(resetDiscardedShots == 1);
    const auto resetDiscardedAcks = reset2.resetDiscardedActionAcks;
    CHECK(resetDiscardedAcks == 1);
    const auto unarrivedReset = reset2.unarrived[IngressIndex(IngressSubstitutionClose::Reset)];
    CHECK(unarrivedReset == 2);
    CHECK(reset2.discarded[IngressIndex(IngressStagedDiscard::LeaveDiscarded)] == 0);
    CheckSubstitutionConservation(reset2);
    CheckIngressConservation(reset2);
}

TEST_CASE("PvP host keeps the first rejections of each player and reason per window for the detail file") {
    using namespace fps::pvp;
    MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Window(5, 1, 2)));
    REQUIRE(host.Advance(2.0 / 60).steps == 2);
    // Twenty players the Match does not hold share the player=0 bucket's bound.
    for (PlayerId unknown = 9; unknown < 29; ++unknown) CHECK_FALSE(host.SubmitInput(Input(unknown, 1)));
    CHECK_FALSE(host.SubmitInput(EpochInput(5, 3, 5, 2)));
    auto drained = host.DrainIngressRecords();
    std::size_t unknownRecords{};
    std::optional<IngressRejectionRecord> epochFuture;
    for (const auto& record : drained.records) {
        const auto* rejection = std::get_if<IngressRejectionRecord>(&record);
        REQUIRE(rejection);
        CHECK(rejection->timeNs > 0);
        if (rejection->reason == IngressInputRejection::UnknownPlayer) {
            ++unknownRecords;
            CHECK(rejection->playerId >= 9);
            CHECK(rejection->cursor == 0);
            CHECK(rejection->currentEpoch == 0);
        } else epochFuture = *rejection;
    }
    const auto keptUnknownRejections = unknownRecords;
    CHECK(keptUnknownRejections == MatchIngressRejectionsPerWindow);
    const auto suppressedUnknownRejections = drained.suppressed;
    CHECK(suppressedUnknownRejections == 4);
    CHECK(drained.dropped == 0);
    REQUIRE(epochFuture);
    CHECK(epochFuture->reason == IngressInputRejection::EpochFuture);
    CHECK(epochFuture->playerId == 5);
    CHECK(epochFuture->epoch == 2);
    CHECK(epochFuture->firstSequence == std::optional<std::uint64_t>{3});
    CHECK(epochFuture->lastSequence == std::optional<std::uint64_t>{5});
    CHECK(epochFuture->commands == 3);
    CHECK(epochFuture->cursor == 2);
    CHECK(epochFuture->currentEpoch == 1);
    CHECK(epochFuture->currentLife == 1);
    // The statistics window, not the drain, renews the bound.
    CHECK_FALSE(host.SubmitInput(Input(9, 1)));
    CHECK(host.DrainIngressRecords().suppressed == 1);
    // Undrained records fill the buffer over many windows; the next is dropped.
    for (std::size_t window = 0; window < MatchIngressRecordCapacity / MatchIngressRejectionsPerWindow; ++window) {
        static_cast<void>(host.TakeIngressStatistics());
        for (std::size_t i = 0; i < MatchIngressRejectionsPerWindow; ++i) CHECK_FALSE(host.SubmitInput(Input(9, 1)));
    }
    static_cast<void>(host.TakeIngressStatistics());
    CHECK_FALSE(host.SubmitInput(Input(9, 1)));
    drained = host.DrainIngressRecords();
    CHECK(drained.records.size() == MatchIngressRecordCapacity);
    const auto droppedOnFullBuffer = drained.dropped;
    CHECK(droppedOnFullBuffer == 1);
    CHECK(drained.suppressed == 0);
}

TEST_CASE("PvP host substitution records time a late arrival exactly as its published slack sample") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point{} + 1h;
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(5, 1)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 1 executed
    now += 17ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 2 substituted at `now`
    const auto substitutedAt = now;
    static_cast<void>(host.TakeSnapshot());
    now += 23ms;
    REQUIRE(host.SubmitInput(Window(5, 2, 3))); // 2 late by 23 ms, 3 in time
    REQUIRE(host.SubmitInput(Window(5, 2, 3)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->players[0].movementSlackSequence == std::optional<std::uint64_t>{2});
    const auto slackMicros = snapshot->players[0].movementSlackMicros;
    REQUIRE(slackMicros);
    CHECK(*slackMicros == -23000);

    static_cast<void>(host.TakeIngressStatistics(true));
    const auto drained = host.DrainIngressRecords();
    std::optional<IngressSubstitution> late;
    for (const auto& record : drained.records)
        if (const auto* substitution = std::get_if<IngressSubstitution>(&record); substitution && substitution->sequence == 2)
            late = *substitution;
    REQUIRE(late);
    CHECK(late->substitutedAt == substitutedAt);
    CHECK(late->firstArrival == std::optional{substitutedAt + 23ms});
    CHECK(late->copiesAfterFirst == 1);
    CHECK(late->close == IngressSubstitutionClose::End);
    const auto ledgerLateMicros = late->LateMicros();
    REQUIRE(ledgerLateMicros);
    CHECK(*ledgerLateMicros == -std::int64_t{*slackMicros});
    const auto line = MatchIngressRecordLine(*late);
    const bool lineCarriesLateMicros = line.find("\"late_us\":23000,") != std::string::npos;
    CHECK(lineCarriesLateMicros);
}

TEST_CASE("PvP host counts where each movement slack sample went on its way to the IPC") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point{} + 1h;
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    static_cast<void>(host.TakeSnapshot());
    REQUIRE(host.SubmitInput(Window(5, 1, 3)));
    now += 1ms;
    REQUIRE(host.SubmitInput(Input(5, 4)));
    now += 3ms;
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 1 executed: published
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 2 executed: published over the untaken 1
    REQUIRE(host.TakeSnapshot());
    REQUIRE(host.Advance(2.0 / 60).steps == 2); // 3 and 4 executed: one publication, 4 (smaller) replaces 3
    REQUIRE(host.TakeSnapshot());
    REQUIRE(host.Advance(3.0 / 60).steps == 3); // 5, 6 and 7 substituted: no sample
    REQUIRE(host.TakeSnapshot());
    now += 8ms;
    REQUIRE(host.SubmitInput(Window(5, 5, 6))); // two late candidates, 6 merged
    REQUIRE(host.SubmitInput(Input(5, 8)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 8 executed and merged; 5 published negative
    const std::vector<PlayerId> replaced{5};
    host.NoteCoalescedSlackSamples(replaced);
    REQUIRE(host.TakeSnapshot());
    REQUIRE(host.SubmitInput(Input(5, 9)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 9 executed: published, left untaken
    REQUIRE(host.SubmitInput(Input(5, 7))); // 7 late: pending
    auto reset = host.RequestReset();
    CHECK(host.Advance(1).steps == 0); // the reset discards 7 and leaves 9 unclaimed
    REQUIRE(reset.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

    auto taken = host.TakeIngressStatistics(true);
    const auto& counts = taken.at(5);
    CHECK(counts.slackExecutedSamples == 6);
    CHECK(counts.slackLateSamples == 3);
    const auto mergedSamples = counts.slackMergedSamples;
    CHECK(mergedSamples == 3);
    const auto discardedSamples = counts.slackDiscardedSamples;
    CHECK(discardedSamples == 1);
    const auto publishedSamples = counts.slackPublishedSamples;
    CHECK(publishedSamples == 5);
    const auto publishedNegativeSamples = counts.slackPublishedNegativeSamples;
    CHECK(publishedNegativeSamples == 1);
    const auto overwrittenSamples = counts.slackOverwrittenSamples;
    CHECK(overwrittenSamples == 1);
    const auto takenSamples = counts.slackTakenSamples;
    CHECK(takenSamples == 3);
    const auto coalescedSamples = counts.slackCoalescedSamples;
    CHECK(coalescedSamples == 1);
    const auto unclaimedSamples = counts.slackUnclaimedSamples;
    CHECK(unclaimedSamples == 1);
    CHECK(counts.slackExecutedSamples + counts.slackLateSamples == mergedSamples + discardedSamples + publishedSamples);
    CHECK(publishedSamples == overwrittenSamples + takenSamples + unclaimedSamples);
}

TEST_CASE("PvP host final window discards pending slack samples and counts an untaken publication") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point{} + 1h;
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(5, 1)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 1 executed
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 2 substituted
    REQUIRE(host.TakeSnapshot());
    REQUIRE(host.SubmitInput(Input(5, 3)));
    REQUIRE(host.Advance(1.0 / 60).steps == 1); // 3 executed: published, left untaken
    now += 5ms;
    REQUIRE(host.SubmitInput(Input(5, 2))); // late: pending at the end
    auto taken = host.TakeIngressStatistics(true);
    const auto& counts = taken.at(5);
    const auto endDiscardedSamples = counts.slackDiscardedSamples;
    CHECK(endDiscardedSamples == 1);
    const auto endUnclaimedSamples = counts.slackUnclaimedSamples;
    CHECK(endUnclaimedSamples == 1);
    CHECK(counts.slackExecutedSamples + counts.slackLateSamples ==
          counts.slackMergedSamples + counts.slackDiscardedSamples + counts.slackPublishedSamples);
    CHECK(counts.slackPublishedSamples ==
          counts.slackOverwrittenSamples + counts.slackTakenSamples + counts.slackUnclaimedSamples);
}

TEST_CASE("PvP host discards the pending slack sample of a player who leaves") {
    using namespace fps::pvp;
    using namespace std::chrono_literals;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::time_point{} + 1h;
    MatchRuntimeHost host(TestArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    REQUIRE(host.SubmitInput(Input(5, 1)));
    REQUIRE(host.Advance(2.0 / 60).steps == 2); // 1 executed, 2 substituted
    now += 5ms;
    REQUIRE(host.SubmitInput(Input(5, 2))); // late: pending
    REQUIRE(host.QueueLeave(2, 5));
    REQUIRE(host.Advance(1.0 / 60).steps == 1);
    const auto taken = host.TakeIngressStatistics();
    const auto& counts = taken.at(5);
    const auto leaveDiscardedSamples = counts.slackDiscardedSamples;
    CHECK(leaveDiscardedSamples == 1);
    CHECK(counts.slackExecutedSamples + counts.slackLateSamples ==
          counts.slackMergedSamples + counts.slackDiscardedSamples + counts.slackPublishedSamples);
}
