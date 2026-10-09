#include "engine/threads/RoleThread.hpp"

#include "engine/base/Assert.hpp"

#include <atomic>
#include <exception>
#include <future>
#include <system_error>
#include <utility>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

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

// Both run on the new thread before the body.
bool ApplyName(const std::string& name) noexcept {
#if defined(__APPLE__)
    return pthread_setname_np(name.c_str()) == 0;
#elif defined(_WIN32)
    // SetThreadDescription exists from Windows 10 1607; look it up instead of linking it.
    using Describe = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const auto module = GetModuleHandleW(L"kernel32.dll");
    const auto describe = module ? reinterpret_cast<Describe>(GetProcAddress(module, "SetThreadDescription")) : nullptr;
    if (!describe) return false;
    const std::wstring wide(name.begin(), name.end());
    return SUCCEEDED(describe(GetCurrentThread(), wide.c_str()));
#elif defined(__linux__)
    return pthread_setname_np(pthread_self(), name.substr(0, 15).c_str()) == 0;
#else
    static_cast<void>(name);
    return false;
#endif
}

bool ApplyPriority(const ThreadPriority priority) noexcept {
#if defined(__APPLE__)
    const qos_class_t qos = priority == ThreadPriority::Interactive ? QOS_CLASS_USER_INTERACTIVE :
        priority == ThreadPriority::Background ? QOS_CLASS_UTILITY : QOS_CLASS_DEFAULT;
    return pthread_set_qos_class_self_np(qos, 0) == 0;
#elif defined(_WIN32)
    const int value = priority == ThreadPriority::Interactive ? THREAD_PRIORITY_ABOVE_NORMAL :
        priority == ThreadPriority::Background ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL;
    return SetThreadPriority(GetCurrentThread(), value) != 0;
#elif defined(__linux__)
    // Raising a thread's priority needs privileges; only lowering is attempted.
    if (priority == ThreadPriority::Normal) return true;
    if (priority == ThreadPriority::Interactive) return false;
    return setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10) == 0;
#else
    return priority == ThreadPriority::Normal;
#endif
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
            state->nameApplied = ApplyName(state->options.name);
            state->priorityApplied = ApplyPriority(state->options.priority);
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

} // namespace Engine::Threads
