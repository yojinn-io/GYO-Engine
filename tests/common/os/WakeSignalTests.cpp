#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/os/WakeSignal.hpp"

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <utility>

// engine/os's wake primitive (docs/architecture/os.md, Wait) on its own; what a
// wake means (latch, deadlines, the re-check) is tested with engine/time's
// Waiter. As the Waiter does, a test sets its latch before it signals and
// re-checks the latch after every block, since a block may also end
// spuriously. Every block that a broken mechanism could leave waiting has a
// timeout, so the failure is a check that fails with its time, not a hang.

using namespace Engine::Os;
using Clock = std::chrono::steady_clock;

namespace {

WakeSignal MakeSignal() {
    auto signal = WakeSignal::Create();
    REQUIRE(signal);
    return std::move(signal).value();
}

// Blocks until the latch is set, as engine/time's Waiter does: check, block,
// check again. Answers how long that took.
Clock::duration ConsumeLatch(WakeSignal& signal, std::atomic<bool>& latch,
                             const std::optional<std::chrono::nanoseconds> perBlock) {
    const auto start = Clock::now();
    while (!latch.exchange(false, std::memory_order_acq_rel)) signal.Block(latch, perBlock);
    return Clock::now() - start;
}

} // namespace

TEST_CASE("Os wake signal reports the platform mechanism") {
    const auto signal = MakeSignal();
#if defined(__APPLE__)
    CHECK(signal.Mechanism() == WaitMechanism::Kqueue);
#elif defined(_WIN32)
    CHECK((signal.Mechanism() == WaitMechanism::HighResolutionWaitableTimer ||
           signal.Mechanism() == WaitMechanism::WaitableTimer));
#else
    CHECK(signal.Mechanism() == WaitMechanism::ConditionVariable);
#endif
}

TEST_CASE("Os wake signal keeps a signal sent before the block") {
    auto signal = MakeSignal();
    std::atomic<bool> latch{false};
    latch.store(true, std::memory_order_release);
    signal.Signal();
    const auto start = Clock::now();
    signal.Block(latch, std::chrono::seconds(5));
    const auto took = Clock::now() - start;
    CHECK_MESSAGE(took < std::chrono::seconds(1), "a signal sent before the block did not end it");
}

TEST_CASE("Os wake signal ends a block on a signal from another thread") {
    auto signal = MakeSignal();
    for (const std::optional<std::chrono::nanoseconds> perBlock :
         {std::optional<std::chrono::nanoseconds>(std::chrono::seconds(5)), std::optional<std::chrono::nanoseconds>()}) {
        CAPTURE(perBlock.has_value());
        std::atomic<bool> latch{false};
        std::thread other([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            latch.store(true, std::memory_order_release);
            signal.Signal();
        });
        const auto took = ConsumeLatch(signal, latch, perBlock);
        other.join();
        CHECK_MESSAGE(took < std::chrono::seconds(1), "the signal from another thread did not end the block");
    }
}

TEST_CASE("Os wake signal ends a block by its relative timeout") {
    auto signal = MakeSignal();
    std::atomic<bool> latch{false};
    // If the timeout was never armed, a rescue ends the block after 3 s so the
    // check fails with its time instead of hanging.
    std::atomic<bool> returned{false};
    std::thread rescue([&] {
        const auto until = Clock::now() + std::chrono::seconds(3);
        while (!returned.load(std::memory_order_acquire) && Clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!returned.load(std::memory_order_acquire)) {
            latch.store(true, std::memory_order_release);
            signal.Signal();
        }
    });
    const auto start = Clock::now();
    signal.Block(latch, std::chrono::milliseconds(20));
    const auto took = Clock::now() - start;
    returned.store(true, std::memory_order_release);
    rescue.join();
    CHECK_MESSAGE(took < std::chrono::seconds(1), "a block with a 20 ms timeout did not end by it");
}

// The waiting thread checks the latch and then blocks; the other thread sets
// the latch and signals at a moment that sweeps across that check (0 to 63
// clock reads after the waiter starts the round). A signal lost between the
// check and the block shows as a block that lasts until its 2 s timeout. This
// is the lost wakeup the condition-variable fallback avoids by reading the
// latch under its mutex (Design Invariant 8).
TEST_CASE("Os wake signal loses no signal sent while the waiter checks its latch") {
    constexpr int Rounds = 20000;
    constexpr int StopAfterLost = 3;
    auto signal = MakeSignal();
    std::atomic<bool> latch{false};
    std::atomic<int> started{-1};
    std::atomic<int> consumed{-1};
    std::atomic<bool> stop{false};
    std::thread signaller([&] {
        for (int round = 0; round < Rounds; ++round) {
            while (started.load(std::memory_order_acquire) != round) {
                if (stop.load(std::memory_order_acquire)) return;
                std::this_thread::yield();
            }
            for (int read = 0; read < round % 64; ++read) static_cast<void>(Clock::now());
            latch.store(true, std::memory_order_release);
            signal.Signal();
            while (consumed.load(std::memory_order_acquire) != round) std::this_thread::yield();
        }
    });
    int lost = 0;
    int rounds = 0;
    for (; rounds < Rounds && lost < StopAfterLost; ++rounds) {
        started.store(rounds, std::memory_order_release);
        if (ConsumeLatch(signal, latch, std::chrono::seconds(2)) >= std::chrono::milliseconds(1500)) ++lost;
        consumed.store(rounds, std::memory_order_release);
    }
    stop.store(true, std::memory_order_release);
    signaller.join();
    CAPTURE(rounds);
    CHECK_MESSAGE(lost == 0, "a signal sent while the waiter checked its latch was lost until the block timed out");
}

TEST_CASE("Os wake signal ignores a signal on a moved-from signal and refuses its use") {
    auto first = MakeSignal();
    auto second = std::move(first);
    first.Signal();  // NOLINT(bugprone-use-after-move): a moved-from signal ignores Signal
    std::atomic<bool> latch{false};
    GYO_CHECK_ASSERTS(first.Block(latch, std::chrono::milliseconds(1)));
    GYO_CHECK_ASSERTS(static_cast<void>(first.Mechanism()));
    latch.store(true, std::memory_order_release);
    second.Signal();
    CHECK(ConsumeLatch(second, latch, std::chrono::seconds(5)) < std::chrono::seconds(1));
}
