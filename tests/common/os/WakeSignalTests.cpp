#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "CaseTiming.hpp"
#include "engine/os/WakeSignal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

// engine/os's wake primitive (docs/architecture/os.md, Wait) on its own; what a
// wake means (latch, deadlines, the re-check) is tested with engine/time's
// Waiter. As the Waiter does, a test that waits for a signal sets its own flag
// before it signals and re-checks the flag after every block, since a block may
// also end spuriously. The primitive never sees that flag: it keeps its own
// pending signal. A block with a timeout that a broken mechanism could leave
// waiting has a rescue, so the failure is a check that fails with its time; a
// block without a timeout runs under a watchdog, which reports and ends the
// process instead of a hang.
//
// The same cases also run against the fallback source on macOS
// (gyo_os_wake_fallback_tests, GYO_WAKE_SIGNAL_FALLBACK): this directory may
// branch on the OS.

using namespace GYO::Engine::OS;
using Clock = std::chrono::steady_clock;

namespace {

WakeSignal MakeWakeSignal() {
    auto created = WakeSignal::Create();
    REQUIRE(created);
    return std::move(created).value();
}

// Blocks until the flag is set, as engine/time's Waiter does: check, block,
// check again. Answers how long that took.
Clock::duration ConsumeFlag(WakeSignal& wake, std::atomic<bool>& flag,
                            const std::optional<std::chrono::nanoseconds> perBlock) {
    const auto start = Clock::now();
    while (!flag.exchange(false, std::memory_order_acq_rel)) wake.Block(perBlock);
    return Clock::now() - start;
}

// The longest of three blocks with a 20 ms timeout. A block may end
// spuriously, so only the longest has to have waited. If the timeout was never
// armed, a rescue starts signalling after 3 s so the caller's check fails with
// its time instead of hanging.
Clock::duration LongestOfThreeTimedBlocks(WakeSignal& wake) {
    std::atomic<bool> returned{false};
    std::thread rescue([&] {
        const auto from = Clock::now() + std::chrono::seconds(3);
        while (!returned.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (Clock::now() >= from) wake.Signal();
        }
    });
    auto longest = Clock::duration::zero();
    for (int block = 0; block < 3; ++block) {
        const auto start = Clock::now();
        wake.Block(std::chrono::milliseconds(20));
        longest = std::max(longest, Clock::now() - start);
    }
    returned.store(true, std::memory_order_release);
    rescue.join();
    return longest;
}

// Ends the process with a message when a case that blocks without a timeout
// has not finished in time: a broken signal would otherwise hang the run until
// CTest kills it, and the message would be lost.
class Watchdog final {
public:
    explicit Watchdog(const char* what) : what_(what), thread_([this] { Watch(); }) {}
    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;
    ~Watchdog() {
        {
            const std::lock_guard lock(mutex_);
            done_ = true;
        }
        condition_.notify_one();
        thread_.join();
    }

private:
    void Watch() {
        std::unique_lock lock(mutex_);
        if (condition_.wait_for(lock, std::chrono::seconds(10), [this] { return done_; })) return;
        // _Exit skips the stdio flush, so what the case already reported is
        // written out first.
        std::fflush(stdout);
        std::fprintf(stderr, "watchdog: %s\n", what_);
        std::fflush(stderr);
        std::_Exit(1);
    }

    const char* what_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool done_{false};
    std::thread thread_;
};

// The longest of three blocks with a 20 ms timeout, each right after a block
// that another thread's signal ended. A rescue as above.
Clock::duration LongestTimedBlockAfterAWake(WakeSignal& wake) {
    std::atomic<bool> returned{false};
    std::thread rescue([&] {
        const auto from = Clock::now() + std::chrono::seconds(3);
        while (!returned.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (Clock::now() >= from) wake.Signal();
        }
    });
    auto longest = Clock::duration::zero();
    for (int round = 0; round < 3; ++round) {
        std::thread other([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            wake.Signal();
        });
        wake.Block(std::chrono::seconds(5));
        other.join();
        const auto start = Clock::now();
        wake.Block(std::chrono::milliseconds(20));
        longest = std::max(longest, Clock::now() - start);
    }
    returned.store(true, std::memory_order_release);
    rescue.join();
    return longest;
}

} // namespace

