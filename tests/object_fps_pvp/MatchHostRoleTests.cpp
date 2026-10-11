#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena RoleArena() {
    Arena arena;
    arena.id = "synthetic_match_role";
    arena.width = arena.depth = 12;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 6}, 0}};
    return arena;
}

} // namespace

TEST_CASE("PvP Match role steps at the tick rate on absolute deadlines") {
    MatchRuntimeHost host(RoleArena());
    std::atomic<int> published{};
    host.SetPublishListener([&] { ++published; });
    std::string error;
    REQUIRE(host.Start(error));
    REQUIRE(host.QueueJoin(1, 1));
    // Each step that publishes a snapshot notifies; poll for a generous count.
    const auto giveUp = std::chrono::steady_clock::now() + 10s;
    while (published.load() < 20 && std::chrono::steady_clock::now() < giveUp) std::this_thread::sleep_for(10ms);
    host.Stop();
    CHECK(published.load() >= 20);
    CHECK_FALSE(host.Error());
    const auto statistics = host.TakeStatistics();
    CHECK(statistics.ticks.late.Count() >= 19);
    CHECK(statistics.ticks.notified == 0);
}

TEST_CASE("PvP Match role wakes for a reset at once instead of waiting for its deadline") {
    MatchRuntimeHost host(RoleArena());
    std::string error;
    REQUIRE(host.Start(error));
    std::this_thread::sleep_for(30ms);
    for (int round = 0; round < 3; ++round) {
        auto reset = host.RequestReset();
        REQUIRE(reset.wait_for(2s) == std::future_status::ready);
        CHECK_NOTHROW(reset.get());
    }
    host.Stop();
    CHECK(host.TakeStatistics().ticks.notified == 3);
}

TEST_CASE("PvP Match publish notification follows snapshots, control results, evictions and resets") {
    MatchRuntimeHost host(RoleArena());
    int published{};
    host.SetPublishListener([&] { ++published; });
    // Nothing elapsed: no tick, nothing published.
    static_cast<void>(host.Advance(0));
    CHECK(published == 0);
    // A join resolves on the next tick, which also publishes its snapshot.
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(1.0 / 60));
    CHECK(published == 1);
    CHECK(host.TakeControlResults().size() == 1);
    // A completed reset notifies even though no tick runs.
    auto reset = host.RequestReset();
    static_cast<void>(host.Advance(0));
    CHECK(reset.wait_for(0s) == std::future_status::ready);
    CHECK(published == 2);
}

TEST_CASE("PvP Match counts snapshots replaced before the I/O layer took them") {
    MatchRuntimeHost host(RoleArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(1.0 / 60));
    static_cast<void>(host.Advance(1.0 / 60)); // replaces the untaken snapshot
    REQUIRE(host.TakeSnapshot());
    static_cast<void>(host.Advance(1.0 / 60)); // the slot was empty
    CHECK(host.TakeStatistics().snapshotOverwrites == 1);
    CHECK(host.TakeStatistics().snapshotOverwrites == 0);
}

TEST_CASE("PvP Match role failures are reported, not thrown across threads") {
    MatchRuntimeHost host(RoleArena());
    std::string error;
    REQUIRE(host.Start(error));
    CHECK_THROWS_AS(static_cast<void>(host.Start(error)), std::logic_error);
    CHECK_THROWS_AS(host.SetPublishListener([] {}), std::logic_error);
    host.Stop();
    host.Stop(); // idempotent
    CHECK_FALSE(host.Error());
}
