#include <doctest/doctest.h>

#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <limits>
#include <numbers>
#include <vector>

namespace {
using namespace fps::pvp;
Arena PredictionArena() {
    Arena arena;
    arena.id = "synthetic_prediction_arena";
    arena.width = arena.depth = 30;
    arena.walls = {{{-1, 0, -1}, {0, 3, 31}}, {{30, 0, -1}, {31, 3, 31}},
                   {{0, 0, -1}, {30, 3, 0}}, {{0, 0, 30}, {30, 3, 31}},
                   {{8, 0, 4}, {9, 3, 10}}, {{5, 0, 9}, {9, 3, 10}}};
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}
void SamePosition(fps::Float3 actual, fps::Float3 expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(0.00001));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(0.00001));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(0.00001));
}
float Distance(fps::Float3 lhs, fps::Float3 rhs) {
    return std::hypot(lhs.x - rhs.x, lhs.z - rhs.z);
}
void ClearBody(const Arena& arena, fps::Float3 position) {
    CHECK(fps::CanPlaceCharacterBody(
        {{position.x, position.y, position.z}, arena.bodyHeight, arena.radius}, arena.walls, {}));
}
void Step(PvpMatch& match) { match.Tick({match.TickCount() + 1, MovementTickSeconds}); }

struct LinkOptions {
    int rtt{};
    bool impaired{};
    bool stall{};
    int fps{60};
    bool continuous{};
};
struct LinkEvidence {
    std::size_t maximumPending{};
    unsigned frameCount{};
    unsigned movingFrames{};
    unsigned snapshotCount{};
    unsigned resends{};
    unsigned reorderedInputs{};
    unsigned lateRecoveryFrames{};
    float maximumDisplayStep{};
    float maximumCorrection{};
    float finalPosition{};
    int firstPredictedMotionMs{-1};
    int firstDisplayedMotionMs{-1};
    int firstAuthorityMotionMs{-1};
    int lastAuthorityMotionBeforeStopMs{-1};
    std::uint64_t maximumSteadyCommandLead{};
    std::uint64_t maximumRecoveredCommandLead{};
    float firstStopPosition{};
};

// A deterministic virtual wire: its millisecond event clock is independent of
// 60 Hz authority, snapshots and worker sends and the selected client render rate. Only
// real public command windows and snapshots cross it; no predictor internals.
LinkEvidence RunLink(LinkOptions options) {
    const auto arena = PredictionArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    LocalPlayerPrediction client(arena);
    client.Reconcile(match.Snapshot().players.front(), 0);
    struct InputPacket { double due; PlayerInput input; unsigned number; };
    struct SnapshotPacket { double due; WorldSnapshot snapshot; };
    std::vector<InputPacket> inputs;
    std::vector<SnapshotPacket> snapshots;
    std::map<std::pair<std::uint64_t, std::uint64_t>, MovementCommand> immutable;
    std::uint32_t random = 0x130421;
    const auto nextRandom = [&] {
        random = random * 1664525U + 1013904223U;
        return random;
    };
    const auto delay = [&] {
        return (std::max)(0, options.rtt / 2 +
            (options.impaired ? static_cast<int>(nextRandom() % 21) - 10 : 0));
    };
    unsigned inputNumber{}, snapshotNumber{}, lastInputNumber{};
    double nextFrame{}, nextSend{}, nextTick = 1000.0 / 60.0, previousFrame{};
    PlayerInput publishedWindow;
    fps::Float3 previousDisplay = client.Observation().renderPosition;
    auto previousAuthorityPosition = match.Snapshot().players.front().position;
    float restingPosition{};
    LinkEvidence result;
    for (int millisecond = 0; millisecond <= 6000; ++millisecond) {
        const double now = millisecond;
        const bool stalled = options.stall && now >= 2700 && now < 2950;
        std::stable_sort(inputs.begin(), inputs.end(), [](const auto& a, const auto& b) {
            return a.due < b.due;
        });
        while (!inputs.empty() && inputs.front().due <= now) {
            auto packet = std::move(inputs.front());
            inputs.erase(inputs.begin());
            if (packet.input.movementEpoch == match.Snapshot().players.front().movementEpoch)
                REQUIRE(match.SubmitInput(packet.input));
            else
                CHECK_FALSE(match.SubmitInput(packet.input));
            if (packet.number < lastInputNumber) ++result.reorderedInputs;
            lastInputNumber = packet.number;
        }
        if (!stalled) {
            std::stable_sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) {
                return a.due < b.due;
            });
            while (!snapshots.empty() && snapshots.front().due <= now) {
                auto snapshot = std::move(snapshots.front().snapshot);
                snapshots.erase(snapshots.begin());
                client.Reconcile(snapshot.players.front(), snapshot.tick);
                ++result.snapshotCount;
                // The acknowledged pose plus precisely the remaining window
                // must reconstruct prediction; acknowledged commands cannot recur.
                if (snapshot.tick == client.Observation().authorityTick) {
                    auto replay = snapshot.players.front();
                    for (const auto& command : client.PendingInput().commands) {
                        CHECK(command.sequence > replay.lastResolvedCommand);
                        replay = StepMovement(arena, replay, command);
                    }
                    SamePosition(client.Observation().predictedPosition, replay.position);
                }
            }
        }
        if (now + 0.000001 >= nextFrame) {
            nextFrame += 1000.0 / options.fps;
            if (!stalled) {
                const float forward = options.continuous || (now >= 300 && now < 2300) ||
                    (options.stall && now >= 3300 && now < 4000) ? 1.0F : 0.0F;
                if (client.Advance((now - previousFrame) / 1000.0, forward, 0, 0, 0))
                    publishedWindow = client.PendingInput();
                previousFrame = now;
                const auto observation = client.Observation();
                if (observation.predictedPosition.z > 2.0001F && result.firstPredictedMotionMs < 0)
                    result.firstPredictedMotionMs = millisecond;
                if (observation.renderPosition.z > 2.0001F && result.firstDisplayedMotionMs < 0)
                    result.firstDisplayedMotionMs = millisecond;
                result.maximumPending = (std::max)(result.maximumPending, observation.pendingCommands);
                result.maximumCorrection = (std::max)(result.maximumCorrection,
                    Distance(observation.correctionOffset, {}));
                ClearBody(arena, observation.renderPosition);
                if (options.continuous && now > 500 && now < 1800) {
                    ++result.frameCount;
                    const auto step = Distance(observation.renderPosition, previousDisplay);
                    if (step > 0.0001F) ++result.movingFrames;
                    result.maximumDisplayStep = (std::max)(result.maximumDisplayStep, step);
                }
                if (now >= 5000 && observation.frozen) ++result.lateRecoveryFrames;
                previousDisplay = observation.renderPosition;
            }
        }
        if (now + 0.000001 >= nextSend) {
            nextSend += 1000.0 / InputSendRate;
            if (!publishedWindow.commands.empty()) {
                ++inputNumber;
                ++result.resends;
                const auto window = publishedWindow;
                REQUIRE_FALSE(window.commands.empty());
                REQUIRE(window.commands.size() <= MaxPendingCommands);
                for (const auto& command : window.commands) {
                    const auto [entry, inserted] = immutable.try_emplace(
                        std::pair{window.movementEpoch, command.sequence}, command);
                    if (!inserted) CHECK(entry->second == command);
                }
                // Drop the first window, two consecutive windows at stop,
                // and a deterministic 5% of the remaining datagrams.
                const bool drop = options.impaired && (inputNumber == 1 || inputNumber == 140 ||
                    inputNumber == 141 || nextRandom() % 20 == 0);
                if (!drop) {
                    inputs.push_back({now + delay(), window, inputNumber});
                    if (options.impaired && inputNumber % 7 == 0)
                        // Deliberately release a duplicate after the next
                        // original packet to exercise stale-window ordering.
                        inputs.push_back({now + delay() + 45, window, inputNumber});
                }
            }
        }

        if (now + 0.000001 >= nextTick) {
            nextTick += 1000.0 / 60.0;
            Step(match);
            const auto authority = match.Snapshot().players.front();
            if (Distance(authority.position, previousAuthorityPosition) > 0.00001F) {
                if (result.firstAuthorityMotionMs < 0) result.firstAuthorityMotionMs = millisecond;
                if (millisecond < 2700) result.lastAuthorityMotionBeforeStopMs = millisecond;
            }
            previousAuthorityPosition = authority.position;
            if (millisecond >= 500 && millisecond < 2200) {
                const auto latest = client.Observation().latestCommand;
                if (latest >= authority.lastResolvedCommand)
                    result.maximumSteadyCommandLead = (std::max)(result.maximumSteadyCommandLead,
                        latest - authority.lastResolvedCommand);
            }
            if (millisecond >= 3300 && millisecond < 3900) {
                const auto latest = client.Observation().latestCommand;
                if (latest >= authority.lastResolvedCommand)
                    result.maximumRecoveredCommandLead = (std::max)(result.maximumRecoveredCommandLead,
                        latest - authority.lastResolvedCommand);
            }
            if (match.TickCount() % SnapshotIntervalTicks == 0) {
                ++snapshotNumber;
                if (!options.impaired || nextRandom() % 20 != 0) {
                    snapshots.push_back({now + delay(), match.Snapshot()});
                    if (options.impaired && snapshotNumber % 11 == 0)
                        snapshots.push_back({now + delay() + 12, match.Snapshot()});
                }
            }
        }
        const auto state = match.Snapshot().players.front();
        if (now == 2600) result.firstStopPosition = state.position.z;
        if (now == 5000) restingPosition = state.position.z;
        if (!options.continuous && now > 5000) CHECK(state.position.z == restingPosition);
    }
    result.finalPosition = match.Snapshot().players.front().position.z;
    CHECK(client.Observation().latestCommand >= match.Snapshot().players.front().lastResolvedCommand);
    CHECK(client.Observation().active);
    return result;
}
} // namespace

