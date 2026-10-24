#pragma once
// Diagnostics only (pv7 contract §2, §3): four-timestamp clock sampling with a
// peer, header-only for the two I/O libraries that own it (IpcHost,
// ClientConnection);
// no value it computes may reach a decision (D32, guarded by
// tests/object_fps_pvp/test_diagnostic_isolation.py). Times are microseconds of
// one monotonic clock; the caller passes them, so tests use any clock.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
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

// Why a window closed (the reason= key, log-contract §1.4): it lasted
// ClockSync::WindowUs, the link closed (final=1), or the Client's connection
// resumed from an interruption and the time echo samples again (v7.1
// outage-resync; the drift regression carries on).
enum class ClockWindowEnd { Window, Final, Resume };

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
    ClockWindowEnd end{ClockWindowEnd::Window};
};

// "clock sync window_ms=... final=0 reason=window": the line both ends log (pv7 §3; the body
// of the clock_sync family in docs/object_fps_pvp/log-contract.zh-Hant.md). A
// drift that is not finite is written as na.
[[nodiscard]] inline std::string ClockWindowLine(const ClockWindow& window);
// The Client's line: "[ObjectFPS/PvP] clock sync player=<id> window_ms=...".
[[nodiscard]] inline std::string ClientClockSyncLine(std::uint64_t player, const ClockWindow& window);

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
    // Closes the window once it lasted WindowUs, or at once for any other end.
    [[nodiscard]] std::optional<ClockWindow> Roll(std::uint64_t nowUs, ClockWindowEnd end = ClockWindowEnd::Window);

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

// The Client worker's clock sync of one joined session. The player id is the
// worker's own copy of its session identity, set at the join and cleared when
// the session ends, so the last window (written when the transport closes,
// after the game thread already cleared its state) still names the player.
class ClientClockSession {
public:
    void Begin(std::uint64_t player, std::uint64_t nowUs) {
        clock_ = ClockSync(nowUs);
        player_ = player;
        active_ = true;
    }
    [[nodiscard]] ClockSync& Clock() noexcept { return clock_; }
    // The line of a window that finished by nowUs, if one did.
    [[nodiscard]] std::optional<std::string> Roll(std::uint64_t nowUs) {
        const auto window = clock_.Roll(nowUs);
        return window ? std::optional<std::string>(ClientClockSyncLine(player_, *window)) : std::nullopt;
    }
    // The final window's line of an active session, which this ends.
    [[nodiscard]] std::optional<std::string> End(std::uint64_t nowUs) {
        if (!active_) return std::nullopt;
        active_ = false;
        const auto window = clock_.Roll(nowUs, ClockWindowEnd::Final);
        std::optional<std::string> line;
        if (window) line = ClientClockSyncLine(player_, *window);
        player_ = 0;
        return line;
    }
    // The connection resumed from an interruption: the window ends early and a
    // new one starts (network-architecture §4, recovery step 1).
    [[nodiscard]] std::optional<std::string> Resume(std::uint64_t nowUs) {
        if (!active_) return std::nullopt;
        const auto window = clock_.Roll(nowUs, ClockWindowEnd::Resume);
        return window ? std::optional<std::string>(ClientClockSyncLine(player_, *window)) : std::nullopt;
    }

private:
    ClockSync clock_;
    std::uint64_t player_{};
    bool active_{};
};

// Implementation (inline: one header, two I/O libraries).

namespace detail {
inline std::int64_t Signed(std::uint64_t value) { return static_cast<std::int64_t>(value); } // below MaxTimeUs: exact

inline std::string Optional(const std::optional<std::int64_t>& value) {
    return value ? std::to_string(*value) : std::string("na");
}
} // namespace detail

inline std::string ClockWindowLine(const ClockWindow& window) {
    std::string drift = "na";
    if (window.driftPpm && std::isfinite(*window.driftPpm)) {
        char text[32];
        std::snprintf(text, sizeof text, "%.3f", *window.driftPpm);
        drift = text;
    }
    return "clock sync window_ms=" + std::to_string(window.windowUs / 1000) + " samples=" + std::to_string(window.samples) +
           " missing=" + (window.samples ? "0" : "1") + " invalid=" + std::to_string(window.invalid) +
           " negative_delay=" + std::to_string(window.negativeDelay) + " replaced=" + std::to_string(window.replaced) +
           " offset_us=" + detail::Optional(window.offsetUs) + " delay_min_us=" + detail::Optional(window.delayMinUs) +
           " delay_max_us=" + detail::Optional(window.delayMaxUs) + " drift_ppm=" + drift + " jump=" + (window.jump ? "1" : "0") +
           " final=" + (window.end == ClockWindowEnd::Final ? "1" : "0") + " reason=" +
           (window.end == ClockWindowEnd::Final ? "final" : window.end == ClockWindowEnd::Resume ? "resume" : "window");
}

