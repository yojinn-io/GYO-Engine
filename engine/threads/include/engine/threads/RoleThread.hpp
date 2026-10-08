#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/time/Waiter.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace Engine::Threads {

enum class ThreadErrorCode : std::uint16_t {
    // The operating system did not start the thread or its wait primitive.
    StartFailed = 1,
};

[[nodiscard]] constexpr const char* ToString(const ThreadErrorCode code) noexcept {
    switch (code) {
    case ThreadErrorCode::StartFailed: return "StartFailed";
    }
    return "Unknown";
}

using ThreadError = Base::Error<ThreadErrorCode>;
static_assert(Base::CodedError<ThreadError>);

// A scheduling hint, mapped per platform (macOS QoS classes, Windows thread
// priorities; on Linux only Background is applied, as nice value 10, which
// fails if the process already runs nicer). It is
// not a timing guarantee: on macOS QoS alone does not remove timer-coalescing
// leeway, which Time::Waiter avoids instead.
enum class ThreadPriority : std::uint8_t { Background, Normal, Interactive };

struct RoleThreadOptions final {
    // Shown by debuggers and profilers; non-empty. Linux keeps the first 15 bytes.
    std::string name;
    ThreadPriority priority{ThreadPriority::Normal};
};

// What a role's loop sees: its stop request and its own Waiter.
class RoleContext final {
public:
    RoleContext(const RoleContext&) = delete;
    RoleContext& operator=(const RoleContext&) = delete;
    [[nodiscard]] bool StopRequested() const noexcept;
    // Waits on the role's Waiter. A stop request or a Notify from any thread wakes it.
    [[nodiscard]] Time::Wake WaitUntil(Time::TimePoint deadline);
    [[nodiscard]] Time::Wake Wait();

private:
    friend class RoleThread;
    struct State;
    explicit RoleContext(State& state) noexcept : state_(state) {}
    State& state_;
};

// One long-lived, named thread running one role's loop. The owner decides the
// order in which roles stop and join; destruction requests a stop and joins.
// The body must return once StopRequested() is true and must not let an
// exception escape (a thread entry point handles its own Runtime Errors). It is
// a std::function, so it must be copyable; hold move-only state through a
// shared_ptr. Notify and RequestStop on a moved-from RoleThread do nothing.
class RoleThread final {
public:
    using Body = std::function<void(RoleContext&)>;

    // Returns once the new thread has applied its name and priority and is
    // about to run the body.
    [[nodiscard]] static Base::Result<RoleThread, ThreadError> Start(RoleThreadOptions options, Body body);

    RoleThread(RoleThread&& other) noexcept;
    RoleThread& operator=(RoleThread&&) = delete;
    RoleThread(const RoleThread&) = delete;
    RoleThread& operator=(const RoleThread&) = delete;
    ~RoleThread();

    // Any thread: wakes the role's wait without stopping it.
    void Notify() noexcept;
    // Any thread: the body's StopRequested() becomes true and its wait wakes.
    void RequestStop() noexcept;
    // Waits for the body to return. Idempotent; not from the role's own thread.
    void Join();
    [[nodiscard]] bool Joinable() const noexcept { return thread_.joinable(); }

    [[nodiscard]] const std::string& Name() const;
    // Whether the platform accepted the name and the priority hint.
    [[nodiscard]] bool NameApplied() const;
    [[nodiscard]] bool PriorityApplied() const;

private:
    RoleThread(std::unique_ptr<RoleContext::State> state, std::thread thread) noexcept;
    std::unique_ptr<RoleContext::State> state_;
    std::thread thread_;
};

} // namespace Engine::Threads
