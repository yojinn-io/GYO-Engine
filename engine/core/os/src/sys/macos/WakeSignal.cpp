// The wake primitive on macOS: a kqueue with a user event and a one-shot timer.
// Moved from engine/core/time's Waiter.cpp (v8-arch-gyo2-os-wait); the platform
// calls are the old ones line for line.
#include "engine/os/WakeSignal.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cerrno>
#include <utility>

#include <sys/event.h>
#include <unistd.h>

namespace GYO::Engine::OS {

struct WakeSignal::Impl final {
    static constexpr uintptr_t UserIdent = 1;
    static constexpr uintptr_t TimerIdent = 2;
    int queue{-1};

    ~Impl() {
        if (queue >= 0) close(queue);
    }
    // The user event is registered with EV_CLEAR, so the kqueue keeps one
    // pending signal and clears it when a block takes it.
    void Signal() noexcept {
        struct kevent change {};
        EV_SET(&change, UserIdent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        static_cast<void>(kevent(queue, &change, 1, nullptr, 0, nullptr));
    }
    // One kevent call arms the one-shot timer (if any) and waits for it or the
    // user event. A timer left armed by an earlier signalled block only causes
    // a spurious wake, which the caller absorbs.
    void Block(const std::optional<std::chrono::nanoseconds> timeout) noexcept {
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
};

Base::Result<WakeSignal, OSError> WakeSignal::Create() {
    auto impl = std::make_unique<Impl>();
    impl->queue = kqueue();
    if (impl->queue < 0) {
        const int error = errno;
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "kqueue() failed", {NativeErrorDomain::Errno, error}));
    }
    struct kevent change {};
    EV_SET(&change, Impl::UserIdent, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (kevent(impl->queue, &change, 1, nullptr, 0, nullptr) < 0) {
        const int error = errno;
        return Base::Err(MakeOSError(OSErrorCode::IOFailed, "kevent(EVFILT_USER) registration failed",
            {NativeErrorDomain::Errno, error}));
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
    return WaitMechanism::Kqueue;
}

} // namespace GYO::Engine::OS
