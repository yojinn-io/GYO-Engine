#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <chrono>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;

Arena CombatHostArena() {
    Arena arena;
    arena.id = "synthetic_combat_host";
    arena.width = arena.depth = 12;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 6}, 0}};
    return arena;
}

struct CombatHostFixture final {
    std::chrono::steady_clock::time_point now{};
    std::size_t clockReads{};
    MatchRuntimeHost host{CombatHostArena(), [this] { ++clockReads; return now; }};

    CombatHostFixture() {
        REQUIRE(host.QueueJoin(1, 1));
        REQUIRE(host.QueueJoin(2, 2));
        REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
        REQUIRE(host.TakeControlResults().size() == 2);
    }

    void Step() { REQUIRE(host.Advance(MovementTickSeconds).steps == 1); }
    ShotDecision Decision(ActionId id, PlayerId playerId = 1) {
        const auto results = host.GetActionResults(playerId);
        REQUIRE(results);
        for (const auto& decision : results->decisions)
            if (decision.actionId == id) return decision;
        FAIL("Missing action decision");
        return {};
    }
};

ActionBatch Shot(ActionId id, std::uint64_t reference = 1, PlayerId playerId = 1) {
    return {playerId, {{id, reference, 0, 0}}};
}
}

TEST_CASE("PvP combat host resolves from original publication age with an inclusive deadline") {
    for (const auto age : {250ms, 251ms}) {
        CombatHostFixture fixture;
        fixture.now += age;
        REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
        CHECK(fixture.host.GetActionResults(1)->decisions.empty());
        fixture.Step();
        const auto decision = fixture.Decision(1);
        CHECK(decision.resolvedTick == 2);
        CHECK(decision.accepted == (age == 250ms));
        CHECK(decision.rejection == (age == 250ms ? ShotRejection::None : ShotRejection::Expired));
    }
    CombatHostFixture fixture;
    fixture.now += 250ms + 1ns;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::Expired);
}

TEST_CASE("PvP combat host does not refresh references on reads retransmits or zero-step advances") {
    CombatHostFixture fixture;
    REQUIRE(fixture.clockReads == 1);
    fixture.now += 200ms;
    REQUIRE(fixture.host.TakeSnapshot()->tick == 1);
    CHECK_FALSE(fixture.host.TakeSnapshot());
    CHECK(fixture.host.Advance(0).steps == 0);
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    CHECK(fixture.clockReads == 1);
    fixture.now += 50ms + 1ns;
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::Expired);
}

TEST_CASE("PvP combat host expiry uses wall age at resolution despite few ticks or dropped time") {
    for (const auto elapsed : {MovementTickSeconds, 1.0}) {
        CombatHostFixture fixture;
        fixture.now += 100ms;
        REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
        fixture.now += 900ms;
        const auto advanced = fixture.host.Advance(elapsed);
        CHECK(advanced.steps == (elapsed == 1.0 ? 5U : 1U));
        CHECK(fixture.Decision(1).resolvedTick == 2);
        CHECK(fixture.Decision(1).rejection == ShotRejection::Expired);
        if (elapsed == 1.0) CHECK(advanced.droppedSeconds > 0.9);
    }
}

TEST_CASE("PvP combat host only exposes final catch-up publications as valid references") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.Advance(3 * MovementTickSeconds).steps == 3);
    REQUIRE(fixture.host.TakeSnapshot()->tick == 4);
    REQUIRE(fixture.host.SubmitActions({1, {{1, 2, 0, 0}, {2, 3, 0, 0}, {3, 4, 0, 0}}})
        == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::InvalidReference);
    CHECK(fixture.Decision(2).rejection == ShotRejection::InvalidReference);
    CHECK(fixture.Decision(3).accepted);
}

