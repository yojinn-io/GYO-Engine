#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/time/LateWakeStats.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/time/Waiter.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

using namespace Engine::Time;
using namespace std::chrono_literals;

namespace {
Waiter MakeWaiter() {
    auto waiter = Waiter::Create();
    REQUIRE(waiter.has_value());
    return std::move(*waiter);
}
} // namespace

// Timing assertions are structural (never early, notified before a far
// deadline); how late a wake is depends on the host and is only measured.

TEST_CASE("Time Waiter uses the platform backend") {
    const auto waiter = MakeWaiter();
#if defined(__APPLE__)
    CHECK(waiter.Backend() == WaitBackend::Kqueue);
#elif defined(_WIN32)
    CHECK((waiter.Backend() == WaitBackend::HighResolutionWaitableTimer || waiter.Backend() == WaitBackend::WaitableTimer));
#else
    CHECK(waiter.Backend() == WaitBackend::ConditionVariable);
#endif
}

TEST_CASE("Time Waiter never wakes before a deadline and reports how late it woke") {
    auto waiter = MakeWaiter();
    for (const auto interval : {0us, 100us, 1000us, 5000us, 16667us}) {
        for (int repeat = 0; repeat < 10; ++repeat) {
            const auto deadline = Now() + interval;
            const auto wake = waiter.WaitUntil(deadline);
            CHECK(wake.reason == WakeReason::Deadline);
            CHECK(wake.planned == deadline);
            CHECK(wake.woke >= deadline);
            CHECK(wake.late == wake.woke - wake.planned);
        }
    }
}

TEST_CASE("Time Waiter returns at once for a deadline already passed") {
    auto waiter = MakeWaiter();
    const auto deadline = Now() - 5ms;
    const auto wake = waiter.WaitUntil(deadline);
    CHECK(wake.reason == WakeReason::Deadline);
    CHECK(wake.late >= 5ms);
}

TEST_CASE("Time Waiter latches a notification sent before the wait") {
    auto waiter = MakeWaiter();
    waiter.Notify();
    const auto wake = waiter.WaitUntil(Now() + 30s);
    CHECK(wake.reason == WakeReason::Notified);
    CHECK(wake.late == Duration::zero());
}

TEST_CASE("Time Waiter coalesces several notifications into one wake") {
    auto waiter = MakeWaiter();
    waiter.Notify();
    waiter.Notify();
    waiter.Notify();
    CHECK(waiter.WaitUntil(Now() + 30s).reason == WakeReason::Notified);
    // The platform signal left by the extra notifications wakes the next wait
    // spuriously (kqueue user event, Windows auto-reset event): it must wait again.
    const auto deadline = Now() + 20ms;
    const auto second = waiter.WaitUntil(deadline);
    CHECK(second.reason == WakeReason::Deadline);
    CHECK(second.woke >= deadline);
}

TEST_CASE("Time Waiter reports a passed deadline first and keeps the notification latched") {
    auto waiter = MakeWaiter();
    waiter.Notify();
    const auto passed = waiter.WaitUntil(Now() - 1ms);
    CHECK(passed.reason == WakeReason::Deadline);
    CHECK(passed.late >= 1ms);
    CHECK(waiter.WaitUntil(Now() + 30s).reason == WakeReason::Notified);
}

TEST_CASE("Time Waiter saturates how late a sentinel deadline is") {
    auto waiter = MakeWaiter();
    const auto wake = waiter.WaitUntil(TimePoint::min());
    CHECK(wake.reason == WakeReason::Deadline);
    CHECK(wake.late > Duration::zero());
}

TEST_CASE("Time Waiter wakes on a notification from another thread before a far deadline") {
    auto waiter = MakeWaiter();
    std::atomic<bool> waiting{false};
    std::thread notifier([&] {
        while (!waiting.load()) std::this_thread::yield();
        std::this_thread::sleep_for(20ms);
        waiter.Notify();
    });
    waiting.store(true);
    const auto wake = waiter.WaitUntil(Now() + 30s);
    notifier.join();
    CHECK(wake.reason == WakeReason::Notified);
}

