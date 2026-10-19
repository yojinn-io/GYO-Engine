#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <algorithm>

#include <atomic>
#include <chrono>
#include <thread>

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

// edge-input-passthrough (diagnostics only): the host times the wait for its
// mutex once per SubmitInput and once per step, and a SubmitInput that waits
// while a step holds the mutex records that wait.
TEST_CASE("PvP host records each wait for its mutex") {
    std::atomic<bool> hold{false}, holding{false};
    MatchRuntimeHost host{CombatHostArena(), [&] {
        if (hold.exchange(false)) {
            holding = true;
            std::this_thread::sleep_for(100ms); // inside Step, under the mutex
        }
        return std::chrono::steady_clock::time_point{};
    }};
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.Advance(MovementTickSeconds).steps == 1);
    static_cast<void>(host.TakeStatistics());
    const PlayerInput input{1, {{1, 1.0f, 0.0f, 0.0f, 0.0f, false}}, 1, 1};
    for (int k = 0; k < 3; ++k) static_cast<void>(host.SubmitInput(input));
    hold = true;
    std::thread step([&] { static_cast<void>(host.Advance(MovementTickSeconds)); });
    while (!holding) std::this_thread::yield();
    static_cast<void>(host.SubmitInput(input));
    step.join();
    const auto statistics = host.TakeStatistics();
    CHECK(statistics.lockWaits.input.Count() == 4);
    CHECK(statistics.lockWaits.step.Count() == 1);
    CHECK(statistics.lockWaits.input.Max() >= 10ms);
}

// pv7 §5 (D-6): the I/O layer learns why the host refused a batch, as the
// host counts it; a reset in progress is Resetting, not an unknown player.
TEST_CASE("PvP combat host tells the I/O layer the reason of each refused batch") {
    CombatHostFixture fixture;
    auto reason = IngressActionRejection::Count;
    CHECK(fixture.host.SubmitActionBatch(Shot(1, 1, 9), 0, &reason) == ActionAdmission::InvalidPlayer);
    CHECK(reason == IngressActionRejection::InvalidPlayer);
    CHECK(fixture.host.SubmitActionBatch(Shot(1), 5, &reason) == ActionAdmission::InvalidBatch);
    CHECK(reason == IngressActionRejection::InvalidBatch);
    reason = IngressActionRejection::Count;
    CHECK(fixture.host.SubmitActionBatch(Shot(1), 0, &reason) == ActionAdmission::Accepted);
    CHECK(reason == IngressActionRejection::Count); // an accepted batch leaves it untouched
    CHECK(fixture.host.SubmitActionBatch(Shot(1, 2), 0, &reason) == ActionAdmission::Conflict);
    CHECK(reason == IngressActionRejection::Conflict);
    auto cleared = fixture.host.RequestReset();
    CHECK(fixture.host.SubmitActionBatch(Shot(2), 0, &reason) == ActionAdmission::InvalidPlayer);
    CHECK(reason == IngressActionRejection::Resetting);
    CHECK(fixture.host.SubmitActionBatch(Shot(3), 0) == ActionAdmission::InvalidPlayer); // without the out-parameter
    static_cast<void>(fixture.host.Advance(MovementTickSeconds));
    REQUIRE(cleared.wait_for(0s) == std::future_status::ready);
}

// pv7 §5 (D-4): the host hands over a snapshot with its publication time on
// the host's clock, the start of both its reference age and the Match hold.
TEST_CASE("PvP combat host hands over each snapshot with its publication time") {
    CombatHostFixture fixture;
    static_cast<void>(fixture.host.TakeSnapshot());
    fixture.now += 3s;
    const auto published = fixture.now;
    fixture.Step();
    fixture.now += 40ms; // taken later: the publication time stays
    CHECK(fixture.host.Now() == fixture.now);
    auto at = std::chrono::steady_clock::time_point::min();
    const auto snapshot = fixture.host.TakeSnapshot(&at);
    REQUIRE(snapshot);
    CHECK(at == published);
    CHECK_FALSE(fixture.host.TakeSnapshot(&at)); // nothing new: at is not read
}