TEST_CASE("PvP prediction seeds only neutral commands and bounds a stalled acknowledgement window") {
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } trace;
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    CHECK_FALSE(client.Advance(1, 1, 0, 0, 0));
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 30);
    CHECK(client.Observation().pendingCommands == InitialCommandLead);
    CHECK_FALSE(client.Advance(0, 1, 0, 0, 0));
    for (const auto& command : client.PendingInput().commands) {
        CHECK(command.moveForward == 0);
        CHECK(command.moveRight == 0);
    }
    for (std::size_t frame = 0; frame < MaxPendingCommands - InitialCommandLead; ++frame)
        static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
    CHECK(client.Observation().frozen);
    CHECK(client.Observation().pendingCommands == MaxPendingCommands);
    CHECK(client.Observation().predictedPosition.z == doctest::Approx(
        2 + (MaxPendingCommands - InitialCommandLead) * arena.movementSpeed * MovementTickSeconds));
    const auto frozen = client.Observation().predictedPosition;
    const auto original = client.PendingInput().commands;
    static_cast<void>(trace.trace->Drain());
    for (int frame = 0; frame < 60; ++frame)
        static_cast<void>(client.Advance(MovementTickSeconds, 0, 1, 1.2F, 0.4F));
    std::uint64_t blockedSteps{};
    double droppedSeconds{};
    for (const auto& event : trace.trace->Drain()) {
        REQUIRE(event.kind == MovementTraceKind::RuntimeGap);
        blockedSteps += event.count;
        droppedSeconds += event.droppedSeconds;
    }
    CHECK(blockedSteps == 60);
    CHECK(droppedSeconds == doctest::Approx(1.0));
    SamePosition(client.Observation().predictedPosition, frozen);
    CHECK(client.PendingInput().commands == original);
    // A newer snapshot with the same ACK must preserve the collapsed render
    // pair used while frozen, rather than interpolating the final step twice.
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 31);
    SamePosition(client.Observation().renderPosition, frozen);
    static_cast<void>(client.Advance(MovementTickSeconds, 0, 0, 0, 0));
    SamePosition(client.Observation().renderPosition, frozen);
    client.Reconcile({1, {2, 0, 2}, 0, 0, InitialCommandLead}, 33);
    CHECK_FALSE(client.Observation().frozen);
    static_cast<void>(client.Advance(MovementTickSeconds, 0, 1, 1.2F, 0.4F));
    CHECK(client.PendingInput().commands.back().yaw == 1.2F);
    CHECK(client.PendingInput().commands.back().sequence == MaxPendingCommands + 1);
}

TEST_CASE("PvP full acknowledgement preserves fixed-step phase and normal 30 FPS production") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    static_cast<void>(client.Advance(0, 1, 0, 0, 0));
    static_cast<void>(client.Advance(0.4 * MovementTickSeconds, 1, 0, 0, 0));
    client.Reconcile({1, {2, 0, 2}, 0, 0, InitialCommandLead}, 2);
    CHECK(client.PendingInput().commands.empty());
    CHECK(client.Observation().latestCommand == InitialCommandLead);
    REQUIRE(client.Advance(0.6 * MovementTickSeconds, 1, 0, 0, 0));
    REQUIRE(client.PendingInput().commands.size() == 1);
    CHECK(client.PendingInput().commands.front().sequence == InitialCommandLead + 1);
    CHECK(client.PendingInput().commands.front().moveForward == 1);
    client.Reconcile({1, {2, 0, 2.05F}, 0, 0, InitialCommandLead + 1}, 3);
    CHECK(client.PendingInput().commands.empty());
    REQUIRE(client.Advance(2 * MovementTickSeconds, 1, 0, 0, 0));
    CHECK(client.PendingInput().commands.size() == 2);
    CHECK(client.Observation().latestCommand == InitialCommandLead + 3);
}

