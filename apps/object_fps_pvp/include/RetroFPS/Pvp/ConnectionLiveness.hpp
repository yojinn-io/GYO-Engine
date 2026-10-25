#pragma once

// The Client's connection liveness of one joined session (v7.1 outage-resync
// client-interrupted; network-architecture §4 "連線中斷與恢復"): connected,
// interrupted or timed out. It decides only from the arrival times the network
// worker passes, each the steady_clock read when the datagram was received, and
// the two constants of Movement.hpp. No clock-sampling value (offset, delay,
// time echo) may reach it (protocol-v7 §2; test_diagnostic_isolation.py lists this
// file as deciding). Header-only and clock-free, so tests drive it with any time.
//
// Before the first Welcome the join may last T_max from Begin. From the first
// Welcome until the first valid snapshot it may last T_max from that Welcome;
// later Welcomes extend nothing. While playing (after the first valid snapshot)
// T_int without a valid snapshot interrupts the connection and T_max ends it.
#include "RetroFPS/Pvp/Movement.hpp"

#include <chrono>

namespace fps::pvp {

class ConnectionLiveness final {
public:
    using Clock = std::chrono::steady_clock;
    enum class Event { None, Interrupted, TimedOut };
    struct Arrival final {
        // Since the previous valid snapshot; zero for the first one.
        Clock::duration gap{};
        // This snapshot ended an interruption.
        bool resumed{};
    };

    // A join at this time: a new session.
    void Begin(Clock::time_point at) noexcept { *this = {}; since_ = at; }
    void Welcome(Clock::time_point arrivedAt) noexcept {
        if (welcomed_) return;
        welcomed_ = true;
        if (!playing_) since_ = arrivedAt;
    }
    // A valid snapshot: one with the local player and a newer tick.
    [[nodiscard]] Arrival Snapshot(Clock::time_point arrivedAt) noexcept {
        const Arrival arrival{playing_ ? arrivedAt - since_ : Clock::duration{}, interrupted_};
        playing_ = true;
        interrupted_ = false;
        since_ = arrivedAt;
        return arrival;
    }
    // What the time now changed: an interruption starts once, a timeout ends the session.
    [[nodiscard]] Event Poll(Clock::time_point now) noexcept {
        if (now - since_ >= ConnectionTimeoutAfter) return Event::TimedOut;
        if (!playing_ || interrupted_ || now - since_ < ConnectionInterruptedAfter) return Event::None;
        interrupted_ = true;
        return Event::Interrupted;
    }
    [[nodiscard]] bool Interrupted() const noexcept { return interrupted_; }
    // A valid snapshot arrived: the connection is in the connected or the interrupted state.
    [[nodiscard]] bool Playing() const noexcept { return playing_; }
    // The last valid snapshot, or what stands for it before the first one.
    [[nodiscard]] Clock::time_point Since() const noexcept { return since_; }
    // When Poll next has something to report.
    [[nodiscard]] Clock::time_point NextDeadline() const noexcept {
        return since_ + (playing_ && !interrupted_ ? ConnectionInterruptedAfter : ConnectionTimeoutAfter);
    }

private:
    Clock::time_point since_{};
    bool welcomed_{};
    bool playing_{};
    bool interrupted_{};
};

// Whether a Poll event records an interruption: one that starts now, or the
// timeout of a session that was playing and not yet interrupted (a worker
// stall or a sleep longer than T_max jumps past T_int), so the summary of a
// session that ended that way does not read as uninterrupted.
[[nodiscard]] constexpr bool PollRecordsInterruption(ConnectionLiveness::Event event, bool playing,
                                                     bool interrupted) noexcept {
    return event == ConnectionLiveness::Event::Interrupted ||
           (event == ConnectionLiveness::Event::TimedOut && playing && !interrupted);
}

} // namespace fps::pvp
