#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"

#include <algorithm>
#include <array>
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
    arena.id = "synthetic_start_phase_record_arena";
    arena.width = arena.depth = 30;
    arena.walls = {{{-1, 0, -1}, {0, 3, 31}}, {{30, 0, -1}, {31, 3, 31}},
                   {{0, 0, -1}, {30, 3, 0}}, {{0, 0, 30}, {30, 3, 31}}};
    arena.spawns = {{{2, 0, 2}, 0}, {{20, 0, 2}, 0}};
    return arena;
}
// The prediction decides a representative wait only once eight frame intervals
// describe the frame rate. Until then it stays pending (a late Host's at once).
constexpr int FrameWindow = 8;
// Back at 60 FPS a guard may need several frame windows before it applies a
// shift again; a recovery runs until it does, for at most this many frames.
constexpr int RecoveryFrameLimit = 256;
// Epoch start: seed, then the first window (sequences 1-3) is published and
// `frames` frames in all have run.
void Start(LocalPlayerPrediction& client, double frameSeconds = MovementTickSeconds, int frames = 1) {
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    REQUIRE(client.Advance(frameSeconds, 1, 0, 0, 0));
    for (int frame = 1; frame < frames; ++frame) static_cast<void>(client.Advance(frameSeconds, 1, 0, 0, 0));
}
template<class Observation>
constexpr bool HasSkipReason = requires(const Observation& observation) { observation.startPhaseSkip; };
constexpr bool ProductSkipReason = HasSkipReason<LocalMovementObservation>;
template<class Prediction>
constexpr bool ReportsSkipReason = requires(const Prediction& prediction) { *prediction.Observation().startPhaseSkip; };
// Whether the skip reason can name a stall reseed's cancellation; resolved
// through the observation so it compiles with and without the enumerator.
template<class Observation>
constexpr bool CancelReasonOf() {
    if constexpr (HasSkipReason<Observation>) {
        using Reason = std::remove_cvref_t<decltype(*std::declval<const Observation&>().startPhaseSkip)>;
        return requires { Reason::CancelledByReseed; };
    } else return false;
}
constexpr bool ProductCancelReason = CancelReasonOf<LocalMovementObservation>();
template<class Observation>
bool ReportsCancellation(const Observation& observation) {
    if constexpr (CancelReasonOf<Observation>()) {
        using Reason = std::remove_cvref_t<decltype(*observation.startPhaseSkip)>;
        return observation.startPhaseSkip && *observation.startPhaseSkip == Reason::CancelledByReseed;
    } else return false;
}

// Synthetic shapes: an older product without the skip reason, a product with
// it, one whose reason can also name a reseed cancellation, and a struct
// without any start-phase field.
struct SyntheticHost {
    std::uint64_t playerId{1}, movementEpoch{1}, lifeGeneration{1};
    std::optional<std::uint32_t> epochStartWaitMicros;
};
struct OlderClient {
    bool active{true};
    std::uint64_t movementEpoch{1}, lifeGeneration{1};
    std::optional<double> epochStartWaitSeconds, startPhaseShiftSeconds;
};
enum class SyntheticSkip { HostLate, FrameRateBelowTick, Future };
struct ReasonedClient : OlderClient {
    std::optional<SyntheticSkip> startPhaseSkip;
};
enum class SyntheticCancelSkip { HostLate, FrameRateBelowTick, CancelledByReseed };
struct CancellingClient : OlderClient {
    std::optional<SyntheticCancelSkip> startPhaseSkip;
};
struct Bare {
    bool active{true};
    std::uint64_t movementEpoch{1};
};

// One probe frame against the real prediction: advance, reconcile an authority
// that has resolved all but the sequence lead (so the window stays short and
// every snapshot of the epoch repeats `wait`), then observe.
template<class Prediction>
struct ProbeFrames {
    Prediction& client;
    StartPhaseRecorder& recorder;
    std::optional<std::uint32_t> wait{};
    std::uint64_t tick{1}, frame{};
    const LocalMovementObservation& Run(double seconds) {
        static_cast<void>(client.Advance(seconds, 0, 0, 0, 0));
        const auto latest = client.Observation().latestCommand;
        PlayerState authority{1, {2, 0, 2}, 0, 0, latest > InitialCommandLead ? latest - InitialCommandLead : 0};
        authority.epochStartWaitMicros = wait;
        client.Reconcile(authority, ++tick);
        recorder.Observe(frame, static_cast<std::int64_t>(frame) * 1'000'000, 1, &authority, client.Observation());
        ++frame;
        return client.Observation();
    }
};

