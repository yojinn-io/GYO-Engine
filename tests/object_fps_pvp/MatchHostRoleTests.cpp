#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
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

// log-contract E1-5: a stall before the wait (here in the publish listener,
// after Advance) shows in the wake lateness and in the tick overrun alike.
TEST_CASE("PvP Match tick overrun shows a stall before the tick wait") {
    MatchRuntimeHost host(RoleArena());
    std::atomic<int> published{};
    host.SetPublishListener([&] {
        if (++published == 30) std::this_thread::sleep_for(40ms);
    });
    std::string error;
    REQUIRE(host.Start(error));
    REQUIRE(host.QueueJoin(1, 1));
    const auto giveUp = std::chrono::steady_clock::now() + 10s;
    while (published.load() < 40 && std::chrono::steady_clock::now() < giveUp) std::this_thread::sleep_for(5ms);
    host.Stop();
    const auto statistics = host.TakeStatistics();
    // Structural lower bounds only (the injected 40 ms), no timing upper bound (FF-2).
    CHECK(statistics.pace.advance.Max() >= 40ms);
    CHECK(statistics.pace.overrun.Max() >= 15ms);
    CHECK(statistics.pace.stalls >= 1);
    CHECK(statistics.ticks.late.Max() >= 15ms);
}

namespace {
// A movement trace whose first armed HostAccepted record sleeps: SubmitInput
// records it while it holds the host mutex.
struct StallingTrace final : MovementTraceSink {
    std::atomic<bool> armed{false};
    void Record(const MovementTraceEvent& event) override {
        if (event.kind == MovementTraceKind::HostAccepted && armed.exchange(false)) std::this_thread::sleep_for(40ms);
    }
};
} // namespace

// log-contract E1-5: a stall after the wake (the host mutex held by the I/O
// side when the simulation thread comes to record its wake) does not make the
// wake late, but lengthens the next tick interval: only the overrun shows it.
TEST_CASE("PvP Match tick overrun shows a stall after the tick wait") {
    MatchRuntimeHost host(RoleArena());
    std::atomic<int> published{};
    std::promise<void> afterPublish;
    auto stalled = afterPublish.get_future();
    host.SetPublishListener([&] {
        if (++published == 30) afterPublish.set_value();
    });
    auto trace = std::make_shared<StallingTrace>();
    struct TraceScope final {
        explicit TraceScope(std::shared_ptr<MovementTraceSink> sink) { SetMovementTrace(std::move(sink)); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } scope(trace);
    std::string error;
    REQUIRE(host.Start(error));
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(stalled.wait_for(10s) == std::future_status::ready);
    // Right after a publication the simulation thread is between Advance and its wait: the stall below holds the
    // mutex across its next wake.
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    const PlayerState* self = nullptr;
    for (const auto& player : snapshot->players)
        if (player.playerId == 1) self = &player;
    REQUIRE(self);
    trace->armed = true;
    CHECK(host.SubmitInput({1, {{self->lastResolvedCommand + 1, 0, 0, 0, 0, false}}, self->movementEpoch,
                            self->lifeGeneration}));
    CHECK_FALSE(trace->armed.load());
    const auto giveUp = std::chrono::steady_clock::now() + 10s;
    while (published.load() < 40 && std::chrono::steady_clock::now() < giveUp) std::this_thread::sleep_for(5ms);
    host.Stop();
    const auto statistics = host.TakeStatistics();
    CHECK(statistics.pace.overrun.Max() >= 15ms);
    CHECK(statistics.pace.stalls >= 1);
    // Recorded, not judged (a timing upper bound): the wakes themselves were on time.
    MESSAGE("tick_late max " << std::chrono::duration_cast<std::chrono::microseconds>(statistics.ticks.late.Max()).count()
            << " us, overrun max "
            << std::chrono::duration_cast<std::chrono::microseconds>(statistics.pace.overrun.Max()).count() << " us");
}

