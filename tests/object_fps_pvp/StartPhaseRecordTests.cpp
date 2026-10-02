#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// The acceptance recorder includes nothing itself; this mirrors its probes.
#include "start_phase_record.hpp"

namespace {
using namespace fps::pvp;
Arena RecorderArena() {
    Arena arena;
    arena.id = "synthetic_phase_record_arena";
    arena.width = arena.depth = 30;
    arena.walls = {{{-1, 0, -1}, {0, 3, 31}}, {{30, 0, -1}, {31, 3, 31}},
                   {{0, 0, -1}, {30, 3, 0}}, {{0, 0, 30}, {30, 3, 31}}};
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}

// Synthetic shapes: a host and client with the phase-tracking fields, and a
// struct without any of them.
struct SyntheticHost {
    std::uint64_t playerId{1}, movementEpoch{1}, lifeGeneration{1};
    std::optional<std::uint64_t> movementSlackSequence;
    std::optional<std::int32_t> movementSlackMicros;
    std::uint32_t connectionQualityFailures{};
};
struct SyntheticClient {
    bool active{true};
    std::uint64_t movementEpoch{1}, lifeGeneration{1};
    PhaseTrackingState phaseTracking{PhaseTrackingState::Acquiring};
    std::optional<double> phaseErrorSeconds, phaseCorrectionSeconds;
    std::uint32_t phaseCorrections{}, phaseLateCorrections{};
};
struct Bare {
    bool active{true};
    std::uint64_t movementEpoch{1};
};

// One probe frame against the real prediction: advance, reconcile an
// authority two commands behind the tip that reports the newest command's
// slack for `error`, then observe.
struct ProbeFrames {
    LocalPlayerPrediction& client;
    StartPhaseRecorder& recorder;
    std::uint64_t tick{1}, frame{};
    std::uint32_t failures{};
    const LocalMovementObservation& Run(std::optional<double> error) {
        static_cast<void>(client.Advance(MovementTickSeconds, 0, 0, 0, 0));
        const auto latest = client.Observation().latestCommand;
        PlayerState authority{1, {2, 0, 2}, 0, 0, latest > InitialCommandLead ? latest - InitialCommandLead : 0};
        authority.connectionQualityFailures = failures;
        if (error && latest > InitialCommandLead) {
            const double age = client.Observation().interpolationAlpha * MovementTickSeconds;
            authority.movementSlackSequence = latest;
            authority.movementSlackMicros = static_cast<std::int32_t>(std::lround(
                (*error + InitialCommandLead * MovementTickSeconds + MovementPhaseTargetSeconds - age) * 1.0e6));
        }
        client.Reconcile(authority, ++tick);
        ++frame;
        recorder.Observe(frame, static_cast<std::int64_t>(frame) * 16'666'667, 1, &authority, client.Observation());
        return client.Observation();
    }
};
void Start(LocalPlayerPrediction& client) {
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    CHECK_FALSE(client.Advance(MovementTickSeconds / 2, 0, 0, 0, 0));
}
const nlohmann::json& OnlyRecord(const nlohmann::json& json) {
    REQUIRE(json["epochs"].size() == 1);
    return json["epochs"][0];
}
std::vector<std::string> States(const nlohmann::json& record) {
    std::vector<std::string> states;
    for (const auto& change : record["state_changes"]) states.push_back(change["state"].get<std::string>());
    return states;
}
} // namespace

TEST_CASE("Phase-tracking record follows the real prediction's decision correction and Host samples") {
    LocalPlayerPrediction client(RecorderArena());
    StartPhaseRecorder recorder;
    Start(client);
    ProbeFrames probe{client, recorder};
    std::optional<std::uint64_t> decisionFrame;
    std::vector<std::uint64_t> sampled;
    for (int index = 0; index < 20; ++index) {
        if (index == 12) probe.failures = 1;
        const auto& observation = probe.Run(0.010);
        if (observation.phaseErrorSeconds && !decisionFrame) decisionFrame = probe.frame;
        // A slewing frame may generate no command: its sample repeats and counts once.
        if (std::find(sampled.begin(), sampled.end(), observation.latestCommand) == sampled.end())
            sampled.push_back(observation.latestCommand);
    }
    REQUIRE(decisionFrame);
    const auto json = recorder.Json();
    CHECK(json["supported"] == true);
    const auto& record = OnlyRecord(json);
    CHECK(record["status"] == "tracking");
    CHECK(record["player_id"] == 1);
    CHECK(record["movement_epoch"] == 1);
    CHECK(record["first_decision_frame"] == *decisionFrame);
    CHECK(record["first_decision_steady_ns"] == static_cast<std::int64_t>(*decisionFrame) * 16'666'667);
    CHECK(record["first_error_seconds"].get<double>() == doctest::Approx(0.010).epsilon(0.001));
    CHECK(record["corrections"] == 1);
    CHECK(record["late_corrections"] == 0);
    CHECK(record["last_correction_seconds"].get<double>() == doctest::Approx(0.010).epsilon(0.001));
    CHECK(record["host_samples"] == sampled.size());
    CHECK(record["host_late_samples"] == 0);
    CHECK(record["host_slack_min_micros"].get<std::int32_t>() <= record["host_slack_max_micros"].get<std::int32_t>());
    CHECK(record["connection_quality_failures_max"] == 1);
    CHECK(States(record) == std::vector<std::string>{"acquiring", "settling", "tracking"});
    CHECK(record["last_state"] == "tracking");
    CHECK(record["last_frame"] == 20);
    CHECK(record["state_changes_dropped"] == 0);
}

