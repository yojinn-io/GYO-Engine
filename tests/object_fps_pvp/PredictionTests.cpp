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
#include <initializer_list>
#include <map>
#include <limits>
#include <numbers>
#include <optional>
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

namespace {
// The start-phase decision needs eight frame intervals. They belong to the
// display loop and survive an epoch reseed, so an earlier epoch at this frame
// rate lets the wait of the epoch started afterwards arm at once.
void WarmFrames(LocalPlayerPrediction& client, std::initializer_list<double> frames) {
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    for (const double seconds : frames) static_cast<void>(client.Advance(seconds, 0, 0, 0, 0));
}
void WarmFrames(LocalPlayerPrediction& client, double seconds = MovementTickSeconds) {
    WarmFrames(client, {seconds, seconds, seconds, seconds, seconds, seconds, seconds, seconds});
}
// Starts epoch 2 at authority tick 10 and publishes its first window (sequences 1-3).
void StartEpoch(LocalPlayerPrediction& client, double seconds = MovementTickSeconds) {
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0, 2}, 10);
    REQUIRE(client.Advance(seconds, 1, 0, 0, 0));
    CHECK(client.PendingInput().commands.size() == InitialCommandLead + 1);
}
PlayerState EpochAuthority(std::uint64_t acknowledged, std::optional<std::uint32_t> waitMicros) {
    PlayerState authority{1, {2, 0, 2}, 0, 0, acknowledged, 2};
    authority.epochStartWaitMicros = waitMicros;
    return authority;
}
// Twins from one epoch start; only `aligned` hears the Host's 18 ms start wait.
// Neutral input and acknowledgements two commands behind the tip keep the
// window short and positions exact, so the twins differ only in their
// fixed-step clocks. Frame() returns how far the aligned clock trails, in seconds.
struct StartPhaseTwins {
    explicit StartPhaseTwins(const Arena& arena) : aligned(arena), unaligned(arena) {
        for (auto* client : {&aligned, &unaligned}) {
            client->Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
            // Half a tick of startup time: the first real step is half a tick old when published.
            CHECK_FALSE(client->Advance(MovementTickSeconds / 2, 0, 0, 0, 0));
        }
    }
    double Frame(double seconds, bool wait = true) {
        ++tick;
        for (auto* client : {&aligned, &unaligned}) {
            static_cast<void>(client->Advance(seconds, 0, 0, 0, 0));
            const auto latest = client->Observation().latestCommand;
            PlayerState authority{1, {2, 0, 2}, 0, 0, latest - InitialCommandLead};
            if (client == &aligned && wait) authority.epochStartWaitMicros = 18000;
            client->Reconcile(authority, tick);
            REQUIRE(client->Observation().pendingCommands == InitialCommandLead);
        }
        return Clock(unaligned) - Clock(aligned);
    }
    // A stall reseed: authority resolved past both tips. The twins seed from
    // the same state, so their fixed-step clocks match from here on.
    void Reseed(double seconds) {
        ++tick;
        for (auto* client : {&aligned, &unaligned}) static_cast<void>(client->Advance(seconds, 0, 0, 0, 0));
        const auto tip = (std::max)(aligned.Observation().latestCommand, unaligned.Observation().latestCommand);
        for (auto* client : {&aligned, &unaligned}) {
            PlayerState authority{1, {2, 0, 2}, 0, 0, tip + 1};
            if (client == &aligned) authority.epochStartWaitMicros = 18000;
            client->Reconcile(authority, tick);
        }
    }
    [[nodiscard]] bool Shifted() const { return aligned.Observation().startPhaseShiftSeconds.has_value(); }
    static double Clock(const LocalPlayerPrediction& client) {
        return (client.Observation().latestCommand + client.Observation().interpolationAlpha) * MovementTickSeconds;
    }
    // Shift = wait + first-step age - target.
    static constexpr double Shift = 0.018 + MovementTickSeconds / 2 - MovementStartPhaseTargetSeconds;
    LocalPlayerPrediction aligned, unaligned;
    std::uint64_t tick{1};
};
} // namespace

