#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/os/WakeSignal.hpp"

#include <algorithm>
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
    // If the timeout was never armed, a rescue starts signalling after 3 s so
    // the check fails with its time instead of hanging.
    std::atomic<bool> returned{false};
    std::thread rescue([&] {
        const auto from = Clock::now() + std::chrono::seconds(3);
        while (!returned.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (Clock::now() < from) continue;
            latch.store(true, std::memory_order_release);
            signal.Signal();
        }
    });
    // A block may end spuriously, so only the longest of three has to have
    // waited; a timer armed for no time at all ends every one at once.
    auto longest = Clock::duration::zero();
    for (int block = 0; block < 3; ++block) {
        const auto start = Clock::now();
        signal.Block(latch, std::chrono::milliseconds(20));
        longest = std::max(longest, Clock::now() - start);
    }
    returned.store(true, std::memory_order_release);
    rescue.join();
    CHECK_MESSAGE(longest < std::chrono::seconds(1), "a block with a 20 ms timeout did not end by it");
    CHECK_MESSAGE(longest >= std::chrono::milliseconds(10), "no block with a 20 ms timeout waited for it");
}

// The waiting thread checks the latch and then blocks; the other thread waits
// for the round to start, spinning so it answers within a cache transfer, and
// then sets the latch and signals. The waiter reaches its check 0 to 63 clock
// reads after it starts the round, so the check sweeps across the moment the
// signal arrives. A signal lost between the check and the block shows as a
// block that lasts until its 2 s timeout. This is the lost wakeup the
// condition-variable fallback avoids by reading the latch under its mutex
// (Design Invariant 8). The spin falls back to yielding, so one CPU still
// makes progress.
TEST_CASE("Os wake signal loses no signal sent while the waiter checks its latch") {
    constexpr int Rounds = 20000;
    constexpr int StopAfterLost = 3;
    constexpr int SpinsBeforeYield = 4096;
    auto signal = MakeSignal();
    std::atomic<bool> latch{false};
    std::atomic<int> started{-1};
    std::atomic<int> consumed{-1};
    std::atomic<bool> stop{false};
    std::thread signaller([&] {
        for (int round = 0; round < Rounds; ++round) {
            for (int spins = 0; started.load(std::memory_order_acquire) != round; ++spins) {
                if (stop.load(std::memory_order_acquire)) return;
                if (spins > SpinsBeforeYield) std::this_thread::yield();
            }
            latch.store(true, std::memory_order_release);
            signal.Signal();
            for (int spins = 0; consumed.load(std::memory_order_acquire) != round; ++spins)
                if (spins > SpinsBeforeYield) std::this_thread::yield();
        }
    });
    int lost = 0;
    int rounds = 0;
    for (; rounds < Rounds && lost < StopAfterLost; ++rounds) {
        started.store(rounds, std::memory_order_release);
        for (int read = 0; read < rounds % 64; ++read) static_cast<void>(Clock::now());
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
    // The signal moved to keeps working: a block on it ends on a signal from
    // another thread.
    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        latch.store(true, std::memory_order_release);
        second.Signal();
    });
    const auto took = ConsumeLatch(second, latch, std::chrono::seconds(5));
    other.join();
    CHECK(took < std::chrono::seconds(1));
}