// The record's bookkeeping, recomputed from the observations the test saw. It
// encodes the recorder's definitions only, never the guard's policy.
struct ExpectedRecord {
    std::vector<std::pair<std::string, std::uint64_t>> changes;
    std::uint64_t withdrawnFrames{}, withdrawals{}, restorations{};
    std::optional<std::uint64_t> firstWithdrawn, firstArmed;
    void See(bool decided, bool shifted, std::uint64_t frame) {
        const std::string previous = changes.empty() ? std::string() : changes.back().first;
        const std::string state = !decided ? "undecided" : shifted ? "shift_armed" :
            firstArmed ? "withdrawn_below_cut" : "skipped_below_cut";
        if (state == "shift_armed" && !firstArmed) firstArmed = frame;
        if (state == "withdrawn_below_cut" && withdrawnFrames++ == 0) firstWithdrawn = frame;
        if (state == previous) return;
        if (previous == "shift_armed" && state == "withdrawn_below_cut") ++withdrawals;
        if ((previous == "withdrawn_below_cut" || previous == "skipped_below_cut") && state == "shift_armed") ++restorations;
        changes.emplace_back(state, frame);
    }
    void Check(const nlohmann::json& epoch) const {
        const auto optional = [](std::optional<std::uint64_t> value) { return value ? nlohmann::json(*value) : nlohmann::json(nullptr); };
        const auto& logged = epoch["client_state_changes"];
        REQUIRE(logged.size() == changes.size());
        for (std::size_t index = 0; index < changes.size(); ++index) {
            CHECK(logged[index]["state"] == changes[index].first);
            CHECK(logged[index]["frame"] == changes[index].second);
            CHECK(logged[index]["steady_ns"] == changes[index].second * 1'000'000);
        }
        CHECK(epoch["client_state_changes_dropped"] == 0);
        CHECK(epoch["withdrawn_frames"] == withdrawnFrames);
        CHECK(epoch["withdrawals"] == withdrawals);
        CHECK(epoch["restorations"] == restorations);
        CHECK(epoch["withdrawn_first_frame"] == optional(firstWithdrawn));
        CHECK(epoch["client_first_armed_frame"] == optional(firstArmed));
        CHECK(epoch["last_client_state"] == changes.back().first);
    }
};

// Compiled only once LocalMovementObservation carries the skip reason; the
// real prediction then has to report it for both skip causes.
template<class Prediction>
void CheckProductSkipReasons(const Arena& arena) {
    if constexpr (ReportsSkipReason<Prediction>) {
        using Reason = std::remove_cvref_t<decltype(*std::declval<const Prediction&>().Observation().startPhaseSkip)>;
        Prediction late(arena);
        Start(late);
        PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
        authority.epochStartWaitMicros = 30000;
        late.Reconcile(authority, 2);
        REQUIRE(late.Observation().startPhaseSkip);
        CHECK(*late.Observation().startPhaseSkip == Reason::HostLate);
        StartPhaseRecorder lateRecord;
        lateRecord.Observe(0, 0, 1, &authority, late.Observation());
        CHECK(lateRecord.Json()["epochs"].at(0)["status"] == "skipped_host_late");

        // Below 60 FPS every frame is longer than one authority tick. The wait
        // itself is representative, so only the frame period can skip it.
        Prediction slow(arena);
        Start(slow, 1.0 / 30, FrameWindow);
        authority.epochStartWaitMicros = 9000;
        slow.Reconcile(authority, 2);
        REQUIRE(slow.Observation().epochStartWaitSeconds);
        CHECK_FALSE(slow.Observation().startPhaseShiftSeconds);
        REQUIRE(slow.Observation().startPhaseSkip);
        CHECK(*slow.Observation().startPhaseSkip == Reason::FrameRateBelowTick);
        StartPhaseRecorder slowRecord;
        slowRecord.Observe(0, 0, 1, &authority, slow.Observation());
        const auto epoch = slowRecord.Json()["epochs"].at(0);
        CHECK(epoch["status"] == "skipped_frame_rate_below_tick");
        CHECK(epoch["client_skip_reason"] == "frame_rate_below_tick");
        CHECK(epoch["client_wait_seconds"].get<double>() == doctest::Approx(0.009));
        CHECK(epoch["client_shift_seconds"].is_null());
        CHECK(epoch["last_client_state"] == "skipped_below_cut");
    } else {
        MESSAGE("LocalMovementObservation has no startPhaseSkip; product skip-reason assertions are inactive");
    }
}

// Every kept-phase frame of a representative wait must name the frame rate.
// The observation type is a template parameter so that the guarded branch
// stays dependent and is never checked against a product without the reason.
template<class Observation>
void SeeFrame(ExpectedRecord& expected, const Observation& observation, std::uint64_t frame) {
    if constexpr (HasSkipReason<Observation>) {
        using Reason = std::remove_cvref_t<decltype(*observation.startPhaseSkip)>;
        if (observation.epochStartWaitSeconds && !observation.startPhaseShiftSeconds) {
            REQUIRE(observation.startPhaseSkip);
            REQUIRE(*observation.startPhaseSkip == Reason::FrameRateBelowTick);
        }
    }
    expected.See(observation.epochStartWaitSeconds.has_value(), observation.startPhaseShiftSeconds.has_value(), frame);
}