TEST_CASE("PvP epoch start wait shifts the fixed-step phase once without moving the display backwards") {
    const auto arena = PredictionArena();
    const auto started = [&](LocalPlayerPrediction& client) {
        WarmFrames(client);
        StartEpoch(client);
    };
    LocalPlayerPrediction aligned(arena), unaligned(arena);
    started(aligned);
    started(unaligned);
    auto authority = EpochAuthority(1, std::nullopt);
    unaligned.Reconcile(authority, 11);
    authority.epochStartWaitMicros = 14781;
    aligned.Reconcile(authority, 11);
    CHECK_FALSE(unaligned.Observation().startPhaseShiftSeconds);
    REQUIRE(aligned.Observation().epochStartWaitSeconds);
    CHECK(*aligned.Observation().epochStartWaitSeconds == doctest::Approx(0.014781));
    REQUIRE(aligned.Observation().startPhaseShiftSeconds);
    CHECK_FALSE(aligned.Observation().startPhaseSkip);
    const double shift = 0.014781 - MovementStartPhaseTargetSeconds;
    CHECK(*aligned.Observation().startPhaseShiftSeconds == doctest::Approx(shift));

    float travelled{};
    for (int frame = 0; frame < 8; ++frame) { // Stays within the pending window.
        static_cast<void>(aligned.Advance(MovementTickSeconds, 1, 0, 0, 0));
        static_cast<void>(unaligned.Advance(MovementTickSeconds, 1, 0, 0, 0));
        const auto distance = Distance(aligned.Observation().renderPosition, {2, 0, 2});
        CHECK(distance + 0.00001F >= travelled);
        travelled = distance;
        // A repeated report of the same epoch never shifts again.
        aligned.Reconcile(authority, 12 + frame);
    }
    CHECK(unaligned.Observation().interpolationAlpha < 0.0001F);
    CHECK(aligned.Observation().interpolationAlpha ==
        doctest::Approx(1.0 - shift / MovementTickSeconds).epsilon(0.0001));
    CHECK(aligned.Observation().latestCommand + 1 == unaligned.Observation().latestCommand);

    // A Host that was late for the start tick reports an unrepresentative wait.
    // That needs no frame evidence: it is rejected as soon as it arrives.
    LocalPlayerPrediction late(arena);
    late.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    REQUIRE(late.Advance(MovementTickSeconds, 1, 0, 0, 0));
    PlayerState lateAuthority{1, {2, 0, 2}, 0, 0, 1};
    lateAuthority.epochStartWaitMicros = 30000;
    late.Reconcile(lateAuthority, 2);
    REQUIRE(late.Observation().epochStartWaitSeconds);
    CHECK_FALSE(late.Observation().startPhaseShiftSeconds);
    CHECK(late.Observation().startPhaseSkip == StartPhaseSkip::HostLate);

    // A wait for an epoch the client reseeded from a later state is not applied;
    // the reseed cancelled the pending start phase.
    LocalPlayerPrediction reseeded(arena);
    started(reseeded);
    auto later = EpochAuthority(5, 9000);
    reseeded.Reconcile(later, 11);
    static_cast<void>(reseeded.Advance(MovementTickSeconds, 1, 0, 0, 0));
    later.lastResolvedCommand = 6;
    reseeded.Reconcile(later, 12);
    CHECK_FALSE(reseeded.Observation().epochStartWaitSeconds);
    CHECK_FALSE(reseeded.Observation().startPhaseShiftSeconds);
    CHECK(reseeded.Observation().startPhaseSkip == StartPhaseSkip::CancelledByReseed);
}

