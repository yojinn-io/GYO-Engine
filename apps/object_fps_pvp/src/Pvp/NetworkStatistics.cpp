#include "RetroFPS/Pvp/NetworkStatistics.hpp"

#include <chrono>
#include <iomanip>
#include <locale>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <time.h>
#endif

namespace fps::pvp {
namespace {

#if defined(_WIN32)
double Seconds(const FILETIME& time) {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return static_cast<double>(value.QuadPart) / 1e7; // 100 ns units
}
#else
double Seconds(const timeval& time) {
    return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_usec) / 1e6;
}
#endif

std::ostringstream LineStream() {
    std::ostringstream line;
    line.imbue(std::locale::classic());
    return line;
}

// Missing values print as "na" so that every line keeps every key.
void Optional(std::ostringstream& line, const std::optional<double>& value, const int precision) {
    if (value) line << std::fixed << std::setprecision(precision) << *value;
    else line << "na";
}

std::int64_t Micros(const Engine::Time::Duration duration) {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

} // namespace

std::optional<double> ProcessCpuSeconds() noexcept {
#if defined(_WIN32)
    FILETIME creation{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exited, &kernel, &user)) return std::nullopt;
    return Seconds(kernel) + Seconds(user);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return std::nullopt;
    return Seconds(usage.ru_utime) + Seconds(usage.ru_stime);
#endif
}

std::optional<double> CurrentThreadCpuSeconds() noexcept {
#if defined(_WIN32)
    FILETIME creation{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &creation, &exited, &kernel, &user)) return std::nullopt;
    return Seconds(kernel) + Seconds(user);
#else
    timespec time{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) return std::nullopt;
    return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_nsec) / 1e9;
#endif
}

void TickWakeStatistics::Record(const Engine::Time::TimePoint planned, const Engine::Time::TimePoint woke,
                                const bool byNotification) {
    if (byNotification) {
        ++notified;
        return;
    }
    if (woke < planned) {
        ++early;
        late.Record(Engine::Time::Duration::zero());
        return;
    }
    late.Record(woke - planned);
}

std::string MatchStatisticsLine(const MatchStatisticsWindow& window) {
    auto line = LineStream();
    const auto& late = window.ticks.late;
    line << "[ObjectFPS/PvP Match] network statistics window_s=" << std::fixed << std::setprecision(3) << window.windowSeconds
         << " cpu_s=";
    Optional(line, window.cpuSeconds, 3);
    line << " cpu_total_s=";
    Optional(line, window.cpuTotalSeconds, 3);
    line << " ipc_iterations=" << window.ipcIterations << " ipc_iterations_per_s=" << std::setprecision(1)
         << (window.windowSeconds > 0 ? static_cast<double>(window.ipcIterations) / window.windowSeconds : 0.0)
         << " tick_deadline_wakes=" << late.Count() << " tick_notified=" << window.ticks.notified
         << " tick_early=" << window.ticks.early
         << " tick_late_p50_le_us=" << Micros(late.QuantileUpperBound(0.50))
         << " tick_late_p99_le_us=" << Micros(late.QuantileUpperBound(0.99))
         << " tick_late_max_us=" << Micros(late.Max()) << " tick_late_bins=";
    for (std::size_t bin = 0; bin < Engine::Time::LateWakeStats::BinCount; ++bin)
        line << (bin ? "," : "") << late.Bins()[bin];
    // Added after the first logs (v7 batch 07), so it comes last.
    line << " snapshot_overwrites=" << window.snapshotOverwrites;
    return line.str();
}

std::string WorkerStatisticsLine(const WorkerStatisticsWindow& window) {
    auto line = LineStream();
    line << "[ObjectFPS/PvP] worker statistics player=" << window.player << " window_s=" << std::fixed
         << std::setprecision(3) << window.windowSeconds
         << " wakes=" << window.wakes << " wakes_per_s=" << std::setprecision(1)
         << (window.windowSeconds > 0 ? static_cast<double>(window.wakes) / window.windowSeconds : 0.0)
         << " cpu_s=";
    Optional(line, window.cpuSeconds, 3);
    return line.str();
}

} // namespace fps::pvp
