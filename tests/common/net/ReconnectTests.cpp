// Reconnecting (declaration D-8): judgement G-9 as declared in section 8. The client closes; its product replaces
// the client World and the new World shakes hands again with the same user id: a new connection, a new Peer, the
// whole World sent again. Ticks are not aligned (that is the clock sync of step 3b); this test does not need them.
#include <doctest/doctest.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

TEST_CASE("reconnect: G-9 a new client World after a disconnect gets the server's World, without what was deleted meanwhile") {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    N::MemoryLinkSettings link;
    link.seed = 0x5EED0009u;
    // Datagrams are held back up to 6 steps, so some of the old connection's arrive after the new one is made.
    link.aToB = {1000, 500, 5000, 6, 0};
    link.bToA = {1000, 500, 5000, 6, 0};
    auto links = N::MemoryLink::Pair(link);
    End server = GameEnd(N::NetRole::Server, GameRules{400, true});
    server.net.link = std::make_shared<N::MemoryLink>(links.first);
    server.net.clock = reader;
    server.net.seed = 17;
    End client = GameEnd(N::NetRole::Client, GameRules{400, true});
    client.net.link = std::make_shared<N::MemoryLink>(links.second);
    client.net.serverEndpoint = N::MemoryLink::EndpointA();
    client.net.userId = 44;
    client.net.clock = reader;
    auto s = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(s), Reported(s));
    auto c = O::Product::Build(Describe(client), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(c), Reported(c));
    auto& match = **s;
    auto& player = **c;
    std::map<std::uint64_t, GameState> record;
    auto frame = [&] {
        REQUIRE(static_cast<bool>(match.RunFrame()));
        auto& serverWorld = match.World("Net").GetWorld();
        record[serverWorld.Resource<W::Tick>()->value] = StateOf(serverWorld);
        REQUIRE(static_cast<bool>(player.RunFrame()));
        links.first.Advance();
        *clock += Ms(16);
    };
    for (int i = 0; i < 80; ++i) frame();
    REQUIRE(PeersOf(match.World("Net").GetWorld()).size() == 1);
    const std::uint32_t oldConnection = PeersOf(match.World("Net").GetWorld())[0].connectionId;

    // The client closes; the product replaces its World at once, and the new World shakes hands again.
    player.Buffer().Find<N::NetService>()->Close();
    const std::set<K::EntityId> before = StateOf(match.World("Net").GetWorld()).entities;
    REQUIRE(static_cast<bool>(player.ReplaceWorld("Net", {})));
    for (int i = 0; i < 30; ++i) frame();
    const auto peers = PeersOf(match.World("Net").GetWorld());
    REQUIRE_MESSAGE(peers.size() == 1, "the server has the new connection's Peer only");
    CHECK_MESSAGE(peers[0].connectionId != oldConnection, "a reconnect is a new connection");
    CHECK(peers[0].userId == 44);
    const auto& ended = SeenIn(match.World("Net").GetWorld()).disconnected;
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == N::DisconnectReason::ClosedByRemote);

    // G-5's method on the new World: at each tick T where D-7 holds and nothing newer was written, equal to tick T.
    std::optional<std::uint32_t> observed;
    std::uint64_t observations = 0, mismatches = 0;
    for (int i = 0; i < 150; ++i) {
        frame();
        const auto& replica = *player.World("Net").GetWorld().Resource<N::NetReplicaState>();
        if (replica.latestConfirmed && *replica.latestConfirmed == replica.maxAppliedTick && observed != replica.latestConfirmed) {
            observed = replica.latestConfirmed;
            ++observations;
            if (StateOf(player.World("Net").GetWorld()) != record.at(*observed)) ++mismatches;
        }
    }
    const GameState now = StateOf(player.World("Net").GetWorld());
    std::uint64_t deletedStillThere = 0, deleted = 0;
    const std::set<K::EntityId> alive = StateOf(match.World("Net").GetWorld()).entities;
    for (const K::EntityId id : before) {
        if (alive.count(id) != 0) continue;
        ++deleted;
        deletedStillThere += now.entities.count(id);
    }
    auto* session = player.Buffer().Find<N::NetService>()->Client();
    const std::uint64_t mismatchedConnection =
        session->Transport()->Stats().drops[static_cast<std::size_t>(N::DropReason::ConnectionMismatch)];
    const std::uint64_t beforeConnected = session->Drops()[static_cast<std::size_t>(N::DropReason::UnknownSource)];
    MESSAGE("G-9: observations=" << observations << " mismatches=" << mismatches << " | entities deleted since the disconnect="
                                 << deleted << " still at the client=" << deletedStillThere
                                 << " | old connection's datagrams dropped: connection id mismatch=" << mismatchedConnection
                                 << " before the new connection was made=" << beforeConnected);
    CHECK(observations > 0);
    CHECK_MESSAGE(mismatches == 0, "G-9: after the reconnect the client equals the server at each confirmed tick");
    CHECK(deleted > 0);
    CHECK_MESSAGE(deletedStillThere == 0, "G-9: an entity the server deleted during the disconnect is not at the client");
    CHECK_MESSAGE(mismatchedConnection + beforeConnected > 0, "G-9: the old connection's datagrams are dropped and counted");
}