// Arm at 60 FPS, run below the cut, recover: the record follows the
// withdrawal and the return instead of keeping its first observation.
template<class Prediction>
void CheckWithdrawnAndRestored(const Arena& arena) {
    if constexpr (ReportsSkipReason<Prediction>) {
        Prediction client(arena);
        StartPhaseRecorder recorder;
        ProbeFrames<Prediction> frames{client, recorder};
        ExpectedRecord expected;
        const auto run = [&](double seconds, int count) {
            for (int index = 0; index < count; ++index) {
                const auto& observation = frames.Run(seconds); // Advances frames.frame.
                SeeFrame(expected, observation, frames.frame - 1);
            }
        };
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
        run(MovementTickSeconds, FrameWindow);
        frames.wait = 9000; // A nonzero shift: wait + first-step age - 4 ms.
        run(MovementTickSeconds, 2);
        REQUIRE(client.Observation().startPhaseShiftSeconds);
        const double armed = *client.Observation().startPhaseShiftSeconds;
        run(1.0 / 30, 16);
        for (int index = 0; index < RecoveryFrameLimit && !client.Observation().startPhaseShiftSeconds; ++index)
            run(MovementTickSeconds, 1);
        run(MovementTickSeconds, 16);
        // The scenario must exercise both directions for the record to be tested.
        REQUIRE(expected.withdrawals >= 1);
        REQUIRE(expected.restorations >= 1);
        REQUIRE(client.Observation().startPhaseShiftSeconds);

        const auto epoch = recorder.Json()["epochs"].at(0);
        CHECK(epoch["status"] == "shift_withdrawn_below_cut");
        expected.Check(epoch);
        CHECK(epoch["client_conflicting_frames"] == 0);
        CHECK(epoch["client_shift_seconds"].get<double>() == doctest::Approx(armed));
        CHECK(epoch["client_armed_shift_seconds"].get<double>() == doctest::Approx(armed));
        CHECK(epoch["client_skip_reason"].is_null());
        CHECK(epoch["withdrawn_first_steady_ns"] == *expected.firstWithdrawn * 1'000'000);
        CHECK(epoch["last_client_shift_seconds"].get<double>() == doctest::Approx(armed));
        CHECK(epoch["last_client_skip_reason"].is_null());
        CHECK(epoch["last_client_frame"] == frames.frame - 1);
        CHECK(epoch["host_wait_undecided_frames"] == 0);
    } else {
        MESSAGE("LocalMovementObservation has no startPhaseSkip; withdrawal bookkeeping is inactive");
    }
}

// A decision taken below the cut: a latching guard keeps the epoch unaligned,
// a restorable one arms it once frames recover. Only the record's bookkeeping
// of whichever happened is asserted.
template<class Prediction>
void CheckBelowCutDecision(const Arena& arena) {
    if constexpr (ReportsSkipReason<Prediction>) {
        Prediction client(arena);
        StartPhaseRecorder recorder;
        ProbeFrames<Prediction> frames{client, recorder};
        ExpectedRecord expected;
        const auto run = [&](double seconds, int count) {
            for (int index = 0; index < count; ++index) {
                const auto& observation = frames.Run(seconds); // Advances frames.frame.
                SeeFrame(expected, observation, frames.frame - 1);
            }
        };
        client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
        run(1.0 / 30, FrameWindow);
        frames.wait = 9000; // A nonzero shift: wait + first-step age - 4 ms.
        run(1.0 / 30, 2);
        REQUIRE(client.Observation().epochStartWaitSeconds);
        REQUIRE_FALSE(client.Observation().startPhaseShiftSeconds);
        for (int index = 0; index < RecoveryFrameLimit && !client.Observation().startPhaseShiftSeconds; ++index)
            run(MovementTickSeconds, 1);
        run(MovementTickSeconds, 16);

        const auto epoch = recorder.Json()["epochs"].at(0);
        expected.Check(epoch);
        CHECK(epoch["status"] == (expected.firstArmed ? "shift_armed_after_below_cut_decision" : "skipped_frame_rate_below_tick"));
        CHECK(epoch["client_skip_reason"] == "frame_rate_below_tick");
        CHECK(epoch["client_shift_seconds"].is_null());
        CHECK(epoch["withdrawn_frames"] == 0);
        CHECK(epoch["withdrawals"] == 0);
        CHECK(epoch["client_conflicting_frames"] == 0);
        if (expected.firstArmed) {
            CHECK(epoch["restorations"] == 1);
            CHECK(epoch["client_armed_shift_seconds"].is_number());
            CHECK(epoch["last_client_state"] == "shift_armed");
        } else {
            CHECK(epoch["client_armed_shift_seconds"].is_null());
            CHECK(epoch["last_client_state"] == "skipped_below_cut");
        }
    } else {
        MESSAGE("LocalMovementObservation has no startPhaseSkip; below-cut decision bookkeeping is inactive");
    }
}
} // namespace

