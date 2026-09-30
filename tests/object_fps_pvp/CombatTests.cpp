#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>

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
        CHECK(Decisions(match).back().damage == 25);
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
    CHECK(CombatPlayer(match, 2).hp == 100);
    CombatStep(match); // The first accepted shot executes at tick 2.
    const auto original = Decisions(match).front();
    CHECK(original.accepted);
    CHECK(original.resolvedTick == 2);
    CHECK(original.hitKind == ShotHitKind::Player);
    CHECK(original.targetId == 2);
    CHECK(original.damage == 25);
    CHECK(original.kind == ActionKind::Shot);
    CHECK(original.lifeGeneration == 1);
    CHECK(original.targetLifeGeneration == 1);
    CHECK(CombatPlayer(match, 1).magazineAmmo == 11);
    CHECK(CombatPlayer(match, 1).nextAllowedShotTick == 12);
    CHECK(CombatPlayer(match, 2).hp == 75);

    for (unsigned repeat = 0; repeat < 40; ++repeat)
        REQUIRE(match.SubmitActions(first) == ActionAdmission::Accepted);
    CombatStep(match, 8);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 11 remains inside cooldown.
    auto decisions = Decisions(match);
    REQUIRE(decisions.size() == 2);
    CHECK(decisions.front() == original);
    CHECK_FALSE(decisions.back().accepted);
    CHECK(decisions.back().rejection == ShotRejection::Cooldown);
    CHECK(CombatPlayer(match, 2).hp == 75);
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 12 is permitted, including after a rejection.
    CHECK(Decisions(match).back().accepted);
    CHECK(CombatPlayer(match, 2).hp == 50);
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
        CHECK(CombatPlayer(match, 2).hp == (age <= 250ms ? 75 : 100));
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
    CHECK(CombatPlayer(match, 2).hp == 100);
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
    CHECK(CombatPlayer(match, 2).hp == 100);
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
    CHECK(CombatPlayer(match, 2).hp == 75);
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
    CHECK(CombatPlayer(match, 2).hp == 75); // No wait for missing action 1.
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
    CHECK(CombatPlayer(match, 2).hp == 75);
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
    CHECK(CombatPlayer(match, 2).hp == 100);
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
    CHECK(CombatPlayer(match, 2).hp == 75);
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
    CHECK(CombatPlayer(match, 2).hp == 50);
    REQUIRE(match.AcknowledgeActions(1, 3));
    CHECK(match.GetActionResults(1)->retiredThrough == 3);
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match, 20);
    CHECK(Decisions(match).empty());
    CHECK(CombatPlayer(match, 2).hp == 50);
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
    CHECK(CombatPlayer(match, 2).hp == 100);
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
    CHECK(CombatPlayer(match, 1).hp == 100);
    CHECK(Decisions(match, 2).front().rejection == ShotRejection::Dead);
    const auto lethal = Decisions(match).back();
    CHECK(lethal.damage == 25);
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
    CHECK(combat.hp == 100);
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
    CHECK(CombatPlayer(match, 1).hp == 75);
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
    CHECK(CombatPlayer(match, 2).hp == 100);
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
