#pragma once

#include "RetroFPS/Pvp/Movement.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace fps::pvp {

// The input token bucket (Movement.hpp, InputSendBurst and InputSendDrainRate)
// as pure functions of send periods (1 / InputSendRate), so the network
// worker's send path, its wake deadline and the tests' simulation model share
// one rule (v7.1 token-tests, C12). Clocks stay with the callers.

// Tokens after elapsedPeriods since the last refill, never above a full bucket.
[[nodiscard]] constexpr double RefilledInputTokens(double tokens, double elapsedPeriods) noexcept {
    return (std::min)(InputSendBurst, tokens + elapsedPeriods);
}

// A window carrying a command never sent: another epoch or life than the last
// send, or a newer last sequence. It goes as soon as one token exists.
[[nodiscard]] constexpr bool FreshInputWindow(bool haveSent, std::uint64_t epoch, std::uint64_t sentEpoch,
                                              std::uint64_t life, std::uint64_t sentLife, bool empty,
                                              std::uint64_t lastSequence, std::uint64_t sentThrough) noexcept {
    return !haveSent || epoch != sentEpoch || life != sentLife || (!empty && lastSequence > sentThrough);
}

// Whether the window goes now: a fresh one with a token, an unchanged one at
// its resend deadline with a full bucket (a resend always leaves one token).
[[nodiscard]] constexpr bool InputWindowSendsNow(bool fresh, double tokens, bool resendDue) noexcept {
    return (fresh && tokens >= 1) || (resendDue && tokens >= InputSendBurst);
}

// The tokens a send costs. One; a fresh window that waited for its token
// (waitedPeriods since it first found less than one) pays less in proportion
// to the wait, without the debt never drains. Only the wait until the token
// existed counts (tokens - 1 accrued past it is a late wake, not a wait), never
// past a full bucket, and at most 1 - InputSendRate / InputSendDrainRate.
[[nodiscard]] constexpr double InputSendCharge(bool fresh, std::optional<double> waitedPeriods, double tokens) noexcept {
    if (!fresh || !waitedPeriods) return 1;
    const double waited = *waitedPeriods - (tokens - 1.0);
    return 1 - (std::min)(1.0 - InputSendRate / InputSendDrainRate,
                          (std::max)(0.0, (std::min)(waited, InputSendBurst - tokens)));
}

// Send periods until the window may go (0: now): one token for a fresh
// window, a full bucket for a resend (which also waits for its deadline).
[[nodiscard]] constexpr double InputSendWaitPeriods(bool fresh, double tokens) noexcept {
    return (std::max)(0.0, (fresh ? 1.0 : InputSendBurst) - tokens);
}

} // namespace fps::pvp
