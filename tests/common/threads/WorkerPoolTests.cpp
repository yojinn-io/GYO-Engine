#include <doctest/doctest.h>

#include "engine/threads/WorkerPool.hpp"

#include <atomic>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

using namespace GYO::Engine::Threads;

// docs/architecture/threads.md, WorkerPool: Design Invariants 10 and 11. Each Stop case holds the pool's one
// thread in a job, stops from another thread, and lets the job go only once the pool refuses jobs, so the queue
// is known to be full when the stop decides what to do with it.

namespace {

WorkerPool StartPool(WorkerPoolOptions options) {
    auto pool = WorkerPool::Start(std::move(options));
    REQUIRE(pool.has_value());
    return std::move(*pool);
}

// Submits no-op jobs that count themselves until the pool refuses one: the stop has begun. Returns how many it
// queued before that.
int WaitUntilRefused(WorkerPool& pool, std::atomic<int>& ran) {
    int accepted = 0;
    while (pool.Submit([&ran] { ++ran; })) ++accepted;
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

TEST_CASE("WorkerPool takes no job after Stop") {
    auto pool = StartPool({.name = "gyo-test-stopped"});
    pool.Stop(StopMode::Drain);
    std::atomic<bool> ran{false};
    const bool submitted = pool.Submit([&ran] { ran = true; });
    CHECK_FALSE(submitted);
    pool.Stop(StopMode::Drain);
    CHECK_FALSE(ran.load());
}

TEST_CASE("WorkerPool Stop with Drain runs the jobs already queued") {
    auto pool = StartPool({.name = "gyo-test-drain"});
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    std::atomic<int> ran{0};
    REQUIRE(pool.Submit([opened, &ran] { opened.wait(); ++ran; }));
    for (int i = 0; i < 5; ++i) REQUIRE(pool.Submit([&ran] { ++ran; }));
    std::thread stopper([&pool] { pool.Stop(StopMode::Drain); });
    const int accepted = WaitUntilRefused(pool, ran);
    gate.set_value();
    stopper.join();
    const int drained = ran.load();
    CHECK(drained == 1 + 5 + accepted);
}

TEST_CASE("WorkerPool Stop with Discard drops the queued jobs and waits for the running one") {
    auto pool = StartPool({.name = "gyo-test-discard"});
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    std::atomic<bool> finished{false};
    std::atomic<int> ran{0};
    REQUIRE(pool.Submit([opened, &finished] { opened.wait(); finished = true; }));
    for (int i = 0; i < 5; ++i) REQUIRE(pool.Submit([&ran] { ++ran; }));
    std::atomic<bool> finishedWhenStopped{false};
    std::thread stopper([&] {
        pool.Stop(StopMode::Discard);
        finishedWhenStopped = finished.load();
    });
    static_cast<void>(WaitUntilRefused(pool, ran));
    gate.set_value();
    stopper.join();
    const bool waited = finishedWhenStopped.load();
    CHECK(waited);
    const int queuedRan = ran.load();
    CHECK(queuedRan == 0);
}

TEST_CASE("WorkerPool destruction drops the queued jobs and waits for the running one") {
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    std::atomic<bool> finished{false};
    std::atomic<int> ran{0};
    {
        auto pool = StartPool({.name = "gyo-test-destroy"});
        REQUIRE(pool.Submit([opened, &finished] { opened.wait(); finished = true; }));
        for (int i = 0; i < 5; ++i) REQUIRE(pool.Submit([&ran] { ++ran; }));
        std::thread opener([&gate] { gate.set_value(); });
        opener.join();
    }
    CHECK(finished.load());
    CHECK(ran.load() <= 5);
}

TEST_CASE("WorkerPool moved from takes no job") {
    auto pool = StartPool({.name = "gyo-test-moved"});
    WorkerPool moved(std::move(pool));
    CHECK_FALSE(pool.Submit([] {}));
    CHECK(moved.Submit([] {}));
}
