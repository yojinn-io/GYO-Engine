#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <cstdint>
#include <memory>

namespace Engine::Time {

enum class TimeErrorCode : std::uint16_t {
    // The operating system refused the wait primitive (kqueue, timer or event).
    WaitPrimitiveUnavailable = 1,
};

[[nodiscard]] constexpr const char* ToString(const TimeErrorCode code) noexcept {
    switch (code) {
    case TimeErrorCode::WaitPrimitiveUnavailable: return "WaitPrimitiveUnavailable";
    }
    return "Unknown";
}

using TimeError = Base::Error<TimeErrorCode>;
static_assert(Base::CodedError<TimeError>);

// The platform mechanism behind a Waiter. A plain waitable timer means the
// high-resolution one was not available on this Windows version.
enum class WaitBackend : std::uint8_t { Kqueue, HighResolutionWaitableTimer, WaitableTimer, ConditionVariable };

[[nodiscard]] constexpr const char* ToString(const WaitBackend backend) noexcept {
    switch (backend) {
    case WaitBackend::Kqueue:                      return "Kqueue";
    case WaitBackend::HighResolutionWaitableTimer: return "HighResolutionWaitableTimer";
    case WaitBackend::WaitableTimer:               return "WaitableTimer";
    case WaitBackend::ConditionVariable:           return "ConditionVariable";
    }
    return "Unknown";
}

enum class WakeReason : std::uint8_t { Deadline, Notified };

struct Wake final {
    WakeReason reason{WakeReason::Deadline};
    // The deadline asked for (TimePoint::max() for an unbounded wait).
    TimePoint planned{};
    // The time base read after waking.
    TimePoint woke{};
    // woke - planned when the deadline woke it, never negative; zero when notified.
    Duration late{};
};

// Waits until a deadline or until another thread notifies, whichever comes
// first. One thread waits; any thread may notify. A notification is latched:
// one sent before the wait starts is not lost, and several before one wake
// coalesce into that wake. A deadline wake is never early: woke >= planned.
// When the deadline has already passed, the wait reports Deadline and leaves a
// pending notification latched, so the next wait returns Notified at once.
//
// Every backend arms relative timeouts and re-reads the time base after each
// wake, so a spurious or early platform wake only waits again. On macOS a
// kqueue timer with NOTE_CRITICAL avoids the timer-coalescing leeway that std
// and SDL sleeps receive (about a quarter to a half of the interval); on
// Windows a high-resolution waitable timer does not depend on the process
// timer resolution.
class Waiter final {
public:
    [[nodiscard]] static Base::Result<Waiter, TimeError> Create();

    Waiter(Waiter&& other) noexcept;
    Waiter& operator=(Waiter&& other) noexcept;
    Waiter(const Waiter&) = delete;
    Waiter& operator=(const Waiter&) = delete;
    ~Waiter();

    // Any thread. Must not race the Waiter's destruction. On a moved-from Waiter
    // it does nothing (it cannot assert, being noexcept): notify the live one.
    void Notify() noexcept;
    // The single waiting thread.
    [[nodiscard]] Wake WaitUntil(TimePoint deadline);
    // The single waiting thread; returns only when notified.
    [[nodiscard]] Wake Wait();

    [[nodiscard]] WaitBackend Backend() const;

private:
    struct Impl;
    explicit Waiter(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Time
