// A client World's connection (docs/architecture/target/v2/net-connect-target-declaration.zh-Hant.md, judgements J1
// to J5): whom a client connects to is the World's NetConnectTarget, read at its first frame; without one it connects
// to nothing, and a client's UDP socket opens with its first target. Each CHECK carries a tag in brackets
// ("[nt-target-...]"): the batch's mutants expect it.
#include <doctest/doctest.h>

#include <memory>
#include <optional>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/net/UdpLink.hpp"

namespace N = GYO::Engine::Net;
namespace OS = GYO::Engine::OS;
using namespace GYO::Test::Net;

namespace {

// A server and a client over a memory link. The client World's target is what *want holds when the World is prepared
// (its first build and every replacement).
struct Pair {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    std::shared_ptr<std::optional<N::NetConnectTarget>> want = std::make_shared<std::optional<N::NetConnectTarget>>();
    std::unique_ptr<O::Product> server, client;

    explicit Pair(std::optional<N::NetConnectTarget> first) {
        *want = std::move(first);
        auto reader = [c = clock] { return *c; };
        End s = GameEnd(N::NetRole::Server, GameRules{400, true});
        s.net.link = std::make_shared<N::MemoryLink>(links.first);
        s.net.clock = reader;
        s.net.seed = 23;
        End c = GameEnd(N::NetRole::Client, GameRules{400, true});
        c.net.link = std::make_shared<N::MemoryLink>(links.second);
        c.net.clock = reader;
        c.build = [inner = c.build, w = want](O::WorldSetup& setup) {
            inner(setup);
            setup.Prepare([w](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
                if (*w) world.SetResource(**w);
                return {};
            });
        };
        auto builtServer = O::Product::Build(Describe(s), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
        server = std::move(builtServer).value();
        auto builtClient = O::Product::Build(Describe(c), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtClient), Reported(builtClient));
        client = std::move(builtClient).value();
    }
    void Frames(const int n) {
        for (int i = 0; i < n; ++i) {
            REQUIRE(static_cast<bool>(server->RunFrame()));
            REQUIRE(static_cast<bool>(client->RunFrame()));
            links.first.Advance();
            *clock += Ms(16);
        }
    }
    std::vector<N::Peer> ServerPeers() { return PeersOf(server->World("Net").GetWorld()); }
    N::NetService& ClientService() { return *client->Buffer().Find<N::NetService>(); }
};

N::NetConnectTarget ToServer(const std::uint64_t user) { return N::NetConnectTarget{N::MemoryLink::EndpointA(), user, {}}; }

} // namespace

TEST_CASE("connect-target: J1 a client World without a NetConnectTarget connects to nothing") {
    Pair pair(std::nullopt);
    pair.Frames(40);
    CHECK_MESSAGE(pair.ServerPeers().empty(), "[nt-target-none] the server has no connection");
    CHECK(pair.ClientService().Client() == nullptr);
}

TEST_CASE("connect-target: J2 J3 the same target keeps the connection, another opens a new one, none closes it") {
    Pair pair(ToServer(7));
    pair.Frames(40);
    REQUIRE(pair.ServerPeers().size() == 1);
    const std::uint32_t first = pair.ServerPeers()[0].connectionId;
    CHECK(pair.ServerPeers()[0].userId == 7);

    // A replacement with the same target: the connection goes on (and the new World asks for a resync, R2).
    REQUIRE(static_cast<bool>(pair.client->ReplaceWorld("Net", {})));
    pair.Frames(20);
    REQUIRE(pair.ServerPeers().size() == 1);
    CHECK_MESSAGE(pair.ServerPeers()[0].connectionId == first, "[nt-target-same-kept] the same connection");

    // Another target (another user id): the old connection is closed and a new one made.
    *pair.want = ToServer(8);
    REQUIRE(static_cast<bool>(pair.client->ReplaceWorld("Net", {})));
    pair.Frames(30);
    // The server has the new one only. (Over one endpoint the new handshake's larger salt also ends the old one at the
    // server, S2 (a): that its Disconnect is sent is judged by the World without a target below and by the next case.)
    const auto& ended = SeenIn(pair.server->World("Net").GetWorld()).disconnected;
    CHECK_MESSAGE((ended.size() == 1 && ended[0].reason == N::DisconnectReason::ClosedByRemote && pair.ServerPeers().size() == 1),
                  "[nt-target-other-closes] ended " << ended.size() << ", peers " << pair.ServerPeers().size());
    REQUIRE(!pair.ServerPeers().empty());
    CHECK_MESSAGE(pair.ServerPeers().back().connectionId != first, "[nt-target-other-new] a new connection");
    CHECK(pair.ServerPeers().back().userId == 8);

    // No target: the connection is closed and none made.
    pair.want->reset();
    REQUIRE(static_cast<bool>(pair.client->ReplaceWorld("Net", {})));
    pair.Frames(30);
    CHECK_MESSAGE(pair.ServerPeers().empty(), "[nt-target-none-closes] the server has no connection left");
    CHECK(pair.ClientService().Client() == nullptr);
}