TEST_CASE("Time Waiter waits without a deadline until notified") {
    auto waiter = MakeWaiter();
    std::atomic<bool> woken{false};
    std::atomic<bool> watchdogFired{false};
    std::thread notifier([&] {
        std::this_thread::sleep_for(20ms);
        waiter.Notify();
    });
    // Bounded on failure: a lost notification is released after 30 s and reported.
    std::thread watchdog([&] {
        const auto until = Now() + 30s;
        while (!woken.load() && Now() < until) std::this_thread::sleep_for(1ms);
        if (!woken.load()) {
            watchdogFired.store(true);
            waiter.Notify();
        }
    });
    const auto wake = waiter.Wait();
    woken.store(true);
    notifier.join();
    watchdog.join();
    CHECK_FALSE(watchdogFired.load());
    CHECK(wake.reason == WakeReason::Notified);
    CHECK(wake.planned == TimePoint::max());
}

TEST_CASE("Time Waiter loses no notification under concurrent producers") {
    auto waiter = MakeWaiter();
    constexpr int Producers = 8;
    constexpr int PerProducer = 500;
    std::atomic<int> sent{0};
    std::atomic<int> finished{0};
    std::vector<std::thread> producers;
    for (int producer = 0; producer < Producers; ++producer)
        producers.emplace_back([&] {
            for (int n = 0; n < PerProducer; ++n) {
                sent.fetch_add(1);
                waiter.Notify();
            }
            finished.fetch_add(1);
            waiter.Notify();
        });
    // Every notification after the last observed count must wake the waiter:
    // once all producers finished, one more wait must return Notified or find
    // nothing left to observe.
    int observed = 0;
    bool lost = false;
    while (finished.load() < Producers || observed < sent.load()) {
        if (waiter.WaitUntil(Now() + 30s).reason != WakeReason::Notified) {
            lost = true;
            break;
        }
        observed = sent.load();
    }
    for (auto& producer : producers) producer.join();
    CHECK_FALSE(lost);
    CHECK(observed == Producers * PerProducer);
}

TEST_CASE("Time Waiter rejects use after move and ignores notifications on a moved-from waiter") {
    auto waiter = MakeWaiter();
    auto moved = std::move(waiter);
    waiter.Notify();
    GYO_CHECK_ASSERTS(waiter.WaitUntil(Now()));
    GYO_CHECK_ASSERTS(waiter.Backend());
    moved.Notify();
    CHECK(moved.WaitUntil(Now() + 30s).reason == WakeReason::Notified);
}

TEST_CASE("Time LateWakeStats bins late wakes and bounds quantiles") {
    LateWakeStats stats;
    CHECK(stats.Count() == 0);
    CHECK(stats.QuantileUpperBound(0.99) == Duration::zero());
    stats.Record(Duration::zero());
    stats.Record(249us);
    stats.Record(250us);
    stats.Record(3ms);
    stats.Record(70ms);
    CHECK(stats.Count() == 5);
    CHECK(stats.Max() == 70ms);
    CHECK(stats.Bins()[0] == 2);
    CHECK(stats.Bins()[1] == 1);
    CHECK(stats.Bins()[4] == 1);
    CHECK(stats.Bins()[LateWakeStats::BinCount - 1] == 1);
    CHECK(stats.QuantileUpperBound(0.4) == 250us);
    CHECK(stats.QuantileUpperBound(0.6) == 500us);
    CHECK(stats.QuantileUpperBound(0.8) == 4ms);
    CHECK(stats.QuantileUpperBound(1.0) == 70ms);
    stats.Record(Wake{WakeReason::Notified, Now(), Now(), Duration::zero()});
    CHECK(stats.Count() == 5);
    stats.Record(Wake{WakeReason::Deadline, TimePoint{}, TimePoint{} + 1ms, 1ms});
    CHECK(stats.Count() == 6);
    GYO_CHECK_ASSERTS(stats.Record(-1us));
    GYO_CHECK_ASSERTS(stats.QuantileUpperBound(0.0));
    stats.Reset();
    CHECK(stats.Count() == 0);
    CHECK(stats.Max() == Duration::zero());
}

TEST_CASE("Time base is the steady clock and NowNs follows it") {
    static_assert(std::is_same_v<MonotonicClock, std::chrono::steady_clock>);
    const auto before = NowNs();
    const auto after = NowNs();
    CHECK(after >= before);
}
