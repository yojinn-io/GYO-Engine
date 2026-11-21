// The wake primitive on Windows: an auto-reset event and a high-resolution
// waitable timer, else a plain one. Moved from engine/core/time's Waiter.cpp
// (v8-arch-gyo2-os-wait); the platform calls are the old ones line for line.
#include "engine/os/WakeSignal.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <utility>

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

namespace Engine::Os {

struct WakeSignal::Impl final {
    WaitMechanism mechanism{WaitMechanism::HighResolutionWaitableTimer};
    HANDLE event{nullptr};
    HANDLE timer{nullptr};

    ~Impl() {
        if (timer) CloseHandle(timer);
        if (event) CloseHandle(event);
    }
    // The event is auto-reset, so it keeps one pending signal and clears it
    // when a wait takes it.
    void Signal() noexcept { static_cast<void>(SetEvent(event)); }
    void Block(const std::optional<std::chrono::nanoseconds> timeout) noexcept {
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
};

Base::Result<WakeSignal, OsError> WakeSignal::Create() {
    auto impl = std::make_unique<Impl>();
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
    return WakeSignal(std::move(impl));
}

WakeSignal::WakeSignal(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
WakeSignal::WakeSignal(WakeSignal&& other) noexcept = default;
WakeSignal& WakeSignal::operator=(WakeSignal&& other) noexcept = default;
WakeSignal::~WakeSignal() = default;

void WakeSignal::Signal() noexcept {
    if (impl_) impl_->Signal();
}

void WakeSignal::Block(const std::optional<std::chrono::nanoseconds> timeout) {
    GYO_ASSERT(impl_ != nullptr);
    impl_->Block(timeout);
}

WaitMechanism WakeSignal::Mechanism() const {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->mechanism;
}

} // namespace Engine::Os