TEST_CASE("PvP repeated perfect full ACKs retain adjacent interpolation without artificial correction") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 0);
    static_cast<void>(client.Advance(0, 1, 0, 0, 0));
    static_cast<void>(client.Advance(1.5 * MovementTickSeconds, 1, 0, 0, 0));
    for (std::uint64_t tick = 1; tick <= 120; ++tick) {
        const auto before = client.Observation();
        REQUIRE(before.previousCommand + 1 == before.latestCommand);
        CHECK(before.interpolationAlpha == doctest::Approx(0.5));
        client.Reconcile({1, before.predictedPosition, 0, 0, before.latestCommand}, tick);
        const auto acknowledged = client.Observation();
        CHECK(client.PendingInput().commands.empty());
        CHECK(acknowledged.previousCommand == before.previousCommand);
        CHECK(acknowledged.currentCommand == before.currentCommand);
        CHECK(acknowledged.interpolationAlpha == before.interpolationAlpha);
        SamePosition(acknowledged.renderPosition, before.renderPosition);
        CHECK(Distance(acknowledged.correctionOffset, {}) < 0.00001F);
        REQUIRE(client.Advance(0.5 * MovementTickSeconds, 1, 0, 0, 0));
        CHECK(client.Observation().latestCommand == before.latestCommand + 1);
        SamePosition(client.Observation().renderPosition, before.predictedPosition);
        static_cast<void>(client.Advance(0.5 * MovementTickSeconds, 1, 0, 0, 0));
    }
}

TEST_CASE("PvP partial previous-endpoint and frozen ACKs preserve the meaning of the render pair") {
    const auto arena = PredictionArena();
    SUBCASE("Partial ACK then ACK of the previous endpoint then full ACK") {
        LocalPlayerPrediction client(arena);
        PlayerState authority{1, {2, 0, 2}, 0, 0, 0};
        client.Reconcile(authority, 0);
        static_cast<void>(client.Advance(0, 1, 0, 0, 0));
        // Establish the complete startup window before exercising a partial
        // ACK; this test needs three real steps, not a clamped bootstrap gap.
        static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
        static_cast<void>(client.Advance(2.25 * MovementTickSeconds, 1, 0, 0, 0));
        const auto commands = client.PendingInput().commands;
        const auto before = client.Observation();
        REQUIRE(before.latestCommand == 5);
        REQUIRE(before.previousCommand == 4);
        for (const auto& command : commands) {
            authority = StepMovement(arena, authority, command);
            if (command.sequence < 3) continue;
            client.Reconcile(authority, command.sequence);
            CHECK(client.Observation().latestCommand == before.latestCommand);
            CHECK(client.Observation().previousCommand == before.previousCommand);
            SamePosition(client.Observation().predictedPosition, before.predictedPosition);
            SamePosition(client.Observation().renderPosition, before.renderPosition);
            CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
        }
    }
    SUBCASE("Full window frozen at its tip remains collapsed after a perfect full ACK") {
        LocalPlayerPrediction client(arena);
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 0);
        static_cast<void>(client.Advance(0, 1, 0, 0, 0));
        while (client.PendingInput().commands.size() < MaxPendingCommands)
            static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
        static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
        const auto before = client.Observation();
        REQUIRE(before.previousCommand == before.latestCommand);
        client.Reconcile({1, before.predictedPosition, 0, 0, before.latestCommand}, 1);
        CHECK_FALSE(client.Observation().frozen);
        CHECK(client.Observation().previousCommand == before.previousCommand);
        SamePosition(client.Observation().renderPosition, before.renderPosition);
        CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
        REQUIRE(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
        CHECK(client.Observation().latestCommand == before.latestCommand + 1);
    }
}

TEST_CASE("PvP full ACK smooths only real endpoint error and retains collision-safe presentation") {
    const auto arena = PredictionArena();
    SUBCASE("A genuine correction decays within 100 ms without an artificial forward component") {
        LocalPlayerPrediction client(arena);
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 0);
        static_cast<void>(client.Advance(0, 1, 0, 0, 0));
        static_cast<void>(client.Advance(1.5 * MovementTickSeconds, 1, 0, 0, 0));
        const auto before = client.Observation();
        auto corrected = before.predictedPosition;
        corrected.x += 0.2F;
        client.Reconcile({1, corrected, 0, 0, before.latestCommand}, 1);
        SamePosition(client.Observation().renderPosition, before.renderPosition);
        CHECK(client.Observation().correctionOffset.x == doctest::Approx(-0.2));
        CHECK(client.Observation().correctionOffset.z == doctest::Approx(0));
        static_cast<void>(client.Advance(0.05, 0, 0, 0, 0));
        CHECK(client.Observation().correctionOffset.x == doctest::Approx(-0.1));
        client.Reconcile({1, corrected, 0, 0, before.latestCommand}, 2);
        static_cast<void>(client.Advance(0.05, 0, 0, 0, 0));
        CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
    }
    SUBCASE("A translated previous endpoint cannot pull the camera through a wall corner") {
        LocalPlayerPrediction client(arena);
        client.Reconcile({1, {7.7F, 0, 4.1F}, 0, 0, 0}, 0);
        static_cast<void>(client.Advance(0, 0, 1, 0, 0));
        static_cast<void>(client.Advance(1.5 * MovementTickSeconds, 0, 1, 0, 0));
        client.Reconcile({1, {8.1F, 0, 3.72F}, 0, 0, client.Observation().latestCommand}, 1);
        for (int frame = 0; frame < 20; ++frame) {
            ClearBody(arena, client.Observation().renderPosition);
            ClearBody(arena, client.Observation().predictedPosition);
            static_cast<void>(client.Advance(1.0 / 144, 1, 0, 0, 0));
        }
    }
}