TEST_CASE("connect-target: J3 a replacement's extra gives the target (the lobby's way)") {
    Pair pair(std::nullopt);
    pair.Frames(10);
    REQUIRE(pair.ServerPeers().empty());
    O::ReplaceRequest request;
    request.extra = [](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(ToServer(9));
        return {};
    };
    REQUIRE(static_cast<bool>(pair.client->ReplaceWorld("Net", std::move(request))));
    pair.Frames(30);
    REQUIRE(pair.ServerPeers().size() == 1);
    CHECK(pair.ServerPeers()[0].userId == 9);
}

TEST_CASE("connect-target: J4 J5 over UDP the socket opens with the first target; a target of the other kind connects to nothing") {
    // A server listens from the start.
    N::NetProductOptions serverOptions;
    serverOptions.role = N::NetRole::Server;
    serverOptions.bind = OS::SocketAddress::Loopback(0);
    End server = GameEnd(N::NetRole::Server, GameRules{});
    server.net = serverOptions;
    auto s = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(s), Reported(s));
    CHECK_MESSAGE((*s)->Buffer().Find<N::NetService>()->UdpAddress().has_value(), "[nt-target-server-listens]");

    // A client without a target opens no socket.
    End lone = GameEnd(N::NetRole::Client, GameRules{});
    lone.net.bind = OS::SocketAddress::Loopback(0);
    auto alone = O::Product::Build(Describe(lone), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(alone), Reported(alone));
    for (int i = 0; i < 5; ++i) REQUIRE(static_cast<bool>((*alone)->RunFrame()));
    CHECK_MESSAGE(!(*alone)->Buffer().Find<N::NetService>()->UdpAddress().has_value(), "[nt-target-no-socket] single play opens no port");

    // A client given an Endpoint over UDP: the other kind, a link failure, no connection, no socket.
    End wrong = GameEnd(N::NetRole::Client, GameRules{});
    wrong.net.bind = OS::SocketAddress::Loopback(0);
    wrong.target.server = N::MemoryLink::EndpointA();
    auto mismatched = O::Product::Build(Describe(wrong), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(mismatched), Reported(mismatched));
    for (int i = 0; i < 3; ++i) REQUIRE(static_cast<bool>((*mismatched)->RunFrame()));
    N::NetService& wrongService = *(*mismatched)->Buffer().Find<N::NetService>();
    CHECK_MESSAGE(wrongService.Client() == nullptr, "[nt-target-wrong-kind] no connection");
    CHECK(wrongService.LinkFailures() == 1);

    // A client given the server's address: its socket opens at its first frame and it connects.
    End player = GameEnd(N::NetRole::Client, GameRules{});
    player.net.bind = OS::SocketAddress::Loopback(0);
    player.target.server = OS::SocketAddress::Loopback((*s)->Buffer().Find<N::NetService>()->UdpAddress()->port);
    player.target.userId = 11;
    auto c = O::Product::Build(Describe(player), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(c), Reported(c));
    CHECK_MESSAGE(!(*c)->Buffer().Find<N::NetService>()->UdpAddress().has_value(), "[nt-target-socket-late] not open before its first frame");
    // Until both ends have the connection (a session closed while still shaking hands has no Disconnect to send).
    const auto connected = [&] {
        const N::ClientSession* session = (*c)->Buffer().Find<N::NetService>()->Client();
        return !PeersOf((*s)->World("Net").GetWorld()).empty() && session != nullptr && session->State() == N::ClientState::Connected;
    };
    for (int i = 0; i < 200 && !connected(); ++i) {
        REQUIRE(static_cast<bool>((*c)->RunFrame()));
        REQUIRE(static_cast<bool>((*s)->RunFrame()));
    }
    CHECK((*c)->Buffer().Find<N::NetService>()->UdpAddress().has_value());
    const auto peers = PeersOf((*s)->World("Net").GetWorld());
    REQUIRE_MESSAGE(peers.size() == 1, "the client connected over UDP");
    CHECK(peers[0].userId == 11);

    // Back in a World that names no server (single play after a match): the socket closes once the Disconnect went.
    REQUIRE(static_cast<bool>((*c)->ReplaceWorld("Net", {.extra = [](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.RemoveResource<N::NetConnectTarget>();
        return {};
    }})));
    for (int i = 0; i < 5; ++i) {
        REQUIRE(static_cast<bool>((*c)->RunFrame()));
        REQUIRE(static_cast<bool>((*s)->RunFrame()));
    }
    CHECK_MESSAGE(!(*c)->Buffer().Find<N::NetService>()->UdpAddress().has_value(), "[nt-target-socket-closed] no port held in single play");
    CHECK_MESSAGE(PeersOf((*s)->World("Net").GetWorld()).empty(), "the server took the player off (the Disconnect went before the close)");
}