TEST_CASE("PvP combat host publication index retains exactly the latest 64 published ticks") {
    CombatHostFixture fixture;
    for (std::size_t i = 0; i < MaxPublishedShotReferences; ++i) fixture.Step();
    REQUIRE(fixture.host.TakeSnapshot()->tick == 65);
    REQUIRE(fixture.host.SubmitActions({1, {{1, 1, 0, 0}, {2, 2, 0, 0}, {3, 66, 0, 0}, {4, 0, 0, 0}}})
        == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::InvalidReference);
    CHECK(fixture.Decision(2).accepted);
    CHECK(fixture.Decision(3).rejection == ShotRejection::InvalidReference);
    CHECK(fixture.Decision(4).rejection == ShotRejection::InvalidReference);
}

TEST_CASE("PvP combat host keeps immutable results across duplicate delivery and lost acknowledgements") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    fixture.Step();
    const auto original = fixture.Decision(1);
    REQUIRE(original.accepted);
    REQUIRE(original.damage == 25);
    auto owned = fixture.host.GetActionResults(1);
    owned->decisions.clear();
    CHECK(fixture.host.GetActionResults(1)->decisions.size() == 1);
    fixture.now += 1s;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    CHECK(fixture.host.SubmitActions({1, {{2, 2, 0, 0}, {1, 2, 0, 0}}}) == ActionAdmission::Conflict);
    fixture.Step();
    CHECK(fixture.Decision(1) == original);
    CHECK(fixture.host.GetActionResults(1)->decisions.size() == 1);
    REQUIRE(fixture.host.TakeSnapshot()->combat.back().hp == 75);
    CHECK_FALSE(fixture.host.QueueActionAcknowledgement(1, 2));
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 1));
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 1));
    CHECK(fixture.host.GetActionResults(1)->retiredThrough == 0); // I/O cannot mutate authority.
    fixture.Step();
    REQUIRE(fixture.host.GetActionResults(1)->retiredThrough == 1);
    CHECK(fixture.host.GetActionResults(1)->decisions.empty());
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->decisions.empty());
    CHECK(fixture.host.TakeSnapshot()->combat.back().hp == 75);
}

TEST_CASE("PvP combat host reserves the complete staged window and rejects batches atomically") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    CHECK(fixture.host.SubmitActions({1, {{2, 1, 0, 0}, {1, 1, 1, 0}}}) == ActionAdmission::Conflict);
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->decisions.size() == 1); // id2 did not leak.
    for (ActionId first = 2; first <= MaxActionWindow; first += MaxActionBatch) {
        ActionBatch batch{1, {}};
        for (ActionId id = first; id < first + MaxActionBatch && id <= MaxActionWindow; ++id)
            batch.shots.push_back({id, 2, 0, 0});
        REQUIRE(fixture.host.SubmitActions(batch) == ActionAdmission::Accepted);
        REQUIRE(fixture.host.SubmitActions(batch) == ActionAdmission::Accepted);
    }
    CHECK(fixture.host.SubmitActions(Shot(33, 2)) == ActionAdmission::OutsideWindow);
    CHECK_FALSE(fixture.host.QueueActionAcknowledgement(1, 32)); // Pending is not a decision.
    fixture.Step();
    REQUIRE(fixture.host.GetActionResults(1)->decisions.size() == MaxActionWindow);
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 32));
    // The already validated queued ACK reserves its future capacity atomically.
    CHECK(fixture.host.SubmitActions(Shot(33, 3)) == ActionAdmission::Accepted);
    fixture.Step();
    REQUIRE(fixture.host.GetActionResults(1)->retiredThrough == 32);
    REQUIRE(fixture.host.SubmitActions(Shot(33, 3)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->decisions.size() == 1);
}

TEST_CASE("PvP combat host action acknowledgement cannot jump unresolved ID gaps") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions(Shot(2)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(2).accepted);
    CHECK_FALSE(fixture.host.QueueActionAcknowledgement(1, 2));
    REQUIRE(fixture.host.SubmitActions(Shot(1, 2)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::Cooldown);
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 2));
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 1)); // Older queued ACK cannot regress.
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->retiredThrough == 2);
    CHECK(fixture.host.GetActionResults(1)->decisions.empty());
}

