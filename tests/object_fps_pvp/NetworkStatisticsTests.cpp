#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/NetworkStatistics.hpp"

#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena StatisticsArena() {
    Arena arena;
    arena.id = "synthetic_network_statistics";
    arena.width = arena.depth = 12;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 6}, 0}};
    return arena;
}

} // namespace

TEST_CASE("Tick wake statistics measure deadline wakes against the planned grid point") {
    const Engine::Time::TimePoint planned{1s};
    TickWakeStatistics ticks;
    ticks.Record(planned, planned + 300us, false);
    ticks.Record(planned, planned + 5ms, false);
    ticks.Record(planned, planned - 20us, false); // early: zero late, counted apart
    ticks.Record(planned, planned + 40ms, true);  // a reset request is not a late wake

    CHECK(ticks.late.Count() == 3);
    CHECK(ticks.early == 1);
    CHECK(ticks.notified == 1);
    CHECK(ticks.late.Max() == 5ms);
    CHECK(ticks.late.Bins()[0] == 1); // the early wake, as zero
    CHECK(ticks.late.Bins()[1] == 1); // 300 us
    CHECK(ticks.late.Bins()[5] == 1); // 5 ms
}

TEST_CASE("Match and worker statistics lines keep every key in a fixed order") {
    MatchStatisticsWindow match{.windowSeconds = 10.0004, .cpuSeconds = 0.25, .cpuTotalSeconds = 12.5,
                                .ipcIterations = 8123, .snapshotOverwrites = 3};
    const Engine::Time::TimePoint planned{1s};
    match.ticks.Record(planned, planned + 300us, false);
    match.ticks.Record(planned, planned + 5ms, false);
    match.ticks.Record(planned, planned, true);
    match.lockWaits.input.Record(100us);
    match.lockWaits.input.Record(3ms);
    match.lockWaits.step.Record(600us);
    // log-contract E1-5: a 30 ms interval overruns the 16.667 ms tick by 13.333 ms and is a stall; an on-time one is 0.
    match.pace.RecordInterval(30ms, 16667us);
    match.pace.RecordInterval(16667us, 16667us);
    match.pace.RecordAdvance(400us);
    CHECK(MatchStatisticsLine(match) ==
          "[ObjectFPS/PvP Match] network statistics window_s=10.000 cpu_s=0.250 cpu_total_s=12.500 ipc_iterations=8123 "
          "ipc_iterations_per_s=812.3 tick_deadline_wakes=2 tick_notified=1 tick_early=0 tick_late_p50_le_us=500 "
          "tick_late_p99_le_us=5000 tick_late_max_us=5000 tick_late_bins=0,1,0,0,0,1,0,0,0,0 snapshot_overwrites=3 "
          "host_lock_input_waits=2 host_lock_input_wait_p99_le_us=3000 host_lock_input_wait_max_us=3000 "
          "host_lock_input_wait_bins=1,0,0,0,1,0,0,0,0,0 host_lock_step_waits=1 host_lock_step_wait_p99_le_us=600 "
          "host_lock_step_wait_max_us=600 host_lock_step_wait_bins=0,0,1,0,0,0,0,0,0,0 "
          "tick_overrun_p99_le_us=13333 tick_overrun_max_us=13333 tick_overrun_bins=1,0,0,0,0,0,1,0,0,0 tick_stalls=1 "
          "advance_p99_le_us=400 advance_max_us=400 advance_bins=0,1,0,0,0,0,0,0,0,0");

    CHECK(MatchStatisticsLine({}) ==
          "[ObjectFPS/PvP Match] network statistics window_s=0.000 cpu_s=na cpu_total_s=na ipc_iterations=0 "
          "ipc_iterations_per_s=0.0 tick_deadline_wakes=0 tick_notified=0 tick_early=0 tick_late_p50_le_us=0 "
          "tick_late_p99_le_us=0 tick_late_max_us=0 tick_late_bins=0,0,0,0,0,0,0,0,0,0 snapshot_overwrites=0 "
          "host_lock_input_waits=0 host_lock_input_wait_p99_le_us=0 host_lock_input_wait_max_us=0 "
          "host_lock_input_wait_bins=0,0,0,0,0,0,0,0,0,0 host_lock_step_waits=0 host_lock_step_wait_p99_le_us=0 "
          "host_lock_step_wait_max_us=0 host_lock_step_wait_bins=0,0,0,0,0,0,0,0,0,0 "
          "tick_overrun_p99_le_us=0 tick_overrun_max_us=0 tick_overrun_bins=0,0,0,0,0,0,0,0,0,0 tick_stalls=0 "
          "advance_p99_le_us=0 advance_max_us=0 advance_bins=0,0,0,0,0,0,0,0,0,0");

    CHECK(WorkerStatisticsLine({.player = 2, .windowSeconds = 10.0, .wakes = 4812, .cpuSeconds = 0.0514}) ==
          "[ObjectFPS/PvP] worker statistics player=2 window_s=10.000 wakes=4812 wakes_per_s=481.2 cpu_s=0.051 "
          "other_version_datagrams=0 received_datagrams=0 foreign_sender_datagrams=0 undecodable_datagrams=0 "
          "stale_datagrams=0 send_blocked=0 snapshot_overflows=0 simulation_overflows=0");
    CHECK(WorkerStatisticsLine({.windowSeconds = 10.0, .wakes = 1}) ==
          "[ObjectFPS/PvP] worker statistics player=0 window_s=10.000 wakes=1 wakes_per_s=0.1 cpu_s=na "
          "other_version_datagrams=0 received_datagrams=0 foreign_sender_datagrams=0 undecodable_datagrams=0 "
          "stale_datagrams=0 send_blocked=0 snapshot_overflows=0 simulation_overflows=0");
    CHECK(WorkerStatisticsLine({.windowSeconds = 10.0, .wakes = 1, .otherVersionDatagrams = 3}).find(
          " cpu_s=na other_version_datagrams=3 ") != std::string::npos);
    // log-contract: the worker's own loss counts, appended last in a fixed order.
    CHECK(WorkerStatisticsLine({.windowSeconds = 10.0, .receivedDatagrams = 900, .foreignSenderDatagrams = 1,
                                .undecodableDatagrams = 2, .staleDatagrams = 3, .sendBlocked = 4,
                                .snapshotOverflows = 5, .simulationOverflows = 6}).ends_with(
          " other_version_datagrams=0 received_datagrams=900 foreign_sender_datagrams=1 undecodable_datagrams=2 "
          "stale_datagrams=3 send_blocked=4 snapshot_overflows=5 simulation_overflows=6"));
}