TEST_CASE("PvP start phase waits for a full frame window and is not applied below about 54.5 FPS") {
    const auto arena = PredictionArena();
    // A first epoch has no frame history: the wait stays pending (the phase
    // the Host timed is unchanged) until eight frames describe the frame rate.
    LocalPlayerPrediction fresh(arena);
    fresh.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    REQUIRE(fresh.Advance(MovementTickSeconds, 0, 0, 0, 0));
    PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
    authority.epochStartWaitMicros = 14781;
    fresh.Reconcile(authority, 2);
    CHECK_FALSE(fresh.Observation().epochStartWaitSeconds);
    CHECK_FALSE(fresh.Observation().startPhaseShiftSeconds);
    CHECK_FALSE(fresh.Observation().startPhaseSkip);
    for (int frame = 0; frame < 7; ++frame) static_cast<void>(fresh.Advance(MovementTickSeconds, 0, 0, 0, 0));
    fresh.Reconcile(authority, 3);
    REQUIRE(fresh.Observation().startPhaseShiftSeconds);
    CHECK(*fresh.Observation().startPhaseShiftSeconds ==
        doctest::Approx(0.014781 - MovementStartPhaseTargetSeconds));
    CHECK_FALSE(fresh.Observation().startPhaseSkip);

    // 59.94 Hz frames are 60 FPS frames.
    LocalPlayerPrediction ntsc(arena);
    WarmFrames(ntsc, 1001.0 / 60000.0);
    StartEpoch(ntsc, 1001.0 / 60000.0);
    ntsc.Reconcile(EpochAuthority(1, 14781), 11);
    CHECK(ntsc.Observation().startPhaseShiftSeconds);
    CHECK_FALSE(ntsc.Observation().startPhaseSkip);

    // Below about 54.5 FPS (a mean above 1.1 tick) the wait is recorded and the
    // shift armed withdrawn: the epoch keeps its unaligned phase while frames
    // stay there.
    for (const double frameSeconds : {1.0 / 30, 1.0 / 40, 1.0 / 50}) {
        INFO("frame seconds ", frameSeconds);
        LocalPlayerPrediction aligned(arena), unaligned(arena);
        for (auto* client : {&aligned, &unaligned}) {
            WarmFrames(*client, frameSeconds);
            StartEpoch(*client, frameSeconds);
        }
        aligned.Reconcile(EpochAuthority(1, 14781), 11);
        unaligned.Reconcile(EpochAuthority(1, std::nullopt), 11);
        REQUIRE(aligned.Observation().epochStartWaitSeconds);
        CHECK(*aligned.Observation().epochStartWaitSeconds == doctest::Approx(0.014781));
        CHECK_FALSE(aligned.Observation().startPhaseShiftSeconds);
        CHECK(aligned.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
        for (int frame = 0; frame < 3; ++frame) {
            static_cast<void>(aligned.Advance(frameSeconds, 1, 0, 0, 0));
            static_cast<void>(unaligned.Advance(frameSeconds, 1, 0, 0, 0));
            CHECK(aligned.Observation().latestCommand == unaligned.Observation().latestCommand);
            CHECK(aligned.Observation().interpolationAlpha == unaligned.Observation().interpolationAlpha);
        }
    }
}

TEST_CASE("PvP start phase frame rate counts every dropped refresh and a long hitch as one") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    // The epoch's first window adds one more 60 FPS interval.
    const auto shifted = [&](std::initializer_list<double> frames) {
        LocalPlayerPrediction client(arena);
        WarmFrames(client, frames);
        StartEpoch(client);
        client.Reconcile(EpochAuthority(1, 14781), 11);
        REQUIRE(client.Observation().epochStartWaitSeconds);
        CHECK((client.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick) !=
              client.Observation().startPhaseShiftSeconds.has_value());
        return client.Observation().startPhaseShiftSeconds.has_value();
    };
    // A 250 ms stall weighs like one missed refresh: 17 ticks over 16 frames.
    CHECK(shifted({T, T, T, T, T, T, T, 0.25, T, T, T, T, T, T, T}));
    // One missed refresh in sixteen frames (56 FPS) stays within the cut...
    CHECK(shifted({T, T, T, T, T, T, T, T, T, T, T, T, T, T, 2 * T}));
    // ...two (53 FPS) do not, nor does a vsync-paced 50 FPS that shows every
    // fifth frame for two refreshes: dropped refreshes are never discarded.
    CHECK_FALSE(shifted({T, T, T, T, T, T, T, T, T, T, T, T, T, 2 * T, 2 * T}));
    CHECK_FALSE(shifted({T, T, T, T, 2 * T, T, T, T, T, 2 * T, T, T, T, T, 2 * T}));
    // Eight frames suffice for a decision.
    CHECK_FALSE(shifted({T, T, T, T, T, 1.0 / 30, 1.0 / 30, 1.0 / 30}));
}