TEST_CASE("PvP combat host leave removes ingress and results without invalidating survivor references") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    REQUIRE(fixture.host.QueueLeave(3, 1));
    REQUIRE(fixture.host.QueueJoin(4, 3));
    REQUIRE(fixture.host.SubmitActions(Shot(1, 1, 2)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK_FALSE(fixture.host.GetActionResults(1));
    CHECK(fixture.host.SubmitActions(Shot(2)) == ActionAdmission::InvalidPlayer);
    CHECK(fixture.Decision(1, 2).accepted);
    REQUIRE(fixture.host.SubmitActions(Shot(1, 2, 3)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1, 3).accepted);
    CHECK(fixture.host.GetActionResults(3)->retiredThrough == 0);
}

TEST_CASE("PvP combat host reset removes queued actions acknowledgements and publication anchors") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    fixture.Step();
    REQUIRE(fixture.host.QueueActionAcknowledgement(1, 1));
    REQUIRE(fixture.host.SubmitActions(Shot(2, 2)) == ActionAdmission::Accepted);
    auto cleared = fixture.host.RequestReset();
    CHECK(fixture.host.SubmitActions(Shot(3, 2)) == ActionAdmission::InvalidPlayer);
    CHECK_FALSE(fixture.host.QueueActionAcknowledgement(1, 1));
    CHECK_FALSE(fixture.host.GetActionResults(1));
    CHECK(fixture.host.Advance(MovementTickSeconds).steps == 0);
    REQUIRE(cleared.wait_for(0s) == std::future_status::ready);
    CHECK_FALSE(fixture.host.TakeSnapshot());
    REQUIRE(fixture.host.QueueJoin(5, 1));
    // Catch-up publishes only tick 3. The former lifecycle's tick 1 anchor is gone.
    REQUIRE(fixture.host.Advance(3 * MovementTickSeconds).steps == 3);
    REQUIRE(fixture.host.SubmitActions(Shot(1)) == ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.Decision(1).rejection == ShotRejection::InvalidReference);
    CHECK(fixture.host.GetActionResults(1)->retiredThrough == 0);
    CHECK(fixture.host.TakeSnapshot()->combat.front().hp == PvpCombatRules.maximumHp);
}

TEST_CASE("PvP wire action and acknowledgement handoff is atomic and can release a full window") {
    CombatHostFixture fixture;
    for (ActionId first=1;first<=MaxActionWindow;first+=MaxActionBatch) {
        ActionBatch batch{1,{}};
        for (ActionId id=first;id<first+MaxActionBatch;++id) batch.shots.push_back({id,1,0,0});
        REQUIRE(fixture.host.SubmitActionBatch(batch,0)==ActionAdmission::Accepted);
    }
    fixture.Step();
    REQUIRE(fixture.host.GetActionResults(1)->decisions.size()==32);
    // A conflict with a retained request prevents both acknowledgement and new insertion.
    CHECK(fixture.host.SubmitActionBatch({1,{{1,1,1,0},{33,2,0,0}}},32)==ActionAdmission::Conflict);
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->retiredThrough==0);
    CHECK(fixture.host.GetActionResults(1)->decisions.size()==32);
    CHECK(fixture.host.SubmitActionBatch({1,{{33,3,0,0}}},33)==ActionAdmission::InvalidBatch);
    REQUIRE(fixture.host.SubmitActionBatch({1,{{33,3,0,0}}},32)==ActionAdmission::Accepted);
    CHECK(fixture.host.GetActionResults(1)->retiredThrough==0); // Still I/O staging only.
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->retiredThrough==32);
    REQUIRE(fixture.host.GetActionResults(1)->decisions.size()==1);
    CHECK(fixture.Decision(33).actionId==33);
    REQUIRE(fixture.host.SubmitActionBatch({1,{}},33)==ActionAdmission::Accepted);
    fixture.Step();
    CHECK(fixture.host.GetActionResults(1)->retiredThrough==33);
    CHECK(fixture.host.GetActionResults(1)->decisions.empty());
}
