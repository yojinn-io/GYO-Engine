#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PvpMatch.hpp"

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
    CHECK(CombatPlayer(match, 1).nextAllowedShotTick == 22);
    CHECK(CombatPlayer(match, 2).hp == 75);

    for (unsigned repeat = 0; repeat < 40; ++repeat)
        REQUIRE(match.SubmitActions(first) == ActionAdmission::Accepted);
    CombatStep(match, 18);
    REQUIRE(match.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 21 remains inside cooldown.
    auto decisions = Decisions(match);
    REQUIRE(decisions.size() == 2);
    CHECK(decisions.front() == original);
    CHECK_FALSE(decisions.back().accepted);
    CHECK(decisions.back().rejection == ShotRejection::Cooldown);
    CHECK(CombatPlayer(match, 2).hp == 75);
    REQUIRE(match.SubmitActions({1, {{3, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match); // Tick 22 is permitted, including after a rejection.
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
        ShotRequest{2, 1, 0, std::numbers::pi_v<float>}}) {
        CHECK(match.SubmitActions({1, {{3, 1, 0, 0}, invalid}}) == ActionAdmission::InvalidBatch);
    }
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

TEST_CASE("PvP zero HP remains targetable mobile and able to shoot until session teardown") {
    PvpMatch match(CombatArena());
    JoinCombatPlayers(match);
    CombatStep(match);
    for (ActionId id = 1; id <= 5; ++id) {
        REQUIRE(match.SubmitActions({1, {{id, 1, 0, 0}}}) == ActionAdmission::Accepted);
        CombatStep(match);
        const auto decision = Decisions(match).back();
        CHECK(decision.accepted);
        CHECK(decision.hitKind == ShotHitKind::Player);
        CHECK(decision.damage == (id <= 4 ? 25 : 0));
        if (id < 5) CombatStep(match, 19);
    }
    CHECK(CombatPlayer(match, 2).hp == 0);
    REQUIRE(match.SubmitInput({2, {{1, -1, 0, 0, 0}}}));
    REQUIRE(match.SubmitActions({2, {{1, 1, std::numbers::pi_v<float>, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(match.Snapshot().players[1].position.z == doctest::Approx(7.95));
    CHECK(Decisions(match, 2).front().accepted);
    CHECK(CombatPlayer(match, 1).hp == 75);
    std::string error;
    REQUIRE(match.Join(2, error)); // Duplicate active join preserves combat state.
    CHECK(CombatPlayer(match, 2).hp == 0);
    CHECK(Decisions(match, 2).size() == 1);
    REQUIRE(match.Leave(2));
    CHECK_FALSE(match.GetActionResults(2));
    REQUIRE(match.Join(3, error));
    CHECK(CombatPlayer(match, 3).hp == 100);
    CHECK(match.GetActionResults(3)->retiredThrough == 0);
    CHECK(Decisions(match, 3).empty());
    REQUIRE(match.SubmitActions({3, {{1, 1, std::numbers::pi_v<float>, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match);
    CHECK(Decisions(match, 3).front().accepted);
    match.Reset();
    CHECK(match.Snapshot().combat.empty());
    CHECK_FALSE(match.GetActionResults(1));
    REQUIRE(match.Join(1, error));
    CHECK(CombatPlayer(match, 1).hp == 100);
    CHECK(match.GetActionResults(1)->retiredThrough == 0);
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
    CHECK(CombatPlayer(match, 2).hp == 50);
    REQUIRE(match.AcknowledgeActions(1, 3));
    CHECK(match.GetActionResults(1)->retiredThrough == 3);
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    CombatStep(match, 20);
    CHECK(Decisions(match).empty());
    CHECK(CombatPlayer(match, 2).hp == 50);
}
