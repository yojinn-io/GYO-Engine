#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ClientSimulation.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PredictionElapsedTime.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace fps::pvp;

// The command-generation path exactly as the application ran it before the
// ClientSimulation seam (master 9a6fa8e, PvpApplication.cpp:419-424 and
// :969-976). Frozen here as the reference the seam must reproduce bit for bit.
class V6ApplicationPath final {
public:
    explicit V6ApplicationPath(const Arena& arena) : prediction_(arena) {}
    void Reset() noexcept {
        lastSnapshotTick_ = 0;
        prediction_.Reset();
        elapsed_.Reset();
    }
    void SelectArena(const Arena& arena) {
        prediction_ = LocalPlayerPrediction(arena);
        elapsed_.Reset();
    }
    void ClearJumpRequest() noexcept { prediction_.ClearJumpRequest(); }
    void Observe(const PlayerState& self, std::uint64_t tick, const std::optional<MovementRules>& rules) {
        if (tick > lastSnapshotTick_) {
            if (rules) prediction_.SetMovementRules(*rules);
            prediction_.Reconcile(self, tick);
            lastSnapshotTick_ = tick;
        }
    }
    std::optional<PlayerInput> Frame(PredictionElapsedTime::Clock::time_point now, const ClientInputSample& input) {
        const double movementElapsed = elapsed_.Sample(now);
        if (!input.controls) prediction_.ClearJumpRequest();
        if (prediction_.Advance(movementElapsed, input.forward, input.right, input.yaw, input.pitch,
                input.controls && input.jump))
            return prediction_.PendingInput();
        return std::nullopt;
    }
    std::optional<FireGateTiming> ShotTiming() const noexcept { return prediction_.ShotTiming(); }
    const LocalMovementObservation& Observation() const noexcept { return prediction_.Observation(); }

private:
    LocalPlayerPrediction prediction_;
    PredictionElapsedTime elapsed_;
    std::uint64_t lastSnapshotTick_{};
};

Arena SeamArena() {
    Arena arena;
    arena.id = "synthetic_client_simulation_arena";
    arena.width = arena.depth = 30;
    arena.walls = {{{-1, 0, -1}, {0, 3, 31}}, {{30, 0, -1}, {31, 3, 31}},
                   {{0, 0, -1}, {30, 3, 0}}, {{0, 0, 30}, {30, 3, 31}},
                   {{8, 0, 4}, {9, 3, 10}}};
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}

struct FrameRecord final {
    double atMilliseconds{};
    std::optional<PlayerInput> published;
    LocalMovementObservation observation;
    std::optional<FireGateTiming> shotTiming;
};

struct Scenario final {
    // Frame interval in milliseconds by virtual time; changes rate, jitter and stalls.
    double (*frameInterval)(double now, unsigned frame){};
    int snapshotDelayMilliseconds{};
    double durationMilliseconds{};
};

double MixedRates(double now, unsigned frame) {
    if (now < 1500) return frame % 3 == 0 ? 17.0 : 16.5;          // 60 FPS with jitter
    if (now < 3000) return frame % 2 == 0 ? 33.0 : 34.0;          // 30 FPS: several snapshots per frame
    if (now < 3500) return 7.0;                                   // 144 FPS
    if (now < 3800) return now < 3510 ? 250.0 : 16.7;             // one main-thread stall
    if (now < 4500) return frame % 5 == 0 ? 120.0 : 16.7;         // isolated long frames
    return 16.7;
}

double Steady60(double, unsigned) { return 1000.0 / 60.0; }

ClientInputSample InputAt(double now) {
    ClientInputSample input;
    input.forward = (now >= 200 && now < 2600) || (now >= 3600 && now < 5200) ? 1.0F : 0.0F;
    input.right = now >= 1200 && now < 2000 ? -0.5F : 0.0F;
    input.yaw = static_cast<float>(now / 4000.0);
    input.pitch = 0.1F;
    input.jump = (now >= 900 && now < 940) || (now >= 2400 && now < 2440) || (now >= 4100 && now < 4140);
    // Controls drop while a jump is requested: the pending request must be cleared.
    input.controls = !(now >= 2410 && now < 2700);
    return input;
}