TEST_CASE("PvP a drop below about 54.5 FPS withdraws the start phase shift mid-slew and restores it on recovery") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    StartPhaseTwins twins(arena);
    for (int index = 0; index < 7; ++index) CHECK(twins.Frame(T, false) == doctest::Approx(0));
    CHECK(twins.Frame(T) == doctest::Approx(0));
    REQUIRE(twins.aligned.Observation().startPhaseShiftSeconds);
    CHECK(*twins.aligned.Observation().startPhaseShiftSeconds == doctest::Approx(StartPhaseTwins::Shift));
    double offset = twins.Frame(T);
    CHECK(offset == doctest::Approx(T / 4));
    // 30 FPS: withdrawn within four frames (133 ms), here before the slew has
    // applied the whole shift.
    int slowFrames{};
    double applied = offset;
    while (twins.Shifted()) {
        REQUIRE(++slowFrames <= 4);
        offset = twins.Frame(2 * T);
        applied = (std::max)(applied, offset);
    }
    CHECK(applied < StartPhaseTwins::Shift);
    CHECK(twins.aligned.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
    // The applied part is slewed back at the same rate and the shift stays
    // withdrawn while frames stay at 30 FPS; the twins then match.
    for (int index = 0; index < 60; ++index) {
        const double next = twins.Frame(2 * T);
        CHECK(next <= offset + 1.0e-9);
        CHECK_FALSE(twins.Shifted());
        offset = next;
    }
    CHECK(offset == doctest::Approx(0).epsilon(0.000001));
    CHECK(twins.aligned.Observation().latestCommand == twins.unaligned.Observation().latestCommand);
    // Back at 60 FPS the slow frames leave the 32-frame window and the restore
    // bound then holds for 32 frames: the same shift returns within 64 frames
    // and is slewed in again; the repeated wait never re-arms it.
    int recoveryFrames{};
    while (!twins.Shifted()) {
        REQUIRE(++recoveryFrames <= 64);
        offset = twins.Frame(T);
        if (!twins.Shifted()) CHECK(offset == doctest::Approx(0).epsilon(0.000001));
    }
    CHECK(recoveryFrames > 32);
    CHECK(*twins.aligned.Observation().startPhaseShiftSeconds == doctest::Approx(StartPhaseTwins::Shift));
    CHECK_FALSE(twins.aligned.Observation().startPhaseSkip);
    for (int index = 0; index < 8; ++index) offset = twins.Frame(T);
    CHECK(offset == doctest::Approx(StartPhaseTwins::Shift).epsilon(0.000001));

    // Reset clears the decision and the frame evidence: a new session's wait
    // waits for eight new frames again.
    auto& aligned = twins.aligned;
    aligned.Reset();
    CHECK_FALSE(aligned.Observation().epochStartWaitSeconds);
    CHECK_FALSE(aligned.Observation().startPhaseShiftSeconds);
    CHECK_FALSE(aligned.Observation().startPhaseSkip);
    aligned.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    REQUIRE(aligned.Advance(T, 0, 0, 0, 0));
    PlayerState rejoined{1, {2, 0, 2}, 0, 0, 1};
    rejoined.epochStartWaitMicros = 18000;
    aligned.Reconcile(rejoined, 2);
    CHECK_FALSE(aligned.Observation().epochStartWaitSeconds);
}

