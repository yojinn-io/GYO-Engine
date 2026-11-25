// A user who connects again while their connection is open (the Client crashed and came back): the newer connection
// takes the player over and the older one is closed (the user's decision of 2026-11-25,
// v8-arch-gyo2-direct-connect-and-kick-old; gateway declaration L5).

#include "WorldTestSupport.hpp"

#include <chrono>
#include <optional>

namespace {

using namespace object_fps_pvp::test;

std::size_t Peers(K::World& world) {
    std::size_t count = 0;
    for (const K::EntityId id : world.Entities()) count += world.Get<N::Peer>(id) != nullptr ? 1 : 0;
    return count;
}

std::size_t PlayersOf(K::World& world, const std::uint64_t userId) {
    std::size_t count = 0;
    for (const K::EntityId id : world.Entities()) {
        const PW::Owner* owner = world.Get<PW::Owner>(id);
        count += owner != nullptr && owner->userId == userId ? 1 : 0;
    }
    return count;
}

// The player a Peer of the World controls (0: none).
K::EntityId Controlled(K::World& world) {
    for (const K::EntityId id : world.Entities()) {
        if (const auto* controls = world.Get<N::PeerControls>(id)) return controls->entity;
    }
    return 0;
}

} // namespace

TEST_CASE("world: a user's newer connection takes their player over and the older one is closed") {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::vector<N::MemoryLink> serverEnds;
    for (int i = 0; i < 2; ++i) {
        N::MemoryLinkSettings settings;
        settings.seed = static_cast<std::uint64_t>(70 + i);
        links.push_back(N::MemoryLink::Pair(settings));
        serverEnds.push_back(links.back().first);
    }
    auto seats = std::make_shared<PW::Seats>(2);
    auto match = BuildMatch(ShippedContent(), std::make_shared<HubLink>(serverEnds), seats, reader);
    auto first = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(links[0].second), 1, reader);
    std::unique_ptr<O::Product> second;
    const auto frames = [&](const int n) {
        for (int i = 0; i < n; ++i) {
            REQUIRE(static_cast<bool>(match->RunFrame()));
            REQUIRE(static_cast<bool>(first->RunFrame()));
            if (second) REQUIRE(static_cast<bool>(second->RunFrame()));
            for (auto& link : links) link.first.Advance();
            *clock += std::chrono::milliseconds(16);
        }
    };
    frames(90);
    const K::EntityId player = PlayerOf(GameWorld(*match), 1);
    REQUIRE(player != 0);
    REQUIRE(Peers(GameWorld(*match)) == 1);

    // The same user again, from another endpoint (a new Client: the old one is still connected).
    second = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(links[1].second), 1, reader);
    frames(90);

    K::World& world = GameWorld(*match);
    CHECK_MESSAGE(Peers(world) == 1, "the older connection was not closed");
    CHECK_MESSAGE(PlayersOf(world, 1) == 1, "the user has " << PlayersOf(world, 1) << " players");
    CHECK_MESSAGE(PlayerOf(world, 1) == player, "the newer connection did not take the player over");
    CHECK_MESSAGE(Controlled(world) == player, "the Match's Peer does not control the player");
    CHECK(seats->Taken() == 1);
    CHECK_MESSAGE(Peers(GameWorld(*first)) == 0, "the replaced Client still has its connection");
    CHECK_MESSAGE(Controlled(GameWorld(*second)) == player, "the newer Client does not control the player");
}
