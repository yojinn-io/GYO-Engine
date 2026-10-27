// The wake primitive behind engine/time's Waiter. Moved from engine/time's
// Waiter.cpp (os-wait-backend, D71) as one source with per-OS branches, so the
// move reads line by line against the old code (os-subsystem open decision
// 11 (c)). The `#else` branch is the generic fallback, not a Linux one.
#include "engine/os/WakeSignal.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <utility>

#if defined(__APPLE__)
#include <cerrno>
#include <sys/event.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <condition_variable>
#include <mutex>
#endif

namespace Engine::Os {

struct WakeSignal::Impl final {
    WaitMechanism mechanism{WaitMechanism::ConditionVariable};

#if defined(__APPLE__)
    static constexpr uintptr_t UserIdent = 1;
    static constexpr uintptr_t TimerIdent = 2;
    int queue{-1};

    ~Impl() {
        if (queue >= 0) close(queue);
    }
    void Signal() noexcept {
        struct kevent change {};
        EV_SET(&change, UserIdent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        static_cast<void>(kevent(queue, &change, 1, nullptr, 0, nullptr));
    }
    // One kevent call arms the one-shot timer (if any) and waits for it or the
    // user event. A timer left armed by an earlier signalled block only causes
    // a spurious wake, which the caller absorbs.
    void Block(const std::atomic<bool>&, const std::optional<std::chrono::nanoseconds> timeout) noexcept {
        struct kevent change {};
        int changes = 0;
        if (timeout) {
            const auto nanoseconds = std::max<std::int64_t>(1, timeout->count());
            EV_SET(&change, TimerIdent, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_NSECONDS | NOTE_CRITICAL,
                static_cast<intptr_t>(nanoseconds), nullptr);
            changes = 1;
        }
        struct kevent event {};
        static_cast<void>(kevent(queue, &change, changes, &event, 1, nullptr));
    }
#elif defined(_WIN32)
    HANDLE event{nullptr};
    HANDLE timer{nullptr};

    ~Impl() {
        if (timer) CloseHandle(timer);
        if (event) CloseHandle(event);
    }
    void Signal() noexcept { static_cast<void>(SetEvent(event)); }
    void Block(const std::atomic<bool>&, const std::optional<std::chrono::nanoseconds> timeout) noexcept {
        if (!timeout) {
            static_cast<void>(WaitForSingleObject(event, INFINITE));
            return;
        }
        // Relative due time in 100 ns units, rounded up; negative means relative.
        const auto hundreds = std::max<long long>(1, (timeout->count() + 99) / 100);
        LARGE_INTEGER due{};
        due.QuadPart = -hundreds;
        if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            const auto milliseconds = static_cast<DWORD>(std::max<long long>(1, (hundreds + 9999) / 10000));
            static_cast<void>(WaitForSingleObject(event, milliseconds));
            return;
        }
        const HANDLE handles[2] = {event, timer};
        static_cast<void>(WaitForMultipleObjects(2, handles, FALSE, INFINITE));
    }
#else
    std::mutex mutex;
    std::condition_variable condition;

    void Signal() noexcept {
        { const std::lock_guard lock(mutex); }
        condition.notify_one();
    }
    void Block(const std::atomic<bool>& latch, const std::optional<std::chrono::nanoseconds> timeout) {
        std::unique_lock lock(mutex);
        if (latch.load(std::memory_order_acquire)) return;
        if (timeout) static_cast<void>(condition.wait_for(lock, *timeout));
        else condition.wait(lock);
    }
#endif
};

Base::Result<WakeSignal, OsError> WakeSignal::Create() {
    auto impl = std::make_unique<Impl>();
#if defined(__APPLE__)
    impl->mechanism = WaitMechanism::Kqueue;
    impl->queue = kqueue();
    if (impl->queue < 0) {
        const int error = errno;
        return Base::Err(MakeOsError(OsErrorCode::IoFailed, "kqueue() failed", {NativeErrorDomain::Errno, error}));
    }
    struct kevent change {};
    EV_SET(&change, Impl::UserIdent, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (kevent(impl->queue, &change, 1, nullptr, 0, nullptr) < 0) {
        const int error = errno;
        return Base::Err(MakeOsError(OsErrorCode::IoFailed, "kevent(EVFILT_USER) registration failed",
            {NativeErrorDomain::Errno, error}));
    }
#elif defined(_WIN32)
    impl->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!impl->event) {
        const auto error = static_cast<std::int64_t>(GetLastError());
        return Base::Err(MakeOsError(OsErrorCode::IoFailed, "CreateEventW failed", {NativeErrorDomain::Win32, error}));
    }
    impl->timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    impl->mechanism = WaitMechanism::HighResolutionWaitableTimer;
    if (!impl->timer) {
        impl->timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        impl->mechanism = WaitMechanism::WaitableTimer;
    }
    if (!impl->timer) {
        const auto error = static_cast<std::int64_t>(GetLastError());
        return Base::Err(MakeOsError(OsErrorCode::IoFailed, "CreateWaitableTimerExW failed",
            {NativeErrorDomain::Win32, error}));
    }
#else
    impl->mechanism = WaitMechanism::ConditionVariable;
#endif
    return WakeSignal(std::move(impl));
}

WakeSignal::WakeSignal(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
WakeSignal::WakeSignal(WakeSignal&& other) noexcept = default;
WakeSignal& WakeSignal::operator=(WakeSignal&& other) noexcept = default;
WakeSignal::~WakeSignal() = default;

void WakeSignal::Signal() noexcept {
    if (impl_) impl_->Signal();
}

void WakeSignal::Block(const std::atomic<bool>& latch, const std::optional<std::chrono::nanoseconds> timeout) {
    GYO_ASSERT(impl_ != nullptr);
    impl_->Block(latch, timeout);
}

WaitMechanism WakeSignal::Mechanism() const {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->mechanism;
}

} // namespace Engine::Os