TEST_CASE("Phase-tracking record returns to acquiring after a stall reseed inside the same record") {
    LocalPlayerPrediction client(RecorderArena());
    StartPhaseRecorder recorder;
    Start(client);
    ProbeFrames probe{client, recorder};
    for (int index = 0; index < 14; ++index) static_cast<void>(probe.Run(0.010));
    REQUIRE(client.Observation().phaseTracking == PhaseTrackingState::Tracking);
    static_cast<void>(client.Advance(MovementTickSeconds, 0, 0, 0, 0));
    const PlayerState passed{1, {2, 0, 2}, 0, 0, client.Observation().latestCommand + 3};
    client.Reconcile(passed, ++probe.tick);
    recorder.Observe(++probe.frame, static_cast<std::int64_t>(probe.frame) * 16'666'667, 1, &passed, client.Observation());
    REQUIRE(client.Observation().phaseTracking == PhaseTrackingState::Acquiring);
    const auto json = recorder.Json();
    const auto& record = OnlyRecord(json);
    CHECK(record["status"] == "tracking"); // decided earlier in this epoch and life
    CHECK(States(record) == std::vector<std::string>{"acquiring", "settling", "tracking", "acquiring"});
    CHECK(record["last_state"] == "acquiring");
}

TEST_CASE("Phase-tracking record separates absent Host samples from an undecided Client") {
    StartPhaseRecorder recorder;
    SyntheticHost host;
    SyntheticClient client;
    recorder.Observe(1, 10, 1, &host, client);
    auto record = OnlyRecord(recorder.Json());
    CHECK(record["status"] == "host_samples_absent");
    CHECK(record["host_samples"] == 0);
    CHECK(record["host_slack_min_micros"].is_null());
    CHECK(record["host_slack_max_micros"].is_null());
    CHECK(record["first_decision_frame"].is_null());
    CHECK(record["first_error_seconds"].is_null());
    CHECK(record["last_error_seconds"].is_null());
    CHECK(record["last_correction_seconds"].is_null());
    host.movementSlackSequence = 4;
    host.movementSlackMicros = -2500;
    recorder.Observe(2, 20, 1, &host, client);
    recorder.Observe(3, 30, 1, &host, client); // the same sample again counts once
    host.movementSlackSequence = 5;
    host.movementSlackMicros = 31000;
    recorder.Observe(4, 40, 1, &host, client);
    record = OnlyRecord(recorder.Json());
    CHECK(record["status"] == "acquiring");
    CHECK(record["host_samples"] == 2);
    CHECK(record["host_late_samples"] == 1);
    CHECK(record["host_slack_min_micros"] == -2500);
    CHECK(record["host_slack_max_micros"] == 31000);
    client.phaseTracking = PhaseTrackingState::Tracking;
    client.phaseErrorSeconds = 0.0003;
    recorder.Observe(5, 50, 1, &host, client);
    record = OnlyRecord(recorder.Json());
    CHECK(record["status"] == "tracking");
    CHECK(record["first_decision_frame"] == 5);
    CHECK(record["first_decision_steady_ns"] == 50);
    CHECK(record["corrections"] == 0);
}

TEST_CASE("Phase-tracking record opens a new record per player epoch and life and never attributes player 0") {
    StartPhaseRecorder recorder;
    SyntheticHost host;
    SyntheticClient client;
    recorder.Observe(1, 10, 1, &host, client);
    client.movementEpoch = host.movementEpoch = 2;
    recorder.Observe(2, 20, 1, &host, client);
    client.lifeGeneration = host.lifeGeneration = 2;
    client.movementEpoch = host.movementEpoch = 3;
    recorder.Observe(3, 30, 1, &host, client);
    // Before the probe knows its player id the Client cannot be attributed.
    recorder.Observe(4, 40, 0, static_cast<const SyntheticHost*>(nullptr), client);
    const auto json = recorder.Json();
    REQUIRE(json["epochs"].size() == 3);
    CHECK(json["epochs"][1]["movement_epoch"] == 2);
    CHECK(json["epochs"][2]["life_generation"] == 2);
    CHECK(json["unattributed_client_frames"] == 1);
    CHECK(json["record_key"] == "player_id, movement_epoch, life_generation");
}

TEST_CASE("Phase-tracking record is bounded and counts what it could not store") {
    StartPhaseRecorder recorder;
    SyntheticHost host;
    SyntheticClient client;
    for (std::uint64_t epoch = 1; epoch <= StartPhaseRecorder::Capacity + 3; ++epoch) {
        client.movementEpoch = host.movementEpoch = epoch;
        recorder.Observe(epoch, static_cast<std::int64_t>(epoch), 1, &host, client);
    }
    auto json = recorder.Json();
    CHECK(json["epochs"].size() == StartPhaseRecorder::Capacity);
    CHECK(json["dropped_observations"] == 6); // host and client lookup per overflowing frame
    // State history: later changes are counted, not stored.
    StartPhaseRecorder history;
    const SyntheticHost steady;
    SyntheticClient toggling;
    for (std::size_t index = 0; index < StartPhaseRecorder::StateChangeCapacity + 10; ++index) {
        toggling.phaseTracking = index % 2 ? PhaseTrackingState::Settling : PhaseTrackingState::Tracking;
        history.Observe(index, static_cast<std::int64_t>(index), 1, &steady, toggling);
    }
    json = history.Json();
    const auto& record = OnlyRecord(json);
    CHECK(record["state_changes"].size() == StartPhaseRecorder::StateChangeCapacity);
    CHECK(record["state_changes_dropped"] == 10);
}

TEST_CASE("Phase-tracking record reports an unsupported product instead of guessing") {
    StartPhaseRecorder recorder;
    const Bare bare;
    recorder.Observe(1, 10, 1, &bare, bare);
    const auto json = recorder.Json();
    CHECK(json["supported"] == false);
    CHECK(json["epochs"].empty());
}
