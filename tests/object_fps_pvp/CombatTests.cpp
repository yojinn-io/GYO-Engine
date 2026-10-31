#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/MatchShotTrace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena CombatArena() {
    Arena arena;
    arena.id = "synthetic_combat_arena";
    arena.width = arena.depth = 20;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 8}, 0}};
    return arena;
}

void JoinCombatPlayers(PvpMatch& match) {
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
}

void CombatStep(PvpMatch& match, unsigned count = 1,
    const ShotReferenceAge& age = [](std::uint64_t tick) -> std::optional<std::chrono::nanoseconds> {
        return tick == 1 ? std::optional{0ns} : std::nullopt;
    }) {
    for (unsigned index = 0; index < count; ++index)
        match.Tick({match.TickCount() + 1, MovementTickSeconds}, age);
}

std::vector<ShotDecision> Decisions(const PvpMatch& match, PlayerId playerId = 1) {
    const auto results = match.GetActionResults(playerId);
    REQUIRE(results);
    return results->decisions;
}

CombatState CombatPlayer(const PvpMatch& match, PlayerId playerId) {
    const auto snapshot = match.Snapshot();
    const auto player = std::find_if(snapshot.combat.begin(), snapshot.combat.end(),
        [=](const auto& state) { return state.playerId == playerId; });
    REQUIRE(player != snapshot.combat.end());
    return *player;
}

PlayerState LifePlayer(const PvpMatch& match, PlayerId playerId) {
    const auto snapshot = match.Snapshot();
    const auto player = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [=](const auto& state) { return state.playerId == playerId; });
    REQUIRE(player != snapshot.players.end());
    return *player;
}

void MoveCombatPlayer(PvpMatch& match, PlayerId id, float forward = 0, float right = 0,
    float yaw = 0, float pitch = 0, bool jump = false) {
    const auto player = LifePlayer(match, id);
    REQUIRE(match.SubmitInput({id,
        {{player.lastResolvedCommand + 1, forward, right, yaw, pitch, jump}},
        player.movementEpoch, player.lifeGeneration}));
}

void DamageThreeTimes(PvpMatch& match) {
    CombatStep(match);
    for (ActionId id = 1; id <= 3; ++id) {
        REQUIRE(match.SubmitActions({1, {{id, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match);
        CHECK(Decisions(match).back().damage == 25000);
        CombatStep(match, 9);
    }
}
}

TEST_CASE("PvP shots apply authority damage once and respect the exact cooldown boundary") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    const ActionBatch first{1, {{1, 1, 0, 0}}};
    REQUIRE(match.SubmitActions(first) == ActionAdmission::Accepted);
    CHECK(CombatPlayer(match, 2).hp == 100000);
    CombatStep(match); // The first accepted shot executes at tick 2.
    const auto original = Decisions(match).front();
    CHECK(original.accepted);
    CHECK(original.resolvedTick == 2);
    CHECK(original.hitKind == ShotHitKind::Player);
    CHECK(original.targetId == 2);
    CHECK(original.damage == 25000);
    CHECK(original.kind == ActionKind::Shot);
    CHECK(original.lifeGeneration == 1);
    CHECK(original.targetLifeGeneration == 1);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 11);
    CHECK(CombatPlayer(match, 1).nextAllowedShotTick == 12);
    CHECK(CombatPlayer(match, 2).hp == 75000);

    for (unsigned repeat = 0; repeat < 40; ++repeat)
        REQUIRE(match.SubmitActions(first) == ActionAdmission::Accepted);
    // A1 (pv7 §6, D136): a shot only K = 2 ticks or less inside the cooldown is
    // held to the boundary, so the rejection is checked three ticks inside.
    CombatStep(match, 6);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 9 remains three ticks inside cooldown.
    auto decisions = Decisions(match);
    REQUIRE(decisions.size() == 2);
    CHECK(decisions.front() == original);
    CHECK_FALSE(decisions.back().accepted);
    CHECK(decisions.back().rejection == ShotRejection::Cooldown);
    CHECK(decisions.back().resolvedTick == 9);
    CHECK(CombatPlayer(match, 2).hp == 75000);
    CombatStep(match, 2);
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 12 is permitted, including after a rejection.
    CHECK(Decisions(match).back().accepted);
    CHECK(CombatPlayer(match, 2).hp == 50000);
}

TEST_CASE("PvP reference expiry uses trusted resolution age and rejection consumes no cooldown") {
    for (const auto age : {250ms - 1ns, 250ms + 0ns, 250ms + 1ns}) {
        PvpMatch match(CombatArena());
        JoinCombatPlayers(match);
        CombatStep(match);
        REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match, 1, [=](std::uint64_t) { return std::optional{age}; });
        const auto decision = Decisions(match).front();
        CHECK(decision.accepted == (age <= 250ms));
        CHECK(decision.rejection == (age <= 250ms ? ShotRejection::None : ShotRejection::Expired));
        CHECK(CombatPlayer(match, 2).hp == (age <= 250ms ? 75000 : 100000));
        if (age > 250ms) {
            CHECK(CombatPlayer(match, 1).nextAllowedShotTick == 0);
            REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
            CombatStep(match);
            CHECK(Decisions(match).back().accepted);
        }
    }

    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    const ActionBatch request{1, {{1, 1, 0, 0}}};
    REQUIRE(match.SubmitActions(request) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions(request) == ActionAdmission::Accepted);
    CombatStep(match, 1, [](std::uint64_t) { return std::optional{251ms}; });
    const auto expired = Decisions(match).front();
    CHECK(expired.rejection == ShotRejection::Expired);
    REQUIRE(match.SubmitActions(request) == ActionAdmission::Accepted);
    CombatStep(match); // A retry with a fresh callback cannot revive a terminal rejection.
    CHECK(Decisions(match).front() == expired);
    CHECK(CombatPlayer(match, 2).hp == 100000);
}

