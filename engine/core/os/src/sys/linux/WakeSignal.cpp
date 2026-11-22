// The wake primitive on Linux, and on any platform CMake selects neither the
// macOS nor the Windows source for: a mutex, a condition variable and a
// pending flag, standard library only. The flag is this primitive's own state
// (os.md Invariant 8): engine/os never reads the caller's latch. A Linux-only
// backend (futex, eventfd, timerfd) would replace this file.
#include "engine/os/WakeSignal.hpp"

#include "engine/base/Assert.hpp"

#include <condition_variable>
#include <mutex>
#include <utility>

namespace GYO::Engine::OS {

struct WakeSignal::Impl final {
    std::mutex mutex;
    std::condition_variable condition;
    bool pending{false};

    // The flag is set under the mutex, so a block that checked it and is about
    // to wait cannot miss the notification.
    void Signal() noexcept {
        {
            const std::lock_guard lock(mutex);
            pending = true;
        }
        condition.notify_one();
    }
    // Every return consumes the pending signal under the mutex, as kqueue's
    // EV_CLEAR and the Windows auto-reset event do: a signal ends one block.
    void Block(const std::optional<std::chrono::nanoseconds> timeout) {
        std::unique_lock lock(mutex);
        if (!pending) {
            if (timeout) static_cast<void>(condition.wait_for(lock, *timeout));
            else condition.wait(lock);
        }
        pending = false;
    }
};

Base::Result<WakeSignal, OsError> WakeSignal::Create() {
    return WakeSignal(std::make_unique<Impl>());
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
    return WaitMechanism::ConditionVariable;
}

} // namespace GYO::Engine::OS
