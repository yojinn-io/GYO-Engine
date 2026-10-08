#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ClientSimulation.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
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
    // v6 took phase evidence only from the newest snapshot of a frame.
    void ObserveSample(const WorldSnapshot&, PlayerId) noexcept {}
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
    // Snapshots delivered this frame and how many distinct slack samples they carried.
    std::size_t delivered{};
    std::size_t distinctSlack{};
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

// Slightly shorter than a tick on the 0.5 ms event grid: never two snapshots in one frame.
double Steady60(double, unsigned) { return 16.5; }

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
// due snapshot, offers each one as phase evidence, reconciles the newest, then
// samples and advances.
template <class Client>
std::vector<FrameRecord> Run(const Scenario& scenario) {
    const auto arena = SeamArena();
    // The real host: snapshots carry its movement slack samples, measured on this virtual clock.
    PredictionElapsedTime::Clock::time_point clock{};
    MatchRuntimeHost host(arena, [&clock] { return clock; });
    REQUIRE(host.QueueJoin(1, 1));
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
        clock = origin + std::chrono::duration_cast<PredictionElapsedTime::Clock::duration>(
            std::chrono::duration<double, std::milli>(now));
        while (!inputs.empty() && inputs.front().due <= now) {
            static_cast<void>(host.SubmitInput(inputs.front().input));
            inputs.erase(inputs.begin());
        }
        if (now + 1e-9 >= nextTick) {
            nextTick += 1000.0 / 60.0;
            REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
            if (auto snapshot = host.TakeSnapshot(); snapshot && !snapshot->players.empty())
                snapshots.push_back({now + scenario.snapshotDelayMilliseconds, std::move(*snapshot)});
        }
        if (now + 1e-9 >= nextSend) {
            nextSend += 1000.0 / InputSendRate;
            if (!published.commands.empty()) inputs.push_back({now + 12, published});
        }
        if (now + 1e-9 < nextFrame) continue;
        nextFrame = now + scenario.frameInterval(now, frame++);
        // A session reset and an arena reselection happen as the application does them.
        if (!resetDone && now >= 2800) { client.Reset(); resetDone = true; }
        std::vector<WorldSnapshot> drained;
        while (!snapshots.empty() && snapshots.front().due <= now) {
            drained.push_back(std::move(snapshots.front().snapshot));
            snapshots.erase(snapshots.begin());
        }
        FrameRecord record;
        record.atMilliseconds = now;
        record.delivered = drained.size();
        std::vector<std::uint64_t> slack;
        for (const auto& snapshot : drained)
            if (const auto& sequence = snapshot.players.front().movementSlackSequence;
                sequence && std::find(slack.begin(), slack.end(), *sequence) == slack.end())
                slack.push_back(*sequence);
        record.distinctSlack = slack.size();
        if (!drained.empty()) {
            if (!arenaSelected && now >= 4300) { client.SelectArena(arena); arenaSelected = true; }
            for (const auto& snapshot : drained) client.ObserveSample(snapshot, 1);
            client.Observe(drained.back().players.front(), drained.back().tick, rules);
        }
        const auto at = origin + std::chrono::duration_cast<PredictionElapsedTime::Clock::duration>(
            std::chrono::duration<double, std::milli>(now));
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
    CHECK(actual.phaseSamples == expected.phaseSamples);
    CHECK(actual.phaseSampleSequence == expected.phaseSampleSequence);
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

void SameFrame(const FrameRecord& actual, const FrameRecord& expected) {
    SameInput(actual.published, expected.published);
    SameObservation(actual.observation, expected.observation);
    SameTiming(actual.shotTiming, expected.shotTiming);
}

// One snapshot per frame: phase tracking sees exactly what v6 saw.
void SameRun(const Scenario& scenario) {
    const auto expected = Run<V6ApplicationPath>(scenario);
    const auto actual = Run<ClientSimulation>(scenario);
    REQUIRE(actual.size() == expected.size());
    std::size_t published{};
    for (std::size_t n = 0; n < expected.size(); ++n) {
        CAPTURE(expected[n].atMilliseconds);
        REQUIRE(expected[n].delivered <= 1);
        SameFrame(actual[n], expected[n]);
        if (expected[n].published) ++published;
    }
    // The scenario must exercise the path: commands are published, samples are
    // taken and the predictor tracks the authority after the reset.
    CHECK(published > 100);
    CHECK(expected.back().observation.phaseSamples > 0);
    CHECK(std::any_of(expected.begin(), expected.end(),
        [](const FrameRecord& record) { return record.observation.phaseErrorSeconds.has_value(); }));
    CHECK(expected.back().observation.active);
}

double Steady144(double, unsigned) { return 1000.0 / 144.0; }
} // namespace

