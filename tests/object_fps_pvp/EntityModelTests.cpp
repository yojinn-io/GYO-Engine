// Guards of the entity data model's Design Invariants
// (docs/object_fps_pvp/entity-model.zh-Hant.md). G3: a published snapshot
// carries the same entities in every component (E-7).
#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fps::pvp {
// The private columns' id sequences, in column order (entity-model G2). A
// friend of PvpMatch; defined here so only the tests can read them.
struct PvpMatchTestAccess final {
    static std::vector<std::pair<std::string, std::vector<EntityId>>> ColumnIds(const PvpMatch& match) {
        std::vector<std::pair<std::string, std::vector<EntityId>>> columns;
        const auto column = [&](const char* name, const auto& values) {
            std::vector<EntityId> ids;
            for (const auto& value : values) ids.push_back(EntityOf(value));
            columns.emplace_back(name, std::move(ids));
        };
        column("bodies", match.bodies_);
        column("combats", match.combats_);
        column("animations", match.animations_);
        column("actionLedgers", match.actionLedgers_);
        column("connections", match.connections_);
        std::vector<EntityId> shapes, actionStart;
        for (const auto& [id, value] : match.shapes_) shapes.push_back(id);
        for (const auto& [id, value] : match.actionsCombat_) actionStart.push_back(id);
        columns.emplace_back("tickShapes", std::move(shapes));
        columns.emplace_back("actionStartCombats", std::move(actionStart));
        return columns;
    }
};
} // namespace fps::pvp

namespace {
using namespace fps::pvp;

Arena EntityModelArena() {
    Arena arena;
    arena.id = "synthetic_entity_model";
    arena.width = arena.depth = 12;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 6}, 0}};
    return arena;
}

template <typename Component>
std::vector<PlayerId> Ids(const std::vector<Component>& components) {
    std::vector<PlayerId> ids;
    for (const auto& component : components) ids.push_back(component.playerId);
    return ids;
}

Arena ThreeSpawnArena() {
    Arena arena;
    arena.id = "synthetic_three_spawns";
    arena.width = arena.depth = 12;
    arena.spawns = {{{2, 0, 2}, 0}, {{2, 0, 6}, 0}, {{2, 0, 10}, 0}};
    return arena;
}

void Step(PvpMatch& match, unsigned count = 1, const ShotReferenceAge& age = {}) {
    for (unsigned i = 0; i < count; ++i) match.Tick({match.TickCount() + 1, 1.0 / 60}, age);
}

void RequireSnapshotIds(const PvpMatch& match, const std::vector<PlayerId>& expected) {
    const auto snapshot = match.Snapshot();
    CAPTURE(snapshot.tick);
    CHECK(Ids(snapshot.players) == expected);
    CHECK(Ids(snapshot.combat) == expected);
    CHECK(Ids(snapshot.animation) == expected);
}

} // namespace

TEST_CASE("G1 EntityColumnsStayOrdered: descending joins and a rejoin keep every column in id order") {
    PvpMatch match(ThreeSpawnArena());
    std::string error;
    // 3, 2, 1: an append would leave the columns in join order.
    REQUIRE(match.Join(3, error));
    REQUIRE(match.Join(2, error));
    REQUIRE(match.Join(1, error));
    for (unsigned t = 0; t < 120; ++t) {
        Step(match);
        RequireSnapshotIds(match, {1, 2, 3});
    }
    REQUIRE(match.Leave(1));
    Step(match);
    RequireSnapshotIds(match, {2, 3});
    REQUIRE(match.Join(1, error)); // smaller than every id present: not an append either
    Step(match);
    RequireSnapshotIds(match, {1, 2, 3});
    for (const auto& [name, ids] : PvpMatchTestAccess::ColumnIds(match)) {
        CAPTURE(name);
        if (name == "tickShapes") continue; // only alive shooters' targets, filled by a shot
        CHECK(ids == std::vector<EntityId>{1, 2, 3});
    }
}

