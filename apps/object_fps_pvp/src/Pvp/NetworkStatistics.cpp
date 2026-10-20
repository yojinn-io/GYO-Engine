#include "RetroFPS/Pvp/NetworkStatistics.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <tuple>
#include <locale>
#include <sstream>
#include <utility>

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

void TickPaceStatistics::RecordInterval(const Engine::Time::Duration interval, const Engine::Time::Duration nominal) {
    overrun.Record(interval > nominal ? interval - nominal : Engine::Time::Duration::zero());
    if (interval * 2 > nominal * 3) ++stalls;
}

void TickPaceStatistics::RecordAdvance(const Engine::Time::Duration took) {
    advance.Record(took > Engine::Time::Duration::zero() ? took : Engine::Time::Duration::zero());
}

std::string IpcStatisticsLine(const IpcLaneStatistics& lanes, const double windowSeconds, const bool final) {
    auto line = LineStream();
    line << "[ObjectFPS/PvP Match] ipc statistics version=1 window_s=" << std::fixed << std::setprecision(3) << windowSeconds
         << " final=" << (final ? 1 : 0);
    for (const auto& [name, writes, stats] : {std::tuple{"snapshot_wait", lanes.snapshotWrites, &lanes.snapshotWait},
                                              std::tuple{"action_late", lanes.actionWrites, &lanes.actionLate}}) {
        line << " " << (stats == &lanes.snapshotWait ? "snapshot_writes=" : "action_writes=") << writes << " " << name
             << "_p99_le_us=" << Micros(stats->QuantileUpperBound(0.99)) << " " << name << "_max_us=" << Micros(stats->Max())
             << " " << name << "_bins=";
        for (std::size_t bin = 0; bin < Engine::Time::LateWakeStats::BinCount; ++bin)
            line << (bin ? "," : "") << stats->Bins()[bin];
    }
    return line.str();
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
    // Added in pv7 (v7.1 edge-input-passthrough), so it comes last.
    for (const auto& [name, waits] : {std::pair{"input", &window.lockWaits.input}, std::pair{"step", &window.lockWaits.step}}) {
        line << " host_lock_" << name << "_waits=" << waits->Count() << " host_lock_" << name
             << "_wait_p99_le_us=" << Micros(waits->QuantileUpperBound(0.99)) << " host_lock_" << name
             << "_wait_max_us=" << Micros(waits->Max()) << " host_lock_" << name << "_wait_bins=";
        for (std::size_t bin = 0; bin < Engine::Time::LateWakeStats::BinCount; ++bin)
            line << (bin ? "," : "") << waits->Bins()[bin];
    }
    // Added in v7.1 log-contract (E1-5), so it comes last.
    for (const auto& [name, stats] : {std::pair{"tick_overrun", &window.pace.overrun}, std::pair{"advance", &window.pace.advance}}) {
        line << " " << name << "_p99_le_us=" << Micros(stats->QuantileUpperBound(0.99)) << " " << name
             << "_max_us=" << Micros(stats->Max()) << " " << name << "_bins=";
        for (std::size_t bin = 0; bin < Engine::Time::LateWakeStats::BinCount; ++bin)
            line << (bin ? "," : "") << stats->Bins()[bin];
        if (stats == &window.pace.overrun) line << " tick_stalls=" << window.pace.stalls;
    }
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
    // Added in pv7 (v7.1 version-errors), so it comes last.
    line << " other_version_datagrams=" << window.otherVersionDatagrams;
    // Added in v7.1 log-contract, so they come last.
    line << " received_datagrams=" << window.receivedDatagrams << " foreign_sender_datagrams=" << window.foreignSenderDatagrams
         << " undecodable_datagrams=" << window.undecodableDatagrams << " stale_datagrams=" << window.staleDatagrams
         << " send_blocked=" << window.sendBlocked << " snapshot_overflows=" << window.snapshotOverflows
         << " simulation_overflows=" << window.simulationOverflows;
    return line.str();
}

void ClientCauseRecords::Hold::Add(const std::optional<std::uint32_t> value) {
    if (!value) ++missing;
    else if (*value > 10'000'000) ++range;
    else {
        ++samples;
        sum += *value;
        maximum = std::max<std::uint64_t>(maximum, *value);
    }
}

void ClientCauseRecords::Snapshot(const std::optional<std::uint32_t> matchHoldUs,
                                  const std::optional<std::uint32_t> gatewayHoldUs) {
    ++snapshots_;
    match_.Add(matchHoldUs);
    gateway_.Add(gatewayHoldUs);
}

void ClientCauseRecords::Results(const std::uint64_t refusedBatches, std::string refusal) {
    refused_ = refusedBatches;
    refusal_ = std::move(refusal);
}

std::string ClientCauseRecords::Line(const std::uint64_t player, const double windowSeconds) {
    auto line = LineStream();
    line << "[ObjectFPS/PvP] cause records player=" << player << " window_s=" << std::fixed << std::setprecision(3)
         << windowSeconds << " snapshots=" << snapshots_;
    for (const auto& [name, hold] : {std::pair{"match_hold", &match_}, std::pair{"gateway_hold", &gateway_}}) {
        line << ' ' << name << "_samples=" << hold->samples << ' ' << name << "_missing=" << hold->missing << ' ' << name
             << "_range=" << hold->range << ' ' << name << "_max_us=" << hold->maximum << ' ' << name
             << "_mean_us=" << (hold->samples ? hold->sum / hold->samples : 0);
    }
    line << " refused_batches=" << refused_ << " last_refusal=" << refusal_;
    snapshots_ = 0;
    match_ = {};
    gateway_ = {};
    return line.str();
}

} // namespace fps::pvp