TEST_CASE("PvP a start phase armed during startup hitches is applied once frames recover") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    StartPhaseTwins twins(arena);
    // Three 50 ms startup frames still dominate the frame evidence when the wait arrives.
    static_cast<void>(twins.Frame(T, false));
    for (int index = 0; index < 3; ++index) static_cast<void>(twins.Frame(0.05, false));
    for (int index = 0; index < 3; ++index) static_cast<void>(twins.Frame(T, false));
    CHECK(twins.Frame(T) == doctest::Approx(0));
    const auto& observation = twins.aligned.Observation();
    REQUIRE(observation.epochStartWaitSeconds);
    CHECK(*observation.epochStartWaitSeconds == doctest::Approx(0.018));
    CHECK_FALSE(observation.startPhaseShiftSeconds);
    CHECK(observation.startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
    // Armed withdrawn, nothing is applied until frames recover; then the shift
    // measured at the epoch start is applied in full.
    int frames{};
    while (!twins.Shifted()) {
        REQUIRE(++frames <= 64);
        const double offset = twins.Frame(T);
        if (!twins.Shifted()) CHECK(offset == doctest::Approx(0).epsilon(0.000001));
    }
    CHECK(*observation.startPhaseShiftSeconds == doctest::Approx(StartPhaseTwins::Shift));
    CHECK_FALSE(observation.startPhaseSkip);
    double offset{};
    for (int index = 0; index < 8; ++index) offset = twins.Frame(T);
    CHECK(offset == doctest::Approx(StartPhaseTwins::Shift).epsilon(0.000001));
}

TEST_CASE("PvP start phase near the cut keeps its state and a short recovery does not restore it") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    StartPhaseTwins twins(arena);
    unsigned toggles{};
    bool shifted{};
    const auto frame = [&](double seconds) {
        static_cast<void>(twins.Frame(seconds));
        if (twins.Shifted() != shifted) {
            ++toggles;
            shifted = !shifted;
        }
    };
    // A vsync-paced 50 FPS display shows every fifth frame for two refreshes.
    int vsyncFrame{};
    const auto vsync50 = [&] { frame(++vsyncFrame % 5 == 0 ? 2 * T : T); };
    for (int index = 0; index < 7; ++index) static_cast<void>(twins.Frame(T, false));
    static_cast<void>(twins.Frame(T));
    REQUIRE(twins.Shifted());
    shifted = true;
    // 55 FPS lies between the restore bound and the cut: an applied shift stays.
    for (int index = 0; index < 120; ++index) frame(1.0 / 55);
    CHECK(toggles == 0);
    for (int index = 0; index < 32; ++index) frame(T);
    // A drop to vsync 50 FPS withdraws it once four doubled frames are in the
    // window: within 20 frames (0.4 s). It stays withdrawn while the drop lasts.
    int dropFrames{};
    while (shifted) {
        REQUIRE(++dropFrames <= 20);
        vsync50();
    }
    for (int index = 0; index < 120; ++index) vsync50();
    CHECK(toggles == 1);
    // Half a second of whole 60 Hz frames inside that drop does not restore it.
    for (int index = 0; index < 30; ++index) frame(T);
    for (int index = 0; index < 60; ++index) vsync50();
    // Nor does 55 FPS: a withdrawn shift stays withdrawn there too.
    for (int index = 0; index < 120; ++index) frame(1.0 / 55);
    CHECK(toggles == 1);
    CHECK(twins.aligned.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
    // Sustained 60 FPS restores it within 64 frames.
    int recoveryFrames{};
    while (!shifted) {
        REQUIRE(++recoveryFrames <= 64);
        frame(T);
    }
    CHECK(toggles == 2);
    CHECK_FALSE(twins.aligned.Observation().startPhaseSkip);
}

