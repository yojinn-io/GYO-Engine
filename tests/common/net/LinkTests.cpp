// The Links (net Module declaration D-3): UdpLink over loopback, and the Endpoint packing.
#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "NetTestSupport.hpp"
#include "GameTestSupport.hpp"
#include "engine/net/UdpLink.hpp"

namespace N = GYO::Engine::Net;
namespace OS = GYO::Engine::OS;
namespace O = GYO::Engine::Orchestration;
using GYO::Test::Net::Bytes;

TEST_CASE("link: an Endpoint packs an IPv4 address and a port and gives them back") {
    const OS::DatagramAddress address{{192, 168, 0, 10}, 40123};
    const N::Endpoint endpoint = N::UdpLink::ToEndpoint(address);
    CHECK(endpoint.value == 0xC0A8000A9CBBull);
    CHECK(N::UdpLink::ToAddress(endpoint) == address);
}

TEST_CASE("link: a datagram goes over UDP loopback with its sender") {
    auto a = N::UdpLink::Open(OS::DatagramAddress::Loopback(0));
    auto b = N::UdpLink::Open(OS::DatagramAddress::Loopback(0));
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    const auto aAddress = a->LocalAddress();
    const auto bAddress = b->LocalAddress();
    REQUIRE(aAddress.has_value());
    REQUIRE(bAddress.has_value());

    std::vector<std::byte> buffer(64);
    auto nothing = b->Receive(buffer);
    REQUIRE(nothing.has_value());
    CHECK_FALSE(nothing->has_value());

    const auto payload = Bytes({1, 2, 3, 4, 5});
    REQUIRE(a->Send(N::UdpLink::ToEndpoint(*bAddress), payload).has_value());
    std::optional<N::ReceivedDatagram> got;
    for (int attempt = 0; attempt < 2000 && !got; ++attempt) {
        auto received = b->Receive(buffer);
        REQUIRE(received.has_value());
        got = *received;
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(got.has_value());
    CHECK(std::vector<std::byte>(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(got->size)) == payload);
    CHECK(got->from == N::UdpLink::ToEndpoint(*aAddress));
}

namespace {

// Runs both Products until the client's World has a Peer or a PeerDisconnected, at most about 4 s.
void Shake(O::Product& server, O::Product& client) {
    for (int i = 0; i < 2000; ++i) {
        REQUIRE(static_cast<bool>(server.RunFrame()));
        REQUIRE(static_cast<bool>(client.RunFrame()));
        auto& world = client.World("Net").GetWorld();
        if (!GYO::Test::Net::PeersOf(world).empty() || !GYO::Test::Net::SeenIn(world).disconnected.empty()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

std::unique_ptr<O::Product> UdpServer() {
    GYO::Test::Net::End server;
    server.net.role = N::NetRole::Server;
    server.net.bind = OS::DatagramAddress::Loopback(0);
    server.role = N::NetWorldOptions{N::NetRole::Server, std::nullopt};
    server.range = {1, 1000};
    auto built = O::Product::Build(GYO::Test::Net::Describe(server), GYO::Test::Net::Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(built), GYO::Test::Net::Reported(built));
    return std::move(*built);
}

std::unique_ptr<O::Product> UdpClient(const OS::DatagramAddress& server, const GYO::Engine::WorldTime::StepLength step) {
    GYO::Test::Net::End client;
    client.net.role = N::NetRole::Client;
    client.net.bind = OS::DatagramAddress::Loopback(0);
    client.target.server = server;
    client.target.userId = 7;
    client.role = N::NetWorldOptions{N::NetRole::Client, std::nullopt};
    client.range = {1001, 2000};
    client.time.step = step;
    auto built = O::Product::Build(GYO::Test::Net::Describe(client), GYO::Test::Net::Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(built), GYO::Test::Net::Reported(built));
    return std::move(*built);
}

} // namespace

TEST_CASE("link: two Products shake hands over UDP loopback, and a step length that differs is rejected (G-7)") {
    auto server = UdpServer();
    const auto address = server->Buffer().Find<N::NetService>()->UdpAddress();
    REQUIRE(address.has_value());
    auto client = UdpClient(OS::DatagramAddress::Loopback(address->port), {});
    Shake(*server, *client);
    const auto peers = GYO::Test::Net::PeersOf(server->World("Net").GetWorld());
    REQUIRE_MESSAGE(peers.size() == 1, "the client connects over UDP loopback");
    CHECK(peers[0].userId == 7);
    CHECK(GYO::Test::Net::PeersOf(client->World("Net").GetWorld()).size() == 1);

    auto other = UdpClient(OS::DatagramAddress::Loopback(address->port), GYO::Engine::WorldTime::StepLength::FromHz(60.0));
    Shake(*server, *other);
    const auto ended = GYO::Test::Net::SeenIn(other->World("Net").GetWorld()).disconnected;
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == N::DisconnectReason::StepLengthMismatch);
    CHECK(GYO::Test::Net::PeersOf(server->World("Net").GetWorld()).size() == 1);
}

TEST_CASE("link: a small G-5 over UDP loopback: the client ends equal to the server (G-7)") {
    using namespace GYO::Test::Net;
    const GameRules rules{90, true};
    End server = GameEnd(N::NetRole::Server, rules);
    server.net.bind = OS::DatagramAddress::Loopback(0);
    auto builtServer = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
    const auto address = (*builtServer)->Buffer().Find<N::NetService>()->UdpAddress();
    REQUIRE(address.has_value());
    End client = GameEnd(N::NetRole::Client, rules);
    client.net.bind = OS::DatagramAddress::Loopback(0);
    client.target.server = OS::DatagramAddress::Loopback(address->port);
    client.target.userId = 9;
    auto builtClient = O::Product::Build(Describe(client), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(builtClient), Reported(builtClient));
    auto& serverWorld = (*builtServer)->World("Net").GetWorld();
    auto& clientWorld = (*builtClient)->World("Net").GetWorld();
    bool equal = false;
    for (int frame = 0; frame < 3000 && !equal; ++frame) {
        REQUIRE(static_cast<bool>((*builtServer)->RunFrame()));
        REQUIRE(static_cast<bool>((*builtClient)->RunFrame()));
        equal = serverWorld.Resource<W::Tick>()->value > rules.lastTick + 5 && StateOf(clientWorld) == StateOf(serverWorld);
        if (!equal) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK_MESSAGE(equal, "over UDP loopback the client ends equal to the server");
    CHECK(StateOf(serverWorld).entities.size() > 5);
    CHECK(clientWorld.Resource<ClientLog>()->messagesBeforeEntity == 0);
}
