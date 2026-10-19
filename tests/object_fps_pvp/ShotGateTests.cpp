#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/ShotGate.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

ShotGateView View(std::uint64_t snapshotTick, std::uint64_t ahead, std::uint64_t epoch = 3, std::uint64_t snapshotEpoch = 3) {
    return {.snapshotTick = snapshotTick, .snapshotEpoch = snapshotEpoch, .snapshotLastResolved = 50, .epoch = epoch,
            .sequence = 50 + ahead};
}

CombatState Combat(std::uint64_t nextAllowedShotTick, ActionId lastShotActionId = 0) {
    CombatState combat;
    combat.playerId = 1;
    combat.nextAllowedShotTick = nextAllowedShotTick;
    combat.lastShotActionId = lastShotActionId;
    return combat;
}

ShotDecision Decision(ActionId id, bool accepted, std::uint64_t resolvedTick, ActionKind kind = ActionKind::Shot) {
    ShotDecision decision;
    decision.actionId = id;
    decision.accepted = accepted;
    decision.resolvedTick = resolvedTick;
    decision.kind = kind;
    decision.rejection = accepted ? ShotRejection::None : ShotRejection::Cooldown;
    return decision;
}
} // namespace

TEST_CASE("PvP shot gate estimates T(S) from the snapshot in sequence space") {
    CHECK(EstimatedShotTick(View(100, 3)) == 103);
    CHECK(EstimatedShotTick(View(100, 12)) == 112);
    CHECK(EstimatedShotTick(View(100, 0)) == 100);            // S already resolved: the snapshot tick
    CHECK(EstimatedShotTick(View(100, 3, 4, 3)) == 100);      // another epoch: the snapshot tick
    ShotGateView behind = View(100, 0);
    behind.sequence = 40;
    CHECK(EstimatedShotTick(behind) == 100);
}

TEST_CASE("PvP shot gate reaches every cooldown it knows of") {
    LocalShotGate gate;
    gate.ObserveSnapshot(100, Combat(105));
    CHECK(gate.RemainingTicks(View(100, 3)) == 2);  // 103 before the snapshot's 105
    CHECK_FALSE(gate.Allows(View(100, 3)));
    CHECK(gate.Allows(View(100, 5)));
    gate.ObserveDecision(Decision(7, true, 104), 10);  // accepted at 104: 114
    CHECK(gate.RemainingTicks(View(100, 5)) == 9);
    CHECK(gate.Allows(View(100, 14)));
    gate.Submitted(8, View(100, 14), 10);  // pending at 114: 124
    CHECK(gate.RemainingTicks(View(100, 14)) == 10);
    CHECK(gate.Allows(View(100, 24)));
    // A snapshot that includes the pending shot takes over its cooldown.
    gate.ObserveSnapshot(115, Combat(124, 8));
    CHECK(gate.RemainingTicks(View(115, 9)) == 0);
    CHECK(gate.RemainingTicks(View(115, 8)) == 1);
}

TEST_CASE("PvP shot gate clears a pending shot at its decision and ignores reloads") {
    LocalShotGate gate;
    gate.ObserveSnapshot(100, Combat(0));
    gate.Submitted(4, View(100, 2), 10);
    CHECK(gate.RemainingTicks(View(100, 2)) == 10);
    gate.ObserveDecision(Decision(4, false, 102), 10);  // refused: no cooldown, pending cleared
    CHECK(gate.Allows(View(100, 2)));
    gate.ObserveDecision(Decision(5, true, 102, ActionKind::Reload), 10);
    CHECK(gate.Allows(View(100, 2)));
    gate.Reset();
    CHECK(gate.Allows(View(0, 1, 1, 1)));
}

TEST_CASE("PvP shot gate sends nothing without an active, unfrozen prediction") {
    LocalShotGate gate;
    gate.ObserveSnapshot(100, Combat(0));
    CHECK_FALSE(gate.Allows(std::nullopt));
    CHECK(gate.RemainingTicks(std::nullopt) >= 1);  // the HUD and the harness see a wait, never zero
    gate.ObserveSnapshot(100, Combat(130));
    CHECK(gate.RemainingTicks(std::nullopt) == 30);
}

namespace {
Arena GateArena() {
    Arena arena;
    arena.id = "synthetic_gate_arena";
    arena.width = arena.depth = 20;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 8}, 0}};
    return arena;
}

struct GatedRun final {
    unsigned sent{}, accepted{}, cooldown{}, held{};
};