TEST_CASE("PvP ClientSimulation reproduces the v6 application command path at one snapshot per frame") {
    // Long enough to cover the session reset (2.8 s) and the arena reselection (4.3 s).
    SameRun({Steady60, 35, 5000});
    SameRun({Steady144, 20, 5000});
}

TEST_CASE("PvP ClientSimulation takes the phase sample of every snapshot of a frame in tick order") {
    const auto v6 = Run<V6ApplicationPath>({MixedRates, 20, 6000});
    const auto actual = Run<ClientSimulation>({MixedRates, 20, 6000});
    REQUIRE(actual.size() == v6.size());
    // Identical until the first frame that drains more than one snapshot.
    std::size_t n = 0;
    for (; n < v6.size() && v6[n].delivered <= 1; ++n) {
        CAPTURE(v6[n].atMilliseconds);
        SameFrame(actual[n], v6[n]);
    }
    REQUIRE(n < v6.size());
    std::size_t multiSampleFrames{}, extraSamples{};
    for (std::size_t k = 1; k < actual.size(); ++k) {
        const auto& before = actual[k - 1].observation;
        const auto& after = actual[k].observation;
        if (after.movementEpoch != before.movementEpoch || after.lifeGeneration != before.lifeGeneration ||
            after.phaseSamples < before.phaseSamples) continue;
        CAPTURE(actual[k].atMilliseconds);
        const auto taken = after.phaseSamples - before.phaseSamples;
        // Each distinct sample of the frame at most once, never a repeat.
        CHECK(taken <= actual[k].distinctSlack);
        if (taken > 0) CHECK(after.phaseSampleSequence > before.phaseSampleSequence);
        if (taken > 1) { ++multiSampleFrames; extraSamples += taken - 1; }
    }
    MESSAGE("multi-sample frames ", multiSampleFrames, ", extra samples ", extraSamples);
    // Several snapshots per frame (30 FPS, stalls) now contribute more than one sample.
    CHECK(multiSampleFrames > 20);
    CHECK(extraSamples > 20);
}

namespace {
// A client after a steady one-snapshot-per-frame start against the real host,
// plus three more snapshots that have not been delivered yet.
struct SteadyClient final {
    ClientSimulation client;
    std::vector<WorldSnapshot> history;
};

SteadyClient SteadyStart() {
    const auto arena = SeamArena();
    PredictionElapsedTime::Clock::time_point clock{};
    MatchRuntimeHost host(arena, [&clock] { return clock; });
    REQUIRE(host.QueueJoin(1, 1));
    SteadyClient result{ClientSimulation(arena), {}};
    auto& client = result.client;
    auto& history = result.history;
    std::vector<PlayerInput> sent;
    const auto tickOnce = [&] {
        // Inputs arrive 4 ms after the previous tick, well before their own.
        clock += std::chrono::milliseconds(4);
        for (const auto& input : sent) static_cast<void>(host.SubmitInput(input));
        sent.clear();
        clock += std::chrono::microseconds(16667 - 4000);
        REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
        auto snapshot = host.TakeSnapshot();
        REQUIRE(snapshot);
        REQUIRE_FALSE(snapshot->players.empty());
        history.push_back(std::move(*snapshot));
    };
    for (int tick = 1; tick <= 180; ++tick) {
        tickOnce();
        client.ObserveSample(history.back(), 1);
        client.Observe(history.back().players.front(), history.back().tick, std::nullopt);
        if (auto window = client.Frame(clock, {1, 0, 0, 0})) sent.push_back(*window);
    }
    // Three more ticks whose snapshots arrive together in one later frame; the
    // client keeps generating and sending commands meanwhile.
    for (int extra = 0; extra < 3; ++extra) {
        tickOnce();
        if (auto window = client.Frame(clock, {1, 0, 0, 0})) sent.push_back(*window);
    }
    return result;
}

std::optional<std::uint64_t> SlackOf(const WorldSnapshot& snapshot) {
    return snapshot.players.front().movementSlackSequence;
}
} // namespace

