#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <thread>

namespace {
fps::pvp::Arena TestArena() {
    fps::pvp::Arena arena;
    arena.id = "synthetic_two_player_arena";
    arena.width = arena.depth = 12;
    arena.walls = {
        {{-1, 0, -1}, {0, 3, 13}}, {{12, 0, -1}, {13, 3, 13}},
        {{0, 0, -1}, {12, 3, 0}}, {{0, 0, 12}, {12, 3, 13}},
        {{5, 0, 4}, {6, 3, 8}},
    };
    arena.spawns = {{{2, 0, 2}, 0}, {{9, 0, 2}, 0}};
    return arena;
}

void Step(fps::pvp::PvpMatch& match, unsigned count = 1) {
    for (unsigned index = 0; index < count; ++index)
        match.Tick({match.TickCount() + 1, 1.0 / 60});
}
}

TEST_CASE("PvP authority joins at distinct valid spawns and owns membership") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    CHECK_FALSE(match.Join(0, error));
    REQUIRE(match.Join(7, error));
    REQUIRE(match.Join(8, error));
    CHECK(match.Join(7, error));
    CHECK_FALSE(match.Join(9, error));
    CHECK(error == "match_full");
    const auto snapshot = match.Snapshot();
    REQUIRE(snapshot.players.size() == 2);
    CHECK(snapshot.players[0].position.x == 2);
    CHECK(snapshot.players[1].position.x == 9);
    CHECK(match.Leave(7));
    CHECK_FALSE(match.Leave(7));
    CHECK(match.Join(9, error));
}

TEST_CASE("PvP moves only when authority ticks and normalizes diagonal movement") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    for (std::uint64_t sequence = 1; sequence <= 100; ++sequence)
        REQUIRE(match.SubmitInput({1, sequence, sequence * 4, 1, 1, 0, 0}));
    CHECK(match.Snapshot().players[0].position.x == 2);
    Step(match);
    const auto player = match.Snapshot().players[0];
    CHECK(std::hypot(player.position.x - 2, player.position.z - 2) == doctest::Approx(3.0 / 60));
    CHECK(player.lastInputSequence == 100);
    CHECK(match.Snapshot().tick == 1);
}

TEST_CASE("PvP duplicate active Join preserves pose sequence and input lifetime") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(7, error));
    REQUIRE(match.SubmitInput({7, 42, 500, 1, 0, 0, 0.2F}));
    Step(match, 5);
    const auto before = match.Snapshot();
    REQUIRE(match.Join(7, error));
    const auto after = match.Snapshot();
    REQUIRE(after.players.size() == 1);
    CHECK(after.tick == before.tick);
    CHECK(after.players[0].position.x == before.players[0].position.x);
    CHECK(after.players[0].position.z == before.players[0].position.z);
    CHECK(after.players[0].pitch == before.players[0].pitch);
    CHECK(after.players[0].lastInputSequence == 42);
    CHECK_FALSE(match.SubmitInput({7, 42, 999999, -1, 0, 0, 0}));
    Step(match, 10); // Fifteen steps since the original input, not since Join.
    const auto stopped = match.Snapshot().players[0];
    CHECK(stopped.position.z == doctest::Approx(2.75));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == stopped.position.z);
}

TEST_CASE("PvP stale invalid and foreign inputs cannot change the world") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, 10, 99999, 1, 0, 0, 0}));
    CHECK_FALSE(match.SubmitInput({1, 9, 0, -1, 0, 0, 0}));
    CHECK_FALSE(match.SubmitInput({1, 10, 0, -1, 0, 0, 0}));
    CHECK_FALSE(match.SubmitInput({2, 11, 0, -1, 0, 0, 0}));
    CHECK_FALSE(match.SubmitInput({1, 11, 0, 2, 0, 0, 0}));
    CHECK_FALSE(match.SubmitInput({1, 11, 0, 1, 0, std::numeric_limits<float>::quiet_NaN(), 0}));
    CHECK_FALSE(match.SubmitInput({1, 11, 0, 1, 0, 0, std::numeric_limits<float>::infinity()}));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == doctest::Approx(2.05));
    CHECK(match.Snapshot().tick == 1);
}

TEST_CASE("PvP absolute aim is not reapplied as a mouse delta and authority clamps pitch") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, 1, 0, 1, 0, std::numbers::pi_v<float> / 2, 1000}));
    Step(match, 3);
    const auto state = match.Snapshot().players[0];
    CHECK(state.position.x == doctest::Approx(2.15));
    CHECK(state.position.z == doctest::Approx(2.0));
    CHECK(state.yaw == doctest::Approx(std::numbers::pi_v<float> / 2));
    CHECK(state.pitch == doctest::Approx(89 * std::numbers::pi_v<float> / 180));
}

