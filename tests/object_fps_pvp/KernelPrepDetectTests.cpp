// v8-product-kernel-prep-detect: characterization of today's behaviour, for the
// kernel migration batches to decide against (docs/object_fps_pvp/plans/v8/
// kernel-prep.zh-Hant.md, first batch; declaration
// v8-product-kernel-prep-detect-declaration.md). Nothing here is a design
// choice: each case pins what the code does at base 1e7625d9, with the values
// read from the run and printed in the assertion messages.
//
// D1  Life's writer and timing: a lethal shot decided at tick T leaves
//     lifeState Dead in the snapshot of T itself (read-then-write inside one
//     tick, PvpMatch::Decide -> Kill); the respawn turns Alive in the snapshot
//     of R = respawnTick (ResolveLifeBoundaries at the start of tick R), and
//     the snapshot of R - 1 is still Dead. Seen from the Match and from the
//     host's published snapshots.
// D3  connectionQualityFailures: the Match never writes it (its own Snapshot
//     carries 0 through a movement reset); the host writes it on the snapshot
//     copy every tick, and a judged window's new value is already in the
//     snapshot of the judgement tick k * W.
//
// Every test case name starts with "kernel-prep" so the ctest
// object_fps_pvp.kernel_prep_detect (doctest -tc=kernel-prep*) runs exactly
// these; the mutants kp-* of mutations.json are judged by that ctest alone.
#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena DuelArena() {
    Arena arena;
    arena.id = "kernel_prep_duel";
    arena.width = arena.depth = 20;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 8}, 0}}; // The shooter at yaw 0 faces the victim.
    arena.BuildWallGrid();
    return arena;
}

std::optional<PlayerState> Player(const WorldSnapshot& snapshot, PlayerId playerId) {
    const auto player = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [=](const auto& state) { return state.playerId == playerId; });
    if (player == snapshot.players.end()) return std::nullopt;
    return *player;
}

// The reference age of the Match-level cases: the shots reference tick 1, published with age 0.
std::optional<std::int64_t> ReferenceAge(std::uint64_t tick) {
    return tick == 1 ? std::optional<std::int64_t>{0} : std::nullopt;
}

void Tick(PvpMatch& match) { match.Tick({match.TickCount() + 1, MovementTickSeconds}, ReferenceAge); }

PlayerInput Window(PlayerId playerId, std::uint64_t first, std::uint64_t last, float forward = 1) {
    PlayerInput result{playerId, {}};
    for (auto sequence = first; sequence <= last; ++sequence) result.commands.push_back({sequence, forward, 0, 0, 0});
    return result;
}

struct LifeTimeline final {
    std::uint64_t killTick{};       // T: the lethal decision's resolvedTick
    std::uint64_t respawnDue{};     // R: respawnTick as the snapshot of T says
    std::uint64_t lifeGenerationBefore{};
};

// Shoots player 1 at player 2 once per cooldown until the snapshot of a tick
// says Dead; returns that tick and the decision that killed.
template <class Snapshot, class Submit, class Advance, class Decisions>
LifeTimeline KillAndPin(Snapshot snapshot, Submit submit, Advance advance, Decisions decisions, const std::string& layer) {
    LifeTimeline timeline;
    timeline.lifeGenerationBefore = Player(snapshot(), 2)->lifeGeneration;
    ActionId actionId = 0;
    for (int shots = 0; shots < 16; ++shots) {
        ++actionId;
        submit(actionId);
        advance();
        const auto state = snapshot();
        const auto victim = Player(state, 2);
        REQUIRE(victim);
        const auto all = decisions();
        REQUIRE_FALSE(all.empty());
        const auto& last = all.back();
        REQUIRE_MESSAGE(last.actionId == actionId, layer << ": the shot of this tick is decided this tick");
        REQUIRE_MESSAGE(last.accepted, layer << ": shot " << actionId << " was accepted");
        REQUIRE_MESSAGE(last.damage > 0, layer << ": shot " << actionId << " hit");
        if (victim->lifeState == LifeState::Dead) {
            timeline.killTick = state.tick;
            timeline.respawnDue = victim->respawnTick;
            // D1 (death): read-then-write inside one tick. The decision that
            // killed was resolved at T, and the snapshot of T is already Dead.
            CHECK_MESSAGE(last.resolvedTick == state.tick,
                "kernel-prep D1 " << layer << ": lethal decision resolved at tick " << last.resolvedTick
                                  << ", snapshot Dead at tick " << state.tick);
            CHECK_MESSAGE(victim->lifeStateTick == state.tick,
                "kernel-prep D1 " << layer << ": lifeStateTick " << victim->lifeStateTick << " at snapshot tick " << state.tick);
            CHECK_MESSAGE(victim->lifeGeneration == timeline.lifeGenerationBefore,
                "kernel-prep D1 " << layer << ": death keeps the life generation");
            CHECK_MESSAGE(victim->respawnTick == state.tick + PvpCombatRules.respawnTicks,
                "kernel-prep D1 " << layer << ": respawnTick " << victim->respawnTick << " = T " << state.tick << " + "
                                  << PvpCombatRules.respawnTicks);
            MESSAGE("kernel-prep D1 " << layer << ": T=" << state.tick << " R=" << victim->respawnTick
                                      << " shots=" << actionId);
            return timeline;
        }
        for (std::uint64_t cooldown = 0; cooldown < PvpCombatRules.cooldownTicks; ++cooldown) advance();
    }
    FAIL("kernel-prep D1: no kill within 16 shots");
    return timeline;
}

