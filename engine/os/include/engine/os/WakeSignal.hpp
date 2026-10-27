#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

// The wake primitive behind engine/time's Waiter (docs/architecture/os.md,
// Wait): one thread blocks until another signals it or a relative timeout
// passes. What a wake means stays with engine/time (time.md): the latch, the
// single waiter, deadlines, the re-check of the time base after each wake and
// the backend names consumers log.

namespace Engine::Os {

// The platform mechanism. engine/time reports it as its WaitBackend, value for
// value. A plain waitable timer means the high-resolution one was not available
// on this Windows version.
enum class WaitMechanism : std::uint8_t { Kqueue, HighResolutionWaitableTimer, WaitableTimer, ConditionVariable };

class WakeSignal final {
public:
    // macOS: a kqueue with a user event, and a one-shot timer armed per block
    // with NOTE_CRITICAL (no timer-coalescing leeway). Windows: an auto-reset
    // event and a high-resolution waitable timer, else a plain one. Other
    // platforms: a mutex and a condition variable, which cannot fail. A
    // primitive the OS refuses is IoFailed with the native error.
    [[nodiscard]] static Base::Result<WakeSignal, OsError> Create();

    WakeSignal(WakeSignal&& other) noexcept;
    WakeSignal& operator=(WakeSignal&& other) noexcept;
    WakeSignal(const WakeSignal&) = delete;
    WakeSignal& operator=(const WakeSignal&) = delete;
    ~WakeSignal();

    // Any thread. Wakes the block in progress, or the next one if none is.
    // The caller sets its latch before signalling. Must not race the signal's
    // destruction; on a moved-from signal it does nothing.
    void Signal() noexcept;

    // The single waiting thread. Returns when signalled, when the relative
    // timeout has passed (none: only a signal ends it), or spuriously; the
    // caller re-checks its latch and its time base. The latch is the caller's
    // state (Design Invariant 8): the condition-variable fallback reads it
    // under its mutex, so a signal sent between the caller's check and the
    // block is not lost; the other mechanisms keep a signal pending by
    // themselves and do not read it. Not noexcept: the fallback lets the
    // standard library's std::system_error pass through (Design Invariant 6).
    void Block(const std::atomic<bool>& latch, std::optional<std::chrono::nanoseconds> timeout);

    [[nodiscard]] WaitMechanism Mechanism() const;

private:
    struct Impl;
    explicit WakeSignal(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Os