TEST_CASE("PvP ClientSimulation counts each drained sample once and reconciles only the newest") {
    auto steady = SteadyStart();
    const auto& client = steady.client;
    const auto& history = steady.history;
    const auto& first = history[history.size() - 3];
    const auto& second = history[history.size() - 2];
    const auto& newer = history.back();
    const auto slack = SlackOf;
    REQUIRE(slack(first));
    REQUIRE(slack(second));
    REQUIRE(slack(newer));
    REQUIRE(*slack(first) < *slack(second));
    REQUIRE(*slack(second) < *slack(newer));
    auto both = client;
    auto newestOnly = client;
    const auto start = client.Observation().phaseSamples;
    // Offered out of order and with a repeat: tick order and de-duplication are the seam's job.
    both.ObserveSample(second, 1);
    both.ObserveSample(newer, 1);
    both.ObserveSample(first, 1);
    both.ObserveSample(second, 1);
    both.Observe(newer.players.front(), newer.tick, std::nullopt);
    newestOnly.ObserveSample(newer, 1);
    newestOnly.Observe(newer.players.front(), newer.tick, std::nullopt);
    CHECK(both.Observation().phaseSamples == start + 3);
    CHECK(newestOnly.Observation().phaseSamples == start + 1);
    CHECK(both.Observation().phaseSampleSequence == *slack(newer));
    // Reconciliation is the newest snapshot's in both cases.
    CHECK(both.Observation().authorityTick == newer.tick);
    CHECK(both.Observation().lastResolvedCommand == newestOnly.Observation().lastResolvedCommand);
    SameVector(both.Observation().predictedPosition, newestOnly.Observation().predictedPosition);
    const auto& older = second;
    // A sample older than the reconciled tick is no longer evidence.
    auto late = both;
    late.ObserveSample(older, 1);
    late.Observe(newer.players.front(), newer.tick, std::nullopt);
    CHECK(late.Observation().phaseSamples == both.Observation().phaseSamples);
}

TEST_CASE("PvP ClientSimulation never reconciles an older snapshot of a drained batch") {
    auto steady = SteadyStart();
    const auto& history = steady.history;
    const auto& older = history[history.size() - 2];
    const auto& newer = history.back();
    // An older snapshot that would move the display if it were reconciled.
    auto displaced = older;
    displaced.players.front().position.x += 0.3F;
    auto both = steady.client;
    auto newestOnly = steady.client;
    both.ObserveSample(displaced, 1);
    both.Observe(newer.players.front(), newer.tick, std::nullopt);
    newestOnly.Observe(newer.players.front(), newer.tick, std::nullopt);
    SameVector(both.Observation().predictedPosition, newestOnly.Observation().predictedPosition);
    SameVector(both.Observation().renderPosition, newestOnly.Observation().renderPosition);
    SameVector(both.Observation().correctionOffset, newestOnly.Observation().correctionOffset);
    CHECK(both.Observation().previousCommand == newestOnly.Observation().previousCommand);
    CHECK(both.PendingInput().commands == newestOnly.PendingInput().commands);
}

TEST_CASE("PvP two late samples in one drained batch correct at once like two late frames") {
    auto steady = SteadyStart();
    const auto& history = steady.history;
    auto first = history[history.size() - 3];
    auto second = history[history.size() - 2];
    const auto& newer = history.back();
    REQUIRE(SlackOf(first));
    REQUIRE(SlackOf(second));
    REQUIRE(steady.client.Observation().phaseTracking == PhaseTrackingState::Tracking);
    // Both older commands arrived after their tick; the newest one in time.
    first.players.front().movementSlackMicros = -2000;
    second.players.front().movementSlackMicros = -3000;
    const auto start = steady.client.Observation().phaseLateCorrections;
    auto batched = steady.client;
    batched.ObserveSample(first, 1);
    batched.ObserveSample(second, 1);
    batched.Observe(newer.players.front(), newer.tick, std::nullopt);
    CHECK(batched.Observation().phaseLateCorrections == start + 1);
    CHECK(batched.Observation().phaseTracking == PhaseTrackingState::Settling);
    // v6 saw only the newest, punctual sample of such a frame.
    auto newestOnly = steady.client;
    newestOnly.Observe(newer.players.front(), newer.tick, std::nullopt);
    CHECK(newestOnly.Observation().phaseLateCorrections == start);
}

TEST_CASE("PvP prediction phase samples of another epoch or life are not evidence") {
    auto steady = SteadyStart();
    const auto& history = steady.history;
    const auto& older = history[history.size() - 2];
    const auto& newer = history.back();
    REQUIRE(SlackOf(older));
    REQUIRE(SlackOf(newer));
    REQUIRE(*SlackOf(older) != *SlackOf(newer));
    const auto start = steady.client.Observation().phaseSamples;
    auto otherEpoch = older;
    ++otherEpoch.players.front().movementEpoch;
    auto otherLife = older;
    ++otherLife.players.front().lifeGeneration;
    for (const auto& foreign : {otherEpoch, otherLife}) {
        auto client = steady.client;
        client.ObserveSample(foreign, 1);
        client.Observe(newer.players.front(), newer.tick, std::nullopt);
        // Only the reconciled snapshot's own sample counts.
        CHECK(client.Observation().phaseSamples == start + 1);
    }
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