// One Client clicking every tick through the gate against the real authority.
// The Client sees the snapshot of the previous tick, holds commands three
// steps ahead of it (S = L + 3, so T(S) = snapshot tick + 3) and gets each
// decision decisionDelay ticks after it. arrivalDelay(n) delays the n-th shot:
// 0 hands it off with the next tick (A = T(S) - 1), d >= 2 makes it late by d - 1.
GatedRun RunGatedClicks(const CombatRules& rules, unsigned ticks, unsigned decisionDelay,
                        const std::function<unsigned(unsigned)>& arrivalDelay) {
    PvpMatch match(GateArena(), rules);
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    const ShotReferenceAge age = [](std::uint64_t) -> std::optional<std::chrono::nanoseconds> { return 0ns; };
    const auto feed = [&] {
        for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
            const auto snapshot = match.Snapshot();
            for (const auto& player : snapshot.players) {
                if (player.playerId != id) continue;
                REQUIRE(match.SubmitInput({id, {{player.lastResolvedCommand + 1, 0, 0, 0, 0, false}},
                    player.movementEpoch, player.lifeGeneration}));
            }
        }
        match.Tick({match.TickCount() + 1, MovementTickSeconds}, age);
    };
    struct Seen final { std::uint64_t epoch{}, last{}; CombatState combat; };
    std::map<std::uint64_t, Seen> snapshots;
    const auto remember = [&] {
        const auto snapshot = match.Snapshot();
        for (std::size_t k = 0; k < snapshot.players.size(); ++k)
            if (snapshot.players[k].playerId == 1)
                snapshots[snapshot.tick] = {snapshot.players[k].movementEpoch, snapshot.players[k].lastResolvedCommand,
                                            snapshot.combat[k]};
    };
    for (unsigned t = 0; t < 4; ++t) {
        feed();
        remember();
    }
    LocalShotGate gate;
    GatedRun run;
    std::multimap<std::uint64_t, ShotRequest> inFlight;  // by the tick before which it is handed off
    std::map<ActionId, ShotDecision> delivered;
    ActionId next{}, acknowledged{};
    for (unsigned k = 0; k < ticks; ++k) {
        const auto now = match.TickCount();
        const auto& seen = snapshots.at(now - 1);
        gate.ObserveSnapshot(now - 1, seen.combat);
        const auto results = match.GetActionResults(1);
        REQUIRE(results);
        for (const auto& decision : results->decisions) {
            if (decision.resolvedTick + decisionDelay > now || delivered.contains(decision.actionId)) continue;
            delivered[decision.actionId] = decision;
            gate.ObserveDecision(decision, rules.cooldownTicks);
            run.accepted += decision.accepted;
            run.cooldown += decision.rejection == ShotRejection::Cooldown;
            const auto timing = match.GetActionTiming(1, decision.actionId);
            REQUIRE(timing);
            run.held += timing->held;
            CHECK(timing->resolvedTick - timing->handoffTick <= MaxShotStepLead + ShotHoldLimit(rules.cooldownTicks));
        }
        while (delivered.contains(acknowledged + 1)) ++acknowledged;
        REQUIRE(match.AcknowledgeActions(1, acknowledged));
        const ShotGateView view{.snapshotTick = now - 1, .snapshotEpoch = seen.epoch, .snapshotLastResolved = seen.last,
                                .epoch = seen.epoch, .sequence = seen.last + 3};
        if (gate.Allows(view)) {
            const ShotRequest shot{++next, now - 1, 1.5F, 0, ActionKind::Shot, 1, view.epoch, view.sequence};
            inFlight.emplace(now + arrivalDelay(run.sent), shot);
            gate.Submitted(shot.actionId, view, rules.cooldownTicks);
            ++run.sent;
        }
        for (auto shot = inFlight.begin(); shot != inFlight.end() && shot->first <= now;) {
            REQUIRE(match.SubmitActions({1, {shot->second}}) == ActionAdmission::Accepted);
            shot = inFlight.erase(shot);
        }
        feed();
        remember();
    }
    return run;
}

constexpr CombatRules LongMagazine{.magazineCapacity = 1000000};
} // namespace

// pv7 §6: within one epoch, shots sent through the gate are never refused for
// the cooldown, also when each arrives up to K ticks after its step.
TEST_CASE("PvP shots sent through the gate meet no authority cooldown") {
    unsigned salt = 12345;
    const auto delay = [&](unsigned) { salt = salt * 1103515245u + 12345u; return (salt >> 16) % 4; }; // 0..3: late by up to 2
    const auto run = RunGatedClicks(LongMagazine, 1200, 1, delay);
    CHECK(run.sent >= 100);
    CHECK(run.cooldown == 0);
    CHECK(run.accepted + 4 >= run.sent); // only the last few decisions are still on their way
    MESSAGE("gated clicks: sent " << run.sent << ", held " << run.held);
}

// A fast weapon (cooldown 4) with decisions slower than the cooldown: one shot
// late by K ticks makes the next ones wait but never refused; late by K + 1 the
// next shot is refused (network-architecture §7: d > K is refused). Late by one
// tick, the snapshot shows the late decision before the next shot leaves.
TEST_CASE("PvP a shot late by more than K ticks makes the next gated shot meet the cooldown") {
    constexpr CombatRules Fast{.cooldownTicks = 4, .magazineCapacity = 1000000};
    for (const unsigned lateness : {0u, 1u, 2u, 3u}) {
        CAPTURE(lateness);
        const auto delay = [&](unsigned n) { return n == 10 && lateness ? 1 + lateness : 0u; };
        const auto run = RunGatedClicks(Fast, 200, 6, delay);
        CHECK(run.sent >= 30);
        if (lateness <= ShotHoldLimit(Fast.cooldownTicks)) {
            CHECK(run.cooldown == 0);
            if (lateness == ShotHoldLimit(Fast.cooldownTicks)) CHECK(run.held >= 1);
        } else {
            CHECK(run.cooldown >= 1);
        }
    }
}
