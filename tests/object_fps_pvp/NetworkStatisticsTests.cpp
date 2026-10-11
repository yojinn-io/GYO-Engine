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
    CHECK(MatchStatisticsLine(match) ==
          "[ObjectFPS/PvP Match] network statistics window_s=10.000 cpu_s=0.250 cpu_total_s=12.500 ipc_iterations=8123 "
          "ipc_iterations_per_s=812.3 tick_deadline_wakes=2 tick_notified=1 tick_early=0 tick_late_p50_le_us=500 "
          "tick_late_p99_le_us=5000 tick_late_max_us=5000 tick_late_bins=0,1,0,0,0,1,0,0,0,0 snapshot_overwrites=3");

    CHECK(MatchStatisticsLine({}) ==
          "[ObjectFPS/PvP Match] network statistics window_s=0.000 cpu_s=na cpu_total_s=na ipc_iterations=0 "
          "ipc_iterations_per_s=0.0 tick_deadline_wakes=0 tick_notified=0 tick_early=0 tick_late_p50_le_us=0 "
          "tick_late_p99_le_us=0 tick_late_max_us=0 tick_late_bins=0,0,0,0,0,0,0,0,0,0 snapshot_overwrites=0");

    CHECK(WorkerStatisticsLine({.player = 2, .windowSeconds = 10.0, .wakes = 4812, .cpuSeconds = 0.0514}) ==
          "[ObjectFPS/PvP] worker statistics player=2 window_s=10.000 wakes=4812 wakes_per_s=481.2 cpu_s=0.051");
    CHECK(WorkerStatisticsLine({.windowSeconds = 10.0, .wakes = 1}) ==
          "[ObjectFPS/PvP] worker statistics player=0 window_s=10.000 wakes=1 wakes_per_s=0.1 cpu_s=na");
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
