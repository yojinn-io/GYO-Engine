// The scenario "a server and two clients" (declaration section 8; a self-made 2D sample,
// v8-arch-gyo2-migration-needs-consent): one Match Product and two Client Products over memory links. Two players
// move, a door opens (the server message DoorOpened and the door's component), and both clients end equal to the
// Match. No guard bound to the implementation is stood up here (AGENTS section 3: after the scenario passes).
#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

// The server's end of several memory links: client i is reached at Endpoint{100 + i}.
class HubLink final : public N::Link {
public:
    explicit HubLink(std::vector<N::MemoryLink> ends) : ends_(std::move(ends)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        const std::size_t index = static_cast<std::size_t>(to.value - 100);
        if (to.value < 100 || index >= ends_.size()) {
            return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::LinkFailed, "no such client"));
        }
        return ends_[index].Send(ends_[index].Remote(), datagram);
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override {
        for (std::size_t i = 0; i < ends_.size(); ++i) {
            auto received = ends_[i].Receive(buffer);
            if (!received) return received;
            if (received->has_value()) return std::optional<N::ReceivedDatagram>{N::ReceivedDatagram{(*received)->size, N::Endpoint{100 + i}}};
        }
        return std::optional<N::ReceivedDatagram>{};
    }

private:
    std::vector<N::MemoryLink> ends_;
};

} // namespace

TEST_CASE("scenario: a server and two clients: players move, a door opens, both clients end equal to the server") {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    N::MemoryLinkSettings lossy;
    lossy.seed = 21;
    lossy.aToB = {1000, 500, 2000, 2};
    lossy.bToA = {1000, 500, 2000, 2};
    auto first = N::MemoryLink::Pair(lossy);
    lossy.seed = 22;
    auto second = N::MemoryLink::Pair(lossy);

    const GameRules rules{60, false};
    End server = GameEnd(N::NetRole::Server, rules);
    server.net.link = std::make_shared<HubLink>(std::vector<N::MemoryLink>{first.first, second.first});
    server.net.clock = reader;
    server.net.seed = 3;
    std::vector<End> clients;
    for (int i = 0; i < 2; ++i) {
        End client = GameEnd(N::NetRole::Client, rules);
        client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
        client.net.link = std::make_shared<N::MemoryLink>(i == 0 ? first.second : second.second);
        client.net.serverEndpoint = N::MemoryLink::EndpointA();
        client.net.userId = static_cast<std::uint64_t>(i + 1);
        client.net.clock = reader;
        clients.push_back(client);
    }
    auto builtServer = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
    std::vector<std::unique_ptr<O::Product>> clientProducts;
    for (const End& client : clients) {
        auto built = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        clientProducts.push_back(std::move(*built));
    }
    auto& match = **builtServer;
    for (int frame = 0; frame < 200; ++frame) {
        REQUIRE(static_cast<bool>(match.RunFrame()));
        for (auto& client : clientProducts) REQUIRE(static_cast<bool>(client->RunFrame()));
        first.first.Advance();
        second.first.Advance();
        *clock += Ms(16);
    }
    const GameState expected = StateOf(match.World("Net").GetWorld());
    CHECK(PeersOf(match.World("Net").GetWorld()).size() == 2);
    REQUIRE(expected.positions.size() == 2);
    REQUIRE(expected.doors.size() == 1);
    CHECK_MESSAGE(expected.doors.begin()->second.open == 0, "the door opened at tick 30 and closed at 60");
    for (std::size_t i = 0; i < clientProducts.size(); ++i) {
        CAPTURE(i);
        auto& world = clientProducts[i]->World("Net").GetWorld();
        CHECK_MESSAGE(Difference(StateOf(world), expected).empty(), "both clients end equal to the server");
        const auto& log = *world.Resource<ClientLog>();
        CHECK_MESSAGE(log.messages == 2, "each client gets the door's two messages");
        CHECK(log.messagesBeforeEntity == 0);
        CHECK(log.positionsChanged > 0);
    }
}
