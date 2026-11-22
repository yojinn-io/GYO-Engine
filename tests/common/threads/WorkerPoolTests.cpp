#include <doctest/doctest.h>

#include "engine/threads/WorkerPool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

using namespace GYO::Engine::Threads;
using namespace std::chrono_literals;

// docs/architecture/threads.md, WorkerPool: the Contracts and Design Invariants 10 and 11. Each Stop case holds the
// pool's one thread in a job that has started, stops from another thread, and lets the job go only once the pool
// refuses jobs, so the queue is known to be full when the stop decides what to do with it, and the running job is
// known to be running.

namespace {

WorkerPool StartPool(WorkerPoolOptions options) {
    auto pool = WorkerPool::Start(std::move(options));
    REQUIRE(pool.has_value());
    return std::move(*pool);
}

// A job that reports it has started, then waits for the gate.
struct HeldJob {
    std::promise<void> started;
    std::promise<void> gate;
    std::shared_future<void> opened = gate.get_future().share();
    std::atomic<bool> finished{false};

    WorkerPool::Job Job() {
        return [this] {
            started.set_value();
            opened.wait();
            finished = true;
        };
    }
    // Returns once the pool's thread runs the job; a pool that never wakes for a submitted job fails here.
    void WaitStarted() { REQUIRE(started.get_future().wait_for(10s) == std::future_status::ready); }
    void Open() { gate.set_value(); }
};

// Submits no-op jobs that count themselves until the pool refuses one: the stop has begun. Returns how many it
// queued before that. A pool that never refuses fails here instead of growing its queue until the test times out.
int WaitUntilRefused(WorkerPool& pool, std::atomic<int>& ran) {
    int accepted = 0;
    while (pool.Submit([&ran] { ++ran; })) {
        ++accepted;
        REQUIRE_MESSAGE(accepted < 1'000'000, "the pool still takes jobs while it stops");
    }
    return accepted;
}

} // namespace

TEST_CASE("WorkerPool runs submitted jobs on its own named threads") {
    auto pool = StartPool({.name = "gyo-test-pool", .threadCount = 3});
    CHECK(pool.ThreadCount() == 3);
    CHECK(pool.ThreadName(0) == "gyo-test-pool (0)");
    CHECK(pool.ThreadName(2) == "gyo-test-pool (2)");
    std::mutex mutex;
    std::set<std::thread::id> threads;
    std::atomic<int> ran{0};
    for (int i = 0; i < 64; ++i) {
        CHECK(pool.Submit([&] {
            std::lock_guard lock(mutex);
            threads.insert(std::this_thread::get_id());
            ++ran;
        }));
    }
    pool.Stop(StopMode::Drain);
    CHECK(ran.load() == 64);
    CHECK(threads.count(std::this_thread::get_id()) == 0);
    CHECK(threads.size() <= 3);
}

TEST_CASE("WorkerPool starts jobs in the order they were submitted") {
    auto pool = StartPool({.name = "gyo-test-order"});
    HeldJob held;
    REQUIRE(pool.Submit(held.Job()));
    held.WaitStarted();
    std::vector<int> order;
    for (int i = 0; i < 8; ++i) REQUIRE(pool.Submit([&order, i] { order.push_back(i); }));
    held.Open();
    pool.Stop(StopMode::Drain);
    const std::vector<int> expected{0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(order == expected);
}

TEST_CASE("WorkerPool takes no job after Stop") {
    auto pool = StartPool({.name = "gyo-test-stopped"});
    pool.Stop(StopMode::Drain);
    const bool submitted = pool.Submit([] {});
    CHECK_FALSE(submitted);
    pool.Stop(StopMode::Drain);
}

TEST_CASE("WorkerPool Stop with Drain runs the jobs already queued") {
    auto pool = StartPool({.name = "gyo-test-drain"});
    HeldJob held;
    std::atomic<int> ran{0};
    REQUIRE(pool.Submit(held.Job()));
    held.WaitStarted();
    for (int i = 0; i < 5; ++i) REQUIRE(pool.Submit([&ran] { ++ran; }));
    std::atomic<int> ranWhenStopped{-1};
    std::thread stopper([&] {
        pool.Stop(StopMode::Drain);
        ranWhenStopped = ran.load();
    });
    const int accepted = WaitUntilRefused(pool, ran);
    held.Open();
    stopper.join();
    const int drained = ranWhenStopped.load();
    CHECK(drained == 5 + accepted);
}

TEST_CASE("WorkerPool Stop with Discard drops the queued jobs and waits for the running one") {
    auto pool = StartPool({.name = "gyo-test-discard"});
    HeldJob held;
    std::atomic<int> ran{0};
    REQUIRE(pool.Submit(held.Job()));
    held.WaitStarted();
    for (int i = 0; i < 5; ++i) REQUIRE(pool.Submit([&ran] { ++ran; }));
    std::atomic<bool> finishedWhenStopped{false};
    std::thread stopper([&] {
        pool.Stop(StopMode::Discard);
        finishedWhenStopped = held.finished.load();
    });
    static_cast<void>(WaitUntilRefused(pool, ran));
    held.Open();
    stopper.join();
    const bool waited = finishedWhenStopped.load();
    CHECK(waited);
    const int queuedRan = ran.load();
    CHECK(queuedRan == 0);
}

// The destructor waits for the running job. That it drops the queued jobs (it is Stop(Discard)) has no guard: the
// test cannot tell when the destructor has begun, so it cannot open the gate only after that.
TEST_CASE("WorkerPool destruction waits for the running job") {
    HeldJob held;
    std::thread opener;
    {
        auto pool = StartPool({.name = "gyo-test-destroy"});
        REQUIRE(pool.Submit(held.Job()));
        held.WaitStarted();
        opener = std::thread([&held] {
            std::this_thread::sleep_for(20ms);
            held.Open();
        });
    }
    CHECK(held.finished.load());
    opener.join();
}

TEST_CASE("WorkerPool moved from takes no job") {
    auto pool = StartPool({.name = "gyo-test-moved"});
    WorkerPool moved(std::move(pool));
    CHECK_FALSE(pool.Submit([] {}));
    CHECK(moved.Submit([] {}));
}
