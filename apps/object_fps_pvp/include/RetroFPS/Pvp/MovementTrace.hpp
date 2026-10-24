#pragma once

// Product-owned, optional diagnostics. No trace value is consumed by gameplay.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace fps::pvp {

enum class MovementTraceKind { Generated, Sent, HostAccepted, Resolved, Reset,
    SnapshotProduced, SnapshotReceived, Presentation, RuntimeGap, Transport, LifecycleCancelled, PhaseDecision };
enum class MovementInputSource { None, Actual, Held, Neutral };
enum class MovementResetReason { None, Backlog, SequenceExhausted, Starvation, LifeRespawn, Resync };
// Which rule decided a PhaseDecision: the first decision after a seed (or after
// a late correction, or after a resume reset), a later window decision, or late
// samples (schema 3). ResumeReset is no decision: the connection resumed and
// phase acquisition restarted without a seed (count 0, the shift is the
// remaining correction it cancelled; v7.1 outage-resync, log-contract §1.4).
enum class MovementPhasePath { None, First, Window, Late, ResumeReset };

struct MovementTraceEvent final {
    MovementTraceKind kind{};
    std::int64_t timeNs{};
    std::uint64_t playerId{};
    std::uint64_t epoch{1};
    std::uint64_t sequence{};
    std::uint64_t authorityTick{};
    MovementInputSource source{MovementInputSource::None};
    std::uint32_t queued{};
    std::uint32_t pending{};
    bool seededNeutral{};
    double droppedSeconds{};
    double frameSeconds{};
    // Counters/ages for diagnostics; Reset/LifeRespawn count is cancelled queued input.
    std::uint64_t count{};
    double ageSeconds{};
    MovementResetReason resetReason{MovementResetReason::None};
    // Successful Sent events span the nonblocking send call: start to timeNs.
    std::int64_t startedNs{};
    std::uint64_t lifeGeneration{1};
    // PhaseDecision only (the Client's phase tracking, LocalPlayerPrediction):
    // the decided error, the correction it started (0 when inside the
    // deadband), the deadband (0 for a late correction). count is the samples
    // the decision used; sequence the newest sample's command.
    MovementPhasePath phasePath{MovementPhasePath::None};
    double phaseErrorSeconds{};
    double phaseShiftSeconds{};
    double phaseDeadbandSeconds{};
};

inline std::int64_t MovementTraceNowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Where TraceMovement sends events: the in-memory buffer below (tests and
// probes read it back) or the file of MovementTraceWriter.hpp.
class MovementTraceSink {
public:
    MovementTraceSink() = default;
    MovementTraceSink(const MovementTraceSink&) = delete;
    MovementTraceSink& operator=(const MovementTraceSink&) = delete;
    virtual ~MovementTraceSink() = default;
    // Any thread; timeNs is already set.
    virtual void Record(const MovementTraceEvent& event) = 0;
};

class MovementTrace final : public MovementTraceSink {
public:
    static constexpr std::size_t Capacity = 8192;
    void Record(const MovementTraceEvent& event) override {
        std::lock_guard lock(mutex_);
        if (events_.size() == Capacity) { ++dropped_; return; }
        events_.push_back(event);
    }
    [[nodiscard]] std::vector<MovementTraceEvent> Drain() {
        std::lock_guard lock(mutex_);
        std::vector<MovementTraceEvent> result(events_.begin(), events_.end());
        events_.clear();
        return result;
    }
    [[nodiscard]] std::uint64_t Dropped() const {
        std::lock_guard lock(mutex_);
        return dropped_;
    }
private:
    mutable std::mutex mutex_;
    std::deque<MovementTraceEvent> events_;
    std::uint64_t dropped_{};
};

// One optional sink per product process. Installation happens before workers
// start; guarded publication also makes shutdown/diagnostic probes safe.
// A mutex is used instead of std::atomic<std::shared_ptr>, which Apple libc++ lacks.
inline std::mutex movementTraceSinkMutex;
inline std::shared_ptr<MovementTraceSink> movementTraceSink;
inline void SetMovementTrace(std::shared_ptr<MovementTraceSink> trace) noexcept {
    std::shared_ptr<MovementTraceSink> previous;
    {
        std::lock_guard lock(movementTraceSinkMutex);
        previous = std::exchange(movementTraceSink, std::move(trace));
    }
}
inline void TraceMovement(MovementTraceEvent event) {
    std::shared_ptr<MovementTraceSink> trace;
    {
        std::lock_guard lock(movementTraceSinkMutex);
        trace = movementTraceSink;
    }
    if (!trace) return;
    if (!event.timeNs) event.timeNs = MovementTraceNowNs();
    trace->Record(event);
}
inline constexpr std::string_view TraceKindName(MovementTraceKind kind) noexcept {
    switch (kind) {
    case MovementTraceKind::Generated: return "generated";
    case MovementTraceKind::Sent: return "sent";
    case MovementTraceKind::HostAccepted: return "host_accepted";
    case MovementTraceKind::Resolved: return "resolved";
    case MovementTraceKind::Reset: return "reset";
    case MovementTraceKind::SnapshotProduced: return "snapshot_produced";
    case MovementTraceKind::SnapshotReceived: return "snapshot_received";
    case MovementTraceKind::Presentation: return "presentation";
    case MovementTraceKind::RuntimeGap: return "runtime_gap";
    case MovementTraceKind::Transport: return "transport";
    case MovementTraceKind::LifecycleCancelled: return "lifecycle_cancelled";
    case MovementTraceKind::PhaseDecision: return "phase_decision";
    }
    return "invalid";
}
inline constexpr std::string_view TraceSourceName(MovementInputSource source) noexcept {
    switch (source) {
    case MovementInputSource::None: return "none";
    case MovementInputSource::Actual: return "actual";
    case MovementInputSource::Held: return "held";
    case MovementInputSource::Neutral: return "neutral";
    }
    return "invalid";
}
inline constexpr std::string_view TracePhasePathName(MovementPhasePath path) noexcept {
    switch (path) {
    case MovementPhasePath::None: return "none";
    case MovementPhasePath::First: return "first";
    case MovementPhasePath::Window: return "window";
    case MovementPhasePath::Late: return "late";
    case MovementPhasePath::ResumeReset: return "resume_reset";
    }
    return "invalid";
}
inline constexpr std::string_view TraceResetReasonName(MovementResetReason reason) noexcept {
    switch (reason) {
    case MovementResetReason::None: return "none";
    case MovementResetReason::Backlog: return "backlog";
    case MovementResetReason::Starvation: return "starvation";
    case MovementResetReason::SequenceExhausted: return "sequence_exhausted";
    case MovementResetReason::LifeRespawn: return "life_respawn";
    case MovementResetReason::Resync: return "resync";
    }
    return "invalid";
}
} // namespace fps::pvp