// Steps to R - 1 (still Dead) and R (Alive), checking both snapshots.
template <class Snapshot, class Advance>
void PinRespawn(const LifeTimeline& timeline, Snapshot snapshot, Advance advance, const std::string& layer) {
    REQUIRE(timeline.respawnDue > timeline.killTick + 1);
    while (snapshot().tick < timeline.respawnDue - 1) {
        advance();
        const auto victim = Player(snapshot(), 2);
        REQUIRE(victim);
        CHECK_MESSAGE(victim->lifeState == LifeState::Dead,
            "kernel-prep D1 " << layer << ": Dead through tick " << snapshot().tick << " (R=" << timeline.respawnDue << ")");
    }
    const auto before = Player(snapshot(), 2);
    REQUIRE(before);
    CHECK_MESSAGE(snapshot().tick == timeline.respawnDue - 1, "kernel-prep D1 " << layer << ": at R - 1");
    CHECK_MESSAGE(before->lifeState == LifeState::Dead, "kernel-prep D1 " << layer << ": snapshot of R - 1 is Dead");
    CHECK_MESSAGE(before->lifeStateTick == timeline.killTick, "kernel-prep D1 " << layer << ": lifeStateTick still T at R - 1");
    advance();
    const auto state = snapshot();
    const auto after = Player(state, 2);
    REQUIRE(after);
    CHECK_MESSAGE(state.tick == timeline.respawnDue, "kernel-prep D1 " << layer << ": at R");
    CHECK_MESSAGE(after->lifeState == LifeState::Alive,
        "kernel-prep D1 " << layer << ": snapshot of R=" << state.tick << " is Alive");
    CHECK_MESSAGE(after->lifeStateTick == state.tick,
        "kernel-prep D1 " << layer << ": lifeStateTick " << after->lifeStateTick << " = R " << state.tick);
    CHECK_MESSAGE(after->lifeGeneration == timeline.lifeGenerationBefore + 1,
        "kernel-prep D1 " << layer << ": lifeGeneration " << after->lifeGeneration << " = "
                          << timeline.lifeGenerationBefore << " + 1");
    CHECK_MESSAGE(after->respawnTick == 0, "kernel-prep D1 " << layer << ": respawnTick cleared at R");
    MESSAGE("kernel-prep D1 " << layer << ": Alive at tick " << state.tick << ", R - T = "
                              << state.tick - timeline.killTick);
}
} // namespace