TEST_CASE("PvP client tick metadata never controls simulation time or input lifetime") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, 1, (std::numeric_limits<std::uint64_t>::max)(), 1, 0, 0, 0}));
    Step(match);
    REQUIRE(match.SubmitInput({1, 2, 0, 1, 0, 0, 0}));
    Step(match, 15);
    const auto stopped = match.Snapshot();
    CHECK(stopped.tick == 16);
    CHECK(stopped.players[0].position.z == doctest::Approx(2.8));
    CHECK_FALSE(match.SubmitInput({1, 2, 99999999, 1, 0, 0, 0}));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == stopped.players[0].position.z);
    CHECK_THROWS_AS(match.Tick({100, 1.0 / 60}), std::invalid_argument);
    CHECK(match.TickCount() == 17);
}

TEST_CASE("PvP forgotten movement stops after the input timeout") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput({1, 1, 0, 1, 0, 0, 0}));
    Step(match, 15);
    const auto stopped = match.Snapshot().players[0].position;
    CHECK(stopped.z == doctest::Approx(2.75));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z == stopped.z);
    Step(match, 60);
    CHECK(match.Snapshot().players[0].position.z == stopped.z);
    CHECK(stopped.z > 2);
    CHECK(stopped.z <= 2.751F);
    REQUIRE(match.SubmitInput({1, 2, 0, 1, 0, 0, 0}));
    Step(match);
    CHECK(match.Snapshot().players[0].position.z > stopped.z);
}

TEST_CASE("PvP pure collision stops at walls while players pass through each other") {
    fps::pvp::PvpMatch match(TestArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    for (std::uint64_t tick = 1; tick <= 140; ++tick) {
        REQUIRE(match.SubmitInput({1, tick, tick, 0, 1, 0, 0}));
        REQUIRE(match.SubmitInput({2, tick, tick, 0, -1, 0, 0}));
        match.Tick({tick, 1.0 / 60});
    }
    const auto players = match.Snapshot().players;
    CHECK(players[0].position.x == doctest::Approx(9).epsilon(0.0001));
    CHECK(players[1].position.x == doctest::Approx(2).epsilon(0.0001));
    CHECK(players[0].position.y == 0);
    for (std::uint64_t tick = 141; tick <= 300; ++tick) {
        REQUIRE(match.SubmitInput({1, tick, tick, 0, 1, 0, 0}));
        match.Tick({tick, 1.0 / 60});
    }
    CHECK(match.Snapshot().players[0].position.x <= 11.7501F);
    CHECK(match.Snapshot().players[0].position.x > 11.7F);
}

TEST_CASE("PvP host orders controls bounds ingress and coalesces input") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 7));
    REQUIRE(host.QueueLeave(2, 7));
    REQUIRE(host.QueueJoin(3, 8));
    CHECK_FALSE(host.SubmitInput({7, 1, 0, 1, 0, 0, 0}));
    CHECK(host.Advance(1.0 / 60).steps == 1);
    const auto results = host.TakeControlResults();
    REQUIRE(results.size() == 3);
    CHECK(results[0].requestId == 1);
    CHECK(results[1].kind == fps::pvp::ControlKind::Leave);
    CHECK(results[2].accepted);
    CHECK_FALSE(host.SubmitInput({7, 1, 0, 1, 0, 0, 0}));
    REQUIRE(host.SubmitInput({8, 1, 0, 1, 0, 0, 0}));
    REQUIRE(host.SubmitInput({8, 2, 0, 0, 1, 0, 0}));
    CHECK_FALSE(host.SubmitInput({8, 1, 0, -1, 0, 0, 0}));
    static_cast<void>(host.Advance(2.0 / 60));
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 3);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 8);
    CHECK(snapshot->players[0].position.x == doctest::Approx(2.1));
    CHECK(snapshot->players[0].position.z == 2);
    CHECK_FALSE(host.SubmitInput({8, 2, 999999, -1, 0, 0, 0}));
    for (std::uint64_t index = 1; index <= 64; ++index) CHECK(host.QueueLeave(index, 8));
    CHECK_FALSE(host.QueueLeave(65, 8));
    static_cast<void>(host.Advance(1.0 / 60));
    CHECK_FALSE(host.QueueLeave(66, 8)); // unread results also count against the bound
    CHECK(host.TakeControlResults().size() == 64);
    CHECK(host.QueueLeave(67, 8));
}

TEST_CASE("PvP host publishes exact cadence candidates and only the latest is retained") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(2.0 / 60));
    CHECK_FALSE(host.TakeSnapshot());
    REQUIRE(host.SubmitInput({1, 1, 0, 1, 0, 0, 0}));
    CHECK(host.Advance(5.0 / 60).steps == 5); // ticks 3 through 7
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 6); // not 7; captures the actual eligible world state
    CHECK(snapshot->players[0].position.z == doctest::Approx(2.2));
    CHECK_FALSE(host.TakeSnapshot());
}

