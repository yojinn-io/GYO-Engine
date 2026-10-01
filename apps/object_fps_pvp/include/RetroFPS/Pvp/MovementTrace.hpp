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
    SnapshotProduced, SnapshotReceived, Presentation, RuntimeGap, Transport, LifecycleCancelled };
enum class MovementInputSource { None, Actual, Held, Neutral };
enum class MovementResetReason { None, Backlog, SequenceExhausted, Starvation, LifeRespawn };

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
};

inline std::int64_t MovementTraceNowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class MovementTrace final {
public:
    static constexpr std::size_t Capacity = 8192;
    void Record(MovementTraceEvent event) {
        if (!event.timeNs) event.timeNs = MovementTraceNowNs();
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
inline std::shared_ptr<MovementTrace> movementTraceSink;
inline void SetMovementTrace(std::shared_ptr<MovementTrace> trace) noexcept {
    std::shared_ptr<MovementTrace> previous;
    {
        std::lock_guard lock(movementTraceSinkMutex);
        previous = std::exchange(movementTraceSink, std::move(trace));
    }
}
inline void TraceMovement(MovementTraceEvent event) {
    std::shared_ptr<MovementTrace> trace;
    {
        std::lock_guard lock(movementTraceSinkMutex);
        trace = movementTraceSink;
    }
    if (trace) trace->Record(event);
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
inline constexpr std::string_view TraceResetReasonName(MovementResetReason reason) noexcept {
    switch (reason) {
    case MovementResetReason::None: return "none";
    case MovementResetReason::Backlog: return "backlog";
    case MovementResetReason::Starvation: return "starvation";
    case MovementResetReason::SequenceExhausted: return "sequence_exhausted";
    case MovementResetReason::LifeRespawn: return "life_respawn";
    }
    return "invalid";
}
} // namespace fps::pvp
