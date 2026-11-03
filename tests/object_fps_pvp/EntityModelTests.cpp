// Guards of the entity data model's Design Invariants
// (docs/object_fps_pvp/entity-model.zh-Hant.md). G3: a published snapshot
// carries the same entities in every component (E-7).
#include <doctest/doctest.h>

#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <vector>

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

} // namespace

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