TEST_CASE("OS wake signal uses the platform mechanism") {
    const auto wake = MakeWakeSignal();
#if defined(GYO_WAKE_SIGNAL_FALLBACK)
    CHECK(wake.Mechanism() == WaitMechanism::ConditionVariable);
#elif defined(__APPLE__)
    CHECK(wake.Mechanism() == WaitMechanism::Kqueue);
#elif defined(_WIN32)
    CHECK((wake.Mechanism() == WaitMechanism::HighResolutionWaitableTimer ||
           wake.Mechanism() == WaitMechanism::WaitableTimer));
#else
    CHECK(wake.Mechanism() == WaitMechanism::ConditionVariable);
#endif
}

TEST_CASE("OS wake signal: a signal sent before the block stays pending") {
    auto wake = MakeWakeSignal();
    wake.Signal();
    const auto start = Clock::now();
    wake.Block(std::chrono::seconds(5));
    const auto took = Clock::now() - start;
    CHECK_MESSAGE(took < std::chrono::seconds(1), "a signal sent before the block did not end it");
}

// A pending signal ends one block, then it is gone, as kqueue's EV_CLEAR and
// the Windows auto-reset event behave. A primitive that kept it would end every
// later block at once and the Waiter would spin.
TEST_CASE("OS wake signal: a consumed signal does not end the next block") {
    auto wake = MakeWakeSignal();
    SUBCASE("signals sent before the block") {
        wake.Signal();
        wake.Signal();
        wake.Signal();
        const auto start = Clock::now();
        wake.Block(std::chrono::seconds(5));
        REQUIRE(Clock::now() - start < std::chrono::seconds(1));
    }
    const auto longest = LongestOfThreeTimedBlocks(wake);
    CHECK_MESSAGE(longest >= std::chrono::milliseconds(10), "a signal already consumed ended the next block");
}

// The same for a signal that arrives during a block: the block it ends takes
// it, so the next block waits for its own timeout.
TEST_CASE("OS wake signal: a signal that ended a block does not end the next one") {
    auto wake = MakeWakeSignal();
    const auto longest = LongestTimedBlockAfterAWake(wake);
    CHECK_MESSAGE(longest >= std::chrono::milliseconds(10), "a signal that ended a block also ended the next one");
}

// A block without a timeout returns only on a signal (or rarely spuriously); a
// primitive that returned at once would turn every untimed Waiter::Wait into a
// busy loop. A timed block first leaves a timer behind, as the Waiter does.
TEST_CASE("OS wake signal: a block without a timeout ends only on a signal") {
    const Watchdog watchdog("a block without a timeout did not end on a signal");
    auto wake = MakeWakeSignal();
    wake.Block(std::chrono::milliseconds(1));
    std::atomic<bool> flag{false};
    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        flag.store(true, std::memory_order_release);
        wake.Signal();
    });
    int blocks = 0;
    while (!flag.exchange(false, std::memory_order_acq_rel)) {
        wake.Block(std::nullopt);
        ++blocks;
    }
    other.join();
    CAPTURE(blocks);
    CHECK_MESSAGE(blocks <= 3, "a block without a timeout returned without a signal");
}

TEST_CASE("OS wake signal ends a block on a signal from another thread") {
    const Watchdog watchdog("a block without a timeout did not end on a signal from another thread");
    auto wake = MakeWakeSignal();
    for (const std::optional<std::chrono::nanoseconds> perBlock :
         {std::optional<std::chrono::nanoseconds>(std::chrono::seconds(5)), std::optional<std::chrono::nanoseconds>()}) {
        CAPTURE(perBlock.has_value());
        std::atomic<bool> flag{false};
        std::thread other([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            flag.store(true, std::memory_order_release);
            wake.Signal();
        });
        const auto took = ConsumeFlag(wake, flag, perBlock);
        other.join();
        CHECK_MESSAGE(took < std::chrono::seconds(1), "the signal from another thread did not end the block");
    }
}

TEST_CASE("OS wake signal ends a block by its relative timeout") {
    auto wake = MakeWakeSignal();
    // A timer armed for no time at all ends every block at once.
    const auto longest = LongestOfThreeTimedBlocks(wake);
    CHECK_MESSAGE(longest < std::chrono::seconds(1), "a block with a 20 ms timeout did not end by it");
    CHECK_MESSAGE(longest >= std::chrono::milliseconds(10), "no block with a 20 ms timeout waited for it");
}

