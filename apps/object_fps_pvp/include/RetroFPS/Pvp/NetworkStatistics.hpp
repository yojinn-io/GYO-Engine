#pragma once

#include "engine/threads/RoleThread.hpp"
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

// The Match simulation thread's tick spacing (log-contract, E1-5). interval is
// the time between two consecutive samples taken before Advance; a stall
// anywhere in the loop (in Advance, in the wait, or after it) lengthens it,
// while TickWakeStatistics sees only a late wake. The late-wake bins are fine
// near zero, so the interval is recorded as its overrun beyond the nominal
// tick; stalls counts the intervals longer than 1.5 nominal ticks. advance is
// the time one Advance took.
struct TickPaceStatistics final {
    Engine::Time::LateWakeStats overrun; // max(0, interval - nominal)
    std::uint64_t stalls{};
    Engine::Time::LateWakeStats advance;

    void RecordInterval(Engine::Time::Duration interval, Engine::Time::Duration nominal);
    void RecordAdvance(Engine::Time::Duration took);
};

// How long callers waited to take the Match host's mutex, in the late-wake
// bins (diagnostics only, D32): the I/O thread's SubmitInput and the
// simulation thread's step.
struct HostLockWaits final {
    Engine::Time::LateWakeStats input;
    Engine::Time::LateWakeStats step;
};

// "role started version=1 role=<name> wait_backend=<backend> name_applied=<0|1>
// priority_applied=<0|1>" (log-contract, N-wait-backend-not-logged): written by
// the role's owner after Start, with its own prefix.
[[nodiscard]] std::string RoleStartedLine(const Engine::Threads::RoleThread& role);

// The Match IPC lanes' pace (log-contract, E1-7), owned by IpcHost: how long a
// published snapshot waited before its frame was chosen for writing, and how
// late a due action-lane send went out (zero for a lane's first send).
struct IpcLaneStatistics final {
    std::uint64_t snapshotWrites{};
    Engine::Time::LateWakeStats snapshotWait;
    std::uint64_t actionWrites{};
    Engine::Time::LateWakeStats actionLate;
};
// "[ObjectFPS/PvP Match] ipc statistics version=1 window_s= final= ..."
[[nodiscard]] std::string IpcStatisticsLine(const IpcLaneStatistics& lanes, double windowSeconds, bool final);

struct MatchStatisticsWindow final {
    double windowSeconds{};
    std::optional<double> cpuSeconds;      // process CPU during the window
    std::optional<double> cpuTotalSeconds; // process CPU since start
    std::uint64_t ipcIterations{};
    TickWakeStatistics ticks;
    std::uint64_t snapshotOverwrites{}; // published snapshots replaced before the I/O layer took them
    HostLockWaits lockWaits;
    TickPaceStatistics pace;
};
[[nodiscard]] std::string MatchStatisticsLine(const MatchStatisticsWindow& window);

struct WorkerStatisticsWindow final {
    std::uint64_t player{}; // the session's player id; zero outside a session
    double windowSeconds{};
    std::uint64_t wakes{};
    std::optional<double> cpuSeconds; // the worker thread's CPU during the window
    std::uint64_t otherVersionDatagrams{}; // from the Gateway, well-formed but another protocol version (pv7)
    // log-contract (N-client-side-loss-counters), the worker's own window counts:
    // datagrams read, and of them the ones from another sender, undecodable
    // (exclusive of other_version), of another session or not newer; sends the
    // socket refused for now; snapshots dropped from the full presentation and
    // simulation queues.
    std::uint64_t receivedDatagrams{}, foreignSenderDatagrams{}, undecodableDatagrams{}, staleDatagrams{};
    std::uint64_t sendBlocked{}, snapshotOverflows{}, simulationOverflows{};
    // v7.1 silent-branch-asserts (D180): datagrams of the session that decode
    // but change nothing. A payload that does not parse; a Snapshot or results
    // before the Welcome; a Snapshot with impossible content (zero tick,
    // sizes, fields, a cursor going back); a valid Snapshot not applied (not
    // newer, or without the local player before the first); a kind the
    // Gateway does not send (Hello, Input, Actions).
    std::uint64_t unparsedDatagrams{}, earlyDatagrams{}, invalidSnapshotDatagrams{}, ignoredSnapshotDatagrams{};
    std::uint64_t unexpectedKindDatagrams{};
};
[[nodiscard]] std::string WorkerStatisticsLine(const WorkerStatisticsWindow& window);

// Diagnostics only (pv7 §5; D32): per statistics window, the two holds the
// Client's snapshots carried (the Match's and the Gateway's, D-4) and the
// Match's latest refusal reply (D-6), as one cause records line. A missing or
// out-of-range hold (pv7 §2 clamps a duration at 10 s) is counted and ignored
// (D-5). Nothing reads it back.
class ClientCauseRecords final {
public:
    void Snapshot(std::optional<std::uint32_t> matchHoldUs, std::optional<std::uint32_t> gatewayHoldUs);
    // refusal: the reason's lower-case name as the line shows it.
    void Results(std::uint64_t refusedBatches, std::string refusal);
    // The line of the window that ends now; starts the next one (the refusal
    // reply is the latest, so it carries over).
    [[nodiscard]] std::string Line(std::uint64_t player, double windowSeconds);

private:
    struct Hold final {
        std::uint64_t samples{}, missing{}, range{}, sum{}, maximum{};
        void Add(std::optional<std::uint32_t> value);
    };
    std::uint64_t snapshots_{}, refused_{};
    std::string refusal_{"unspecified"};
    Hold match_, gateway_;
};

} // namespace fps::pvp
