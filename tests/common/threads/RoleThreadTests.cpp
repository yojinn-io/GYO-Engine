#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/threads/RoleThread.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <utility>

using namespace Engine::Threads;
using namespace std::chrono_literals;

namespace {
RoleThread StartRole(RoleThreadOptions options, RoleThread::Body body) {
    auto role = RoleThread::Start(std::move(options), std::move(body));
    REQUIRE(role.has_value());
    return std::move(*role);
}
} // namespace

TEST_CASE("Threads a stop request wakes a role blocked in its wait") {
    std::promise<void> returned;
    auto done = returned.get_future();
    std::atomic<bool> waiting{false};
    auto role = StartRole({"gyo-test-stop"}, [&returned, &waiting](RoleContext& context) {
        while (!context.StopRequested()) {
            waiting.store(true);
            static_cast<void>(context.Wait());
        }
        returned.set_value();
    });
    CHECK(role.Joinable());
    // The stop must reach a role that is already blocked in its wait.
    while (!waiting.load()) std::this_thread::yield();
    std::this_thread::sleep_for(20ms);
    role.RequestStop();
    // Bounded: a stop that does not wake the role is reported, then released.
    const bool stopped = done.wait_for(30s) == std::future_status::ready;
    CHECK_MESSAGE(stopped, "the stop request did not wake the role's wait");
    if (!stopped) role.Notify();
    role.Join();
    CHECK_FALSE(role.Joinable());
    role.Join(); // idempotent
}

TEST_CASE("Threads destroying a role stops and joins it") {
    std::atomic<bool> returned{false};
    auto role = std::make_unique<RoleThread>(StartRole({"gyo-test-destroy"}, [&returned](RoleContext& context) {
        while (!context.StopRequested()) static_cast<void>(context.Wait());
        returned.store(true);
    }));
    auto* const live = role.get();
    // Destroyed on a helper thread so a destructor that never returns is reported, then released.
    auto destroyed = std::async(std::launch::async, [&role] { role.reset(); });
    const bool finished = destroyed.wait_for(30s) == std::future_status::ready;
    CHECK_MESSAGE(finished, "destroying the role did not stop it");
    if (!finished) live->Notify();
    destroyed.wait();
    CHECK(returned.load());
}

TEST_CASE("Threads Notify wakes a role without stopping it") {
    std::atomic<int> wakes{0};
    auto role = StartRole({"gyo-test-notify"}, [&wakes](RoleContext& context) {
        while (!context.StopRequested()) {
            if (context.WaitUntil(Engine::Time::Now() + 30s).reason == Engine::Time::WakeReason::Notified)
                wakes.fetch_add(1);
        }
    });
    for (int expected = 1; expected <= 3; ++expected) {
        role.Notify();
        const auto until = std::chrono::steady_clock::now() + 30s;
        while (wakes.load() < expected && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
        REQUIRE(wakes.load() >= expected);
    }
    role.RequestStop();
    role.Join();
}

TEST_CASE("Threads a role reports its name and whether the platform applied it and the priority") {
    for (const auto priority : {ThreadPriority::Background, ThreadPriority::Normal, ThreadPriority::Interactive}) {
        auto role = StartRole({"gyo-test-priority", priority}, [](RoleContext& context) {
            while (!context.StopRequested()) static_cast<void>(context.Wait());
        });
        CHECK(role.Name() == "gyo-test-priority");
#if defined(__APPLE__) || defined(__linux__)
        CHECK(role.NameApplied());
#endif
        if (priority == ThreadPriority::Normal) CHECK(role.PriorityApplied());
#if defined(__APPLE__) || defined(_WIN32)
        CHECK(role.PriorityApplied());
#endif
        MESSAGE("priority ", static_cast<int>(priority), " name applied ", role.NameApplied(),
            " priority applied ", role.PriorityApplied());
    }
}

TEST_CASE("Threads the body runs on its own thread after Start returns") {
    std::thread::id bodyThread;
    std::promise<void> ran;
    auto ready = ran.get_future();
    auto role = StartRole({"gyo-test-identity"}, [&](RoleContext& context) {
        bodyThread = std::this_thread::get_id();
        ran.set_value();
        while (!context.StopRequested()) static_cast<void>(context.Wait());
    });
    ready.wait();
    CHECK(bodyThread != std::this_thread::get_id());
    role.RequestStop();
    role.Join();
}

TEST_CASE("Threads a role needs a name and a body") {
    GYO_CHECK_ASSERTS(RoleThread::Start({""}, [](RoleContext&) {}));
    GYO_CHECK_ASSERTS(RoleThread::Start({"gyo-test-empty"}, RoleThread::Body{}));
}