TEST_CASE("PvP a withdrawal slew in 4.0-4.4 tick frames never makes the runtime drop a step") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    // About 14 FPS, alternating two frame lengths. Neither clock drops a step
    // at these lengths by itself, but a frame plus a withdrawal slew of a
    // quarter of it can exceed the runtime's five catch-up steps. The slew
    // takes only what the runtime can still step, so the phase moves
    // continuously back to the unshifted one instead of losing a step.
    for (const double first : {4.0, 4.2, 4.4})
        for (const double second : {4.0, 4.2, 4.4}) {
            INFO("frame ticks ", first, " and ", second);
            StartPhaseTwins twins(arena);
            for (int index = 0; index < 7; ++index) static_cast<void>(twins.Frame(T, false));
            static_cast<void>(twins.Frame(T));
            REQUIRE(twins.Shifted());
            double offset{};
            for (int index = 0; index < 8; ++index) offset = twins.Frame(T);
            REQUIRE(offset == doctest::Approx(StartPhaseTwins::Shift).epsilon(0.000001));
            for (int index = 0; index < 40; ++index) {
                const double seconds = (index % 2 == 0 ? first : second) * T;
                const double next = twins.Frame(seconds);
                // Within a microsecond (float interpolation alpha); a lost step is a tick.
                CHECK(next <= offset + 1.0e-6);
                CHECK(offset - next <= seconds * 0.25 + 1.0e-6);
                CHECK(next >= -1.0e-6);
                offset = next;
            }
            CHECK_FALSE(twins.Shifted());
            CHECK(twins.aligned.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
            CHECK(offset == doctest::Approx(0).epsilon(0.000001));
            CHECK(twins.aligned.Observation().latestCommand == twins.unaligned.Observation().latestCommand);
        }
}

TEST_CASE("PvP a stall reseed cancels an armed or withdrawn start phase without moving the phase") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    for (const bool withdrawn : {false, true}) {
        INFO("withdrawn ", withdrawn);
        StartPhaseTwins twins(arena);
        for (int index = 0; index < 7; ++index) static_cast<void>(twins.Frame(T, false));
        static_cast<void>(twins.Frame(T));
        REQUIRE(twins.Shifted());
        for (int index = 0; index < 8; ++index) static_cast<void>(twins.Frame(T));
        if (withdrawn) {
            int slowFrames{};
            while (twins.Shifted()) {
                REQUIRE(++slowFrames <= 4);
                static_cast<void>(twins.Frame(2 * T));
            }
            REQUIRE(twins.aligned.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
        }
        // The reseed rebases the phase the Host timed: the shift is cleared
        // and the wait already taken stays reported.
        twins.Reseed(withdrawn ? 2 * T : T);
        const auto& observation = twins.aligned.Observation();
        REQUIRE(observation.epochStartWaitSeconds);
        CHECK(*observation.epochStartWaitSeconds == doctest::Approx(0.018));
        CHECK_FALSE(observation.startPhaseShiftSeconds);
        CHECK(observation.startPhaseSkip == StartPhaseSkip::CancelledByReseed);
        // Nothing is armed any more: neither 30 FPS nor a long run of 60 FPS
        // frames changes the reason or brings a shift back, and the twins'
        // clocks, seeded alike, stay equal.
        for (int index = 0; index < 60; ++index) {
            CHECK(twins.Frame(2 * T) == doctest::Approx(0).epsilon(0.000001));
            CHECK(observation.startPhaseSkip == StartPhaseSkip::CancelledByReseed);
        }
        for (int index = 0; index < 120; ++index) {
            CHECK(twins.Frame(T) == doctest::Approx(0).epsilon(0.000001));
            CHECK_FALSE(twins.Shifted());
            CHECK(observation.startPhaseSkip == StartPhaseSkip::CancelledByReseed);
        }
        CHECK(twins.aligned.Observation().latestCommand == twins.unaligned.Observation().latestCommand);
        REQUIRE(observation.epochStartWaitSeconds);
        CHECK(*observation.epochStartWaitSeconds == doctest::Approx(0.018));
        twins.aligned.Reset();
        CHECK_FALSE(observation.epochStartWaitSeconds);
        CHECK_FALSE(observation.startPhaseSkip);
    }
}