// pv7 §5: the Client's cause records line counts each hold's samples, missing
// and out-of-range values apart, keeps the latest refusal reply across
// windows and starts each window empty.
TEST_CASE("The Client's cause records line counts the holds and keeps the latest refusal") {
    ClientCauseRecords records;
    CHECK(records.Line(0, 10.0) ==
          "[ObjectFPS/PvP] cause records player=0 window_s=10.000 snapshots=0 match_hold_samples=0 match_hold_missing=0 "
          "match_hold_range=0 match_hold_max_us=0 match_hold_mean_us=0 gateway_hold_samples=0 gateway_hold_missing=0 "
          "gateway_hold_range=0 gateway_hold_max_us=0 gateway_hold_mean_us=0 refused_batches=0 last_refusal=unspecified");
    records.Snapshot(100, 10'000'000);
    records.Snapshot(300, 10'000'001);
    records.Snapshot(std::nullopt, 50);
    records.Results(3, "resetting");
    CHECK(records.Line(4, 9.5) ==
          "[ObjectFPS/PvP] cause records player=4 window_s=9.500 snapshots=3 match_hold_samples=2 match_hold_missing=1 "
          "match_hold_range=0 match_hold_max_us=300 match_hold_mean_us=200 gateway_hold_samples=2 gateway_hold_missing=0 "
          "gateway_hold_range=1 gateway_hold_max_us=10000000 gateway_hold_mean_us=5000025 refused_batches=3 "
          "last_refusal=resetting");
    CHECK(records.Line(4, 10.0).ends_with(" gateway_hold_mean_us=0 refused_batches=3 last_refusal=resetting"));
}