inline std::string ClientClockSyncLine(std::uint64_t player, const ClockWindow& window) {
    static constexpr std::size_t Event = sizeof("clock sync ") - 1;
    return "[ObjectFPS/PvP] clock sync player=" + std::to_string(player) + " " + ClockWindowLine(window).substr(Event);
}

inline std::uint64_t ClockSync::Stamp(std::uint64_t nowUs) {
    lastTransmitUs_ = std::max(nowUs, lastTransmitUs_ + 1);
    transmits_.push_back(lastTransmitUs_);
    if (transmits_.size() > RememberedTransmits) transmits_.pop_front();
    return lastTransmitUs_;
}

inline std::optional<std::pair<std::uint64_t, std::uint64_t>> ClockSync::Observe(const ClockStamp& peer, std::uint64_t receivedUs) {
    // The peer's own stream: at least 1 and strictly increasing.
    if (peer.transmitUs < 1 || peer.transmitUs >= MaxTimeUs || receivedUs >= MaxTimeUs ||
        (lastPeerTransmitUs_ && peer.transmitUs <= *lastPeerTransmitUs_)) {
        ++invalid_;
        return std::nullopt;
    }
    lastPeerTransmitUs_ = peer.transmitUs;
    echo_ = std::pair{peer.transmitUs, receivedUs};
    if (peer.originUs.has_value() != peer.receiveUs.has_value()) {
        ++invalid_;
        return echo_;
    }
    if (!peer.originUs) return echo_; // nothing of ours to measure against yet
    const auto t1 = *peer.originUs, t2 = *peer.receiveUs, t3 = peer.transmitUs, t4 = receivedUs;
    if (t2 > t3 || std::find(transmits_.begin(), transmits_.end(), t1) == transmits_.end()) {
        ++invalid_;
        return echo_;
    }
    // t1 is our own transmit and t2 <= t3 < MaxTimeUs: all four are exact in int64.
    const auto delay = (detail::Signed(t4) - detail::Signed(t1)) - (detail::Signed(t3) - detail::Signed(t2));
    if (delay < 0) {
        ++negativeDelay_;
        return echo_;
    }
    const auto offset = ((detail::Signed(t2) - detail::Signed(t1)) + (detail::Signed(t3) - detail::Signed(t4))) / 2;
    ++samples_;
    if (!best_ || delay < best_->delayUs) best_ = Sample{t4, offset, delay};
    delayMaxUs_ = std::max(delayMaxUs_.value_or(delay), delay);
    return echo_;
}

inline std::optional<ClockWindow> ClockSync::Roll(std::uint64_t nowUs, ClockWindowEnd end) {
    const auto elapsed = nowUs > windowStartUs_ ? nowUs - windowStartUs_ : 0;
    if (end == ClockWindowEnd::Window && elapsed < WindowUs) return std::nullopt;
    ClockWindow window{.windowUs = elapsed, .samples = samples_, .invalid = invalid_, .negativeDelay = negativeDelay_,
                       .replaced = replaced_, .end = end};
    if (best_) {
        window.offsetUs = best_->offsetUs;
        window.delayMinUs = best_->delayUs;
        window.delayMaxUs = delayMaxUs_;
        if (!segment_.empty()) {
            const auto& previous = segment_.back();
            const auto span = best_->atUs > previous.atUs ? best_->atUs - previous.atUs : 0;
            const auto allowance = std::max(JumpFloorUs, detail::Signed(span / (1'000'000 / JumpAllowancePpm)));
            const auto uncertainty = (previous.delayUs + best_->delayUs) / 2;
            if (std::llabs(best_->offsetUs - previous.offsetUs) > allowance + uncertainty) {
                window.jump = true;
                segment_.clear();
            }
        }
        segment_.push_back({best_->atUs, best_->offsetUs, best_->delayUs});
        if (segment_.size() > DriftWindows) segment_.pop_front();
    }
    if (segment_.size() >= 2) {
        // Least squares of offset over time; x from the segment's first point.
        double meanX = 0, meanY = 0;
        for (const auto& point : segment_) {
            meanX += static_cast<double>(point.atUs - segment_.front().atUs);
            meanY += static_cast<double>(point.offsetUs);
        }
        meanX /= static_cast<double>(segment_.size());
        meanY /= static_cast<double>(segment_.size());
        double covariance = 0, variance = 0;
        for (const auto& point : segment_) {
            const auto x = static_cast<double>(point.atUs - segment_.front().atUs) - meanX;
            covariance += x * (static_cast<double>(point.offsetUs) - meanY);
            variance += x * x;
        }
        if (variance > 0) window.driftPpm = covariance / variance * 1e6;
    }
    windowStartUs_ = nowUs;
    best_.reset();
    delayMaxUs_.reset();
    samples_ = invalid_ = negativeDelay_ = replaced_ = 0;
    return window;
}

} // namespace fps::pvp
