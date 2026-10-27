#include "engine/time/Waiter.hpp"

#include "engine/base/Assert.hpp"
#include "engine/os/WakeSignal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <utility>

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

// The platform wake primitive is engine/os's (os-wait-backend, D71); what a wake
// means stays here: the latch, the single waiter, the deadline loop and the
// re-check of the time base after every platform wake.
struct Waiter::Impl final {
    explicit Impl(Os::WakeSignal wakeSignal) noexcept : primitive(std::move(wakeSignal)) {}

    // The latch: set by Notify, consumed by the waiting thread. It is Time's
    // state; engine/os reads it through the reference Block takes and keeps no
    // copy (time.md Invariant 8).
    std::atomic<bool> notified{false};
    // Guards the single-waiter contract.
    std::atomic<bool> waiting{false};
    Os::WakeSignal primitive;
};

namespace {

// The engine/os mechanism as this Waiter's backend: the same values in the same
// order, so the names consumers log stay (Invariant 7).
WaitBackend ToBackend(const Os::WaitMechanism mechanism) noexcept {
    static_assert(static_cast<int>(Os::WaitMechanism::Kqueue) == static_cast<int>(WaitBackend::Kqueue));
    static_assert(static_cast<int>(Os::WaitMechanism::HighResolutionWaitableTimer) ==
                  static_cast<int>(WaitBackend::HighResolutionWaitableTimer));
    static_assert(static_cast<int>(Os::WaitMechanism::WaitableTimer) == static_cast<int>(WaitBackend::WaitableTimer));
    static_assert(static_cast<int>(Os::WaitMechanism::ConditionVariable) ==
                  static_cast<int>(WaitBackend::ConditionVariable));
    return static_cast<WaitBackend>(mechanism);
}

} // namespace

Base::Result<Waiter, TimeError> Waiter::Create() {
    auto primitive = Os::WakeSignal::Create();
    if (!primitive)
        return Base::Err(TimeError::Make(TimeErrorCode::WaitPrimitiveUnavailable, primitive.error().message,
            Base::CauseDetail(primitive.error())));
    return Waiter(std::make_unique<Impl>(std::move(primitive).value()));
}

Waiter::Waiter(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Waiter::Waiter(Waiter&& other) noexcept = default;
Waiter& Waiter::operator=(Waiter&& other) noexcept = default;
Waiter::~Waiter() = default;

void Waiter::Notify() noexcept {
    if (!impl_) return;
    impl_->notified.store(true, std::memory_order_release);
    impl_->primitive.Signal();
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
        if (bounded) impl_->primitive.Block(impl_->notified, std::min<Duration>(deadline - now, MaximumBlock));
        else impl_->primitive.Block(impl_->notified, std::nullopt);
    }
}

Wake Waiter::Wait() { return WaitUntil(TimePoint::max()); }

WaitBackend Waiter::Backend() const {
    GYO_ASSERT(impl_ != nullptr);
    return ToBackend(impl_->primitive.Mechanism());
}

} // namespace Engine::Time