TEST_CASE("PvP host Leave discards accepted pending input before a replacement joins") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 7));
    static_cast<void>(host.Advance(1.0 / 60));
    const auto joined = host.TakeControlResults();
    REQUIRE(joined.size() == 1);
    REQUIRE(joined[0].accepted);
    REQUIRE(host.SubmitInput({7, 99, 900, 0, 1, 0, 0}));
    REQUIRE(host.QueueLeave(2, 7));
    REQUIRE(host.QueueJoin(3, 8));
    static_cast<void>(host.Advance(1.0 / 60));
    const auto controls = host.TakeControlResults();
    REQUIRE(controls.size() == 2);
    CHECK(controls[0].kind == fps::pvp::ControlKind::Leave);
    CHECK(controls[0].accepted);
    CHECK(controls[1].playerId == 8);
    REQUIRE(controls[1].accepted);
    CHECK_FALSE(host.SubmitInput({7, 100, 901, 1, 0, 0, 0}));
    static_cast<void>(host.Advance(1.0 / 60));
    const auto fresh = host.TakeSnapshot();
    REQUIRE(fresh);
    REQUIRE(fresh->players.size() == 1);
    CHECK(fresh->players[0].playerId == 8);
    CHECK(fresh->players[0].position.x == 2);
    CHECK(fresh->players[0].position.z == 2);
    CHECK(fresh->players[0].lastInputSequence == 0);
    REQUIRE(host.SubmitInput({8, 1, 1, 1, 0, 0, 0}));
    static_cast<void>(host.Advance(3.0 / 60));
    const auto moved = host.TakeSnapshot();
    REQUIRE(moved);
    REQUIRE(moved->players.size() == 1);
    CHECK(moved->players[0].position.z == doctest::Approx(2.15));
    CHECK(moved->players[0].lastInputSequence == 1);
}

TEST_CASE("PvP host disconnect reset is acknowledged by simulation before a new session") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    REQUIRE(host.QueueJoin(1, 1));
    static_cast<void>(host.Advance(3.0 / 60));
    REQUIRE(host.SubmitInput({1, 1, 0, 1, 0, 0, 0}));
    REQUIRE(host.QueueJoin(2, 2));
    auto reset = host.RequestReset();
    CHECK(reset.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
    CHECK_FALSE(host.QueueJoin(3, 3));
    CHECK_FALSE(host.SubmitInput({1, 2, 0, 1, 0, 0, 0}));
    CHECK(host.Advance(1).steps == 0);
    CHECK(reset.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
    CHECK_NOTHROW(reset.get());
    CHECK(host.TakeControlResults().empty());
    CHECK_FALSE(host.TakeSnapshot());
    REQUIRE(host.QueueJoin(4, 4));
    static_cast<void>(host.Advance(3.0 / 60));
    const auto snapshot = host.TakeSnapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->tick == 3);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 4);
    CHECK(snapshot->players[0].position.z == 2);
}

TEST_CASE("PvP arena validates content without campaign or rendering dependencies") {
    auto arena = TestArena();
    std::string error;
    CHECK(arena.Validate(error));
    arena.version = 2;
    CHECK_FALSE(arena.Validate(error));
    arena.version = 1;
    arena.spawns[1] = arena.spawns[0];
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.spawns[0].position.y = 1;
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.spawns[0].position = {5.5F, 0, 5};
    CHECK_FALSE(arena.Validate(error));
    arena = TestArena();
    arena.walls[0].maximum.x = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(arena.Validate(error));
}

TEST_CASE("PvP arena loads its versioned standalone content contract") {
#ifdef PVP_DOMAIN_TEST_ARENA_PATH
    const std::filesystem::path fixture = PVP_DOMAIN_TEST_ARENA_PATH;
#else
    const auto fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures/arena.json";
#endif
    std::string error;
    const auto arena = fps::pvp::Arena::Load(fixture, error);
    INFO(error);
    REQUIRE(arena);
    CHECK(arena->id == "synthetic_contract_arena");
    CHECK(arena->version == 1);
    CHECK(arena->walls.size() == 4);
    CHECK(arena->spawns.size() == 2);
    CHECK(arena->movementSpeed == 3);
    CHECK_FALSE(fps::pvp::Arena::Load(fixture.parent_path() / "missing-arena.json", error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("PvP host owns its clock and reset completes independently of I/O") {
    fps::pvp::MatchRuntimeHost host(TestArena());
    std::jthread simulation([&](std::stop_token stop) { host.Run(stop); });
    auto reset = host.RequestReset();
    REQUIRE(reset.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK_NOTHROW(reset.get());
    REQUIRE(host.QueueJoin(1, 1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::optional<fps::pvp::WorldSnapshot> snapshot;
    while (!snapshot && std::chrono::steady_clock::now() < deadline) {
        auto candidate = host.TakeSnapshot();
        if (candidate && !candidate->players.empty()) snapshot = std::move(candidate);
        if (!snapshot) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(snapshot);
    REQUIRE(snapshot->players.size() == 1);
    CHECK(snapshot->players[0].playerId == 1);
    simulation.request_stop();
}