TEST_CASE("Start-phase record arms a shift once per epoch with its first frame and time") {
    const auto arena = RecorderArena();
    LocalPlayerPrediction client(arena);
    Start(client, MovementTickSeconds, FrameWindow);
    StartPhaseRecorder recorder;
    PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
    recorder.Observe(10, 1000, 1, &authority, client.Observation());
    authority.epochStartWaitMicros = 14781;
    client.Reconcile(authority, 2);
    recorder.Observe(11, 2000, 1, &authority, client.Observation());
    static_cast<void>(client.Advance(MovementTickSeconds, 1, 0, 0, 0));
    recorder.Observe(12, 3000, 1, &authority, client.Observation()); // Same values: no conflict.

    const auto json = recorder.Json();
    CHECK(json["supported"] == true);
    CHECK(json["skip_reason_supported"] == ProductSkipReason);
    CHECK(json["cancel_reason_supported"] == ProductCancelReason);
    CHECK(json["dropped_observations"] == 0);
    CHECK(json["unattributed_client_frames"] == 0);
    CHECK(json["record_capacity"] == StartPhaseRecorder::Capacity);
    CHECK(json["state_change_capacity"] == StartPhaseRecorder::StateChangeCapacity);
    CHECK(json["pending_frame_allowance"] == StartPhaseRecorder::PendingFrameAllowance);
    CHECK(json["status_scope"].get<std::string>().find("not observable") != std::string::npos);
    REQUIRE(json["epochs"].size() == 1);
    const auto& epoch = json["epochs"][0];
    CHECK(epoch["player_id"] == 1);
    CHECK(epoch["movement_epoch"] == 1);
    CHECK(epoch["life_generation"] == 1);
    CHECK(epoch["status"] == "shift_armed");
    CHECK(epoch["first_observed_frame"] == 10);
    CHECK(epoch["first_observed_steady_ns"] == 1000);
    CHECK(epoch["host_wait_micros"] == 14781);
    CHECK(epoch["host_wait_first_frame"] == 11);
    CHECK(epoch["host_wait_first_steady_ns"] == 2000);
    CHECK(epoch["client_wait_seconds"].get<double>() == doctest::Approx(0.014781));
    REQUIRE(client.Observation().startPhaseShiftSeconds);
    CHECK(epoch["client_shift_seconds"].get<double>() == doctest::Approx(*client.Observation().startPhaseShiftSeconds));
    CHECK(epoch["client_first_frame"] == 11);
    CHECK(epoch["client_skip_reason"].is_null());
    CHECK(epoch["host_wait_conflicting_frames"] == 0);
    CHECK(epoch["client_conflicting_frames"] == 0);
    // The armed state since frame 11, still current at the last frame.
    CHECK(epoch["client_armed_shift_seconds"] == epoch["client_shift_seconds"]);
    CHECK(epoch["client_first_armed_frame"] == 11);
    CHECK(epoch["client_first_armed_steady_ns"] == 2000);
    CHECK(epoch["last_client_state"] == "shift_armed");
    CHECK(epoch["last_client_frame"] == 12);
    CHECK(epoch["last_client_steady_ns"] == 3000);
    CHECK(epoch["last_client_shift_seconds"] == epoch["client_shift_seconds"]);
    CHECK(epoch["withdrawn_frames"] == 0);
    CHECK(epoch["withdrawn_first_frame"].is_null());
    CHECK(epoch["withdrawals"] == 0);
    CHECK(epoch["restorations"] == 0);
    REQUIRE(epoch["client_state_changes"].size() == 2);
    CHECK(epoch["client_state_changes"][0] == nlohmann::json{{"state", "undecided"}, {"frame", 10}, {"steady_ns", 1000}});
    CHECK(epoch["client_state_changes"][1] == nlohmann::json{{"state", "shift_armed"}, {"frame", 11}, {"steady_ns", 2000}});
    CHECK(epoch["client_state_changes_dropped"] == 0);
}

TEST_CASE("Start-phase record keeps the phase for a late Host and names the reason only when reported") {
    const auto arena = RecorderArena();
    LocalPlayerPrediction late(arena);
    Start(late);
    PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
    authority.epochStartWaitMicros = 30000;
    late.Reconcile(authority, 2);
    StartPhaseRecorder recorder;
    recorder.Observe(0, 0, 1, &authority, late.Observation());
    const auto epoch = recorder.Json()["epochs"].at(0);
    CHECK(epoch["host_wait_micros"] == 30000);
    CHECK(epoch["client_wait_seconds"].get<double>() == doctest::Approx(0.03));
    CHECK(epoch["client_shift_seconds"].is_null());
    if constexpr (ProductSkipReason) {
        CHECK(epoch["status"] == "skipped_host_late");
        CHECK(epoch["client_skip_reason"] == "host_late");
    } else {
        // An older product reports no reason; the record never guesses one.
        CHECK(epoch["status"] == "rejected_or_skipped_unknown");
        CHECK(epoch["client_skip_reason"].is_null());
    }
    CheckProductSkipReasons<LocalPlayerPrediction>(arena);
}