// A deterministic loop shaped like the application: each frame drains every
// due snapshot, observes only the newest one, then samples and advances.
template <class Client>
std::vector<FrameRecord> Run(const Scenario& scenario) {
    const auto arena = SeamArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    Client client(arena);
    const MovementRules rules{1.4F, 21.0F};
    struct SnapshotPacket { double due; WorldSnapshot snapshot; };
    struct InputPacket { double due; PlayerInput input; };
    std::vector<SnapshotPacket> snapshots;
    std::vector<InputPacket> inputs;
    std::vector<FrameRecord> records;
    PlayerInput published;
    const PredictionElapsedTime::Clock::time_point origin{};
    double nextFrame{}, nextTick = 1000.0 / 60.0, nextSend{};
    unsigned frame{};
    bool resetDone{}, arenaSelected{};
    for (double now = 0; now <= scenario.durationMilliseconds; now += 0.5) {
        while (!inputs.empty() && inputs.front().due <= now) {
            static_cast<void>(match.SubmitInput(inputs.front().input));
            inputs.erase(inputs.begin());
        }
        if (now + 1e-9 >= nextTick) {
            nextTick += 1000.0 / 60.0;
            match.Tick({match.TickCount() + 1, MovementTickSeconds});
            snapshots.push_back({now + scenario.snapshotDelayMilliseconds, match.Snapshot()});
        }
        if (now + 1e-9 >= nextSend) {
            nextSend += 1000.0 / InputSendRate;
            if (!published.commands.empty()) inputs.push_back({now + 12, published});
        }
        if (now + 1e-9 < nextFrame) continue;
        nextFrame = now + scenario.frameInterval(now, frame++);
        // A session reset and an arena reselection happen as the application does them.
        if (!resetDone && now >= 2800) { client.Reset(); resetDone = true; }
        std::optional<WorldSnapshot> newest;
        while (!snapshots.empty() && snapshots.front().due <= now) {
            newest = std::move(snapshots.front().snapshot);
            snapshots.erase(snapshots.begin());
        }
        if (newest) {
            if (!arenaSelected && now >= 4300) { client.SelectArena(arena); arenaSelected = true; }
            client.Observe(newest->players.front(), newest->tick, rules);
        }
        const auto at = origin + std::chrono::duration_cast<PredictionElapsedTime::Clock::duration>(
            std::chrono::duration<double, std::milli>(now));
        FrameRecord record;
        record.atMilliseconds = now;
        record.published = client.Frame(at, InputAt(now));
        if (record.published) published = *record.published;
        record.observation = client.Observation();
        record.shotTiming = client.ShotTiming();
        records.push_back(std::move(record));
    }
    return records;
}

void SameInput(const std::optional<PlayerInput>& actual, const std::optional<PlayerInput>& expected) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (!expected) return;
    CHECK(actual->playerId == expected->playerId);
    CHECK(actual->movementEpoch == expected->movementEpoch);
    CHECK(actual->lifeGeneration == expected->lifeGeneration);
    CHECK(actual->observedAuthorityTick == expected->observedAuthorityTick);
    CHECK(actual->commands == expected->commands);
}

void SameVector(Engine::Math::Vec3 actual, Engine::Math::Vec3 expected) {
    CHECK(actual.x == expected.x);
    CHECK(actual.y == expected.y);
    CHECK(actual.z == expected.z);
}

void SameObservation(const LocalMovementObservation& actual, const LocalMovementObservation& expected) {
    SameVector(actual.predictedPosition, expected.predictedPosition);
    SameVector(actual.renderPosition, expected.renderPosition);
    SameVector(actual.correctionOffset, expected.correctionOffset);
    CHECK(actual.lastResolvedCommand == expected.lastResolvedCommand);
    CHECK(actual.latestCommand == expected.latestCommand);
    CHECK(actual.authorityTick == expected.authorityTick);
    CHECK(actual.pendingCommands == expected.pendingCommands);
    CHECK(actual.active == expected.active);
    CHECK(actual.frozen == expected.frozen);
    CHECK(actual.movementEpoch == expected.movementEpoch);
    CHECK(actual.interpolationAlpha == expected.interpolationAlpha);
    CHECK(actual.verticalVelocity == expected.verticalVelocity);
    CHECK(actual.grounded == expected.grounded);
    CHECK(actual.phaseTracking == expected.phaseTracking);
    CHECK(actual.phaseErrorSeconds == expected.phaseErrorSeconds);
    CHECK(actual.phaseCorrectionSeconds == expected.phaseCorrectionSeconds);
    CHECK(actual.phaseCorrections == expected.phaseCorrections);
    CHECK(actual.phaseLateCorrections == expected.phaseLateCorrections);
}

