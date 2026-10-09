#include "engine/time/Waiter.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <string>

#if defined(__APPLE__)
#include <cerrno>
#include <cstring>
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

namespace Engine::Time {
namespace {

// Longest single platform wait; longer deadlines simply wait again.
constexpr auto MaximumBlock = std::chrono::hours(1);

// now - deadline, saturated: a deadline far in the past (for example
// TimePoint::min() as an "already due" sentinel) must not overflow.
Duration LateSince(const TimePoint deadline, const TimePoint now) noexcept {
    if (deadline.time_since_epoch() < now.time_since_epoch() - Duration::max()) return Duration::max();
    return now - deadline;
}

} // namespace

struct Waiter::Impl final {
    // The latch: set by Notify, consumed by the waiting thread.
    std::atomic<bool> notified{false};
    // Guards the single-waiter contract.
    std::atomic<bool> waiting{false};
    WaitBackend backend{WaitBackend::ConditionVariable};

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
    // user event. A timer left armed by an earlier notified wait only causes a
    // spurious wake, which the caller absorbs.
    void Block(const std::optional<Duration> timeout) noexcept {
        struct kevent change {};
        int changes = 0;
        if (timeout) {
            const auto nanoseconds = std::max<std::int64_t>(1,
                std::chrono::duration_cast<std::chrono::nanoseconds>(*timeout).count());
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
    void Block(const std::optional<Duration> timeout) noexcept {
        if (!timeout) {
            static_cast<void>(WaitForSingleObject(event, INFINITE));
            return;
        }
        // Relative due time in 100 ns units, rounded up; negative means relative.
        const auto hundreds = std::max<long long>(1,
            (std::chrono::duration_cast<std::chrono::nanoseconds>(*timeout).count() + 99) / 100);
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
    void Block(const std::optional<Duration> timeout) {
        std::unique_lock lock(mutex);
        if (notified.load(std::memory_order_acquire)) return;
        if (timeout) static_cast<void>(condition.wait_for(lock, *timeout));
        else condition.wait(lock);
    }
#endif
};

Base::Result<Waiter, TimeError> Waiter::Create() {
    auto impl = std::make_unique<Impl>();
#if defined(__APPLE__)
    impl->backend = WaitBackend::Kqueue;
    impl->queue = kqueue();
    if (impl->queue < 0)
        return Base::Err(TimeError::Make(TimeErrorCode::WaitPrimitiveUnavailable, "kqueue() failed",
            std::string(std::strerror(errno))));
    struct kevent change {};
    EV_SET(&change, Impl::UserIdent, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (kevent(impl->queue, &change, 1, nullptr, 0, nullptr) < 0)
        return Base::Err(TimeError::Make(TimeErrorCode::WaitPrimitiveUnavailable,
            "kevent(EVFILT_USER) registration failed", std::string(std::strerror(errno))));
#elif defined(_WIN32)
    impl->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!impl->event)
        return Base::Err(TimeError::Make(TimeErrorCode::WaitPrimitiveUnavailable, "CreateEventW failed",
            "GetLastError=" + std::to_string(GetLastError())));
    impl->timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    impl->backend = WaitBackend::HighResolutionWaitableTimer;
    if (!impl->timer) {
        impl->timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        impl->backend = WaitBackend::WaitableTimer;
    }
    if (!impl->timer)
        return Base::Err(TimeError::Make(TimeErrorCode::WaitPrimitiveUnavailable, "CreateWaitableTimerExW failed",
            "GetLastError=" + std::to_string(GetLastError())));
#else
    impl->backend = WaitBackend::ConditionVariable;
#endif
    return Waiter(std::move(impl));
}

Waiter::Waiter(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Waiter::Waiter(Waiter&& other) noexcept = default;
Waiter& Waiter::operator=(Waiter&& other) noexcept = default;
Waiter::~Waiter() = default;

void Waiter::Notify() noexcept {
    if (!impl_) return;
    impl_->notified.store(true, std::memory_order_release);
    impl_->Signal();
}

Wake Waiter::WaitUntil(const TimePoint deadline) {
    GYO_ASSERT(impl_ != nullptr);
    const bool alreadyWaiting = impl_->waiting.exchange(true, std::memory_order_acq_rel);
    GYO_ASSERT(!alreadyWaiting);
    struct Leave final {
        std::atomic<bool>& waiting;
        ~Leave() { waiting.store(false, std::memory_order_release); }
    } leave{impl_->waiting};
    const bool bounded = deadline != TimePoint::max();
    for (;;) {
        // A passed deadline is reported first and leaves a pending notification
        // latched for the next wait, so frequent notifications neither postpone
        // a deadline nor hide how late it woke.
        const auto now = Now();
        if (bounded && now >= deadline) return {WakeReason::Deadline, deadline, now, LateSince(deadline, now)};
        if (impl_->notified.exchange(false, std::memory_order_acq_rel))
            return {WakeReason::Notified, deadline, Now(), Duration::zero()};
        if (bounded) impl_->Block(std::min<Duration>(deadline - now, MaximumBlock));
        else impl_->Block(std::nullopt);
    }
}

Wake Waiter::Wait() { return WaitUntil(TimePoint::max()); }

WaitBackend Waiter::Backend() const {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->backend;
}

} // namespace Engine::Time
