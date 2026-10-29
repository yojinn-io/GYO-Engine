#include "engine/threads/RoleThread.hpp"

#include "engine/base/Assert.hpp"
#include "engine/os/Thread.hpp"

#include <atomic>
#include <exception>
#include <future>
#include <system_error>
#include <utility>

namespace Engine::Threads {

struct RoleContext::State final {
    State(RoleThreadOptions roleOptions, Time::Waiter roleWaiter)
        : options(std::move(roleOptions)), waiter(std::move(roleWaiter)) {}
    RoleThreadOptions options;
    Time::Waiter waiter;
    std::atomic<bool> stop{false};
    bool nameApplied{};
    bool priorityApplied{};
};

namespace {

// The role's priority as engine/os's hint: the same three levels, value for
// value, so a cast carries any value through as the old per-OS code read it.
static_assert(static_cast<int>(ThreadPriority::Background) == static_cast<int>(Os::Thread::PriorityHint::Background));
static_assert(static_cast<int>(ThreadPriority::Normal) == static_cast<int>(Os::Thread::PriorityHint::Normal));
static_assert(static_cast<int>(ThreadPriority::Interactive) == static_cast<int>(Os::Thread::PriorityHint::Interactive));
Os::Thread::PriorityHint Hint(const ThreadPriority priority) noexcept {
    return static_cast<Os::Thread::PriorityHint>(priority);
}

} // namespace

bool RoleContext::StopRequested() const noexcept { return state_.stop.load(std::memory_order_acquire); }
Time::Wake RoleContext::WaitUntil(const Time::TimePoint deadline) { return state_.waiter.WaitUntil(deadline); }
Time::Wake RoleContext::Wait() { return state_.waiter.Wait(); }

Base::Result<RoleThread, ThreadError> RoleThread::Start(RoleThreadOptions options, Body body) {
    GYO_ASSERT(!options.name.empty());
    GYO_ASSERT(static_cast<bool>(body));
    auto waiter = Time::Waiter::Create();
    if (!waiter)
        return Base::Err(ThreadError::Make(ThreadErrorCode::StartFailed,
            "Role thread " + options.name + " has no wait primitive", Base::CauseDetail(waiter.error())));
    auto state = std::make_unique<RoleContext::State>(std::move(options), std::move(*waiter));
    std::promise<void> started;
    auto ready = started.get_future();
    std::thread thread;
    try {
        thread = std::thread([state = state.get(), body = std::move(body), started = std::move(started)]() mutable {
            state->nameApplied = Os::Thread::SetCurrentName(state->options.name);
            state->priorityApplied = Os::Thread::SetCurrentPriority(Hint(state->options.priority));
            started.set_value();
            RoleContext context(*state);
            body(context);
        });
    } catch (const std::system_error& error) {
        return Base::Err(ThreadError::Make(ThreadErrorCode::StartFailed,
            "Role thread " + state->options.name + " could not start", error.what()));
    }
    // The promise is fulfilled before the body runs, so this never waits on the body.
    ready.wait();
    return RoleThread(std::move(state), std::move(thread));
}

RoleThread::RoleThread(std::unique_ptr<RoleContext::State> state, std::thread thread) noexcept
    : state_(std::move(state)), thread_(std::move(thread)) {}

RoleThread::RoleThread(RoleThread&& other) noexcept = default;

RoleThread::~RoleThread() {
    if (!thread_.joinable()) return;
    RequestStop();
    // No GYO_ASSERT in a destructor: destroying a role from its own thread makes
    // join throw resource_deadlock_would_occur, which terminates here.
    thread_.join();
}

void RoleThread::Notify() noexcept {
    if (state_) state_->waiter.Notify();
}

void RoleThread::RequestStop() noexcept {
    if (!state_) return;
    state_->stop.store(true, std::memory_order_release);
    state_->waiter.Notify();
}

void RoleThread::Join() {
    if (!thread_.joinable()) return;
    GYO_ASSERT(thread_.get_id() != std::this_thread::get_id());
    thread_.join();
}

const std::string& RoleThread::Name() const {
    GYO_ASSERT(state_ != nullptr);
    return state_->options.name;
}

bool RoleThread::NameApplied() const {
    GYO_ASSERT(state_ != nullptr);
    return state_->nameApplied;
}

bool RoleThread::PriorityApplied() const {
    GYO_ASSERT(state_ != nullptr);
    return state_->priorityApplied;
}

Time::WaitBackend RoleThread::WaitBackend() const {
    GYO_ASSERT(state_ != nullptr);
    return state_->waiter.Backend();
}

} // namespace Engine::Threads