TEST_CASE("Start-phase record separates a pending Client from a Host wait it never armed") {
    const auto arena = RecorderArena();
    // The Host wait arrives while the Client still collects frame intervals.
    LocalPlayerPrediction early(arena);
    StartPhaseRecorder pending;
    ProbeFrames<LocalPlayerPrediction> frames{early, pending, 9000};
    early.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    frames.Run(MovementTickSeconds);
    REQUIRE_FALSE(early.Observation().epochStartWaitSeconds);
    auto epoch = pending.Json()["epochs"].at(0);
    CHECK(epoch["status"] == "pending_frame_window");
    CHECK(epoch["host_wait_undecided_frames"] == 1);
    CHECK(epoch["client_active_frames_at_last_undecided"] == 1);
    CHECK(epoch["last_client_state"] == "undecided");
    // Once eight intervals describe 60 FPS, the repeated wait is taken.
    while (!early.Observation().epochStartWaitSeconds && frames.frame <= StartPhaseRecorder::PendingFrameAllowance)
        frames.Run(MovementTickSeconds);
    REQUIRE(early.Observation().startPhaseShiftSeconds);
    epoch = pending.Json()["epochs"].at(0);
    CHECK(epoch["status"] == "shift_armed");
    CHECK(epoch["host_wait_undecided_frames"] == frames.frame - 1);
    CHECK(epoch["client_active_frames_at_last_undecided"].get<std::uint64_t>() <= StartPhaseRecorder::PendingFrameAllowance);

    // A catch-up reseed rebases the timed phase: observed for longer than the
    // pending allowance, the Client never takes the wait. A product that
    // reports the cancellation is named as such; otherwise it stays undecided.
    LocalPlayerPrediction reseeded(arena);
    Start(reseeded);
    StartPhaseRecorder recorder;
    PlayerState later{1, {2, 0, 2}, 0, 0, 5};
    later.epochStartWaitMicros = 9000;
    reseeded.Reconcile(later, 6);
    const bool reported = ReportsCancellation(reseeded.Observation());
    for (std::uint64_t frame = 0; frame <= StartPhaseRecorder::PendingFrameAllowance; ++frame) {
        recorder.Observe(frame, 0, 1, &later, reseeded.Observation());
        static_cast<void>(reseeded.Advance(MovementTickSeconds, 1, 0, 0, 0));
    }
    later.lastResolvedCommand = 6;
    reseeded.Reconcile(later, 7);
    recorder.Observe(StartPhaseRecorder::PendingFrameAllowance + 1, 0, 1, &later, reseeded.Observation());
    REQUIRE_FALSE(reseeded.Observation().startPhaseShiftSeconds);
    epoch = recorder.Json()["epochs"].at(0);
    CHECK(epoch["host_wait_micros"] == 9000);
    CHECK(epoch["client_shift_seconds"].is_null());
    CHECK(epoch["client_conflicting_frames"] == 0);
    if (reported) {
        CHECK(epoch["status"] == "cancelled_by_reseed");
        CHECK(epoch["client_cancelled_first_frame"] == 0);
        CHECK(epoch["last_client_state"] == "cancelled_by_reseed");
    } else {
        REQUIRE_FALSE(reseeded.Observation().epochStartWaitSeconds);
        CHECK(epoch["status"] == "not_armed_or_invalidated");
        CHECK(epoch["client_wait_seconds"].is_null());
        CHECK(epoch["client_cancelled_frames"] == 0);
        CHECK(epoch["client_active_frames_at_last_undecided"] == StartPhaseRecorder::PendingFrameAllowance + 2);
    }

    StartPhaseRecorder absent;
    LocalPlayerPrediction waiting(arena);
    Start(waiting);
    const PlayerState quiet{1, {2, 0, 2}, 0, 0, 1};
    absent.Observe(0, 0, 1, &quiet, waiting.Observation());
    CHECK(absent.Json()["epochs"].at(0)["status"] == "host_wait_absent");
}

TEST_CASE("Start-phase record follows an armed shift withdrawn below the cut and restored") {
    CheckWithdrawnAndRestored<LocalPlayerPrediction>(RecorderArena());
}

TEST_CASE("Start-phase record follows a decision taken below the cut") {
    CheckBelowCutDecision<LocalPlayerPrediction>(RecorderArena());
}

