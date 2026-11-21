#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

// The wake primitive behind engine/time's Waiter (docs/architecture/os.md,
// Wait): one thread blocks until another signals it or a relative timeout
// passes. What a wake means stays with engine/time (time.md): the latch, the
// single waiter, deadlines, the re-check of the time base after each wake and
// the backend names consumers log. engine/os never reads that latch.

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
    // platforms: a mutex, a condition variable and a pending flag, which cannot
    // fail. A primitive the OS refuses is IoFailed with the native error.
    [[nodiscard]] static Base::Result<WakeSignal, OsError> Create();

    WakeSignal(WakeSignal&& other) noexcept;
    WakeSignal& operator=(WakeSignal&& other) noexcept;
    WakeSignal(const WakeSignal&) = delete;
    WakeSignal& operator=(const WakeSignal&) = delete;
    ~WakeSignal();

    // Any thread. Wakes the block in progress. A signal sent while no block is
    // in progress stays pending and ends the next block at once; several
    // pending signals end one block. The pending signal is this primitive's own
    // state on every platform (the kqueue user event, the Windows auto-reset
    // event, the fallback's flag) and is consumed when a block returns. Must
    // not race the signal's destruction; on a moved-from signal it does nothing.
    void Signal() noexcept;

    // The single waiting thread. Returns when signalled, when the relative
    // timeout has passed (none: only a signal ends it), or spuriously; the
    // caller re-checks its own state and its time base. Not noexcept: the
    // fallback lets the standard library's std::system_error pass through
    // (Design Invariant 6).
    void Block(std::optional<std::chrono::nanoseconds> timeout);

    [[nodiscard]] WaitMechanism Mechanism() const;

private:
    struct Impl;
    explicit WakeSignal(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Os