TEST_CASE("PvP every discarded covered frame interval is observable below the long-frame threshold") {
    for (const double frameSeconds : {0.051, 0.083, 0.099}) {
        for (const bool fullyAcknowledged : {false, true}) {
            struct TraceScope {
                std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
                TraceScope() { SetMovementTrace(trace); }
                ~TraceScope() { SetMovementTrace(nullptr); }
            } scope;
            LocalPlayerPrediction client(PredictionArena());
            client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
            if (fullyAcknowledged) {
                static_cast<void>(client.Advance(0, 0, 0, 0, 0));
                client.Reconcile({1, {2, 0, 2}, 0, 0, InitialCommandLead}, 2);
                REQUIRE(client.PendingInput().commands.empty());
            }
            static_cast<void>(scope.trace->Drain());
            static_cast<void>(client.Advance(frameSeconds, 1, 0, 0, 0));
            unsigned gaps{};
            for (const auto& event : scope.trace->Drain()) {
                if (event.kind != MovementTraceKind::RuntimeGap) continue;
                ++gaps;
                CHECK(event.droppedSeconds == doctest::Approx(frameSeconds - MovementTickSeconds));
                CHECK(event.frameSeconds == frameSeconds);
                CHECK(event.count == 0);
            }
            CHECK(gaps == 1);
        }
    }
}

TEST_CASE("PvP fully acknowledged long gaps retain phase without creating catch-up backlog") {
    for (const double gap : {0.083, 0.108485702, 0.25}) {
        LocalPlayerPrediction client(PredictionArena());
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
        static_cast<void>(client.Advance(0, 1, 0, 0, 0));
        static_cast<void>(client.Advance(0.4 * MovementTickSeconds, 1, 0, 0, 0));
        client.Reconcile({1, {2, 0, 2}, 0, 0, InitialCommandLead}, 2);
        REQUIRE(client.PendingInput().commands.empty());
        REQUIRE(client.Advance(gap, 1, 0, 0.4F, 0.2F));
        CHECK(client.PendingInput().commands.size() == 1);
        CHECK(client.Observation().latestCommand == InitialCommandLead + 1);
        CHECK(client.Observation().interpolationAlpha == doctest::Approx(0.4));
        CHECK(client.Observation().predictedPosition.z < 2.051F);
    }
}

TEST_CASE("PvP epoch snapshots replace old history and reject stale namespaces independently of world tick") {
    LocalPlayerPrediction client(PredictionArena());
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 10);
    static_cast<void>(client.Advance(0, 1, 0, 0, 0));
    static_cast<void>(client.Advance(0.005, 1, 0, 0, 0));
    client.Reconcile({1, {2.3F, 0, 2}, 0, 0, 1}, 11);
    CHECK(std::abs(client.Observation().correctionOffset.x) > 0.1F);
    client.Reconcile({1, {3, 0, 3}, 0.4F, 0.2F, 0, 2, 0}, 20);
    CHECK(client.Observation().movementEpoch == 2);
    CHECK(client.Observation().lastResolvedCommand == 0);
    CHECK(client.PendingInput().movementEpoch == 2);
    REQUIRE(client.PendingInput().commands.size() == InitialCommandLead);
    CHECK(client.PendingInput().commands.front().sequence == 1);
    SamePosition(client.Observation().renderPosition, {3, 0, 3});
    SamePosition(client.Observation().correctionOffset, {});
    static_cast<void>(client.Advance(0.012, 1, 0, 0.4F, 0.2F));
    CHECK(client.PendingInput().commands.size() == InitialCommandLead); // No old fraction.
    static_cast<void>(client.Advance(0.005, 1, 0, 0.4F, 0.2F));
    CHECK(client.PendingInput().commands.size() == InitialCommandLead + 1);
    const auto before = client.Observation();
    client.Reconcile({1, {20, 0, 20}, 0, 0, 900, 1, 0}, 21);
    client.Reconcile({1, {20, 0, 20}, 0, 0, 0, 3, 0}, 19);
    CHECK(client.Observation().authorityTick == before.authorityTick);
    CHECK(client.PendingInput().movementEpoch == 2);
    SamePosition(client.Observation().predictedPosition, before.predictedPosition);
}

TEST_CASE("PvP first complete window retains exactly two neutral steps without manufacturing elapsed time") {
    const auto arena = PredictionArena();
    for (const int fps : {30, 60, 144}) {
        for (const double firstElapsed : {0.0, 0.002}) {
            INFO("fps ", fps, " first elapsed ", firstElapsed);
            LocalPlayerPrediction client(arena);
            client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
            const auto seed = client.PendingInput().commands;
            REQUIRE(seed.size() == 2);
            CHECK_FALSE(client.Advance(firstElapsed, 1, 0, 0.4F, 0.2F));
            CHECK(client.PendingInput().commands == seed);
            SamePosition(client.Observation().predictedPosition, {2, 0, 2});
            double elapsed = firstElapsed;
            unsigned frames{};
            bool publish{};
            while (!publish && frames < 3) {
                // Repeated unstarted snapshots must retain the fractional step.
                client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 2 + frames);
                elapsed += 1.0 / fps;
                publish = client.Advance(1.0 / fps, 1, 0, 0.4F, 0.2F);
                ++frames;
            }
            REQUIRE(publish);
            const auto window = client.PendingInput();
            const auto steps = static_cast<std::size_t>(std::floor(elapsed / MovementTickSeconds + 1e-8));
            REQUIRE(window.commands.size() == InitialCommandLead + steps);
            CHECK(window.commands[0] == seed[0]);
            CHECK(window.commands[1] == seed[1]);
            for (std::size_t index = InitialCommandLead; index < window.commands.size(); ++index) {
                CHECK(window.commands[index].sequence == index + 1);
                CHECK(window.commands[index].moveForward == 1);
                CHECK(window.commands[index].yaw == 0.4F);
            }
            auto expected = PlayerState{1, {2, 0, 2}, 0, 0, 0};
            for (const auto& command : window.commands) expected = StepMovement(arena, expected, command);
            SamePosition(client.Observation().predictedPosition, expected.position);

            client.Reconcile({1, expected.position, 0, 0, 0, 2, 0}, 100);
            CHECK_FALSE(client.Advance(0, 1, 0, 0, 0));
            REQUIRE(client.PendingInput().commands.size() == InitialCommandLead);
            CHECK(client.PendingInput().movementEpoch == 2);
            CHECK(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
            CHECK(client.PendingInput().commands.size() == InitialCommandLead + 1);
        }
    }
    // A same-epoch cursor already in motion must not wait for a new startup.
    LocalPlayerPrediction running(arena);
    running.Reconcile({1, {2, 0, 2}, 0, 0, 90}, 100);
    CHECK(running.Advance(0, 1, 0, 0, 0));
    CHECK(running.PendingInput().commands.front().sequence == 91);
}