TEST_CASE("Start-phase record opens a new record per player epoch and life") {
    const auto arena = RecorderArena();
    LocalPlayerPrediction client(arena);
    Start(client, MovementTickSeconds, FrameWindow);
    StartPhaseRecorder recorder;
    PlayerState authority{1, {2, 0, 2}, 0, 0, 1};
    authority.epochStartWaitMicros = 2000;
    client.Reconcile(authority, 2);
    recorder.Observe(0, 0, 1, &authority, client.Observation());

    // A respawn opens a new life and epoch; the prediction clears its record.
    PlayerState respawned{1, {2, 0, 2}, 0, 0, 0};
    respawned.lifeGeneration = respawned.movementEpoch = 2;
    client.Reconcile(respawned, 3);
    CHECK_FALSE(client.Observation().epochStartWaitSeconds);
    recorder.Observe(1, 0, 1, &respawned, client.Observation());

    PlayerState nextEpoch = respawned;
    nextEpoch.movementEpoch = 3;
    client.Reconcile(nextEpoch, 4);
    recorder.Observe(2, 0, 1, &nextEpoch, client.Observation());

    // Life alone, and the same epoch and life of another player, are distinct.
    const OlderClient inactive{false};
    const SyntheticHost otherLife{1, 1, 2, std::nullopt}, otherPlayer{2, 3, 2, 4000};
    recorder.Observe(3, 0, 1, &otherLife, inactive);
    recorder.Observe(4, 0, 2, &otherPlayer, inactive);

    const auto epochs = recorder.Json()["epochs"];
    const std::vector<std::array<int, 3>> keys{{1, 1, 1}, {1, 2, 2}, {1, 3, 2}, {1, 1, 2}, {2, 3, 2}};
    REQUIRE(epochs.size() == keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
        CHECK(epochs[index]["player_id"] == keys[index][0]);
        CHECK(epochs[index]["movement_epoch"] == keys[index][1]);
        CHECK(epochs[index]["life_generation"] == keys[index][2]);
    }
    CHECK(epochs[0]["status"] == "shift_armed");
    CHECK(epochs[1]["status"] == "host_wait_absent");
    // Never seen by an active Client: no pending claim either.
    CHECK(epochs[4]["status"] == "not_armed_or_invalidated");
    CHECK(epochs[4]["host_wait_micros"] == 4000);
    CHECK(epochs[4]["last_client_state"].is_null());
    CHECK(epochs[4]["client_state_changes"].empty());
}

TEST_CASE("Start-phase record maps reported skip reasons and never guesses a missing one") {
    StartPhaseRecorder recorder;
    const auto observe = [&](std::uint64_t epoch, const auto& client) {
        const SyntheticHost host{1, epoch, 1, 30000};
        recorder.Observe(epoch, 0, 1, &host, client);
    };
    ReasonedClient client;
    client.epochStartWaitSeconds = .03;
    const std::array<std::pair<std::optional<SyntheticSkip>, const char*>, 4> cases{{
        {SyntheticSkip::HostLate, "skipped_host_late"}, {SyntheticSkip::FrameRateBelowTick, "skipped_frame_rate_below_tick"},
        {SyntheticSkip::Future, "rejected_or_skipped_unknown"}, {std::nullopt, "rejected_or_skipped_unknown"}}};
    for (std::uint64_t index = 0; index < cases.size(); ++index) {
        client.movementEpoch = index + 1;
        client.startPhaseSkip = cases[index].first;
        observe(index + 1, client);
    }
    client.movementEpoch = 1;
    client.startPhaseSkip = SyntheticSkip::FrameRateBelowTick; // A changed reason is a conflict, not a rewrite.
    observe(1, client);
    auto json = recorder.Json();
    CHECK(json["skip_reason_supported"] == true);
    REQUIRE(json["epochs"].size() == cases.size());
    for (std::size_t index = 0; index < cases.size(); ++index)
        CHECK(json["epochs"][index]["status"] == cases[index].second);
    CHECK(json["epochs"][0]["client_skip_reason"] == "host_late");
    CHECK(json["epochs"][0]["client_conflicting_frames"] == 1);
    CHECK(json["epochs"][0]["last_client_skip_reason"] == "frame_rate_below_tick");
    CHECK(json["epochs"][0]["withdrawn_frames"] == 0);
    CHECK(json["epochs"][2]["client_skip_reason"] == "unrecognised");
    CHECK(json["epochs"][2]["last_client_state"] == "skipped_unknown");
    CHECK(json["epochs"][3]["client_skip_reason"].is_null());

    StartPhaseRecorder older;
    OlderClient legacy;
    legacy.epochStartWaitSeconds = .03;
    const SyntheticHost host{1, 1, 1, 30000};
    older.Observe(0, 0, 1, &host, legacy);
    json = older.Json();
    CHECK(json["supported"] == true);
    CHECK(json["skip_reason_supported"] == false);
    CHECK(json["epochs"][0]["status"] == "rejected_or_skipped_unknown");
}