// The waiting thread checks its flag and then blocks; the other thread waits
// for the round to start, spinning so it answers within a cache transfer, and
// then sets the flag and signals. The waiter reaches its check 0 to 63 clock
// reads after it starts the round, so the check sweeps across the moment the
// signal arrives. A signal lost between the check and the block shows as a
// block that lasts until its 2 s timeout. A round that found its flag already
// set, without a block, leaves the signal pending, which would end the next
// round's first block at once and move it off the sweep, so such a round takes
// it once the signaller has sent it.
// This is the lost wakeup the fallback avoids by setting and reading its
// pending flag under its mutex (Design Invariant 8). The spin falls back to
// yielding, so one CPU still makes progress.
TEST_CASE("OS wake signal loses no signal sent while the waiter checks its flag") {
    constexpr int Rounds = 20000;
    constexpr int StopAfterLost = 3;
    constexpr int SpinsBeforeYield = 4096;
    auto wake = MakeWakeSignal();
    std::atomic<bool> flag{false};
    std::atomic<int> started{-1};
    std::atomic<int> signalled{-1};
    std::atomic<int> consumed{-1};
    std::atomic<bool> stop{false};
    std::thread signaller([&] {
        for (int round = 0; round < Rounds; ++round) {
            for (int spins = 0; started.load(std::memory_order_acquire) != round; ++spins) {
                if (stop.load(std::memory_order_acquire)) return;
                if (spins > SpinsBeforeYield) std::this_thread::yield();
            }
            flag.store(true, std::memory_order_release);
            wake.Signal();
            signalled.store(round, std::memory_order_release);
            for (int spins = 0; consumed.load(std::memory_order_acquire) != round; ++spins)
                if (spins > SpinsBeforeYield) std::this_thread::yield();
        }
    });
    int lost = 0;
    int rounds = 0;
    // Observation only (v8-arch-gyo2-os-test-timeout): how the case's time is
    // spent, noted every NoteEvery rounds and at the end. No check reads it.
    // A round's time is from the waiter's check to its flag (the clock read the
    // lost check already takes), so it leaves out a round's trailing take.
    constexpr int NoteEvery = 2000;
    const auto caseStart = Clock::now();
    int blockedRounds = 0;
    int slowRounds = 0;
    auto longestRound = Clock::duration::zero();
    const auto note = [&](const char* when) {
        GYO::Test::CaseTiming::Note("lost-wakeup %s rounds=%d blocked=%d slow(>=10ms)=%d longest=%.3fs elapsed=%.3fs",
            when, rounds, blockedRounds, slowRounds, std::chrono::duration<double>(longestRound).count(),
            std::chrono::duration<double>(Clock::now() - caseStart).count());
    };
    for (; rounds < Rounds && lost < StopAfterLost; ++rounds) {
        if (rounds > 0 && rounds % NoteEvery == 0) note("progress");
        started.store(rounds, std::memory_order_release);
        for (int read = 0; read < rounds % 64; ++read) static_cast<void>(Clock::now());
        const auto start = Clock::now();
        bool blocked = false;
        while (!flag.exchange(false, std::memory_order_acq_rel)) {
            wake.Block(std::chrono::seconds(2));
            blocked = true;
        }
        // The observation below reuses this clock read, so it adds no work
        // between the rounds' phases.
        const auto waited = Clock::now() - start;
        if (waited >= std::chrono::milliseconds(1500)) ++lost;
        if (!blocked) {
            for (int spins = 0; signalled.load(std::memory_order_acquire) != rounds; ++spins)
                if (spins > SpinsBeforeYield) std::this_thread::yield();
            wake.Block(std::chrono::seconds(2));
        }
        consumed.store(rounds, std::memory_order_release);
        if (blocked) ++blockedRounds;
        if (waited >= std::chrono::milliseconds(10)) ++slowRounds;
        longestRound = std::max(longestRound, waited);
    }
    note("end");
    stop.store(true, std::memory_order_release);
    signaller.join();
    CAPTURE(rounds);
    CHECK_MESSAGE(lost == 0, "a signal sent while the waiter checked its flag was lost until the block timed out");
}

TEST_CASE("OS wake signal ignores a signal on a moved-from signal and refuses its use") {
    auto first = MakeWakeSignal();
    auto second = std::move(first);
    first.Signal();  // NOLINT(bugprone-use-after-move): a moved-from signal ignores Signal
    GYO_CHECK_ASSERTS(first.Block(std::chrono::milliseconds(1)));
    GYO_CHECK_ASSERTS(static_cast<void>(first.Mechanism()));
    // The signal moved to keeps working: a block on it ends on a signal from
    // another thread.
    std::atomic<bool> flag{false};
    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        flag.store(true, std::memory_order_release);
        second.Signal();
    });
    const auto took = ConsumeFlag(second, flag, std::chrono::seconds(5));
    other.join();
    CHECK(took < std::chrono::seconds(1));
}