void SameTiming(const std::optional<FireGateTiming>& actual, const std::optional<FireGateTiming>& expected) {
    REQUIRE(actual.has_value() == expected.has_value());
    if (!expected) return;
    CHECK(actual->authorityTick == expected->authorityTick);
    CHECK(actual->lastResolvedCommand == expected->lastResolvedCommand);
    CHECK(actual->latestCommand == expected->latestCommand);
    CHECK(actual->secondsSinceStep == expected->secondsSinceStep);
    CHECK(actual->phaseShiftSeconds == expected->phaseShiftSeconds);
    CHECK(actual->phaseDecided == expected->phaseDecided);
}

void SameRun(const Scenario& scenario) {
    const auto expected = Run<V6ApplicationPath>(scenario);
    const auto actual = Run<ClientSimulation>(scenario);
    REQUIRE(actual.size() == expected.size());
    std::size_t published{};
    for (std::size_t n = 0; n < expected.size(); ++n) {
        CAPTURE(expected[n].atMilliseconds);
        SameInput(actual[n].published, expected[n].published);
        SameObservation(actual[n].observation, expected[n].observation);
        SameTiming(actual[n].shotTiming, expected[n].shotTiming);
        if (expected[n].published) ++published;
    }
    // The scenario must exercise the path: commands are published and the
    // predictor tracks the authority after the reset.
    CHECK(published > 100);
    CHECK(expected.back().observation.active);
}
} // namespace

TEST_CASE("PvP ClientSimulation reproduces the v6 application command path bit for bit across rates and stalls") {
    SameRun({MixedRates, 20, 6000});
}

TEST_CASE("PvP ClientSimulation reproduces the v6 application command path at a steady 60 FPS") {
    SameRun({Steady60, 35, 4000});
}

TEST_CASE("PvP ClientSimulation ignores snapshots that are not newer than the last observed tick") {
    const auto arena = SeamArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    ClientSimulation client(arena);
    match.Tick({1, MovementTickSeconds});
    const auto first = match.Snapshot();
    client.Observe(first.players.front(), first.tick, std::nullopt);
    REQUIRE(client.Observation().active);
    const auto seeded = client.Observation().authorityTick;
    // After an arena reselection the gate is kept: the same tick is ignored, so the
    // fresh predictor stays inactive until a newer snapshot arrives.
    client.SelectArena(arena);
    client.Observe(first.players.front(), first.tick, std::nullopt);
    CHECK_FALSE(client.Observation().active);
    match.Tick({2, MovementTickSeconds});
    const auto second = match.Snapshot();
    client.Observe(second.players.front(), second.tick, std::nullopt);
    CHECK(client.Observation().active);
    CHECK(client.Observation().authorityTick > seeded);
    // Reset clears the gate.
    client.Reset();
    client.Observe(second.players.front(), second.tick, std::nullopt);
    CHECK(client.Observation().active);
}

TEST_CASE("PvP ClientSimulation drops a pending jump while controls are unavailable") {
    const auto arena = SeamArena();
    PvpMatch match(arena);
    std::string error;
    REQUIRE(match.Join(1, error));
    ClientSimulation client(arena);
    match.Tick({1, MovementTickSeconds});
    client.Observe(match.Snapshot().players.front(), 1, MovementRules{1.4F, 21.0F});
    const PredictionElapsedTime::Clock::time_point origin{};
    static_cast<void>(client.Frame(origin, {}));
    static_cast<void>(client.Frame(origin + std::chrono::milliseconds(20), {}));
    // A short frame requests a jump but completes no step, so the request is pending.
    static_cast<void>(client.Frame(origin + std::chrono::milliseconds(22), {0, 0, 0, 0, true, true}));
    const auto latest = client.Observation().latestCommand;
    // Controls drop before the next step: the pending request must not reach a command.
    const auto window = client.Frame(origin + std::chrono::milliseconds(60), {0, 0, 0, 0, false, false});
    REQUIRE(window.has_value());
    REQUIRE(client.Observation().latestCommand > latest);
    for (const auto& command : window->commands)
        if (command.sequence > latest) CHECK_FALSE(command.jumpRequested);
    // With controls the same edge does reach the next step.
    static_cast<void>(client.Frame(origin + std::chrono::milliseconds(62), {0, 0, 0, 0, true, true}));
    const auto before = client.Observation().latestCommand;
    const auto jumped = client.Frame(origin + std::chrono::milliseconds(100), {});
    REQUIRE(jumped.has_value());
    CHECK(std::any_of(jumped->commands.begin(), jumped->commands.end(),
        [before](const MovementCommand& command) { return command.sequence > before && command.jumpRequested; }));
}