TEST_CASE("PvP a stall reseed inside the frame window cancels the pending start phase and the next epoch starts afresh") {
    const auto arena = PredictionArena();
    constexpr double T = MovementTickSeconds;
    LocalPlayerPrediction client(arena);
    const auto& observation = client.Observation();
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    REQUIRE(client.Advance(T, 0, 0, 0, 0));
    PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
    authority.epochStartWaitMicros = 14781;
    client.Reconcile(authority, 2);
    static_cast<void>(client.Advance(T, 0, 0, 0, 0));
    // Two frame intervals: the wait is still deferred.
    authority.lastResolvedCommand = 2;
    client.Reconcile(authority, 3);
    CHECK_FALSE(observation.epochStartWaitSeconds);
    CHECK_FALSE(observation.startPhaseSkip);
    static_cast<void>(client.Advance(T, 0, 0, 0, 0));
    authority.lastResolvedCommand = observation.latestCommand + 1;
    client.Reconcile(authority, 4);
    CHECK_FALSE(observation.epochStartWaitSeconds);
    CHECK_FALSE(observation.startPhaseShiftSeconds);
    CHECK(observation.startPhaseSkip == StartPhaseSkip::CancelledByReseed);
    // The frame window fills later; the repeated wait is never taken.
    std::uint64_t tick = 4;
    for (int frame = 0; frame < 16; ++frame) {
        static_cast<void>(client.Advance(T, 0, 0, 0, 0));
        authority.lastResolvedCommand = observation.latestCommand - InitialCommandLead;
        client.Reconcile(authority, ++tick);
        CHECK_FALSE(observation.epochStartWaitSeconds);
        CHECK_FALSE(observation.startPhaseShiftSeconds);
        CHECK(observation.startPhaseSkip == StartPhaseSkip::CancelledByReseed);
    }
    // A new epoch clears the cancellation and decides its own wait at once
    // from the carried frame intervals.
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0, 2}, ++tick);
    CHECK_FALSE(observation.epochStartWaitSeconds);
    CHECK_FALSE(observation.startPhaseSkip);
    REQUIRE(client.Advance(T, 1, 0, 0, 0));
    client.Reconcile(EpochAuthority(1, 14781), ++tick);
    REQUIRE(observation.startPhaseShiftSeconds);
    CHECK(*observation.startPhaseShiftSeconds == doctest::Approx(0.014781 - MovementStartPhaseTargetSeconds));
    CHECK_FALSE(observation.startPhaseSkip);

    // A late Host's epoch armed nothing: a reseed keeps its reason.
    LocalPlayerPrediction late(arena);
    WarmFrames(late);
    StartEpoch(late);
    late.Reconcile(EpochAuthority(1, 30000), 11);
    REQUIRE(late.Observation().startPhaseSkip == StartPhaseSkip::HostLate);
    static_cast<void>(late.Advance(T, 0, 0, 0, 0));
    late.Reconcile(EpochAuthority(late.Observation().latestCommand + 1, 30000), 12);
    CHECK(late.Observation().epochStartWaitSeconds);
    CHECK(late.Observation().startPhaseSkip == StartPhaseSkip::HostLate);
}

TEST_CASE("PvP start phase skip reasons are cleared by Reset") {
    const auto arena = PredictionArena();
    LocalPlayerPrediction slow(arena), late(arena);
    WarmFrames(slow, 1.0 / 30);
    StartEpoch(slow, 1.0 / 30);
    slow.Reconcile(EpochAuthority(1, 14781), 11);
    REQUIRE(slow.Observation().startPhaseSkip == StartPhaseSkip::FrameRateBelowTick);
    WarmFrames(late);
    StartEpoch(late);
    late.Reconcile(EpochAuthority(1, 30000), 11);
    REQUIRE(late.Observation().startPhaseSkip == StartPhaseSkip::HostLate);
    for (auto* client : {&slow, &late}) {
        REQUIRE(client->Observation().epochStartWaitSeconds);
        client->Reset();
        CHECK_FALSE(client->Observation().epochStartWaitSeconds);
        CHECK_FALSE(client->Observation().startPhaseShiftSeconds);
        CHECK_FALSE(client->Observation().startPhaseSkip);
    }
}