TEST_CASE("PvP unknown zero future and negative-age shot references receive terminal decisions") {
    for (const std::uint64_t reference : {0ULL, 2ULL, 99ULL}) {
        PvpMatch match(CombatArena());
        JoinCombatPlayers(match);
        CombatStep(match);
        REQUIRE(match.SubmitActions({1, {{1, reference, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // Only tick 1 has a published reference.
        const auto decision = Decisions(match).front();
        CHECK_FALSE(decision.accepted);
        CHECK(decision.rejection == ShotRejection::InvalidReference);
        CHECK(match.AcknowledgeActions(1, 1));
    }
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match, 1, [](std::uint64_t) { return std::optional{-1ns}; });
    CHECK(Decisions(match).front().rejection == ShotRejection::InvalidReference);
}

TEST_CASE("PvP accepted misses consume cooldown and burst requests have stable action ordering") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    // Intentionally reversed IDs. The first ray points upward into empty space.
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}, {1, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto decisions = Decisions(match);
    REQUIRE(decisions.size() == 2);
    CHECK(decisions[0].actionId == 1);
    CHECK(decisions[0].accepted);
    CHECK(decisions[0].hitKind == ShotHitKind::Miss);
    CHECK(decisions[0].damage == 0);
    CHECK(decisions[1].actionId == 2);
    CHECK(decisions[1].rejection == ShotRejection::Cooldown);
    CHECK(CombatPlayer(match, 2).hp == 100000);
}

TEST_CASE("PvP action conflicts and malformed batches are rejected atomically") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    const ShotRequest original{1, 1, 0, 0};
    REQUIRE(match.SubmitActions({1, {original}}) == ActionAdmission::Accepted);
    CHECK(match.SubmitActions({1, {{2, 1, 0, 0}, {1, 1, 1, 0}}}) == ActionAdmission::Conflict);
    CHECK(match.SubmitActions({1, {{2, 1, 0, 0}, {2, 1, 1, 0}}}) == ActionAdmission::Conflict);
    for (const auto invalid : {
        ShotRequest{0, 1, 0, 0},
        ShotRequest{2, 1, std::numeric_limits<float>::quiet_NaN(), 0},
        ShotRequest{2, 1, 0, std::numeric_limits<float>::infinity()},
        ShotRequest{2, 1, 1.0e6F + 1, 0},
        ShotRequest{2, 1, 0, std::numbers::pi_v<float>},
        ShotRequest{2, 1, 0, 0, ActionKind::Shot, 0},
        ShotRequest{2, 1, 0, 0, static_cast<ActionKind>(99), 1},
        ShotRequest{2, 1, 1, 0, ActionKind::Reload, 1}}) {
        CHECK(match.SubmitActions({1, {{3, 1, 0, 0}, invalid}}) == ActionAdmission::InvalidBatch);
    }
    CHECK(match.SubmitActions({1, {{1, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Conflict);
    CHECK(match.SubmitActions({1, {{1, 1, 0, 0, ActionKind::Shot, 2}}}) == ActionAdmission::Conflict);
    CHECK(match.SubmitActions({1, {}}) == ActionAdmission::InvalidBatch);
    CHECK(match.SubmitActions({0, {original}}) == ActionAdmission::InvalidPlayer);
    CHECK(match.SubmitActions({99, {original}}) == ActionAdmission::InvalidPlayer);
    CombatStep(match);
    REQUIRE(Decisions(match).size() == 1);
    CHECK(Decisions(match).front().actionId == 1);
    CHECK(CombatPlayer(match, 2).hp == 75000);
    CHECK(match.SubmitActions({1, {{1, 1, 0.1F, 0}, {2, 1, 0, 0}}}) == ActionAdmission::Conflict);
    CombatStep(match);
    CHECK(Decisions(match).size() == 1); // Immutable after resolution as well as before it.
}

TEST_CASE("PvP action gaps resolve immediately but retirement requires contiguous terminal decisions") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CHECK_FALSE(match.CanAcknowledgeActions(1, 2));
    CHECK_FALSE(match.AcknowledgeActions(1, 2));
    CombatStep(match);
    CHECK(CombatPlayer(match, 2).hp == 75000); // No wait for missing action 1.
    CHECK_FALSE(match.AcknowledgeActions(1, 2));
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CHECK_FALSE(match.AcknowledgeActions(1, 1)); // Pending is not terminal yet.
    CombatStep(match);
    const auto retained = Decisions(match);
    REQUIRE(retained.size() == 2);
    CHECK(retained[0].rejection == ShotRejection::Cooldown);
    CHECK(retained[1].accepted);
    CombatStep(match, 300);
    CHECK(Decisions(match) == retained); // ACK loss never evicts an outstanding decision.
    CHECK_FALSE(match.AcknowledgeActions(1, 3));
    CHECK(match.CanAcknowledgeActions(1, 2));
    CHECK(Decisions(match) == retained); // Validation itself has no mutation.
    REQUIRE(match.AcknowledgeActions(1, 2));
    CHECK(match.GetActionResults(1)->retiredThrough == 2);
    CHECK(Decisions(match).empty());
    CHECK(match.AcknowledgeActions(1, 2));
    CHECK(match.AcknowledgeActions(1, 1));
    CHECK(match.AcknowledgeActions(1, 0));
    REQUIRE(match.SubmitActions({1, {{1, 1, 1, 0}, {2, 1, 1, 0}}}) == ActionAdmission::Accepted);
    CHECK(match.SubmitActions({1, {{1, 1, 0, 0}, {1, 1, 1, 0}, {3, 1, 0, 0}}}) == ActionAdmission::Conflict);
    CombatStep(match);
    CHECK(Decisions(match).empty()); // Even changed content at/below the retired floor is inert.
    CHECK(CombatPlayer(match, 2).hp == 75000);
    CHECK_FALSE(match.GetActionResults(99));
    CHECK_FALSE(match.AcknowledgeActions(99, 0));
}

TEST_CASE("PvP action capacity and sequence distance stay bounded until acknowledgement") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    ActionBatch excessive{1, {}};
    for (ActionId id = 1; id <= MaxActionBatch + 1; ++id) excessive.shots.push_back({id, 1, 0, 0});
    CHECK(match.SubmitActions(excessive) == ActionAdmission::InvalidBatch);
    CHECK(match.SubmitActions({1, {{33, 1, 0, 0}}}) == ActionAdmission::OutsideWindow);
    CHECK(match.SubmitActions({1, {{std::numeric_limits<ActionId>::max(), 1, 0, 0}}}) == ActionAdmission::OutsideWindow);
    for (ActionId first = 1; first <= MaxActionWindow; first += MaxActionBatch) {
        ActionBatch batch{1, {}};
        for (ActionId id = first; id < first + MaxActionBatch; ++id) batch.shots.push_back({id, 1, 0, 0});
        REQUIRE(match.SubmitActions(batch) == ActionAdmission::Accepted);
    }
    REQUIRE(match.SubmitActions({1, {{32, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CHECK_FALSE(match.AcknowledgeActions(1, 32));
    CombatStep(match);
    REQUIRE(Decisions(match).size() == MaxActionWindow);
    REQUIRE(match.SubmitActions({1, {{32, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CHECK(match.SubmitActions({1, {{33, 1, 0, 0}}}) == ActionAdmission::OutsideWindow);
    REQUIRE(match.AcknowledgeActions(1, 1));
    REQUIRE(match.SubmitActions({1, {{33, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CHECK(match.SubmitActions({1, {{34, 1, 0, 0}}}) == ActionAdmission::OutsideWindow);
    CombatStep(match);
    CHECK(Decisions(match).size() == MaxActionWindow);
    REQUIRE(match.AcknowledgeActions(1, 33));
    CHECK(Decisions(match).empty());
    CHECK(match.GetActionResults(1)->retiredThrough == 33);
}

TEST_CASE("PvP staged action admission validates immutable contents without changing authority") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    const std::vector<ShotRequest> staged{{1, 1, 0, 0}};
    CHECK(match.CanSubmitActions({1, {{1, 1, 0, 0}, {2, 1, 0, 0}}}, staged) == ActionAdmission::Accepted);
    CHECK(match.CanSubmitActions({1, {{1, 1, 1, 0}, {2, 1, 0, 0}}}, staged) == ActionAdmission::Conflict);
    CombatStep(match);
    CHECK(Decisions(match).empty());
    CHECK(CombatPlayer(match, 2).hp == 100000);
}

TEST_CASE("PvP shots use post-movement authority poses for every participant and absolute request aim") {
    auto arena = CombatArena();
    arena.movementSpeed = 60;
    arena.spawns[1].position.x = 3;
    PvpMatch match(arena);
    JoinCombatPlayers(match);
    CombatStep(match);
    REQUIRE(match.SubmitInput({1, {{1, 0, 1, 0, 0}}}));
    REQUIRE(match.SubmitInput({2, {{1, 0, -1, 0, 0}}}));
    // Crossing sideways makes both the entirely old world and a half-moved
    // world miss this ray. Only the completed authority poses are a hit.
    const float aim = std::atan2(-1.0F, 6.0F);
    REQUIRE(match.SubmitActions({1, {{1, 1, aim, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto snapshot = match.Snapshot();
    CHECK(snapshot.players[0].position.x == 3);
    CHECK(snapshot.players[1].position.x == 2);
    CHECK(snapshot.players[0].yaw == 0); // Shot aim does not overwrite movement view state.
    const auto decision = Decisions(match).front();
    CHECK(decision.accepted);
    CHECK(decision.hitKind == ShotHitKind::Player);
    CHECK(decision.targetId == 2);
    CHECK(CombatPlayer(match, 2).hp == 75000);
}

TEST_CASE("PvP movement epoch reset preserves pending actions retained decisions and HP") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    REQUIRE(match.SubmitInput({1, {{1, 0, 0, 0, 0}}}));
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto original = Decisions(match).front();
    CombatStep(match, 28);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Thirty running movement ticks schedule exhausted-lead recovery.
    REQUIRE(match.Snapshot().players.front().movementEpoch == 1);
    const auto cooldown = CombatPlayer(match, 1).nextAllowedShotTick;
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // The movement reset boundary must still resolve the pending action.
    CHECK(match.Snapshot().players.front().movementEpoch == 2);
    const auto decisions = Decisions(match);
    REQUIRE(decisions.size() == 3);
    CHECK(decisions.front() == original);
    CHECK(decisions[1].accepted);
    CHECK(decisions.back().rejection == ShotRejection::Cooldown);
    CHECK(CombatPlayer(match, 1).nextAllowedShotTick == cooldown);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 10);
    CHECK(CombatPlayer(match, 1).lifeGeneration == 1);
    CHECK(match.Snapshot().players.front().lifeGeneration == 1);
    CHECK(CombatPlayer(match, 2).hp == 50000);
    REQUIRE(match.AcknowledgeActions(1, 3));
    CHECK(match.GetActionResults(1)->retiredThrough == 3);
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match, 20);
    CHECK(Decisions(match).empty());
    CHECK(CombatPlayer(match, 2).hp == 50000);
}

TEST_CASE("PvP twelve world or miss shots exhaust ammo before cooldown and reload completes on tick ninety") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    for (ActionId id = 1; id <= 12; ++id) {
        const float pitch = id % 2 ? -0.5F : 0.5F;
        REQUIRE(match.SubmitActions({1, {{id, 1, 0, pitch}}}) == ActionAdmission::Accepted);
        CombatStep(match);
        const auto decision = Decisions(match).back();
        REQUIRE(decision.accepted);
        CHECK(decision.hitKind == (id % 2 ? ShotHitKind::Miss : ShotHitKind::World));
        CHECK(decision.targetLifeGeneration == 0);
        CHECK(CombatPlayer(match, 1).magazineAmmo == 12 - id);
        if (id < 12) CombatStep(match, 9);
    }
    const auto emptied = CombatPlayer(match, 1);
    CHECK(emptied.lastShotActionId == 12);
    CHECK(emptied.lastShotTick == 112);
    REQUIRE(match.SubmitActions({1, {{13, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match).back().rejection == ShotRejection::EmptyMagazine);
    CHECK(CombatPlayer(match, 1) == emptied);

    const ShotRequest reload{14, 1, 0, 0, ActionKind::Reload, 1};
    REQUIRE(match.SubmitActions({1, {reload}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto started = CombatPlayer(match, 1);
    CHECK(started.magazineAmmo == 0);
    CHECK(started.reloadActionId == 14);
    CHECK(started.reloadStartTick == 114);
    CHECK(started.reloadEndTick == 204);
    const auto acceptedReload = Decisions(match).back();
    CHECK(acceptedReload.accepted);
    CHECK(acceptedReload.kind == ActionKind::Reload);
    CHECK(acceptedReload.hitKind == ShotHitKind::Miss);
    CHECK(acceptedReload.targetId == 0);
    CHECK(acceptedReload.damage == 0);

    REQUIRE(match.SubmitActions({1, {reload, {15, 1, 0, 0},
        {16, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    MoveCombatPlayer(match, 1, 1, 0, 0, 0, true);
    CombatStep(match);
    CHECK(LifePlayer(match, 1).position.y > 0);
    CHECK(LifePlayer(match, 1).position.z > 2);
    CHECK(Decisions(match)[14].rejection == ShotRejection::Reloading);
    CHECK(Decisions(match)[15].rejection == ShotRejection::Reloading);
    CHECK(CombatPlayer(match, 1) == started);
    while (match.TickCount() < started.reloadEndTick - 1) CombatStep(match);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 0);
    REQUIRE(match.SubmitActions({1, {{17, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match).back().accepted);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 11);
    CHECK(CombatPlayer(match, 1).reloadActionId == 0);
    CHECK(CombatPlayer(match, 1).reloadStartTick == 0);
    CHECK(CombatPlayer(match, 1).reloadEndTick == 0);
    REQUIRE(match.SubmitActions({1, {reload}}) == ActionAdmission::Accepted);
    CombatStep(match, 90);
    CHECK(Decisions(match)[13] == acceptedReload);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 11); // A late duplicate cannot refill again.
    CHECK(CombatPlayer(match, 2).hp == 100000);
}

TEST_CASE("PvP reload format and gameplay rejections remain terminal and side effect free") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match).back().rejection == ShotRejection::MagazineFull);
    CHECK(CombatPlayer(match, 1).reloadActionId == 0);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto spent = CombatPlayer(match, 1);
    REQUIRE(match.SubmitActions({1, {{3, 0, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match).back().rejection == ShotRejection::InvalidReference);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match, 1, [](std::uint64_t) { return std::optional{251ms}; });
    CHECK(Decisions(match).back().rejection == ShotRejection::Expired);
    CHECK(CombatPlayer(match, 1) == spent);
    REQUIRE(match.SubmitActions({1, {{5, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto end = CombatPlayer(match, 1).reloadEndTick;
    CHECK(end == match.TickCount() + 90); // Missing only one round still takes ninety ticks.
    REQUIRE(match.AcknowledgeActions(1, 5));
    while (match.TickCount() < end) CombatStep(match);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 12);
}

TEST_CASE("PvP lethal action immediately prevents same tick retaliation and corpse hits") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match); // Tick 31, victim has 25 HP and both weapons are ready.
    REQUIRE(match.SubmitActions({2, {{1, 1, std::numbers::pi_v<float>, 0}}}) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto dead = LifePlayer(match, 2);
    CHECK(dead.lifeState == LifeState::Dead);
    CHECK(dead.lifeStateTick == 32);
    CHECK(dead.respawnTick == 212);
    CHECK(dead.lifeGeneration == 1);
    CHECK(CombatPlayer(match, 2).hp == 0);
    CHECK(CombatPlayer(match, 2).magazineAmmo == 12);
    CHECK(CombatPlayer(match, 1).hp == 100000);
    CHECK(Decisions(match, 2).front().rejection == ShotRejection::Dead);
    const auto lethal = Decisions(match).back();
    CHECK(lethal.damage == 25000);
    CHECK(lethal.targetLifeGeneration == 1);
    MoveCombatPlayer(match, 2, 1, 1, 1, 0.5F, true);
    REQUIRE(match.SubmitActions({2, {{2, 0, 0, 0, ActionKind::Reload, 1},
        {3, 0, 0, 0, ActionKind::Shot, 2}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto neutral = LifePlayer(match, 2);
    CHECK(neutral.position.x == dead.position.x);
    CHECK(neutral.position.y == dead.position.y);
    CHECK(neutral.position.z == dead.position.z);
    CHECK(neutral.yaw == dead.yaw);
    CHECK(neutral.pitch == dead.pitch);
    CHECK(neutral.lastResolvedCommand == 1);
    CHECK(Decisions(match, 2)[1].rejection == ShotRejection::Dead); // Before invalid reference.
    CHECK(Decisions(match, 2)[2].rejection == ShotRejection::InvalidLife); // Before death.
    CombatStep(match, 8);
    REQUIRE(match.SubmitActions({1, {{5, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match).back().accepted);
    CHECK(Decisions(match).back().hitKind == ShotHitKind::Miss);
    CHECK(Decisions(match).back().damage == 0);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 7);
    std::string error;
    REQUIRE(match.Join(2, error));
    CHECK(LifePlayer(match, 2).lifeState == LifeState::Dead);
    CHECK(Decisions(match, 2).size() == 3);
}

TEST_CASE("PvP respawn resets movement and combat while retaining cross life action ledger and ids") {
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } trace;
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({2, {{1, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    CombatStep(match); // A miss spends one victim round before the lethal shot.
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions({2, {{2, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Player 1 kills first: victim reload never starts.
    const auto due = LifePlayer(match, 2).respawnTick;
    const auto retained = Decisions(match, 2);
    REQUIRE(retained.size() == 2);
    CHECK(retained[0].accepted);
    CHECK(retained[1].rejection == ShotRejection::Dead);
    while (match.TickCount() < due - 1) {
        MoveCombatPlayer(match, 2); // Normal dead neutral stream avoids starvation recovery.
        CombatStep(match);
    }
    const auto before = LifePlayer(match, 2);
    REQUIRE(before.lifeState == LifeState::Dead);
    CHECK(before.lifeGeneration == 1);
    REQUIRE(match.SubmitInput({2,
        {{before.lastResolvedCommand + 1, 1, 0, 0, 0, true},
         {before.lastResolvedCommand + 2, 1, 0, 0, 0, false}}, before.movementEpoch, 1}));
    REQUIRE(match.SubmitActions({2, {
        {1, 1, 0, -0.5F}, // Retained old decision stays immutable.
        {3, 0, 0, 0, ActionKind::Reload, 1}, // Unresolved old life receives a terminal rejection.
        {4, 0, 0, 0, ActionKind::Shot, 3},
        {5, 1, std::numbers::pi_v<float>, 0, ActionKind::Shot, 2}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto respawned = LifePlayer(match, 2);
    CHECK(match.TickCount() == due);
    CHECK(respawned.lifeState == LifeState::Alive);
    CHECK(respawned.lifeGeneration == 2);
    CHECK(respawned.lifeStateTick == due);
    CHECK(respawned.respawnTick == 0);
    CHECK(respawned.movementEpoch == before.movementEpoch + 1);
    CHECK(respawned.lastResolvedCommand == 0);
    CHECK(respawned.contiguousPendingCommands == 0);
    CHECK(respawned.position.x == 2);
    CHECK(respawned.position.y == 0);
    CHECK(respawned.position.z == 8);
    CHECK(respawned.grounded);
    CHECK(respawned.verticalVelocity == 0);
    const auto combat = CombatPlayer(match, 2);
    CHECK(combat.lifeGeneration == 2);
    CHECK(combat.hp == 100000);
    CHECK(combat.magazineAmmo == 11); // Same-boundary action 5 consumes the new magazine.
    CHECK(combat.lastShotActionId == 5);
    CHECK(combat.lastShotTick == due);
    CHECK(combat.reloadActionId == 0);
    const auto decisions = Decisions(match, 2);
    REQUIRE(decisions.size() == 5);
    CHECK(decisions[0] == retained[0]);
    CHECK(decisions[1] == retained[1]);
    CHECK(decisions[2].rejection == ShotRejection::StaleLife);
    CHECK(decisions[2].kind == ActionKind::Reload);
    CHECK(decisions[2].lifeGeneration == 1);
    CHECK(decisions[3].rejection == ShotRejection::InvalidLife);
    CHECK(decisions[4].accepted);
    CHECK(decisions[4].lifeGeneration == 2);
    CHECK(decisions[4].targetLifeGeneration == 1);
    CHECK(CombatPlayer(match, 1).hp == 75000);
    CHECK_FALSE(match.SubmitInput({2, {{1, 1, 0, 0, 0}}, respawned.movementEpoch, 1}));
    CHECK_FALSE(match.SubmitInput({2, {{1, 1, 0, 0, 0}}, before.movementEpoch, 2}));
    CHECK_FALSE(match.SubmitInput({2, {{1, 1, 0, 0, 0}}, respawned.movementEpoch, 3}));
    CHECK(match.SubmitInput({2, {{1, 1, 0, 0, 0}}, respawned.movementEpoch, 2}));
    const auto events = trace.trace->Drain();
    unsigned lifeResets{};
    for (const auto& event : events) {
        if (event.kind == MovementTraceKind::Reset && event.resetReason == MovementResetReason::LifeRespawn) {
            ++lifeResets;
            CHECK(event.playerId == 2);
            CHECK(event.authorityTick == due);
            CHECK(event.epoch == respawned.movementEpoch);
            CHECK(event.lifeGeneration == 2);
            CHECK(event.count == 2);
        }
        CHECK_FALSE((event.kind == MovementTraceKind::Resolved && event.playerId == 2 && event.authorityTick == due));
    }
    CHECK(lifeResets == 1);
    REQUIRE(match.AcknowledgeActions(2, 5));
    CHECK(match.GetActionResults(2)->retiredThrough == 5);
    CHECK(Decisions(match, 2).empty());
    REQUIRE(match.Leave(2));
    std::string error;
    REQUIRE(match.Join(2, error));
    CHECK(LifePlayer(match, 2).lifeGeneration == 1);
    CHECK(CombatPlayer(match, 2).magazineAmmo == 12);
    CHECK(match.GetActionResults(2)->retiredThrough == 0);
}

TEST_CASE("PvP death cancels an active reload and keeps airborne neutral gravity") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({2, {{1, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(match.SubmitActions({2, {{2, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(CombatPlayer(match, 2).reloadActionId == 2);
    const auto originalEnd = CombatPlayer(match, 2).reloadEndTick;
    MoveCombatPlayer(match, 2, 0, 0, 0, 0, true);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto dead = LifePlayer(match, 2);
    REQUIRE(dead.lifeState == LifeState::Dead);
    REQUIRE(dead.position.y > 0);
    REQUIRE(dead.verticalVelocity > 0);
    CHECK_FALSE(dead.grounded);
    CHECK(CombatPlayer(match, 2).magazineAmmo == 11);
    CHECK(CombatPlayer(match, 2).reloadActionId == 0);
    CHECK(CombatPlayer(match, 2).reloadStartTick == 0);
    CHECK(CombatPlayer(match, 2).reloadEndTick == 0);
    float maximumHeight = dead.position.y;
    while (match.TickCount() <= originalEnd) {
        MoveCombatPlayer(match, 2, 1, 1, 1.5F, 1, true);
        CombatStep(match);
        const auto current = LifePlayer(match, 2);
        maximumHeight = (std::max)(maximumHeight, current.position.y);
        CHECK(current.position.x == dead.position.x);
        CHECK(current.position.z == dead.position.z);
        CHECK(current.yaw == dead.yaw);
        CHECK(current.pitch == dead.pitch);
        CHECK(current.lifeState == LifeState::Dead);
        CHECK(CombatPlayer(match, 2).magazineAmmo == 11);
    }
    CHECK(maximumHeight == doctest::Approx(0.6).epsilon(0.002));
    CHECK(LifePlayer(match, 2).grounded);
    CHECK(LifePlayer(match, 2).position.y == 0);
    CHECK(LifePlayer(match, 2).verticalVelocity == 0);
    const auto due = dead.respawnTick;
    while (match.TickCount() < due) CombatStep(match);
    CHECK(CombatPlayer(match, 2).magazineAmmo == 12);
    CHECK(CombatPlayer(match, 2).hp == 100000);
    CHECK(CombatPlayer(match, 2).nextAllowedShotTick == 0);
    CHECK(CombatPlayer(match, 2).lastShotActionId == 0);
    CHECK(CombatPlayer(match, 2).lastShotTick == 0);
}

TEST_CASE("PvP a fully blocked respawn retains its due deadline and retries until one capsule fits") {
    auto arena = CombatArena();
    arena.movementSpeed = 18; // One command is exactly the half-spawn separation.
    arena.spawns[1].position.z = 2.6F;
    PvpMatch match(arena);
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto dead = LifePlayer(match, 2);
    MoveCombatPlayer(match, 1, 1);
    CombatStep(match); // Live capsule at the midpoint overlaps both spawn capsules.
    MoveCombatPlayer(match, 1);
    CombatStep(match);
    while (match.TickCount() < dead.respawnTick + 2) CombatStep(match);
    const auto blocked = LifePlayer(match, 2);
    CHECK(blocked.lifeState == LifeState::Dead);
    CHECK(blocked.lifeGeneration == 1);
    CHECK(blocked.respawnTick == dead.respawnTick);
    CHECK(blocked.movementEpoch == dead.movementEpoch);
    MoveCombatPlayer(match, 1, 0, 1);
    CombatStep(match);
    MoveCombatPlayer(match, 1, 0, 1);
    CombatStep(match);
    CHECK(LifePlayer(match, 2).lifeState == LifeState::Dead); // Spawn is checked before movement.
    MoveCombatPlayer(match, 1);
    CombatStep(match);
    const auto alive = LifePlayer(match, 2);
    CHECK(alive.lifeState == LifeState::Alive);
    CHECK(alive.lifeGeneration == 2);
    CHECK(alive.movementEpoch == blocked.movementEpoch + 1);
    CHECK(alive.position.z == 2); // Exact distance tie uses configured first spawn.
    CHECK(alive.lastResolvedCommand == 0);
}

TEST_CASE("PvP respawn selects the farthest free spawn and dead opponents do not block joins") {
    SUBCASE("Farthest is selected even when the first spawn is free") {
        auto arena = CombatArena();
        arena.movementSpeed = 60;
        PvpMatch match(arena);
        JoinCombatPlayers(match);
        DamageThreeTimes(match);
        REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match);
        const auto due = LifePlayer(match, 2).respawnTick;
        MoveCombatPlayer(match, 1, 1);
        CombatStep(match); // Opponent now z=3: both free; second spawn is farther away.
        MoveCombatPlayer(match, 1);
        CombatStep(match);
        while (match.TickCount() < due) CombatStep(match);
        CHECK(LifePlayer(match, 2).lifeState == LifeState::Alive);
        CHECK(LifePlayer(match, 2).position.z == 8);
    }
    SUBCASE("With no living opponent the first free spawn wins") {
        PvpMatch match(CombatArena());
        JoinCombatPlayers(match);
        DamageThreeTimes(match);
        REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match);
        const auto due = LifePlayer(match, 2).respawnTick;
        REQUIRE(match.Leave(1));
        while (match.TickCount() < due) CombatStep(match);
        CHECK(LifePlayer(match, 2).position.z == 2);
        CHECK(LifePlayer(match, 2).lifeGeneration == 2);
    }
}

TEST_CASE("PvP hit record is written only by player hits and follows the victim's life") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    CHECK(CombatPlayer(match, 2).lastDamageTick == 0);
    CHECK(CombatPlayer(match, 2).damageCount == 0);
    CHECK(CombatPlayer(match, 2).lastAttackerId == 0);

    // A miss, a reload of a full magazine and a cooldown rejection write nothing.
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, -0.5F}}}) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions({2, {{1, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(Decisions(match).back().hitKind == ShotHitKind::Miss);
    CHECK(Decisions(match).back().hitPart == HitPart::Unspecified); // pv7 §7: only a player hit has a part
    CHECK(Decisions(match, 2).back().hitPart == HitPart::Unspecified);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(Decisions(match).back().rejection == ShotRejection::Cooldown);
    CHECK(CombatPlayer(match, 1).damageCount == 0);
    CHECK(CombatPlayer(match, 2).damageCount == 0);

    // Same-tick mutual fire: each victim records the other as attacker.
    CombatStep(match, 8);
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions({2, {{2, 1, std::numbers::pi_v<float>, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto tick = match.TickCount();
    REQUIRE(Decisions(match).back().hitKind == ShotHitKind::Player);
    REQUIRE(Decisions(match, 2).back().hitKind == ShotHitKind::Player);
    // pv7 §7: v7.1 has no hitbox, so every player hit is Body, never Head.
    CHECK(Decisions(match).back().hitPart == HitPart::Body);
    CHECK(Decisions(match, 2).back().hitPart == HitPart::Body);
    for (const auto& [victim, attacker] : {std::pair<PlayerId, PlayerId>{2, 1}, {1, 2}}) {
        const auto record = CombatPlayer(match, victim);
        CHECK(record.lastDamageTick == tick);
        CHECK(record.damageCount == 1);
        CHECK(record.lastAttackerId == attacker);
        CHECK(record.lastHitPart == HitPart::Body);
        CHECK(record.hp == PvpCombatRules.maximumHp - PvpCombatRules.shotDamage);
    }

    // A retransmitted action is not resolved again.
    const auto before = CombatPlayer(match, 2);
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match, 10);
    CHECK(CombatPlayer(match, 2) == before);
}

TEST_CASE("PvP lethal hit records the killer at the death tick and respawn clears the record") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match); // Tick 31, victim has 25 HP and both weapons are ready.
    CHECK(CombatPlayer(match, 2).damageCount == 3);
    REQUIRE(match.SubmitActions({2, {{1, 1, std::numbers::pi_v<float>, 0}}}) == ActionAdmission::Accepted);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    const auto dead = LifePlayer(match, 2);
    const auto record = CombatPlayer(match, 2);
    REQUIRE(dead.lifeState == LifeState::Dead);
    CHECK(record.lastDamageTick == dead.lifeStateTick);
    CHECK(record.damageCount == 4);
    CHECK(record.lastAttackerId == 1);
    CHECK(record.hp == PvpCombatRules.maximumHp - std::min(PvpCombatRules.maximumHp, record.damageCount * PvpCombatRules.shotDamage));
    // The dead victim's same-tick shot is rejected and writes nothing.
    CHECK(Decisions(match, 2).front().rejection == ShotRejection::Dead);
    CHECK(CombatPlayer(match, 1).damageCount == 0);
    CHECK(ValidDamageRecord(record, true, dead.lifeStateTick, match.TickCount(), PvpCombatRules.maximumHp));

    while (match.TickCount() < dead.respawnTick - 1) {
        MoveCombatPlayer(match, 2);
        CombatStep(match);
        CHECK(CombatPlayer(match, 2).lastDamageTick == dead.lifeStateTick); // Kept while dead.
    }
    CombatStep(match);
    REQUIRE(LifePlayer(match, 2).lifeGeneration == 2);
    CHECK(CombatPlayer(match, 2).lastDamageTick == 0);
    CHECK(CombatPlayer(match, 2).damageCount == 0);
    CHECK(CombatPlayer(match, 2).lastAttackerId == 0);
    CHECK(CombatPlayer(match, 2).lastHitPart == HitPart::Unspecified); // the fourth field resets with the rest
}

TEST_CASE("PvP hit record decode check accepts only the contract shapes") {
    // pv7 §7: four fields, the hit part being Body or Head when set.
    CombatState record{.playerId = 1, .hp = 50000, .lastDamageTick = 15, .damageCount = 2, .lastAttackerId = 7,
                       .lastHitPart = HitPart::Body};
    CHECK(ValidDamageRecord(record, false, 10, 20, 100000));
    CHECK(ValidDamageRecord(CombatState{.playerId = 1}, false, 10, 20, 100000));
    auto bad = [&](auto edit, bool dead = false, std::uint64_t lifeStateTick = 10) {
        auto copy = record;
        edit(copy);
        return !ValidDamageRecord(copy, dead, lifeStateTick, 20, 100000);
    };
    CHECK(bad([](CombatState& c) { c.lastDamageTick = 0; }));
    CHECK(bad([](CombatState& c) { c.damageCount = 0; }));
    CHECK(bad([](CombatState& c) { c.lastAttackerId = 0; }));
    CHECK(bad([](CombatState& c) { c.lastHitPart = HitPart::Unspecified; }));
    CHECK(bad([](CombatState& c) { c.lastHitPart = static_cast<HitPart>(4); }));
    CHECK(bad([](CombatState& c) { c.lastHitPart = static_cast<HitPart>(99); }));
    CHECK(bad([](CombatState& c) { c = CombatState{.playerId = 1, .lastHitPart = HitPart::Body}; }));
    CHECK_FALSE(bad([](CombatState& c) { c.lastHitPart = HitPart::Head; })); // v8's part, accepted already
    CHECK_FALSE(bad([](CombatState& c) { c.lastHitPart = HitPart::Leg; }));  // pv9 §2
    CHECK(bad([](CombatState& c) { c.lastDamageTick = 9; }));
    CHECK(bad([](CombatState& c) { c.lastDamageTick = 21; }));
    CHECK(bad([](CombatState& c) { c.lastAttackerId = 1; }));
    CHECK(bad([](CombatState& c) { c.damageCount = 100001; }));
    CHECK_FALSE(bad([](CombatState& c) { c.damageCount = 100000; }));
    CHECK(bad([](CombatState& c) { c = CombatState{.playerId = 1}; }, true, 18));
    CHECK(bad([](CombatState&) {}, true, 18)); // Dead of an older hit.
    CHECK_FALSE(bad([](CombatState& c) { c.lastDamageTick = 18; }, true, 18));
}

namespace {
// Four spawns on a square: joins take (2,2), (8,8), then (2,8) and (8,2) in
// content order on the tie (pv6 contract §5). Player 2 at (8,8) is in the line
// of fire of player 3 at (2,8) facing +X, player 4 at (8,2) facing +Z and
// player 1 at (2,2) facing the diagonal.
Arena QuadArena() {
    Arena arena = CombatArena();
    arena.id = "synthetic_quad_arena";
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 8}, 0}, {{8, 0, 8}, 0}, {{8, 0, 2}, 0}};
    return arena;
}

constexpr float AtVictim[] = {0, std::numbers::pi_v<float> / 4, 0, std::numbers::pi_v<float> / 2, 0};

void JoinFour(PvpMatch& match) {
    std::string error;
    for (PlayerId id = 1; id <= 4; ++id) REQUIRE_MESSAGE(match.Join(id, error), error);
}

void Fire(PvpMatch& match, PlayerId shooter, ActionId id) {
    REQUIRE(match.SubmitActions({shooter, {{id, 1, AtVictim[shooter], 0}}}) == ActionAdmission::Accepted);
}
} // namespace

TEST_CASE("PvP rooms hold four players: the fifth is full and a two-spawn arena blocks the third") {
    static_assert(MaxPlayers == 4);
    PvpMatch match(QuadArena());
    JoinFour(match);
    const auto snapshot = match.Snapshot();
    REQUIRE(snapshot.players.size() == 4);
    const auto at = [&](PlayerId id) { return LifePlayer(match, id).position; };
    CHECK((at(1).x == 2 && at(1).z == 2));
    CHECK((at(2).x == 8 && at(2).z == 8));
    CHECK((at(3).x == 2 && at(3).z == 8));
    CHECK((at(4).x == 8 && at(4).z == 2));
    std::string error;
    CHECK(match.Join(3, error)); // A rejoin of a present player is idempotent.
    CHECK_FALSE(match.Join(5, error));
    CHECK(error == "match_full");

    PvpMatch narrow(CombatArena());
    JoinCombatPlayers(narrow);
    CHECK_FALSE(narrow.Join(3, error));
    CHECK(error == "spawn_blocked");
}

TEST_CASE("PvP same tick shots from two and three players all hit and the record keeps the last attacker") {
    for (const std::vector<PlayerId>& shooters : {std::vector<PlayerId>{3, 4}, std::vector<PlayerId>{1, 3, 4}}) {
        CAPTURE(shooters.size());
        PvpMatch match(QuadArena());
        JoinFour(match);
        CombatStep(match);
        for (const auto shooter : shooters) Fire(match, shooter, 1);
        CombatStep(match);
        for (const auto shooter : shooters) {
            const auto decision = Decisions(match, shooter).back();
            CAPTURE(shooter);
            CHECK(decision.hitKind == ShotHitKind::Player);
            CHECK(decision.targetId == 2);
            CHECK(decision.damage == 25000);
        }
        const auto victim = CombatPlayer(match, 2);
        CHECK(victim.hp == 100000 - 25000 * shooters.size());
        CHECK(victim.damageCount == shooters.size());
        // Adjudication order is player id within the tick: the highest id hits last.
        CHECK(victim.lastAttackerId == shooters.back());
        CHECK(victim.lastDamageTick == 2);
    }
}

TEST_CASE("PvP a lethal hit inside a tick lets the later shots of that tick pass through the body") {
    PvpMatch match(QuadArena());
    JoinFour(match);
    CombatStep(match);
    // Two earlier hits leave 50 HP.
    Fire(match, 1, 1);
    CombatStep(match, 10);
    Fire(match, 1, 2);
    CombatStep(match, 10);
    REQUIRE(CombatPlayer(match, 2).hp == 50000);
    // Player 1 then 3 kill it; player 4's shot of the same tick is no longer at a target.
    Fire(match, 1, 3);
    Fire(match, 3, 1);
    Fire(match, 4, 1);
    CombatStep(match);
    CHECK(Decisions(match, 1).back().hitKind == ShotHitKind::Player);
    CHECK(Decisions(match, 3).back().hitKind == ShotHitKind::Player);
    CHECK(Decisions(match, 3).back().damage == 25000);
    const auto passed = Decisions(match, 4).back();
    CHECK(passed.accepted);
    CHECK(passed.hitKind != ShotHitKind::Player);
    CHECK(passed.damage == 0);
    const auto victim = CombatPlayer(match, 2);
    CHECK(victim.hp == 0);
    CHECK(victim.damageCount == 4);
    CHECK(victim.lastAttackerId == 3);
    CHECK(LifePlayer(match, 2).lifeState == LifeState::Dead);
}

TEST_CASE("PvP players dying in the same tick respawn in the same tick at distinct free spawns") {
    PvpMatch match(QuadArena());
    JoinFour(match);
    CombatStep(match);
    // Player 3 kills 2 and player 1, facing +X, kills 4 (four hits each, ten ticks apart).
    for (ActionId id = 1; id <= 4; ++id) {
        Fire(match, 3, id);
        REQUIRE(match.SubmitActions({1, {{id, 1, std::numbers::pi_v<float> / 2, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match, id < 4 ? 10 : 1);
    }
    const auto two = LifePlayer(match, 2), four = LifePlayer(match, 4);
    REQUIRE(two.lifeState == LifeState::Dead);
    REQUIRE(four.lifeState == LifeState::Dead);
    REQUIRE(two.respawnTick == four.respawnTick);
    while (match.TickCount() < two.respawnTick) CombatStep(match);
    const auto two2 = LifePlayer(match, 2), four2 = LifePlayer(match, 4);
    CHECK(two2.lifeState == LifeState::Alive);
    CHECK(four2.lifeState == LifeState::Alive);
    CHECK(two2.lifeGeneration == 2);
    CHECK(four2.lifeGeneration == 2);
    CHECK(Engine::Math::Length(two2.position - four2.position) >= 2 * CombatArena().radius);
}

TEST_CASE("PvP match names each input admission and CanSubmitInput agrees with it") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Lethal: player 2 respawns in a later epoch and life.
    const auto due = LifePlayer(match, 2).respawnTick;
    while (match.TickCount() < due) {
        MoveCombatPlayer(match, 2);
        CombatStep(match);
    }
    const auto victim = LifePlayer(match, 2);
    REQUIRE(victim.lifeGeneration == 2);
    REQUIRE(victim.movementEpoch >= 2);
    REQUIRE(victim.lastResolvedCommand == 0);
    const auto epoch = victim.movementEpoch;
    const auto window = [&](std::uint64_t sequence, std::uint64_t movementEpoch, std::uint64_t life, float forward = 0) {
        return PlayerInput{2, {{sequence, forward, 0, 0, 0}}, movementEpoch, life};
    };
    std::set<InputAdmission> named;
    const auto admits = [&](const PlayerInput& input, InputAdmission expected) {
        const auto admission = match.AdmitInput(input);
        named.insert(admission);
        CHECK(admission == expected);
        CHECK(match.CanSubmitInput(input) == (expected == InputAdmission::Accepted));
    };
    admits(window(1, epoch, 2), InputAdmission::Accepted);
    REQUIRE(match.SubmitInput(window(1, epoch, 2)));
    admits({3, {{1, 0, 0, 0, 0}}, 1, 1}, InputAdmission::UnknownPlayer);
    // The player is checked before the window: an unknown player's malformed window is unknown.
    const auto unknownMalformed = match.AdmitInput({99, {}, 1, 1});
    CHECK(unknownMalformed == InputAdmission::UnknownPlayer);
    admits({2, {}, epoch, 2}, InputAdmission::Malformed);
    admits(window(1, epoch - 1, 2), InputAdmission::EpochOld);
    admits(window(1, epoch + 1, 2), InputAdmission::EpochFuture);
    admits(window(1, epoch, 1), InputAdmission::LifeOld);
    admits(window(1, epoch, 3), InputAdmission::LifeFuture);
    admits(window(MaxFutureCommands + 1, epoch, 2), InputAdmission::BeyondWindow);
    admits(window(1, epoch, 2, 1), InputAdmission::ConflictQueued);
    // The epoch is checked before the life, the window before a queued conflict.
    admits(window(1, epoch - 1, 1), InputAdmission::EpochOld);
    admits({2, {{1, 1, 0, 0, 0}, {MaxFutureCommands + 1, 0, 0, 0, 0}}, epoch, 2}, InputAdmission::ConflictQueued);
    admits({2, {{2, 0, 0, 0, 0}, {MaxFutureCommands + 1, 0, 0, 0, 0}}, epoch, 2}, InputAdmission::BeyondWindow);
    CHECK(named.size() == 9);
}

// pv7 §6, §7: E, S and the view tick are request content: they take part in
// comparing a retransmission (operator==), like every other field.
TEST_CASE("PvP shot request equality includes E, S and the view tick") {
    const ShotRequest request{1, 5, 0, 0, ActionKind::Shot, 1, 2, 30, 7 * 65536};
    for (auto edit : {+[](ShotRequest& r) { ++r.movementEpoch; }, +[](ShotRequest& r) { ++r.commandSequence; },
                      +[](ShotRequest& r) { ++r.viewTickQ16; }}) {
        auto other = request;
        edit(other);
        CHECK_FALSE(other == request);
    }
    CHECK(ShotRequest{request} == request);
}

// pv7 §7: the part a decision carries, and the three damage bounds.
TEST_CASE("PvP hit part and maximum shot damage decode checks") {
    for (const int part : {1, 2, 3}) CHECK(ValidDecisionHitPart(ShotHitKind::Player, part)); // Leg: pv9 §2
    for (const int part : {0, 4, 99, -1}) CHECK_FALSE(ValidDecisionHitPart(ShotHitKind::Player, part));
    for (const auto kind : {ShotHitKind::Miss, ShotHitKind::World}) {
        CHECK(ValidDecisionHitPart(kind, 0));
        for (const int part : {1, 2, 3, 4, 99}) CHECK_FALSE(ValidDecisionHitPart(kind, part));
    }
    CHECK(ValidShotDamageBounds(25000, 25000, 100000));
    CHECK(ValidShotDamageBounds(25000, 100000, 100000));
    CHECK_FALSE(ValidShotDamageBounds(25000, 24999, 100000));
    CHECK_FALSE(ValidShotDamageBounds(25000, 100001, 100000));
    CHECK_FALSE(ValidShotDamageBounds(0, 25000, 100000));
    CHECK(ValidShotDamageBounds(PvpCombatRules.shotDamage, PvpCombatRules.maximumShotDamage, PvpCombatRules.maximumHp));
    // pv9 §3: the most damage is a head hit.
    CHECK(PvpCombatRules.maximumShotDamage == PartDamage(PvpCombatRules.shotDamage, PvpCombatRules.headPermille));
}

// v8-product-hit-rules-permille P3: part damage in milli-points.
TEST_CASE("PvP part damage follows the permille multipliers") {
    CHECK(ShotDamage(PvpCombatRules, HitPart::Head, 100000) == 50000);
    CHECK(ShotDamage(PvpCombatRules, HitPart::Body, 100000) == 25000);
    CHECK(PartDamage(PvpCombatRules.shotDamage, PvpCombatRules.legPermille) == 18750);
    CHECK(ShotDamage(PvpCombatRules, HitPart::Body, 10000) == 10000); // hp left caps it
    CHECK(ShotDamage(PvpCombatRules, HitPart::Head, 30000) == 30000);
    CHECK(ShotDamage(PvpCombatRules, HitPart::Leg, 100000) == 18750); // pv9 §2
    CHECK(ShotDamage(PvpCombatRules, HitPart::Leg, 10000) == 10000);
    CHECK(ValidPartDamage(PvpCombatRules));
    CHECK(PartDamage(3000000, 2000) == 6000000); // the product passes 32 bits
    CHECK(ValidPartDamage(3000000, 2000, 6000000));
    CHECK_FALSE(ValidPartDamage(25001, 750, 50000)); // 18750.75 does not divide
    CHECK_FALSE(ValidPartDamage(25000, 0, 50000));   // no damage
    CHECK(ValidPartDamage(25000, 2000, 50000));
    CHECK_FALSE(ValidPartDamage(25000, 2001, 50000)); // above maximumShotDamage
}

TEST_CASE("PvP Match rejects rules whose part damage is invalid") {
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.shotDamage = 20001}), std::invalid_argument); // leg 15000.75
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.legPermille = 0}), std::invalid_argument);
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.headPermille = 2001}), std::invalid_argument);
    CHECK_NOTHROW(PvpMatch(CombatArena(), CombatRules{.shotDamage = 20000}));
}

// P4: the HUD rounds milli-points up.
TEST_CASE("PvP HUD shows whole points rounded up") {
    for (const auto [milli, shown] : {std::pair{0U, 0U}, {1U, 1U}, {999U, 1U}, {1000U, 1U}, {1001U, 2U},
                                      {100000U, 100U}})
        CHECK(DisplayPoints(milli) == shown);
}

// P7: with the least part damage, one life takes at most six hits; the
// authority digest checks every combat record against the same bound.
TEST_CASE("PvP one life takes at most six hits") {
    const auto least = std::min({PartDamage(PvpCombatRules.shotDamage, PvpCombatRules.headPermille),
                                 PartDamage(PvpCombatRules.shotDamage, PvpCombatRules.bodyPermille),
                                 PartDamage(PvpCombatRules.shotDamage, PvpCombatRules.legPermille)});
    CHECK((PvpCombatRules.maximumHp + least - 1) / least == 6);
}

// pv7 §6: the path A1 would take at A, from E, S and the request's life
// against the shooter's epoch, last resolved command and life after A.
TEST_CASE("PvP shadow records classify the A1 path at the handoff tick") {
    constexpr std::uint64_t a = 100;
    CHECK(ClassifyShotStep(a, 3, 20, 1, 3, 15, 1).path == ShotStepPath::OnTime);
    CHECK(ClassifyShotStep(a, 3, 20, 1, 3, 15, 1).tick == 105);
    CHECK(ClassifyShotStep(a, 3, 15 + MaxShotStepLead - 1, 1, 3, 15, 1).path == ShotStepPath::OnTime);
    CHECK(ClassifyShotStep(a, 3, 15 + MaxShotStepLead, 1, 3, 15, 1).path == ShotStepPath::HoldLimit);
    CHECK(ClassifyShotStep(a, 3, 15 + MaxShotStepLead, 1, 3, 15, 1).tick == a + MaxShotStepLead);
    CHECK(ClassifyShotStep(a, 3, 15, 1, 3, 15, 1).path == ShotStepPath::Late);
    CHECK(ClassifyShotStep(a, 3, 0, 1, 3, 15, 1).tick == a);
    CHECK(ClassifyShotStep(a, 2, 20, 1, 3, 15, 1).path == ShotStepPath::Epoch);
    CHECK(ClassifyShotStep(a, 0, 0, 1, 3, 15, 1).path == ShotStepPath::Epoch); // no prediction: E = 0
    CHECK(ClassifyShotStep(a, 3, 20, 1, 3, 15, 2).path == ShotStepPath::Life);
    CHECK(ClassifyShotStep(a, 3, 1, 1, 3, 0, 1).path == ShotStepPath::HoldLimit); // the epoch resolved nothing yet
    CHECK(MaxShotStepLead == 13);
}

// pv7 §7: a record-only check, never a rejection.
TEST_CASE("PvP shadow records check the view tick without refusing anything") {
    CHECK(ValidViewTick(ActionKind::Shot, 9, 0));
    CHECK(ValidViewTick(ActionKind::Shot, 9, 65536));
    CHECK(ValidViewTick(ActionKind::Shot, 9, 9 * 65536));
    CHECK_FALSE(ValidViewTick(ActionKind::Shot, 9, 9 * 65536 + 1));
    CHECK_FALSE(ValidViewTick(ActionKind::Shot, 9, 65535));
    CHECK_FALSE(ValidViewTick(ActionKind::Shot, 0, 65536));
    // observed x 65536 beyond 64 bits does not wrap into a refusal.
    CHECK(ValidViewTick(ActionKind::Shot, std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max()));
    CHECK(ValidViewTick(ActionKind::Reload, 9, 0));
    CHECK_FALSE(ValidViewTick(ActionKind::Reload, 9, 65536));
}

TEST_CASE("PvP shadow record lines and the shot trace path") {
    const ShotShadowRecord record{.timeNs = 1234567, .playerId = 2, .actionId = 9, .kind = ActionKind::Shot, .handoffTick = 100, .epoch = 3,
        .sequence = 20, .requestLife = 1, .currentEpoch = 3, .lastResolved = 15, .life = 1,
        .step = {ShotStepPath::OnTime, 105}, .dueTick = 105, .resolvedTick = 107, .held = true,
        .cooldownHoldTicks = 2, .rejection = ShotRejection::None,
        .viewTickQ16 = 98 * 65536 + 32768, .viewValid = true, .viewLagQ16 = 2 * 65536 - 32768};
    CHECK(MatchShotRecordLine(record) ==
          "{\"version\":2,\"kind\":\"decision\",\"time_ns\":1234567,\"player_id\":2,\"action_id\":9,\"action_kind\":\"shot\",\"a\":100,\"e\":3,"
          "\"s\":20,\"request_life\":1,\"epoch_at_a\":3,\"last_resolved_at_a\":15,\"life_at_a\":1,\"path\":\"on_time\","
          "\"step_tick\":105,\"due_tick\":105,\"resolved_tick\":107,\"held\":true,\"cooldown_hold_ticks\":2,"
          "\"rejection\":\"none\",\"view_tick_q16\":6455296,\"view_valid\":true,\"view_lag_q16\":98304}");
    ShotShadowRecord reload{.kind = ActionKind::Reload, .viewTickQ16 = 5, .viewValid = false};
    CHECK(MatchShotRecordLine(reload).find("\"cooldown_hold_ticks\":null") != std::string::npos);
    CHECK(MatchShotRecordLine(reload).find("\"view_valid\":false,\"view_lag_q16\":null}") != std::string::npos);
    CHECK(MatchShotTraceEndLine(4, 1, 2) == "{\"version\":2,\"kind\":\"trace_end\",\"records\":4,\"dropped\":1,\"view_tick_invalid\":2}");
    CHECK(MatchShotTracePath("out/match-commands.jsonl") == std::filesystem::path("out/match-shots.jsonl"));
    CHECK(MatchShotTracePath("out/trace.jsonl") == std::filesystem::path("out/trace.jsonl.shots.jsonl"));
    CHECK(MatchShotTracePath({}).empty());
}

// ---- A1: shots decided at their command step (pv7 §6, v7.1 a1-shot-step) ----
namespace {
// Rules where one hit kills: the order of two shots decides who dies.
constexpr CombatRules OneHitRules{.shotDamage = 100000, .maximumShotDamage = 100000, .headPermille = 1000};

// One command per mover per tick, so each mover's cursor L moves one step per tick.
void StepMovers(PvpMatch& match, unsigned count = 1, std::initializer_list<PlayerId> movers = {1, 2},
    const ShotReferenceAge& age = [](std::uint64_t tick) -> std::optional<std::chrono::nanoseconds> {
        return tick == 1 ? std::optional{0ns} : std::nullopt;
    }) {
    for (unsigned index = 0; index < count; ++index) {
        for (const auto id : movers) MoveCombatPlayer(match, id);
        CombatStep(match, 1, age);
    }
}

// A shot claiming the command `lead` steps after the shooter's current cursor:
// the next tick resolves cursor + 1, so it is due lead - 1 ticks after A.
ShotRequest StepShot(const PvpMatch& match, ActionId id, std::uint64_t lead, PlayerId shooter = 1, float yaw = 0) {
    const auto state = LifePlayer(match, shooter);
    return {id, 1, yaw, 0, ActionKind::Shot, state.lifeGeneration, state.movementEpoch,
        state.lastResolvedCommand + lead};
}

ShotDecision DecisionOf(const PvpMatch& match, ActionId id, PlayerId playerId = 1) {
    for (const auto& decision : Decisions(match, playerId))
        if (decision.actionId == id) return decision;
    FAIL("no decision for action " << id);
    return {};
}

std::optional<ShotDecision> MaybeDecision(const PvpMatch& match, ActionId id, PlayerId playerId = 1) {
    for (const auto& decision : Decisions(match, playerId))
        if (decision.actionId == id) return decision;
    return std::nullopt;
}

PvpMatch A1Match(const CombatRules& rules = PvpCombatRules) {
    PvpMatch match(CombatArena(), rules);
    JoinCombatPlayers(match);
    CombatStep(match);
    return match;
}
} // namespace

TEST_CASE("PvP A1 cooldown hold bound follows the cooldown (D136)") {
    CHECK(MaxShotStepLead == 13);
    CHECK(MaxShotHoldTicks == 2);
    CHECK(ShotHoldLimit(0) == 0);
    CHECK(ShotHoldLimit(1) == 1);
    CHECK(ShotHoldLimit(2) == 2);
    CHECK(ShotHoldLimit(3) == 2);
    CHECK(ShotHoldLimit(10) == 2);
}

TEST_CASE("PvP A1 decides an on-time shot at its command step and a late or other-epoch one at hand-off") {
    for (const std::uint64_t lead : {2u, 3u, 13u}) {
        auto match = A1Match();
        StepMovers(match, 5);
        const auto a = match.TickCount() + 1;
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, lead)}}) == ActionAdmission::Accepted);
        StepMovers(match, static_cast<unsigned>(lead - 1));
        if (lead > 2) CHECK_FALSE(MaybeDecision(match, 1));
        StepMovers(match);
        const auto decision = DecisionOf(match, 1);
        CHECK(decision.accepted);
        CHECK(decision.resolvedTick == a + lead - 1); // T(S) = A + (S - L(A))
        const auto timing = match.GetActionTiming(1, 1);
        REQUIRE(timing);
        CHECK(timing->handoffTick == a);
        CHECK(timing->dueTick == a + lead - 1);
        CHECK(timing->resolvedTick == a + lead - 1);
        CHECK_FALSE(timing->held);
    }
    for (const std::uint64_t lead : {0u, 1u}) { // S <= L(A): late, decided at A
        auto match = A1Match();
        StepMovers(match, 5);
        const auto a = match.TickCount() + 1;
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, lead)}}) == ActionAdmission::Accepted);
        StepMovers(match);
        CHECK(DecisionOf(match, 1).resolvedTick == a);
    }
    { // E = 0 and an old epoch: decided at A as in pv6
        auto match = A1Match();
        StepMovers(match, 5);
        auto shot = StepShot(match, 1, 5);
        shot.movementEpoch = 0;
        auto old = StepShot(match, 2, 5);
        old.movementEpoch = LifePlayer(match, 1).movementEpoch + 1; // not the current epoch either
        REQUIRE(match.SubmitActions({1, {shot}}) == ActionAdmission::Accepted);
        const auto a = match.TickCount() + 1;
        StepMovers(match);
        CHECK(DecisionOf(match, 1).resolvedTick == a);
        CHECK(DecisionOf(match, 1).accepted);
        StepMovers(match, 12);
        REQUIRE(match.SubmitActions({1, {old}}) == ActionAdmission::Accepted);
        StepMovers(match);
        CHECK(DecisionOf(match, 2).resolvedTick == match.TickCount());
    }
}

TEST_CASE("PvP A1 waits at most W ticks for a step that does not come") {
    { // the epoch resolved nothing yet and never starts: A + W
        auto match = A1Match();
        const auto a = match.TickCount() + 1;
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 3)}}) == ActionAdmission::Accepted);
        CombatStep(match, static_cast<unsigned>(MaxShotStepLead));
        CHECK_FALSE(MaybeDecision(match, 1));
        CombatStep(match);
        CHECK(DecisionOf(match, 1).resolvedTick == a + MaxShotStepLead);
    }
    { // the epoch starts before A + W: the due rule wins over the bound, D = T(S)
        auto match = A1Match();
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 3)}}) == ActionAdmission::Accepted); // S = 3
        CombatStep(match, 2);
        StepMovers(match, 2, {1});
        CHECK_FALSE(MaybeDecision(match, 1));
        StepMovers(match, 1, {1}); // command 3 resolves now
        CHECK(DecisionOf(match, 1).resolvedTick == match.TickCount());
        CHECK(LifePlayer(match, 1).lastResolvedCommand == 3);
    }
    { // S = 0 with L = 0: L >= S already, decided at A (not the hold bound)
        auto match = A1Match();
        const auto a = match.TickCount() + 1;
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 0)}}) == ActionAdmission::Accepted);
        CombatStep(match);
        CHECK(DecisionOf(match, 1).resolvedTick == a);
    }
    { // a claim at least W ahead of L(A): A + W
        auto match = A1Match();
        StepMovers(match, 5);
        const auto a = match.TickCount() + 1;
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 21)}}) == ActionAdmission::Accepted);
        StepMovers(match, static_cast<unsigned>(MaxShotStepLead));
        CHECK_FALSE(MaybeDecision(match, 1));
        StepMovers(match);
        CHECK(DecisionOf(match, 1).resolvedTick == a + MaxShotStepLead);
    }
}

TEST_CASE("PvP A1 decides when the shooter's epoch resets before the command step") {
    auto match = A1Match();
    StepMovers(match, 1, {1}); // player 1 starts, then sends nothing: a starvation reset follows
    auto probe = match;
    const auto epoch = LifePlayer(match, 1).movementEpoch;
    std::uint64_t reset{};
    for (unsigned t = 0; t < 200 && !reset; ++t) {
        CombatStep(probe);
        if (LifePlayer(probe, 1).movementEpoch != epoch) reset = probe.TickCount();
    }
    REQUIRE(reset != 0);
    while (match.TickCount() + 3 < reset) CombatStep(match);
    const auto a = match.TickCount() + 1; // reset - 2
    REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 6)}}) == ActionAdmission::Accepted); // T(S) = A + 5
    CombatStep(match, 2);
    CHECK_FALSE(MaybeDecision(match, 1));
    CombatStep(match);
    CHECK(match.TickCount() == reset);
    CHECK(DecisionOf(match, 1).resolvedTick == reset);
    CHECK(match.GetActionTiming(1, 1)->dueTick == reset);
    CHECK(match.GetActionTiming(1, 1)->handoffTick == a);
}

TEST_CASE("PvP A1 holds a shot at most K ticks inside the cooldown and judges it once more") {
    for (const std::uint64_t gap : {1u, 2u, 3u}) {
        auto match = A1Match();
        REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // accepted at 2, next allowed 12
        CombatStep(match, static_cast<unsigned>(12 - gap - 3));
        REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
        const auto a = match.TickCount() + 1;
        REQUIRE(a == 12 - gap);
        CombatStep(match);
        if (gap > ShotHoldLimit(PvpCombatRules.cooldownTicks)) {
            CHECK(DecisionOf(match, 2).rejection == ShotRejection::Cooldown);
            CHECK(DecisionOf(match, 2).resolvedTick == a);
            continue;
        }
        CHECK_FALSE(MaybeDecision(match, 2));
        for (unsigned repeat = 0; repeat < 3; ++repeat) // a retransmission while held is a duplicate
            REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match, static_cast<unsigned>(gap));
        const auto decision = DecisionOf(match, 2);
        CHECK(decision.accepted);
        CHECK(decision.resolvedTick == 12);
        CHECK(Decisions(match).size() == 2);
        CHECK(CombatPlayer(match, 1).nextAllowedShotTick == 22);
        const auto timing = match.GetActionTiming(1, 2);
        REQUIRE(timing);
        CHECK(timing->held);
        CHECK(timing->dueTick == a);
        CHECK(timing->resolvedTick == 12);
    }
    { // a fast weapon (cooldown 4): gap 3 is refused, gap 2 is held
        constexpr CombatRules Fast{.cooldownTicks = 4};
        auto match = A1Match(Fast);
        REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // accepted at 2, next allowed 6
        REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // A = 3, gap 3
        CHECK(DecisionOf(match, 2).rejection == ShotRejection::Cooldown);
        REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // A = 4, gap 2
        CHECK_FALSE(MaybeDecision(match, 3));
        CombatStep(match, 2);
        CHECK(DecisionOf(match, 3).accepted);
        CHECK(DecisionOf(match, 3).resolvedTick == 6);
    }
}

TEST_CASE("PvP A1 judges a held shot again in full and never holds it twice") {
    { // the shooter dies during the hold: Dead
        auto match = A1Match(OneHitRules);
        constexpr float away = 1.5F; // a wall shot: nobody is hit
        REQUIRE(match.SubmitActions({1, {{1, 1, away, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // accepted at 2, next allowed 12
        REQUIRE(DecisionOf(match, 1).hitKind != ShotHitKind::Player);
        CombatStep(match, 7);
        REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // A = 10, gap 2: held to 12
        REQUIRE_FALSE(MaybeDecision(match, 2));
        REQUIRE(match.SubmitActions({2, {{1, 1, 3.14159F, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // player 2 kills player 1 at 11
        REQUIRE(LifePlayer(match, 1).lifeState == LifeState::Dead);
        CombatStep(match);
        CHECK(DecisionOf(match, 2).rejection == ShotRejection::Dead);
        CHECK(DecisionOf(match, 2).resolvedTick == 12);
    }
    { // a smaller ActionId arrives during the hold and fires the last round: EmptyMagazine
        constexpr CombatRules Two{.magazineCapacity = 2};
        auto match = A1Match(Two);
        constexpr float away = 1.5F;
        REQUIRE(match.SubmitActions({1, {{1, 1, away, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // accepted at 2: one round left, next allowed 12
        CombatStep(match, 7);
        REQUIRE(match.SubmitActions({1, {{3, 1, away, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // A = 10, gap 2: held to 12
        REQUIRE(match.SubmitActions({1, {{2, 1, away, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match); // A = 11: the smaller id goes first and is held to 12 as well
        CHECK_FALSE(MaybeDecision(match, 2));
        CHECK_FALSE(MaybeDecision(match, 3));
        CombatStep(match);
        CHECK(DecisionOf(match, 2).accepted);
        CHECK(DecisionOf(match, 3).rejection == ShotRejection::EmptyMagazine);
        CHECK(DecisionOf(match, 3).resolvedTick == 12);
    }
}

TEST_CASE("PvP A1 never holds a shot twice") {
    // Cooldown 2 (K = 2): a held shot judged again right behind a smaller id
    // that fired at its boundary is still 2 ticks inside; it is refused.
    constexpr CombatRules Short{.cooldownTicks = 2};
    auto match = A1Match(Short);
    constexpr float away = 1.5F;
    REQUIRE(match.SubmitActions({1, {{1, 1, away, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // accepted at 2, next allowed 4
    REQUIRE(match.SubmitActions({1, {{3, 1, away, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // A = 3, gap 1: held to 4
    REQUIRE(match.SubmitActions({1, {{2, 1, away, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // A = 4: id 2 fires first (next allowed 6), id 3 is 2 inside again
    CHECK(DecisionOf(match, 2).accepted);
    CHECK(DecisionOf(match, 3).rejection == ShotRejection::Cooldown);
    CHECK(DecisionOf(match, 3).resolvedTick == 4);
}

TEST_CASE("PvP A1 keeps one player's actions in ActionId order behind a waiting one") {
    auto match = A1Match();
    constexpr float away = 1.5F;
    REQUIRE(match.SubmitActions({1, {{1, 1, away, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // accepted at 2, next allowed 12
    CombatStep(match, 7);
    REQUIRE(match.SubmitActions({1, {{2, 1, away, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // A = 10: held to 12
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0, ActionKind::Reload}}}) == ActionAdmission::Accepted);
    CombatStep(match); // A = 11: the reload is due but waits behind the held shot
    CHECK_FALSE(MaybeDecision(match, 3));
    CombatStep(match);
    CHECK(DecisionOf(match, 2).accepted);
    CHECK(DecisionOf(match, 2).resolvedTick == 12);
    CHECK(DecisionOf(match, 3).accepted);
    CHECK(DecisionOf(match, 3).resolvedTick == 12);
    CHECK(CombatPlayer(match, 1).reloadActionId == 3);
}

TEST_CASE("PvP A1 orders one tick's decisions across players by hand-off tick first") {
    auto match = A1Match(OneHitRules);
    StepMovers(match, 5);
    // Player 2 hands its shot off first (A = t + 1, D = t + 3); player 1 one tick
    // later (A = t + 2) with the same D. Player 2 decides first and kills player 1.
    const auto t = match.TickCount();
    REQUIRE(match.SubmitActions({2, {StepShot(match, 1, 3, 2, 3.14159F)}}) == ActionAdmission::Accepted);
    StepMovers(match);
    REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 2, 1, 0)}}) == ActionAdmission::Accepted);
    StepMovers(match, 2);
    REQUIRE(match.TickCount() == t + 3);
    const auto two = DecisionOf(match, 1, 2);
    const auto one = DecisionOf(match, 1, 1);
    CHECK(two.resolvedTick == t + 3);
    CHECK(one.resolvedTick == t + 3);
    CHECK(two.accepted);
    CHECK(two.targetId == 1);
    CHECK(one.rejection == ShotRejection::Dead);
    CHECK(LifePlayer(match, 2).lifeState == LifeState::Alive);
}

TEST_CASE("PvP A1 measures the reference age once at hand-off") {
    { // 200 ms at A, 300 ms later: accepted
        auto match = A1Match();
        StepMovers(match, 5);
        unsigned calls{};
        const ShotReferenceAge age = [&](std::uint64_t) -> std::optional<std::chrono::nanoseconds> {
            return ++calls == 1 ? 200ms : 300ms;
        };
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 4)}}) == ActionAdmission::Accepted);
        StepMovers(match, 4, {1, 2}, age);
        CHECK(calls == 1);
        CHECK(DecisionOf(match, 1).accepted);
    }
    { // already over 250 ms at A: Expired, decided at D
        auto match = A1Match();
        StepMovers(match, 5);
        const auto a = match.TickCount() + 1;
        const ShotReferenceAge age = [](std::uint64_t) -> std::optional<std::chrono::nanoseconds> { return 300ms; };
        REQUIRE(match.SubmitActions({1, {StepShot(match, 1, 4)}}) == ActionAdmission::Accepted);
        StepMovers(match, 4, {1, 2}, age);
        CHECK(DecisionOf(match, 1).rejection == ShotRejection::Expired);
        CHECK(DecisionOf(match, 1).resolvedTick == a + 3);
    }
}

TEST_CASE("PvP A1 rejects invalid combat rules") {
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.shotDamage = 0}), std::invalid_argument);
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.shotDamage = 50001}), std::invalid_argument); // > maximum 50000
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.magazineCapacity = 0}), std::invalid_argument);
    CHECK_THROWS_AS(PvpMatch(CombatArena(), CombatRules{.shotRange = -1}), std::invalid_argument);
    CHECK_NOTHROW(PvpMatch(CombatArena(), OneHitRules));
}

// v7.1 outage-resync match-resync: a Resync requested for the tick a respawn
// resolves merges into the respawn: one epoch change, reason LifeRespawn, no
// reset counted for connection quality and no Resync.
TEST_CASE("PvP a Resync requested before a respawn boundary merges into the respawn") {
    struct TraceScope {
        std::shared_ptr<MovementTrace> trace = std::make_shared<MovementTrace>();
        TraceScope() { SetMovementTrace(trace); }
        ~TraceScope() { SetMovementTrace(nullptr); }
    } trace;
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(LifePlayer(match, 2).lifeState == LifeState::Dead);
    const auto due = LifePlayer(match, 2).respawnTick;
    while (match.TickCount() < due - 1) {
        MoveCombatPlayer(match, 2);
        CombatStep(match);
    }
    const auto before = LifePlayer(match, 2);
    static_cast<void>(trace.trace->Drain());
    CHECK(match.RequestMovementResync(2));
    CombatStep(match);
    const auto respawned = LifePlayer(match, 2);
    CombatStep(match); // and nothing is left over for the tick after it
    CHECK(LifePlayer(match, 2).movementEpoch == respawned.movementEpoch);
    CHECK(respawned.lifeGeneration == before.lifeGeneration + 1);
    CHECK(respawned.movementEpoch == before.movementEpoch + 1);
    std::vector<MovementResetReason> reasons;
    for (const auto& event : trace.trace->Drain())
        if (event.kind == MovementTraceKind::Reset && event.playerId == 2) reasons.push_back(event.resetReason);
    CHECK(reasons == std::vector<MovementResetReason>{MovementResetReason::LifeRespawn});
    const auto quality = match.GetMovementQuality(2);
    REQUIRE(quality);
    CHECK(quality->resets == 0);
    CHECK(quality->resyncs == 0);
}

// v8-product-crouch (protocol-v8 §4): a crouched death respawns standing. Both
// players crouch first so the level shots from a crouched eye still hit.
TEST_CASE("PvP a crouched death respawns standing") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
        const auto player = LifePlayer(match, id);
        REQUIRE(match.SubmitInput({id, {{player.lastResolvedCommand + 1, 0, 0, 0, 0, false, true}},
            player.movementEpoch, player.lifeGeneration}));
    }
    CombatStep(match);
    REQUIRE(LifePlayer(match, 1).crouched);
    REQUIRE(LifePlayer(match, 2).crouched);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(LifePlayer(match, 2).lifeState == LifeState::Dead);
    CHECK(LifePlayer(match, 2).crouched); // frozen while dead
    const auto due = LifePlayer(match, 2).respawnTick;
    while (match.TickCount() < due) CombatStep(match);
    const auto respawned = LifePlayer(match, 2);
    REQUIRE(respawned.lifeState == LifeState::Alive);
    CHECK_FALSE(respawned.crouched);
}


// v8-product-anim-authority A2: the Match advances every player's animation
// once a tick and puts it on the snapshot (protocol-v9 §4.2, §4.4).
namespace {
const PlayerAnimation& AnimationOf(const WorldSnapshot& snapshot, PlayerId playerId) {
    const auto found = std::find_if(snapshot.animation.begin(), snapshot.animation.end(),
        [=](const auto& value) { return value.playerId == playerId; });
    REQUIRE(found != snapshot.animation.end());
    return *found;
}
} // namespace

TEST_CASE("PvP Match snapshots carry one animation value per player from the join on") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    auto snapshot = match.Snapshot(); // before any tick
    REQUIRE(snapshot.animation.size() == 2);
    CHECK(snapshot.animation[0].playerId == snapshot.players[0].playerId);
    CHECK(snapshot.animation[1].playerId == snapshot.players[1].playerId);
    CHECK(AnimationOf(snapshot, 1).jumpPhase == JumpPhase::Grounded);
    CHECK(ValidPlayerAnimations(snapshot.players, snapshot.animation, snapshot.tick));
    MoveCombatPlayer(match, 1, 1);
    CombatStep(match);
    for (int i = 0; i < 10; ++i) {
        MoveCombatPlayer(match, 1, 1);
        CombatStep(match);
    }
    snapshot = match.Snapshot();
    const auto& walking = AnimationOf(snapshot, 1);
    CHECK(walking.gaitPhaseQ16 > 0);
    CHECK(walking.moveWeightPermille == 1000);
    CHECK(AnimationOf(snapshot, 2).gaitPhaseQ16 == 0); // the other player stood still
    MoveCombatPlayer(match, 1, 0, 0, 0, 0, true);
    CombatStep(match);
    snapshot = match.Snapshot();
    CHECK(AnimationOf(snapshot, 1).jumpPhase == JumpPhase::Start);
    CHECK(AnimationOf(snapshot, 1).jumpPhaseTick == snapshot.tick);
}

TEST_CASE("PvP Match animation freezes through death and restarts at the respawn tick") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    DamageThreeTimes(match);
    REQUIRE(match.SubmitActions({1, {{4, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    REQUIRE(LifePlayer(match, 2).lifeState == LifeState::Dead);
    const auto frozen = AnimationOf(match.Snapshot(), 2);
    const auto due = LifePlayer(match, 2).respawnTick;
    while (match.TickCount() < due - 1) {
        MoveCombatPlayer(match, 2);
        CombatStep(match);
        CHECK(AnimationOf(match.Snapshot(), 2) == frozen);
    }
    while (LifePlayer(match, 2).lifeState == LifeState::Dead) {
        MoveCombatPlayer(match, 2);
        CombatStep(match);
    }
    const auto snapshot = match.Snapshot();
    const auto& reborn = AnimationOf(snapshot, 2);
    CHECK(reborn.lifeGeneration == 2);
    CHECK(reborn.gaitPhaseQ16 == 0);
    CHECK(reborn.jogWeightPermille == 0); // no step across the death (A1 review C1)
    CHECK(reborn.jumpPhaseTick == LifePlayer(match, 2).lifeStateTick);
}

// E4: each §4.5 condition the Match guards before a snapshot leaves it.
TEST_CASE("PvP Match refuses to publish an animation the Gateway would refuse") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match, 3);
    const auto valid = match.Snapshot();
    REQUIRE(ValidPlayerAnimations(valid.players, valid.animation, valid.tick));
    const auto refused = [&](auto edit) {
        auto snapshot = valid;
        edit(snapshot);
        CHECK_FALSE(ValidPlayerAnimations(snapshot.players, snapshot.animation, snapshot.tick));
        GYO_CHECK_ASSERTS(RequireValidAnimation(snapshot));
    };
    refused([](auto& s) { s.animation.pop_back(); });                              // a player without a value
    refused([](auto& s) { s.animation[1].playerId = 7; });                          // an unknown player
    refused([](auto& s) { s.animation[1].playerId = s.animation[0].playerId; });    // a player twice
    refused([](auto& s) { s.animation[0].lifeGeneration = 2; });                    // another life
    refused([](auto& s) { s.animation[0].moveWeightPermille = 1001; });
    refused([](auto& s) { s.animation[0].jogWeightPermille = 1001; });
    refused([](auto& s) { s.animation[0].sprintWeightPermille = 1; });              // before sprint presentation
    refused([](auto& s) { s.animation[0].jumpPhase = JumpPhase::Unspecified; });
    refused([](auto& s) { s.animation[0].jumpPhase = static_cast<JumpPhase>(5); });
    refused([](auto& s) { s.animation[0].jumpPhaseTick = s.tick + 1; });
    refused([](auto& s) { s.animation[0].jumpPhase = JumpPhase::Start; });         // grounded
    refused([](auto& s) { s.animation[0].jumpPhase = JumpPhase::Airborne; });      // grounded
    refused([](auto& s) { s.players[0].grounded = false; });                       // Grounded in the air
    refused([](auto& s) { s.players[0].grounded = false; s.animation[0].jumpPhase = JumpPhase::Land; });
    // Accepted: another order, and a dead player is not cross-checked.
    auto swapped = valid;
    std::swap(swapped.animation[0], swapped.animation[1]);
    CHECK(ValidPlayerAnimations(swapped.players, swapped.animation, swapped.tick));
    auto dead = valid;
    dead.players[0].lifeState = LifeState::Dead;
    dead.animation[0].jumpPhase = JumpPhase::Airborne;
    CHECK(ValidPlayerAnimations(dead.players, dead.animation, dead.tick));
}
