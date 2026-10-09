#pragma once

#include "engine/time/LateWakeStats.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace fps::pvp {

// Diagnostics only: the network path's processes each write one key=value
// statistics line per window of this length. Recording never changes timing.
inline constexpr double NetworkStatisticsSeconds = 10.0;

// CPU time (user + system) used so far, in seconds; nullopt when the platform
// refuses the query. Windows counts in scheduler ticks (about 15.6 ms).
[[nodiscard]] std::optional<double> ProcessCpuSeconds() noexcept;
[[nodiscard]] std::optional<double> CurrentThreadCpuSeconds() noexcept;

// How the Match simulation thread's waits for the next tick ended. planned is
// the tick grid point: the sample taken before Advance plus its
// secondsUntilNextTick.
struct TickWakeStatistics final {
    Engine::Time::LateWakeStats late; // deadline wakes, measured against planned
    std::uint64_t notified{};         // woken by a reset request: not a late wake
    std::uint64_t early{};            // deadline wakes before planned, recorded as zero late

    void Record(Engine::Time::TimePoint planned, Engine::Time::TimePoint woke, bool byNotification);
};

struct MatchStatisticsWindow final {
    double windowSeconds{};
    std::optional<double> cpuSeconds;      // process CPU during the window
    std::optional<double> cpuTotalSeconds; // process CPU since start
    std::uint64_t ipcIterations{};
    TickWakeStatistics ticks;
    std::uint64_t snapshotOverwrites{}; // published snapshots replaced before the I/O layer took them
};
[[nodiscard]] std::string MatchStatisticsLine(const MatchStatisticsWindow& window);

struct WorkerStatisticsWindow final {
    std::uint64_t player{}; // the session's player id; zero outside a session
    double windowSeconds{};
    std::uint64_t wakes{};
    std::optional<double> cpuSeconds; // the worker thread's CPU during the window
};
[[nodiscard]] std::string WorkerStatisticsLine(const WorkerStatisticsWindow& window);

} // namespace fps::pvp