TEST_CASE("connect-target: J3 closed just before the replacement, its Disconnect still goes out (not left to time out)") {
    Pair pair(ToServer(7));
    pair.Frames(40);
    REQUIRE(pair.ServerPeers().size() == 1);
    // The product closes and replaces the World at once, with no frame between (D-8's way), to a World without a target.
    pair.ClientService().Close();
    pair.want->reset();
    REQUIRE(static_cast<bool>(pair.client->ReplaceWorld("Net", {})));
    pair.Frames(5);
    const auto& ended = SeenIn(pair.server->World("Net").GetWorld()).disconnected;
    CHECK_MESSAGE((ended.size() == 1 && ended[0].reason == N::DisconnectReason::ClosedByRemote && pair.ServerPeers().empty()),
                  "[nt-target-closed-sent] the server took the player off at once: ended " << ended.size());
}

TEST_CASE("connect-target: J4 J5 a client socket that cannot open, and an address over a given Link, connect to nothing") {
    // The port is taken: the client's socket cannot open; told as a link failure, no connection.
    auto taken = N::UdpLink::Open(OS::SocketAddress::Loopback(0));
    REQUIRE(static_cast<bool>(taken));
    const auto port = taken->LocalAddress()->port;
    End blocked = GameEnd(N::NetRole::Client, GameRules{});
    blocked.net.bind = OS::SocketAddress::Loopback(port);
    blocked.target.server = OS::SocketAddress::Loopback(1);
    auto b = O::Product::Build(Describe(blocked), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(b), Reported(b));
    for (int i = 0; i < 3; ++i) REQUIRE(static_cast<bool>((*b)->RunFrame()));
    N::NetService& blockedService = *(*b)->Buffer().Find<N::NetService>();
    CHECK_MESSAGE((blockedService.Client() == nullptr && blockedService.LinkFailures() == 1 && !blockedService.UdpAddress()),
                  "[nt-target-open-failed] no socket, no connection, one link failure");

    // An address over a Link the product gave: the other kind.
    auto links = N::MemoryLink::Pair({});
    End wrong = GameEnd(N::NetRole::Client, GameRules{});
    wrong.net.link = std::make_shared<N::MemoryLink>(links.second);
    wrong.target.server = OS::SocketAddress::Loopback(40000);
    auto w = O::Product::Build(Describe(wrong), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(w), Reported(w));
    for (int i = 0; i < 3; ++i) REQUIRE(static_cast<bool>((*w)->RunFrame()));
    N::NetService& wrongService = *(*w)->Buffer().Find<N::NetService>();
    CHECK(wrongService.Client() == nullptr);
    CHECK(wrongService.LinkFailures() == 1);
}