TEST_CASE("Start-phase record bounds its state history and keeps exact transition counts") {
    constexpr auto capacity = StartPhaseRecorder::StateChangeCapacity;
    StartPhaseRecorder recorder;
    ReasonedClient client;
    const SyntheticHost host{1, 1, 1, 4000};
    std::uint64_t frame{};
    const auto observe = [&] {
        recorder.Observe(frame, static_cast<std::int64_t>(frame) * 10, 1, &host, client);
        ++frame;
    };
    const auto armed = [&](double shift) { client.startPhaseShiftSeconds = shift; client.startPhaseSkip.reset(); };
    const auto belowCut = [&] { client.startPhaseShiftSeconds.reset(); client.startPhaseSkip = SyntheticSkip::FrameRateBelowTick; };
    client.epochStartWaitSeconds = .004;
    belowCut();
    observe(); // Decided below the cut, then armed and withdrawn repeatedly.
    for (std::size_t cycle = 0; cycle < capacity; ++cycle) {
        armed(.002);
        observe();
        belowCut();
        observe();
        observe();
    }
    auto epoch = recorder.Json()["epochs"].at(0);
    CHECK(epoch["status"] == "shift_withdrawn_below_cut");
    CHECK(epoch["client_skip_reason"] == "frame_rate_below_tick");
    CHECK(epoch["client_first_armed_frame"] == 1);
    CHECK(epoch["restorations"] == capacity);
    CHECK(epoch["withdrawals"] == capacity);
    CHECK(epoch["withdrawn_frames"] == 2 * capacity);
    CHECK(epoch["withdrawn_first_frame"] == 2);
    CHECK(epoch["withdrawn_first_steady_ns"] == 20);
    CHECK(epoch["last_client_state"] == "withdrawn_below_cut");
    CHECK(epoch["client_conflicting_frames"] == 0);
    REQUIRE(epoch["client_state_changes"].size() == capacity);
    CHECK(epoch["client_state_changes_dropped"] == 1 + 2 * capacity - capacity);
    CHECK(epoch["client_state_changes"][0]["state"] == "skipped_below_cut");
    CHECK(epoch["client_state_changes"][1]["state"] == "shift_armed");
    CHECK(epoch["client_state_changes"][2]["state"] == "withdrawn_below_cut");
    CHECK(epoch["client_state_changes"][3]["frame"] == 4);

    // A changed armed shift, a changed wait and a lost decision contradict the
    // record; each such frame is a conflict and the last state still follows.
    armed(.003);
    observe();
    client.epochStartWaitSeconds = .005;
    observe();
    client.epochStartWaitSeconds.reset();
    client.startPhaseShiftSeconds.reset();
    observe();
    epoch = recorder.Json()["epochs"].at(0);
    CHECK(epoch["client_conflicting_frames"] == 3);
    CHECK(epoch["client_wait_seconds"].get<double>() == doctest::Approx(.004));
    CHECK(epoch["client_armed_shift_seconds"].get<double>() == doctest::Approx(.002));
    CHECK(epoch["last_client_state"] == "undecided");
    CHECK(epoch["last_client_wait_seconds"].is_null());
    CHECK(epoch["status"] == "shift_withdrawn_below_cut");
}

TEST_CASE("Start-phase record reports an unsupported product instead of guessing") {
    StartPhaseRecorder recorder;
    CHECK(recorder.Json()["supported"].is_null());
    const Bare bare;
    recorder.Observe(0, 0, 1, &bare, bare);
    const auto json = recorder.Json();
    CHECK(json["supported"] == false);
    CHECK(json["skip_reason_supported"].is_null());
    CHECK(json["epochs"].empty());
}

TEST_CASE("Start-phase record is bounded and counts lookups it could not store") {
    StartPhaseRecorder recorder;
    OlderClient client;
    const SyntheticHost* noHost{};
    for (std::uint64_t epoch = 1; epoch <= StartPhaseRecorder::Capacity + 6; ++epoch) {
        client.movementEpoch = epoch;
        recorder.Observe(epoch, 0, 1, noHost, client);
    }
    client.movementEpoch = 1;
    client.epochStartWaitSeconds = .002;
    client.startPhaseShiftSeconds = -.002;
    recorder.Observe(99, 0, 1, noHost, client); // An existing record still updates when full.
    const auto json = recorder.Json();
    CHECK(json["epochs"].size() == StartPhaseRecorder::Capacity);
    CHECK(json["dropped_observations"] == 6);
    CHECK(json["epochs"][0]["status"] == "shift_armed");
    CHECK(json["epochs"].back()["movement_epoch"] == StartPhaseRecorder::Capacity);
}