TEST_CASE("G2 EraseEntityRemovesEveryColumn: a leave empties every column and tick cache of the entity") {
    PvpMatch match(ThreeSpawnArena());
    std::string error;
    REQUIRE(match.Join(1, error));
    REQUIRE(match.Join(2, error));
    Step(match);
    // One accepted shot fills the tick shapes and the action-start combats.
    REQUIRE(match.SubmitActions({1, {{1, 1, 0, 0, ActionKind::Shot, 1}}}) == ActionAdmission::Accepted);
    Step(match, 1, [](std::uint64_t) { return std::optional<std::chrono::nanoseconds>{std::chrono::nanoseconds{0}}; });
    const auto results = match.GetActionResults(1);
    REQUIRE(results);
    REQUIRE(results->decisions.size() == 1);
    CHECK(results->decisions[0].accepted);
    for (const auto& [name, ids] : PvpMatchTestAccess::ColumnIds(match)) {
        CAPTURE(name);
        CHECK(ids == std::vector<EntityId>{1, 2});
    }
    REQUIRE(match.Leave(1));
    for (const auto& [name, ids] : PvpMatchTestAccess::ColumnIds(match)) {
        CAPTURE(name);
        CHECK(ids == std::vector<EntityId>{2});
    }
    CHECK_FALSE(match.ContainsPlayer(1));
    CHECK_FALSE(match.GetActionResults(1));
    CHECK_FALSE(match.GetMovementQuality(1));
    CHECK_FALSE(match.GetActionTiming(1, 1));
    CHECK(match.AdmitInput({1, {{1, 1, 0, 0, 0}}, 1, 1}) == InputAdmission::UnknownPlayer);
    RequireSnapshotIds(match, {2});
    // A leftover row would make this re-join a duplicate insert (GYO_ASSERT).
    REQUIRE(match.Join(1, error));
    Step(match);
    RequireSnapshotIds(match, {1, 2});
}

TEST_CASE("G3 PublicationComponentsAgree: an eviction removes the player from every snapshot component") {
    auto clock = std::chrono::steady_clock::time_point{} + std::chrono::hours(1);
    MatchRuntimeHost host(EntityModelArena(), [&clock] { return clock; });
    REQUIRE(host.QueueJoin(1, 1));
    REQUIRE(host.QueueJoin(2, 2));
    std::map<PlayerId, std::uint64_t> next{{1, 1}, {2, 1}}, epochs{{1, 1}, {2, 1}};
    std::optional<WorldSnapshot> latest;
    bool evicted = false;
    // Player 2 reports snapshots a quarter second old (above the 160 ms
    // reference age limit): three failed windows after the unjudged first one.
    for (std::uint64_t t = 0; t < 6 * ConnectionQualityWindowTicks && !evicted; ++t) {
        for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
            const PlayerState* state = nullptr;
            if (latest)
                for (const auto& player : latest->players)
                    if (player.playerId == id) state = &player;
            if (!state || state->lifeState == LifeState::Dead) continue;
            if (state->movementEpoch != epochs[id]) {
                epochs[id] = state->movementEpoch;
                next[id] = state->lastResolvedCommand + 1;
            }
            const auto observed = id == 2 && latest->tick > 15 ? latest->tick - 15 : latest->tick;
            static_cast<void>(host.SubmitInput({id, {{next[id]++, 1.0F, 0, 0, 0, false}}, state->movementEpoch,
                state->lifeGeneration, observed}));
        }
        clock += std::chrono::nanoseconds(1'000'000'000 / AuthorityTickRate);
        static_cast<void>(host.Advance(MovementTickSeconds));
        if (auto snapshot = host.TakeSnapshot()) {
            latest = std::move(snapshot);
            CAPTURE(latest->tick);
            CHECK(Ids(latest->combat) == Ids(latest->players));
            CHECK(Ids(latest->animation) == Ids(latest->players));
        }
        for (const auto& eviction : host.TakeEvictions()) {
            CHECK(eviction.playerId == 2);
            evicted = true;
        }
    }
    // Without an eviction the checks above prove nothing.
    REQUIRE(evicted);
    REQUIRE(latest);
    CHECK(Ids(latest->players) == std::vector<PlayerId>{1});
}