TEST_CASE("PvP unpublished bootstrap bounds accumulated startup catch-up without dropping normal 30 FPS steps") {
    const auto arena = PredictionArena();
    struct Case { std::vector<double> prefix; double gap; };
    for (const auto& scenario : std::vector<Case>{{{0}, 0.051}, {{0.002}, 0.051},
            {{0.015}, 0.049}, {{0, 0.007, 0.007}, 0.049},
            {{0}, 3 * MovementTickSeconds}, {{0.015}, 3 * MovementTickSeconds - 0.015}}) {
        INFO("gap ", scenario.gap, " prefix count ", scenario.prefix.size());
        LocalPlayerPrediction client(arena);
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
        double fractionalSeconds{};
        for (const auto elapsed : scenario.prefix) {
            REQUIRE_FALSE(client.Advance(elapsed, 1, 0, 0, 0));
            fractionalSeconds += elapsed;
        }
        const auto seed = client.PendingInput().commands;
        const auto trace = std::make_shared<MovementTrace>();
        SetMovementTrace(trace);
        const bool publish = client.Advance(scenario.gap, 1, 0, 0, 0);
        SetMovementTrace({});
        REQUIRE(publish);
        const auto window = client.PendingInput();
        REQUIRE(window.commands.size() == 3); // Previously sequence 1..5.
        CHECK(window.commands[0] == seed[0]);
        CHECK(window.commands[1] == seed[1]);
        CHECK(window.commands[2].sequence == 3);
        CHECK(window.commands[2].moveForward == 1);
        CHECK(client.Observation().predictedPosition.z == doctest::Approx(2.05));
        CHECK(client.Observation().interpolationAlpha == doctest::Approx(fractionalSeconds / MovementTickSeconds));
        const auto events = trace->Drain();
        REQUIRE(events.size() == 2);
        CHECK(events[0].kind == MovementTraceKind::Generated);
        CHECK(events[0].sequence == 3);
        CHECK(events[1].kind == MovementTraceKind::RuntimeGap);
        CHECK(events[1].frameSeconds == scenario.gap);
        CHECK(events[1].droppedSeconds == doctest::Approx(scenario.gap - MovementTickSeconds));
        CHECK(events[1].count == 0);
        // The bootstrap guard is over; subsequent normal 30 FPS frames retain
        // both legitimate fixed steps and the initial fractional remainder.
        REQUIRE(client.Advance(1.0 / 30, 1, 0, 0, 0));
        CHECK(client.PendingInput().commands.size() == 5);
    }
    for (const double firstElapsed : {0.0, 0.002, 0.015}) {
        LocalPlayerPrediction ordinary(arena);
        ordinary.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
        REQUIRE_FALSE(ordinary.Advance(firstElapsed, 1, 0, 0, 0));
        REQUIRE(ordinary.Advance(1.0 / 30, 1, 0, 0, 0));
        CHECK(ordinary.PendingInput().commands.size() == 4);
        CHECK(ordinary.Observation().predictedPosition.z == doctest::Approx(2.1));
        CHECK(ordinary.Observation().interpolationAlpha == doctest::Approx(firstElapsed / MovementTickSeconds));
    }
}

TEST_CASE("PvP fixed two-command startup sustains production across dense render worker and authority phases") {
    constexpr int unitsPerSecond = 720000;
    constexpr int tickUnits = unitsPerSecond / AuthorityTickRate;
    constexpr int workerPollUnits = unitsPerSecond / 500; // Actual worker: at most 2 ms.
    const auto arena = PredictionArena();
    for (const int fps : {30, 60, 144}) {
        for (const int workerPhase : {0, 500, 1000}) {
            for (int authorityPhase = 0; authorityPhase < tickUnits; authorityPhase += 500) {
                for (const bool workerFirst : {false, true}) {
                  for (const double firstFrameSeconds : {0.0, 0.002, -1.0}) {
                    INFO("fps ", fps, " worker ", workerPhase, " authority ", authorityPhase,
                        " worker first ", workerFirst, " first elapsed ", firstFrameSeconds);
                    PvpMatch match(arena);
                    std::string error;
                    REQUIRE(match.Join(1, error));
                    LocalPlayerPrediction client(arena);
                    PlayerInput published;
                    WorldSnapshot latest;
                    WorldSnapshot received;
                    std::uint64_t lastSnapshot{}, lastGenerated = InitialCommandLead;
                    unsigned actualCommands{};
                    unsigned steadyCommands{};
                    unsigned steadyResolutions{}, steadyActual{};
                    std::map<std::uint64_t, MovementCommand> accepted;
                    int nextFrame{}, nextWorker = workerPhase, nextAuthority = authorityPhase;
                    int nextSend{};
                    bool haveSent{};
                    int previousFrame{};
                    const auto render = [&](int now) {
                        if (now != nextFrame) return;
                        nextFrame += unitsPerSecond / fps;
                        const bool wasActive = client.Observation().active;
                        if (received.tick > lastSnapshot) {
                            client.Reconcile(received.players.front(), received.tick);
                            lastSnapshot = received.tick;
                        }
                        const double elapsed = !wasActive && client.Observation().active &&
                            firstFrameSeconds >= 0 ? firstFrameSeconds :
                            double(now - previousFrame) / unitsPerSecond;
                        if (client.Advance(elapsed, 1, 0, 0, 0))
                            published = client.PendingInput();
                        previousFrame = now;
                        CHECK(client.Observation().pendingCommands <= MaxPendingCommands);
                        for (const auto& command : client.PendingInput().commands) {
                            if (command.sequence > lastGenerated) {
                                CHECK(command.moveForward == 1);
                                ++actualCommands;
                                if (now >= 2 * unitsPerSecond) ++steadyCommands;
                                lastGenerated = command.sequence;
                            }
                        }
                    };
                    const auto send = [&](int now) {
                        if (now != nextWorker) return;
                        // Network receives ACKs and prunes its cache even while
                        // render is asleep. Empty polls do not spend deadlines;
                        // full acknowledgement releases the prior resend deadline.
                        if (latest.tick > received.tick) received = latest;
                        if (!received.players.empty()) {
                            const auto ack = received.players.front().lastResolvedCommand;
                            const bool hadCommands = !published.commands.empty();
                            published.commands.erase(std::remove_if(published.commands.begin(),
                                published.commands.end(), [ack](const auto& command) {
                                    return command.sequence <= ack;
                                }), published.commands.end());
                            if (hadCommands && published.commands.empty()) {
                                haveSent = false;
                                nextSend = 0;
                            }
                        }
                        if ((!haveSent || now >= nextSend) && !published.commands.empty()) {
                            REQUIRE(match.SubmitInput(published));
                            const auto cursor = match.Snapshot().players.front().lastResolvedCommand;
                            for (const auto& command : published.commands)
                                if (command.sequence > cursor) accepted.try_emplace(command.sequence, command);
                            haveSent = true;
                            nextSend = now + tickUnits;
                        }
                        nextWorker = now + (haveSent && nextSend > now ?
                            (std::min)(workerPollUnits, nextSend - now) : workerPollUnits);
                    };
                    for (;;) {
                        const int now = (std::min)({nextFrame, nextWorker, nextAuthority});
                        if (now > 3 * unitsPerSecond) break;
                        if (workerFirst) { send(now); render(now); }
                        else { render(now); send(now); }
                        if (now == nextAuthority) {
                            nextAuthority += tickUnits;
                            const auto previousCursor = match.Snapshot().players.front().lastResolvedCommand;
                            Step(match);
                            latest = match.Snapshot();
                            const auto cursor = latest.players.front().lastResolvedCommand;
                            if (now >= 2 * unitsPerSecond && cursor > previousCursor) {
                                ++steadyResolutions;
                                if (accepted.contains(cursor)) ++steadyActual;
                            }
                            while (!accepted.empty() && accepted.begin()->first <= cursor)
                                accepted.erase(accepted.begin());
                            CHECK(latest.players.front().movementEpoch == 1);
                            CHECK(latest.players.front().contiguousPendingCommands <= 3);
                        }
                    }
                    CHECK(actualCommands >= 176);
                    CHECK(steadyCommands >= 60);
                    CHECK(steadyCommands <= 62);
                    REQUIRE(steadyResolutions >= 60);
                    CHECK(steadyActual * 100 >= steadyResolutions * 99);
                    CHECK(client.Observation().movementEpoch == 1);
                    // Bootstrap follows a completed authority snapshot through
                    // worker receipt and render. It cannot resolve sequence 1
                    // before the snapshot that lets the client create it.
                    CHECK(client.Observation().predictedPosition.z >=
                        2 + (actualCommands - 1) * arena.movementSpeed * MovementTickSeconds - 0.0001F);
                    CHECK(client.Observation().predictedPosition.z <=
                        2 + actualCommands * arena.movementSpeed * MovementTickSeconds + 0.0001F);
                  }
                }
            }
        }
    }
}