TEST_CASE("Start-phase record maps a reported reseed cancellation and keeps it final") {
    StartPhaseRecorder recorder;
    const SyntheticHost decidedHost{1, 1, 1, 4000}, pendingHost{1, 2, 1, 4000};
    CancellingClient client;
    std::uint64_t frame{};
    const auto observe = [&](const SyntheticHost& host) {
        client.movementEpoch = host.movementEpoch;
        recorder.Observe(frame, static_cast<std::int64_t>(frame) * 10, 1, &host, client);
        ++frame;
    };
    // Epoch 1: armed, then cancelled with the decided wait kept (frame 2), then
    // a frame without the wait; both are the cancellation, not a conflict.
    client.epochStartWaitSeconds = .004;
    client.startPhaseShiftSeconds = .002;
    observe(decidedHost);
    observe(decidedHost);
    client.startPhaseShiftSeconds.reset();
    client.startPhaseSkip = SyntheticCancelSkip::CancelledByReseed;
    observe(decidedHost);
    client.epochStartWaitSeconds.reset();
    observe(decidedHost);
    auto json = recorder.Json();
    CHECK(json["skip_reason_supported"] == true);
    CHECK(json["cancel_reason_supported"] == true);
    auto epoch = json["epochs"].at(0);
    CHECK(epoch["status"] == "cancelled_by_reseed");
    CHECK(epoch["client_shift_seconds"].get<double>() == doctest::Approx(.002)); // The first decision is kept.
    CHECK(epoch["client_cancelled_frames"] == 2);
    CHECK(epoch["client_cancelled_first_frame"] == 2);
    CHECK(epoch["client_cancelled_first_steady_ns"] == 20);
    CHECK(epoch["client_conflicting_frames"] == 0);
    CHECK(epoch["last_client_state"] == "cancelled_by_reseed");
    CHECK(epoch["last_client_skip_reason"] == "cancelled_by_reseed");
    REQUIRE(epoch["client_state_changes"].size() == 2);
    CHECK(epoch["client_state_changes"][1] == nlohmann::json{{"state", "cancelled_by_reseed"}, {"frame", 2}, {"steady_ns", 20}});
    // A cancellation is final for its epoch and life: an armed frame afterwards contradicts it.
    client.epochStartWaitSeconds = .004;
    client.startPhaseShiftSeconds = .002;
    client.startPhaseSkip.reset();
    observe(decidedHost);
    epoch = recorder.Json()["epochs"].at(0);
    CHECK(epoch["client_conflicting_frames"] == 1);
    CHECK(epoch["status"] == "cancelled_by_reseed");

    // Epoch 2: a pending phase cancelled before any decision carries no wait.
    client.epochStartWaitSeconds.reset();
    client.startPhaseShiftSeconds.reset();
    observe(pendingHost);
    client.startPhaseSkip = SyntheticCancelSkip::CancelledByReseed;
    observe(pendingHost);
    epoch = recorder.Json()["epochs"].at(1);
    CHECK(epoch["status"] == "cancelled_by_reseed");
    CHECK(epoch["client_wait_seconds"].is_null());
    CHECK(epoch["client_first_frame"].is_null());
    CHECK(epoch["client_cancelled_first_frame"] == frame - 1);
    CHECK(epoch["client_conflicting_frames"] == 0);

    // The enumerator is optional: a reason without it still compiles and reads unsupported.
    StartPhaseRecorder older;
    ReasonedClient reasoned;
    older.Observe(0, 0, 1, &decidedHost, reasoned);
    CHECK(older.Json()["cancel_reason_supported"] == false);
}

TEST_CASE("Start-phase record names a reseed cancellation of an armed shift only when the product reports it") {
    const auto arena = RecorderArena();
    LocalPlayerPrediction client(arena);
    StartPhaseRecorder recorder;
    ProbeFrames<LocalPlayerPrediction> frames{client, recorder};
    client.Reconcile({1, {2, 0, 2}, 0, 0, 0}, 1);
    for (int index = 0; index < FrameWindow; ++index) frames.Run(MovementTickSeconds);
    frames.wait = 9000;
    frames.Run(MovementTickSeconds);
    frames.Run(MovementTickSeconds);
    REQUIRE(client.Observation().startPhaseShiftSeconds);
    // A stall reseed: the authority has resolved past the predicted tip.
    PlayerState stalled{1, {2, 0, 2}, 0, 0, client.Observation().latestCommand + 4};
    stalled.epochStartWaitMicros = 9000;
    client.Reconcile(stalled, ++frames.tick);
    const auto reseedFrame = frames.frame;
    recorder.Observe(frames.frame, static_cast<std::int64_t>(frames.frame) * 1'000'000, 1, &stalled, client.Observation());
    ++frames.frame;
    frames.Run(MovementTickSeconds);
    const auto json = recorder.Json();
    const auto epoch = json["epochs"].at(0);
    if (ReportsCancellation(client.Observation())) {
        CHECK(epoch["status"] == "cancelled_by_reseed");
        CHECK(epoch["client_cancelled_first_frame"] == reseedFrame);
        CHECK(epoch["last_client_shift_seconds"].is_null());
        CHECK(epoch["client_conflicting_frames"] == 0);
    } else {
        // Without the reason the observation keeps reading armed after the
        // reseed; the evidence reader derives the cancellation from the trace.
        CHECK(json["cancel_reason_supported"] == ProductCancelReason);
        CHECK(epoch["status"] == "shift_armed");
        CHECK(epoch["client_cancelled_frames"] == 0);
        CHECK(epoch["last_client_state"] == "shift_armed");
    }
}

TEST_CASE("Start-phase record never attributes a Client observed before its player id is known") {
    // The probe's frame-start connection state may still lack the player id
    // when that frame's Update joins and activates the prediction.
    StartPhaseRecorder recorder;
    OlderClient client;
    const SyntheticHost host{1, 1, 1, 4000};
    recorder.Observe(0, 0, 0, &host, client);
    for (std::uint64_t frame = 1; frame <= 3; ++frame) recorder.Observe(frame, static_cast<std::int64_t>(frame) * 10, 1, &host, client);
    const auto json = recorder.Json();
    CHECK(json["unattributed_client_frames"] == 1);
    REQUIRE(json["epochs"].size() == 1);
    const auto& epoch = json["epochs"][0];
    CHECK(epoch["player_id"] == 1);
    CHECK(epoch["host_wait_first_frame"] == 0); // The Host side is keyed by its own id.
    REQUIRE(epoch["client_state_changes"].size() == 1);
    CHECK(epoch["client_state_changes"][0]["frame"] == 1);
    // The unattributed frame still counts toward the Client's active run.
    CHECK(epoch["client_active_frames_at_last_undecided"] == 4);
    CHECK(epoch["status"] == "pending_frame_window");
}
