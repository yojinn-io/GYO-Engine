#include "RetroFPS/Pvp/ClockSync.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace fps::pvp {
namespace {
std::int64_t Signed(std::uint64_t value) { return static_cast<std::int64_t>(value); } // below MaxTimeUs: exact

std::string Optional(const std::optional<std::int64_t>& value) {
    return value ? std::to_string(*value) : std::string("na");
}
} // namespace

std::string ClockWindowLine(const ClockWindow& window) {
    std::string drift = "na";
    if (window.driftPpm) {
        char text[32];
        std::snprintf(text, sizeof text, "%.3f", *window.driftPpm);
        drift = text;
    }
    return "clock sync window_ms=" + std::to_string(window.windowUs / 1000) + " samples=" + std::to_string(window.samples) +
           " missing=" + (window.samples ? "0" : "1") + " invalid=" + std::to_string(window.invalid) +
           " negative_delay=" + std::to_string(window.negativeDelay) + " replaced=" + std::to_string(window.replaced) +
           " offset_us=" + Optional(window.offsetUs) + " delay_min_us=" + Optional(window.delayMinUs) +
           " delay_max_us=" + Optional(window.delayMaxUs) + " drift_ppm=" + drift + " jump=" + (window.jump ? "1" : "0") +
           " final=" + (window.final ? "1" : "0");
}

std::uint64_t ClockSync::Stamp(std::uint64_t nowUs) {
    lastTransmitUs_ = std::max(nowUs, lastTransmitUs_ + 1);
    transmits_.push_back(lastTransmitUs_);
    if (transmits_.size() > RememberedTransmits) transmits_.pop_front();
    return lastTransmitUs_;
}

std::optional<std::pair<std::uint64_t, std::uint64_t>> ClockSync::Observe(const ClockStamp& peer, std::uint64_t receivedUs) {
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
    const auto delay = (Signed(t4) - Signed(t1)) - (Signed(t3) - Signed(t2));
    if (delay < 0) {
        ++negativeDelay_;
        return echo_;
    }
    const auto offset = ((Signed(t2) - Signed(t1)) + (Signed(t3) - Signed(t4))) / 2;
    ++samples_;
    if (!best_ || delay < best_->delayUs) best_ = Sample{t4, offset, delay};
    delayMaxUs_ = std::max(delayMaxUs_.value_or(delay), delay);
    return echo_;
}

std::optional<ClockWindow> ClockSync::Roll(std::uint64_t nowUs, bool final) {
    const auto elapsed = nowUs > windowStartUs_ ? nowUs - windowStartUs_ : 0;
    if (!final && elapsed < WindowUs) return std::nullopt;
    ClockWindow window{.windowUs = elapsed, .samples = samples_, .invalid = invalid_, .negativeDelay = negativeDelay_,
                       .replaced = replaced_, .final = final};
    if (best_) {
        window.offsetUs = best_->offsetUs;
        window.delayMinUs = best_->delayUs;
        window.delayMaxUs = delayMaxUs_;
        if (!segment_.empty()) {
            const auto& previous = segment_.back();
            const auto span = best_->atUs > previous.atUs ? best_->atUs - previous.atUs : 0;
            const auto allowance = std::max(JumpFloorUs, Signed(span / (1'000'000 / JumpAllowancePpm)));
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