TEST_CASE("PvP command clocks retain a bounded queue during a clean sixty-second virtual soak") {
    constexpr int unitsPerSecond = 7200;
    constexpr int tickUnits = unitsPerSecond / AuthorityTickRate;
    for (const int fps : {60, 144}) {
        const auto arena = PredictionArena();
        PvpMatch match(arena);
        std::string error;
        REQUIRE(match.Join(1, error));
        LocalPlayerPrediction client(arena);
        client.Reconcile(match.Snapshot().players.front(), 0);
        PlayerInput published;
        WorldSnapshot latest;
        std::uint64_t lastSnapshot{};
        int nextFrame{}, nextSend{}, nextAuthority = tickUnits - 1, previousFrame{};
        std::deque<std::uint32_t> queueSamples;
        std::uint32_t queueSum{};
        for (;;) {
            const int now = (std::min)({nextFrame, nextSend, nextAuthority});
            if (now > 60 * unitsPerSecond) break;
            // Adversarial ordering: the worker may miss an input published at
            // the same instant, but it cannot create a long-term pacing drift.
            if (now == nextSend) {
                nextSend += tickUnits;
                if (!published.commands.empty()) REQUIRE(match.SubmitInput(published));
            }
            if (now == nextFrame) {
                nextFrame += unitsPerSecond / fps;
                if (latest.tick > lastSnapshot) {
                    client.Reconcile(latest.players.front(), latest.tick);
                    lastSnapshot = latest.tick;
                }
                if (client.Advance(double(now - previousFrame) / unitsPerSecond, 1, 0, 0, 0))
                    published = client.PendingInput();
                previousFrame = now;
                CHECK(published.commands.size() <= MaxPendingCommands);
            }
            if (now == nextAuthority) {
                nextAuthority += tickUnits;
                Step(match);
                latest = match.Snapshot();
                const auto& state = latest.players.front();
                CHECK(state.movementEpoch == 1);
                queueSamples.push_back(state.contiguousPendingCommands);
                queueSum += state.contiguousPendingCommands;
                if (queueSamples.size() > MovementBacklogSampleTicks) {
                    queueSum -= queueSamples.front();
                    queueSamples.pop_front();
                }
                CHECK(queueSum <= 90);
            }
        }
        CHECK(client.Observation().latestCommand == InitialCommandLead + 60 * AuthorityTickRate);
        CHECK(client.Observation().movementEpoch == 1);
    }
}

TEST_CASE("PvP excessive backlog recovers after lost reset snapshots and old epoch packets") {
    const auto arena = PredictionArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    LocalPlayerPrediction client(arena);
    client.Reconcile(match.Snapshot().players.front(), 0);
    static_cast<void>(client.Advance(0, 1, 0, 0, 0));
    while (client.PendingInput().commands.size() < MaxPendingCommands)
        static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
    const auto staleWindow = client.PendingInput();
    WorldSnapshot latest;
    unsigned lostResetSnapshots = 2;
    unsigned resetCount{};
    std::uint64_t previousEpoch = 1;
    float stoppedPosition{};
    for (unsigned tick = 1; tick <= 180; ++tick) {
        if (!latest.players.empty()) {
            if (latest.players.front().movementEpoch > client.Observation().movementEpoch && lostResetSnapshots)
                --lostResetSnapshots;
            else
                client.Reconcile(latest.players.front(), latest.tick);
        }
        static_cast<void>(client.Advance(MovementTickSeconds, tick < 100 ? 1.0F : 0.0F, 0, 0, 0));
        const auto input = client.PendingInput();
        if (!input.commands.empty()) {
            if (input.movementEpoch == match.Snapshot().players.front().movementEpoch)
                REQUIRE(match.SubmitInput(input));
            else
                CHECK_FALSE(match.SubmitInput(input));
        }
        Step(match);
        latest = match.Snapshot();
        const auto& state = latest.players.front();
        if (state.movementEpoch != previousEpoch) {
            ++resetCount;
            previousEpoch = state.movementEpoch;
            CHECK(state.lastResolvedCommand == 0);
        }
        if (state.movementEpoch > 1) CHECK_FALSE(match.SubmitInput(staleWindow));
        if (tick >= 60) {
            CHECK(state.movementEpoch == 2);
            CHECK(client.Observation().movementEpoch == 2);
            CHECK(state.contiguousPendingCommands <= InitialCommandLead);
            CHECK(client.Observation().pendingCommands <= InitialCommandLead + 1);
        }
        if (tick == 110) stoppedPosition = state.position.z;
        if (tick > 110) CHECK(state.position.z == stoppedPosition);
    }
    CHECK(resetCount == 1);
    CHECK(lostResetSnapshots == 0);
    CHECK(match.TickCount() == 180);
}