TEST_CASE("kernel-prep D1 the Match writes death in the tick of the lethal decision and life at respawnTick") {
    PvpMatch match(DuelArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    Tick(match); // Tick 1 is the published reference the shots name.
    const auto timeline = KillAndPin(
        [&] { return match.Snapshot(); },
        [&](ActionId id) { REQUIRE(match.SubmitActions({1, {{id, 1, 0, 0}}}) == ActionAdmission::Accepted); },
        [&] { Tick(match); },
        [&] { const auto results = match.GetActionResults(1); REQUIRE(results); return results->decisions; },
        std::string{"match"});
    PinRespawn(timeline, [&] { return match.Snapshot(); }, [&] { Tick(match); }, std::string{"match"});
}

TEST_CASE("kernel-prep D1 the host publishes the same death and respawn ticks") {
    std::chrono::steady_clock::time_point now{}; // Constant: every reference age is 0.
    MatchRuntimeHost host(DuelArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.QueueJoin(2, 2));
    REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
    std::optional<WorldSnapshot> latest = host.TakeSnapshot();
    REQUIRE(latest);
    REQUIRE(latest->tick == 1);
    const auto advance = [&] {
        REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
        auto published = host.TakeSnapshot();
        REQUIRE(published); // SnapshotIntervalTicks == 1: every tick publishes.
        latest = std::move(published);
    };
    const auto timeline = KillAndPin(
        [&] { return *latest; },
        [&](ActionId id) { REQUIRE(host.SubmitActions({1, {{id, 1, 0, 0}}}) == ActionAdmission::Accepted); },
        advance,
        [&] { const auto results = host.GetActionResults(1); REQUIRE(results); return results->decisions; },
        std::string{"host"});
    PinRespawn(timeline, [&] { return *latest; }, advance, std::string{"host"});
}

TEST_CASE("kernel-prep D3 the Match never writes connectionQualityFailures, even through a movement reset") {
    PvpMatch match(DuelArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.SubmitInput(Window(1, 1, 2)));
    std::uint64_t checked = 0;
    for (int step = 0; step < 42; ++step) {
        match.Tick({match.TickCount() + 1, MovementTickSeconds});
        const auto player = Player(match.Snapshot(), 1);
        REQUIRE(player);
        CHECK_MESSAGE(player->connectionQualityFailures == 0,
            "kernel-prep D3 match: the Match's snapshot carries 0 at tick " << match.TickCount());
        ++checked;
    }
    // The scenario is real: the Starvation fuse reset the epoch (PvpMatchTests
    // "counts resolved, substituted and reset movement for connection quality").
    const auto quality = match.GetMovementQuality(1);
    REQUIRE(quality);
    CHECK_MESSAGE(quality->resets == 1, "kernel-prep D3 match: one reset happened");
    MESSAGE("kernel-prep D3 match: " << checked << " snapshots, resets=" << quality->resets);
}

TEST_CASE("kernel-prep D3 the host writes connectionQualityFailures on the published copy at the judgement tick") {
    // As PvpMatchTests "evicts a player after three failed connection-quality
    // windows", with the degraded player holding the larger id (6): a write to
    // the wrong player of the ascending snapshot would land on 5.
    std::chrono::steady_clock::time_point now{};
    MatchRuntimeHost host(DuelArena(), [&] { return now; });
    REQUIRE(host.QueueJoin(1, 5));
    REQUIRE(host.QueueJoin(2, 6));
    REQUIRE(host.Advance(MovementTickSeconds).steps == 1); // The joins happen at tick 1.
    auto first = host.TakeSnapshot();
    REQUIRE(first);
    std::map<PlayerId, std::uint64_t> next{{5, 1}, {6, 1}};
    std::uint64_t latestTick = first->tick;
    std::map<std::uint64_t, std::uint32_t> degraded, clean; // tick -> failures
    std::optional<std::uint64_t> evictedAt;
    REQUIRE(Player(*first, 6));
    degraded[first->tick] = Player(*first, 6)->connectionQualityFailures;
    clean[first->tick] = Player(*first, 5)->connectionQualityFailures;
    const auto windowTicks = ConnectionQualityWindowTicks;
    while (!evictedAt && latestTick < 6 * windowTicks) {
        for (auto& [player, sequence] : next) {
            auto window = Window(player, sequence, sequence, 0);
            // Player 6 reports a snapshot twelve ticks (200 ms) old; player 5 the latest.
            window.observedAuthorityTick = player == 6 ? (latestTick > 12 ? latestTick - 12 : latestTick) : latestTick;
            REQUIRE(host.SubmitInput(window));
            ++sequence;
        }
        now += std::chrono::nanoseconds(16'666'667);
        REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
        const auto snapshot = host.TakeSnapshot();
        REQUIRE(snapshot);
        latestTick = snapshot->tick;
        const auto six = Player(*snapshot, 6);
        const auto five = Player(*snapshot, 5);
        REQUIRE(five);
        clean[latestTick] = five->connectionQualityFailures;
        if (six) degraded[latestTick] = six->connectionQualityFailures;
        else evictedAt = latestTick;
        if (!host.TakeEvictions().empty()) next.erase(6);
    }
    REQUIRE_MESSAGE(evictedAt, "kernel-prep D3 host: player 6 is evicted");
    // Read from the run: the first tick of each value.
    const auto firstTickOf = [&](std::uint32_t value) -> std::optional<std::uint64_t> {
        for (const auto& [tick, failures] : degraded) if (failures == value) return tick;
        return std::nullopt;
    };
    const auto firstOne = firstTickOf(1);
    const auto firstTwo = firstTickOf(2);
    REQUIRE(firstOne);
    REQUIRE(firstTwo);
    MESSAGE("kernel-prep D3 host: W=" << windowTicks << " failures=1 from tick " << *firstOne << ", =2 from tick "
                                      << *firstTwo << ", evicted at tick " << *evictedAt);
    // The first window after the join is not judged; each later one fails and
    // the new count is in the snapshot of the judgement tick k * W itself.
    CHECK_MESSAGE(*firstOne == 2 * windowTicks, "kernel-prep D3 host: 1 appears at tick 2W, not later");
    CHECK_MESSAGE(*firstTwo == 3 * windowTicks, "kernel-prep D3 host: 2 appears at tick 3W, not later");
    CHECK_MESSAGE(*evictedAt == 4 * windowTicks, "kernel-prep D3 host: evicted at tick 4W");
    std::uint64_t checked = 0;
    for (const auto& [tick, failures] : degraded) {
        const std::uint32_t expected = tick < 2 * windowTicks ? 0 : tick < 3 * windowTicks ? 1 : 2;
        CHECK_MESSAGE(failures == expected,
            "kernel-prep D3 host: player 6 carries " << expected << " at tick " << tick << " (every tick, not only the judgement tick)");
        ++checked;
    }
    for (const auto& [tick, failures] : clean)
        CHECK_MESSAGE(failures == 0, "kernel-prep D3 host: player 5 carries 0 at tick " << tick);
    CHECK(checked == *evictedAt - 1);
}