namespace {
std::vector<ShotDecision> HostDecisions(const MatchRuntimeHost& host, PlayerId playerId) {
    const auto results = host.GetActionResults(playerId);
    return results ? results->decisions : std::vector<ShotDecision>{};
}
}

// pv7 §7: the view tick never reaches a decision (E and S do since A1). The
// same requests with other view ticks (an invalid one included) give the same
// decisions and the same world; the host only records them, one record per
// decision, and counts the invalid view tick.
TEST_CASE("PvP combat host records the view tick without deciding by it") {
    CombatHostFixture plain, carrying;
    const auto shot = [](ActionId id, std::uint64_t view) {
        return ShotRequest{id, 1, 0, 0, ActionKind::Shot, 1, 7, 40, view};
    };
    REQUIRE(plain.host.SubmitActions({1, {shot(1, 0)}}) == ActionAdmission::Accepted);
    REQUIRE(carrying.host.SubmitActions({1, {shot(1, 900 * 65536)}}) == ActionAdmission::Accepted); // beyond observed: invalid
    REQUIRE(plain.host.SubmitActions({2, {{1, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
    REQUIRE(carrying.host.SubmitActions({2, {{1, 1, 0, 0, ActionKind::Reload, 1, 0, 0, 65536}}}) == ActionAdmission::Accepted);
    plain.Step();
    carrying.Step();
    for (const PlayerId player : {PlayerId{1}, PlayerId{2}}) {
        REQUIRE(HostDecisions(plain.host, player).size() == 1);
        CHECK(HostDecisions(plain.host, player) == HostDecisions(carrying.host, player));
    }
    CHECK(plain.host.TakeSnapshot()->combat == carrying.host.TakeSnapshot()->combat);

    const auto without = plain.host.DrainShotRecords();
    const auto with = carrying.host.DrainShotRecords();
    REQUIRE(without.records.size() == 2);
    REQUIRE(with.records.size() == 2);
    CHECK(without.viewTickInvalid == 0);
    CHECK(with.viewTickInvalid == 2); // the shot's beyond its observed tick, the reload's nonzero
    const auto shooter = std::find_if(with.records.begin(), with.records.end(), [](const auto& r) { return r.playerId == 1; });
    REQUIRE(shooter != with.records.end());
    const auto decision = HostDecisions(carrying.host, 1).front();
    CHECK(shooter->actionId == 1);
    CHECK(shooter->handoffTick == decision.resolvedTick);
    CHECK(shooter->dueTick == decision.resolvedTick);
    CHECK(shooter->resolvedTick == decision.resolvedTick);
    CHECK(shooter->epoch == 7);
    CHECK(shooter->sequence == 40);
    CHECK(shooter->step.path == ShotStepPath::Epoch); // E = 7 is not the shooter's epoch: decided at A
    CHECK(shooter->rejection == decision.rejection);
    CHECK_FALSE(shooter->viewValid);
    CHECK_FALSE(shooter->viewLagQ16);
    REQUIRE(shooter->cooldownHoldTicks);
    CHECK(*shooter->cooldownHoldTicks == 0);
    CHECK_FALSE(shooter->held);
    const auto reloader = std::find_if(with.records.begin(), with.records.end(), [](const auto& r) { return r.playerId == 2; });
    REQUIRE(reloader != with.records.end());
    CHECK(reloader->kind == ActionKind::Reload);
    CHECK_FALSE(reloader->cooldownHoldTicks);
    CHECK(carrying.host.DrainShotRecords().records.empty()); // drained once
}

// A valid view tick gives the lag from the decision tick (pv7 §7 view_lag).
TEST_CASE("PvP combat host records the view lag of a valid view tick") {
    CombatHostFixture fixture;
    fixture.Step();
    fixture.Step();
    const auto observed = fixture.host.TakeSnapshot()->tick;
    REQUIRE(observed >= 2);
    REQUIRE(fixture.host.SubmitActions({1, {{1, observed, 0, 0, ActionKind::Shot, 1, 0, 0, (observed - 1) * 65536 + 16384}}}) ==
            ActionAdmission::Accepted);
    fixture.Step();
    const auto records = fixture.host.DrainShotRecords();
    REQUIRE(records.records.size() == 1);
    const auto& record = records.records.front();
    CHECK(record.viewValid);
    REQUIRE(record.viewLagQ16);
    CHECK(*record.viewLagQ16 == static_cast<std::int64_t>(record.handoffTick * 65536) -
                                static_cast<std::int64_t>((observed - 1) * 65536 + 16384));
    CHECK(records.viewTickInvalid == 0);
}

// A shot two ticks inside the cooldown is held to its boundary (A1, D136):
// the record keeps A, the first due tick D (here A: E = 0 is another epoch)
// and the decision tick J = D + 2, written when the decision comes.
TEST_CASE("PvP combat host records a held shot when it is decided") {
    CombatHostFixture fixture;
    REQUIRE(fixture.host.SubmitActions({1, {{1, 1, 0, 0}}}) == ActionAdmission::Accepted);
    fixture.Step();
    const auto first = HostDecisions(fixture.host, 1).front();
    REQUIRE(first.accepted);
    for (unsigned k = 0; k < PvpCombatRules.cooldownTicks - 3; ++k) fixture.Step();
    REQUIRE(fixture.host.SubmitActions({1, {{2, 1, 0, 0}}}) == ActionAdmission::Accepted);
    fixture.Step(); // A: two ticks before the boundary
    CHECK(fixture.host.DrainShotRecords().records.size() == 1); // only the first
    fixture.Step();
    CHECK(fixture.host.DrainShotRecords().records.empty());
    fixture.Step();
    const auto records = fixture.host.DrainShotRecords();
    REQUIRE(records.records.size() == 1);
    const auto& second = records.records.front();
    REQUIRE(second.actionId == 2);
    CHECK(second.rejection == ShotRejection::None);
    CHECK(second.step.path == ShotStepPath::Epoch);
    CHECK(second.dueTick == second.handoffTick);
    CHECK(second.resolvedTick == first.resolvedTick + PvpCombatRules.cooldownTicks);
    CHECK(second.resolvedTick == second.dueTick + 2);
    CHECK(second.held);
    REQUIRE(second.cooldownHoldTicks);
    CHECK(*second.cooldownHoldTicks == 2);
}

// D127: each shadow record carries when the host recorded it, on the movement
// trace's clock (MovementTraceNowNs, the time_ns of match-commands.jsonl), so
// the offline join can order records and events across a reconnection.
TEST_CASE("PvP combat host stamps each shadow record on the movement trace's clock") {
    CombatHostFixture fixture;
    std::vector<std::pair<std::int64_t, std::int64_t>> windows;
    for (ActionId id = 1; id <= 3; ++id) {
        REQUIRE(fixture.host.SubmitActions({1, {{id, 1, 0, 0, ActionKind::Reload, 1}}}) == ActionAdmission::Accepted);
        const auto before = MovementTraceNowNs();
        fixture.Step();
        windows.emplace_back(before, MovementTraceNowNs());
    }
    const auto records = fixture.host.DrainShotRecords().records;
    REQUIRE(records.size() == 3);
    for (std::size_t k = 0; k < records.size(); ++k) {
        CAPTURE(k);
        CHECK(records[k].timeNs >= windows[k].first);
        CHECK(records[k].timeNs <= windows[k].second);
        if (k) CHECK(records[k].timeNs >= records[k - 1].timeNs);
    }
}