TEST_CASE("PvP sequence and epoch boundaries never wrap into an old command namespace") {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    LocalPlayerPrediction client(PredictionArena());
    client.Reconcile({1, {2, 0, 2}, 0, 0, maximum - 1, maximum, 0}, 1);
    REQUIRE(client.PendingInput().commands.size() == 1);
    CHECK(client.PendingInput().commands.front().sequence == maximum);
    CHECK(client.PendingInput().movementEpoch == maximum);
    CHECK(client.Observation().frozen);
    static_cast<void>(client.Advance(1, 1, 0, 0, 0));
    CHECK(client.Observation().latestCommand == maximum);
    client.Reconcile({1, {2, 0, 2}, 0, 0, maximum, maximum, 0}, 2);
    CHECK(client.PendingInput().commands.empty());
    CHECK_FALSE(client.Advance(1, 1, 0, 0, 0));
    CHECK(client.Observation().latestCommand == maximum);
    client.Reconcile({1, {20, 0, 20}, 0, 0, 0, 0, 0}, 3);
    CHECK(client.PendingInput().movementEpoch == maximum);
    CHECK(client.Observation().authorityTick == 2);
    PvpMatch match(PredictionArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    CHECK_FALSE(match.SubmitInput({1, {{maximum, 1, 0, 0, 0}}, 1}));
    CHECK_FALSE(match.SubmitInput({1, {{1, 1, 0, 0, 0}}, maximum}));
    CHECK_FALSE(match.SubmitInput({1, {{1, 1, 0, 0, 0}}, 0}));
    Step(match);
    CHECK(match.Snapshot().players.front().lastResolvedCommand == 0);
    CHECK(match.Snapshot().players.front().movementEpoch == 1);
}

TEST_CASE("PvP prediction and authority execute identical straight diagonal turning stopping and wall commands") {
    const auto arena = PredictionArena();
    for (int scenario = 0; scenario < 6; ++scenario) {
        INFO("movement scenario ", scenario);
        PvpMatch match(arena);
        std::string error;
        REQUIRE(match.Join(1, error));
        LocalPlayerPrediction client(arena);
        auto reference = match.Snapshot().players.front();
        std::map<std::uint64_t, PlayerState> expected{{0, reference}};
        client.Reconcile(reference, 0);
        for (unsigned frame = 0; frame < 240; ++frame) {
            const float forward = scenario == 3 && frame > 60 ? 0.0F : 1.0F;
            const float right = scenario == 1 || scenario == 5 ? 1.0F : 0.0F;
            const float yaw = scenario == 2 ? static_cast<float>(frame) * 0.015F :
                scenario == 4 ? std::numbers::pi_v<float> / 2 : 0.0F;
            const bool send = client.Advance(MovementTickSeconds, forward, right, yaw, 0.2F);
            const auto window = client.PendingInput();
            for (const auto& command : window.commands) {
                if (command.sequence <= reference.lastResolvedCommand) continue;
                reference = StepMovement(arena, reference, command);
                expected.emplace(command.sequence, reference);
            }
            SamePosition(client.Observation().predictedPosition, reference.position);
            if (send) REQUIRE(match.SubmitInput(window));
            Step(match);
            const auto authority = match.Snapshot().players.front();
            REQUIRE(expected.contains(authority.lastResolvedCommand));
            SamePosition(authority.position, expected.at(authority.lastResolvedCommand).position);
            if (match.TickCount() % SnapshotIntervalTicks == 0) client.Reconcile(authority, match.TickCount());
            CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
            ClearBody(arena, client.Observation().renderPosition);
        }
        if (scenario == 0) CHECK(reference.position.z == doctest::Approx(14).epsilon(0.0001));
        if (scenario == 3) CHECK(reference.position.z == doctest::Approx(5.05).epsilon(0.0001));
        if (scenario == 5) CHECK(reference.position.z < 8.751F); // inner wall/corner
    }
}

TEST_CASE("PvP presentation interpolates independently of snapshots at 30 60 and 144 FPS") {
    for (const auto fps : {30, 60, 144}) {
        INFO("render FPS ", fps);
        const auto evidence = RunLink({0, false, false, fps, true});
        REQUIRE(evidence.frameCount > 30);
        CHECK(evidence.movingFrames == evidence.frameCount);
        CHECK(evidence.maximumDisplayStep <= 3.0F / fps + 0.004F);
        CHECK(evidence.maximumPending <= MaxPendingCommands);
        CHECK(evidence.maximumCorrection < 0.0001F);
        CHECK(evidence.resends >= 350);
        CHECK(evidence.resends <= 365);
    }
}

TEST_CASE("PvP windows recover first stop burst loss duplicates reordering jitter and client stalls on LAN") {
    for (const auto rtt : {0, 20, 40}) {
        INFO("round trip milliseconds ", rtt);
        const auto evidence = RunLink({rtt, true, true, 60, false});
        CHECK(evidence.maximumPending <= MaxPendingCommands);
        CHECK(evidence.snapshotCount > 100);
        CHECK(evidence.reorderedInputs > 0);
        CHECK(evidence.finalPosition > 8);
        CHECK(evidence.lateRecoveryFrames < 30);
        CHECK(evidence.maximumCorrection < 1);
        CHECK(evidence.firstPredictedMotionMs == 300);
        CHECK(evidence.firstAuthorityMotionMs <= 450);
        CHECK(evidence.lastAuthorityMotionBeforeStopMs <= 2500);
        CHECK(std::abs(evidence.firstStopPosition - 8.0F) <= 0.35F);
    }
}

TEST_CASE("PvP LAN first complete window keeps local onset immediate and authority delay within its step budget") {
    CHECK(InitialCommandLead == 2);
    for (const auto rtt : {0, 20, 40}) {
        INFO("reliable LAN round trip milliseconds ", rtt);
        const auto evidence = RunLink({rtt, false, false, 60, false});
        const int transportTicks = rtt == 40 ? 1 : 0;
        // The complete initial window contains the two neutral commands and
        // one legitimately generated command. RTT40 adds one transport tick;
        // this virtual wire observes arrivals on integer milliseconds.
        const auto lead = InitialCommandLead + 1 + transportTicks;
        const double delayBudgetMs = std::ceil(lead * MovementTickSeconds * 1000);
        CHECK(evidence.firstPredictedMotionMs == 300);
        CHECK(evidence.firstDisplayedMotionMs <= 317);
        CHECK(evidence.firstAuthorityMotionMs > evidence.firstPredictedMotionMs);
        CHECK(evidence.firstAuthorityMotionMs - 300 <= delayBudgetMs);
        CHECK(evidence.maximumSteadyCommandLead == lead);
        CHECK(evidence.lastAuthorityMotionBeforeStopMs - 2300 <= delayBudgetMs);
        CHECK(evidence.firstStopPosition == doctest::Approx(8).epsilon(0.0001));
        CHECK(evidence.maximumCorrection < 0.0001F);
    }
}

TEST_CASE("PvP small corrections converge within 100 ms and large corrections relocate immediately") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    static_cast<void>(client.Advance(0, 0, 0, 0, 0)); // Establish the initial frame before correction.
    client.Reconcile({1, {2.4F, 0, 2}, 0, 0, 1}, 2);
    CHECK(client.Observation().renderPosition.x == doctest::Approx(2));
    CHECK(client.Observation().correctionOffset.x == doctest::Approx(-0.4));
    static_cast<void>(client.Advance(0.05, 0, 0, 0, 0));
    CHECK(client.Observation().renderPosition.x == doctest::Approx(2.2));
    // A confirming snapshot must not restart the already-decaying correction.
    client.Reconcile({1, {2.4F, 0, 2}, 0, 0, 2}, 3);
    static_cast<void>(client.Advance(0.05, 0, 0, 0, 0));
    CHECK(client.Observation().renderPosition.x == doctest::Approx(2.4));
    CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
    client.Reconcile({1, {4, 0, 2}, 0, 0, 3}, 4);
    CHECK(client.Observation().renderPosition.x == doctest::Approx(4));
    CHECK(Distance(client.Observation().correctionOffset, {}) < 0.00001F);
}

