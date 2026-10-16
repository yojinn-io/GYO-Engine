#pragma once
// Diagnostics only (pv7 contract §2, §3): four-timestamp clock sampling with a
// peer. Only the I/O layer owns it (IpcHost; ClientConnection from time-echo);
// no value it computes may reach a decision (D32, guarded by
// tests/object_fps_pvp/test_diagnostic_isolation.py). Times are microseconds of
// one monotonic clock; the caller passes them, so tests use any clock.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>

namespace fps::pvp {

// One ClockSample as it travels, in its sender's clock.
struct ClockStamp {
    std::optional<std::uint64_t> originUs;  // the last peer transmitUs the sender read, as is
    std::optional<std::uint64_t> receiveUs; // when the sender read it
    std::uint64_t transmitUs{};             // when the sender serialized this sample
};

// One window of samples (about ClockSync::WindowUs), as logged.
struct ClockWindow {
    std::uint64_t windowUs{};
    std::uint64_t samples{};       // usable samples
    std::uint64_t invalid{};       // semantic violations, ignored (no veto, pv7 §2)
    std::uint64_t negativeDelay{}; // samples dropped for a negative delay
    std::uint64_t replaced{};      // own unsent samples replaced by a newer one
    std::optional<std::int64_t> offsetUs; // peer minus this clock, from the minimum-delay sample
    std::optional<std::int64_t> delayMinUs, delayMaxUs;
    std::optional<double> driftPpm; // offset regression over the windows since the last jump
    bool jump{};  // the offset moved beyond both samples' uncertainty and the drift allowance
    bool final{}; // the window closed with the link, not after WindowUs
};

// "clock sync window_ms=... final=0": the line both ends log (pv7 §3).
[[nodiscard]] std::string ClockWindowLine(const ClockWindow& window);

class ClockSync {
public:
    static constexpr std::uint64_t WindowUs = 10'000'000;
    // Own transmits remembered to match a returned origin (pv7 §3).
    static constexpr std::size_t RememberedTransmits = 8;
    // Window offsets kept for the drift regression.
    static constexpr std::size_t DriftWindows = 30;
    // A jump needs more than both samples' uncertainty (delay / 2 each) plus
    // this drift allowance: 100 ppm of the time between the two samples, and
    // at least 1 ms.
    static constexpr std::int64_t JumpFloorUs = 1'000;
    static constexpr std::int64_t JumpAllowancePpm = 100;
    // Larger times are not microseconds of a monotonic clock (about 285 years);
    // they are refused, which also keeps the arithmetic exact in int64.
    static constexpr std::uint64_t MaxTimeUs = std::uint64_t{1} << 53;

    ClockSync() = default;
    explicit ClockSync(std::uint64_t nowUs) : windowStartUs_(nowUs) {}

    // The transmitUs of an outgoing sample serialized now: strictly increasing,
    // at least 1, and remembered for origin matching.
    [[nodiscard]] std::uint64_t Stamp(std::uint64_t nowUs);
    // A peer sample read at receivedUs (this clock). Returns the (origin,
    // receive) pair the next own sample echoes, unless the peer's transmit is
    // invalid; a sample whose origin matches a remembered transmit is measured.
    std::optional<std::pair<std::uint64_t, std::uint64_t>> Observe(const ClockStamp& peer, std::uint64_t receivedUs);
    // The latest valid peer transmit and when it was read, for the next sample.
    [[nodiscard]] std::optional<std::pair<std::uint64_t, std::uint64_t>> Echo() const noexcept { return echo_; }
    // An own sample waiting to be sent was replaced by a newer one (D57).
    void NoteReplaced() noexcept { ++replaced_; }
    // Closes the window once it lasted WindowUs, or at once when final.
    [[nodiscard]] std::optional<ClockWindow> Roll(std::uint64_t nowUs, bool final = false);

private:
    struct Sample {
        std::uint64_t atUs{}; // this clock, when the reply was read
        std::int64_t offsetUs{};
        std::int64_t delayUs{};
    };
    struct Point {
        std::uint64_t atUs{};
        std::int64_t offsetUs{};
        std::int64_t delayUs{};
    };
    std::uint64_t windowStartUs_{};
    std::uint64_t lastTransmitUs_{};
    std::deque<std::uint64_t> transmits_;
    std::optional<std::uint64_t> lastPeerTransmitUs_;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> echo_;
    std::optional<Sample> best_; // the window's minimum-delay sample
    std::optional<std::int64_t> delayMaxUs_;
    std::uint64_t samples_{}, invalid_{}, negativeDelay_{}, replaced_{};
    std::deque<Point> segment_; // one point per window with samples, since the last jump
};

} // namespace fps::pvp