TEST_CASE("CPU time readings never decrease and grow with work on the calling thread") {
    const auto process = ProcessCpuSeconds();
    const auto thread = CurrentThreadCpuSeconds();
    REQUIRE(process);
    REQUIRE(thread);
    const auto until = std::chrono::steady_clock::now() + 100ms;
    volatile std::uint64_t spin = 0;
    while (std::chrono::steady_clock::now() < until) spin = spin + 1;
    const auto processAfter = ProcessCpuSeconds();
    const auto threadAfter = CurrentThreadCpuSeconds();
    REQUIRE(processAfter);
    REQUIRE(threadAfter);
    CHECK(*processAfter >= *process);
    CHECK(*threadAfter > *thread);
}

TEST_CASE("The Match host reports its tick waits per window and resets them") {
    MatchRuntimeHost host{StatisticsArena()};
    std::string error;
    REQUIRE(host.Start(error));
    // Poll rather than sleep a fixed time: a loaded machine may run the
    // simulation role late, and the windows together keep every wait.
    std::uint64_t deadlineWakes{}, notified{};
    const auto giveUp = std::chrono::steady_clock::now() + 10s;
    auto lastTake = std::chrono::steady_clock::now();
    while (deadlineWakes < 3 && std::chrono::steady_clock::now() < giveUp) {
        std::this_thread::sleep_for(20ms);
        const auto window = host.TakeStatistics().ticks;
        lastTake = std::chrono::steady_clock::now();
        deadlineWakes += window.late.Count();
        notified += window.notified;
    }
    CHECK(deadlineWakes >= 3);
    CHECK(notified == 0);
    auto reset = host.RequestReset();
    REQUIRE(reset.wait_for(2s) == std::future_status::ready);
    std::this_thread::sleep_for(50ms);
    host.Stop();
    const std::chrono::duration<double> window = std::chrono::steady_clock::now() - lastTake;
    // Only the waits since the previous take, and the reset request woke the
    // role once. The window includes the reset itself, which a loaded machine
    // stretches, so the bound is the window's tick count plus a little slack.
    const auto last = host.TakeStatistics().ticks;
    CHECK(last.notified == 1);
    CHECK(static_cast<double>(last.late.Count()) <= window.count() * 60.0 + 3.0);
}

TEST_CASE("Match IPC statistics line keeps its keys in a fixed order (log-contract, E1-7)") {
    IpcLaneStatistics lanes;
    lanes.snapshotWrites = 2;
    lanes.snapshotWait.Record(100us);
    lanes.snapshotWait.Record(700us);
    lanes.actionWrites = 1;
    lanes.actionLate.Record(0us);
    CHECK(IpcStatisticsLine(lanes, 10.0004, false) ==
          "[ObjectFPS/PvP Match] ipc statistics version=1 window_s=10.000 final=0 snapshot_writes=2 "
          "snapshot_wait_p99_le_us=700 snapshot_wait_max_us=700 snapshot_wait_bins=1,0,1,0,0,0,0,0,0,0 action_writes=1 "
          "action_late_p99_le_us=0 action_late_max_us=0 action_late_bins=1,0,0,0,0,0,0,0,0,0");
    CHECK(IpcStatisticsLine({}, 2.5, true).starts_with("[ObjectFPS/PvP Match] ipc statistics version=1 window_s=2.500 final=1 "));
}

TEST_CASE("Role started line names the role's own timer backend (log-contract, N-wait-backend-not-logged)") {
    auto role = Engine::Threads::RoleThread::Start({"gyo-test-role"}, [](Engine::Threads::RoleContext& context) {
        while (!context.StopRequested()) static_cast<void>(context.Wait());
    });
    REQUIRE(role.has_value());
    const auto line = RoleStartedLine(*role);
    CHECK(line.starts_with(std::string("role started version=1 role=gyo-test-role wait_backend=") +
                           Engine::Time::ToString(role->WaitBackend()) + " name_applied="));
    CHECK(line.find(" priority_applied=") != std::string::npos);
    role->RequestStop();
    role->Join();
}