TEST_CASE("PvP display correction cannot sweep the local camera across a static corner") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    client.Reconcile({1, {7.72F, 0, 4.1F}, 0, 0, 0}, 1);
    // Both endpoints are valid, but the direct correction line cuts the corner.
    client.Reconcile({1, {8.1F, 0, 3.72F}, 0, 0, 1}, 2);
    CHECK(Distance(client.Observation().renderPosition, {7.72F, 0, 4.1F}) > 0.01F);
    for (int frame = 0; frame < 20; ++frame) {
        static_cast<void>(client.Advance(1.0 / 144, 1, 1, 0, 0));
        ClearBody(arena, client.Observation().renderPosition);
        ClearBody(arena, client.Observation().predictedPosition);
    }
}

TEST_CASE("PvP authority catch-up reseeds neutral lead and lifecycle reset discards old motion") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction client(arena);
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    for (std::size_t frame = 0; frame < MaxPendingCommands - InitialCommandLead; ++frame)
        static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
    client.Reconcile({1, {2, 0, 3}, 0, 0, 40}, 60);
    REQUIRE(client.PendingInput().commands.size() == InitialCommandLead);
    CHECK(client.PendingInput().commands.front().sequence == 41);
    CHECK(client.PendingInput().commands.back().sequence == 40 + InitialCommandLead);
    for (const auto& command : client.PendingInput().commands) CHECK(command.moveForward == 0);
    // Focus loss produces new neutral commands; it cannot mutate sent commands.
    static_cast<void>(client.Advance(MovementTickSeconds, 0, 0, 0.7F, 0.3F));
    CHECK(client.PendingInput().commands.back().moveForward == 0);
    CHECK(client.PendingInput().commands.back().yaw == 0.7F);
    const auto tip = client.Observation().latestCommand;
    client.Reconcile({1, {9, 0, 9}, 0, 0, 39}, 59);
    CHECK(client.Observation().latestCommand == tip);
    client.Reset(); // leave/disconnect uses this same path
    CHECK_FALSE(client.Observation().active);
    CHECK(client.PendingInput().commands.empty());
    CHECK_FALSE(client.Advance(0.1, 1, 0, 0, 0));
    client.Reconcile({2, {20, 0, 2}, 0, 0, 0}, 90);
    CHECK(client.PendingInput().playerId == 2);
    CHECK(client.PendingInput().commands.front().sequence == 1);
    SamePosition(client.Observation().renderPosition, {20, 0, 2});
    client.Reconcile({3, {2, 0, 2}, 0, 0, 0}, 1); // identity itself also resets
    CHECK(client.PendingInput().playerId == 3);
    CHECK(client.Observation().authorityTick == 1);
    CHECK(client.PendingInput().commands.front().sequence == 1);
}

TEST_CASE("PvP reseed does not catch up elapsed time already covered by authority") {
    const auto arena = PredictionArena();
    for (const auto previousFrameSeconds : {0.083, 0.108485702, 0.25}) {
        INFO("elapsed interval before authoritative reseed ", previousFrameSeconds);
        LocalPlayerPrediction client(arena);
        // Real GUI trace: before a capture pause ACK20/tip26; the next received
        // authority has passed that tip. It establishes a fresh bounded lead.
        client.Reconcile({1, {5, 0, 5}, 0, 0, 20}, 2994);
        static_cast<void>(client.Advance(0, 0, 0, 0, 0));
        static_cast<void>(client.Advance(4 * MovementTickSeconds, 0, 0, 0, 0));
        REQUIRE(client.Observation().latestCommand == 26);
        client.Reconcile({1, {5, 0, 5}, 0, 0, 27}, 3000);
        REQUIRE(client.Observation().pendingCommands == InitialCommandLead);
        CHECK(client.Advance(previousFrameSeconds, 1, 0, 0.4F, 0.2F));
        CHECK(client.Observation().latestCommand == 27 + InitialCommandLead + 1);
        CHECK(client.Observation().pendingCommands == InitialCommandLead + 1);
        CHECK(Distance(client.Observation().predictedPosition, {5, 0, 5}) ==
            doctest::Approx(arena.movementSpeed * MovementTickSeconds));
        CHECK(client.PendingInput().commands.back().yaw == 0.4F);
        CHECK(client.PendingInput().commands.back().pitch == 0.2F);
        static_cast<void>(client.Advance(MovementTickSeconds, 0, 0, 0.6F, 0.3F));
        CHECK(client.Observation().latestCommand == 27 + InitialCommandLead + 2);
        CHECK(client.PendingInput().commands.back().moveForward == 0);
    }
    // Startup has the same authoritative baseline and must not predict a
    // quarter second of input collected before the player even joined.
    LocalPlayerPrediction joining(arena);
    joining.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    CHECK(joining.Advance(0.25, 1, 0, 0, 0));
    CHECK(joining.Observation().pendingCommands == InitialCommandLead + 1);
    CHECK(joining.Observation().predictedPosition.z == doctest::Approx(2.05));
    static_cast<void>(joining.Advance(MovementTickSeconds, 1, 0, 0, 0));
    CHECK(joining.Observation().renderPosition.z == doctest::Approx(2.05));

    // Worker/render/authority recovery and the complete bounded lead window
    // are exercised with independent clocks in MovementRecoveryTests.cpp.
}
