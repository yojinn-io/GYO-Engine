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
    lossy.aToB = {1000, 500, 2000, 2, 0};
    lossy.bToA = {1000, 500, 2000, 2, 0};
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
        client.target.server = N::MemoryLink::EndpointA();
        client.target.userId = static_cast<std::uint64_t>(i + 1);
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

// ---- The narrow recheck of the design review (R1, B2's rest): a client connecting in a frame without a fixed step.

namespace {

// At the server: each PeerConnected answered with a DoorOpened to everybody ("someone joined"), noted with the frame.
struct Joins {
    std::uint64_t frame = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> list; // (frame, user id)
};
inline void HashValue(K::Hasher&, const Joins&) {}

K::SystemResult AnnounceJoins(K::MessageReader<N::PeerConnected> joined, K::ResMut<Joins> log, K::MessageWriter<DoorOpened> opened) {
    ++log->frame;
    for (const N::PeerConnected* join : joined.Read()) {
        opened.Send(DoorOpened{0, 1});
        log->list.emplace_back(log->frame, join->userId);
    }
    return {};
}

struct JoinRun {
    bool joinedWithoutStep = false;
    std::vector<std::uint64_t> expected, sentToPeer, received;
};

// Two clients connect; the third is built `delay` frames later. The server runs half a step a frame.
JoinRun RunJoins(const int delay) {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    for (int i = 0; i < 3; ++i) {
        N::MemoryLinkSettings settings;
        settings.seed = static_cast<std::uint64_t>(40 + i);
        links.push_back(N::MemoryLink::Pair(settings));
    }
    const GameRules rules{1, false};
    End server = GameEnd(N::NetRole::Server, rules);
    server.net.link = std::make_shared<HubLink>(std::vector<N::MemoryLink>{links[0].first, links[1].first, links[2].first});
    server.net.clock = reader;
    server.net.seed = 4;
    server.build = [rules](O::WorldSetup& s) {
        RegisterGame(s, rules);
        s.Registrar().Resource<Joins>({.name = "Joins"});
        K::SystemOptions options = N::Detail::MountedAt("AnnounceJoins", "Update");
        options.mounts.front().conditions.push_back(N::OnServer());
        s.Registrar().AddSystem<&AnnounceJoins>(std::move(options));
        s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(Joins{});
            world.SetResource(W::TimeSampling{7'812'500, W::SamplingMode::ManualDelta});
            return {};
        });
    };
    auto builtServer = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
    auto& match = **builtServer;
    std::vector<std::unique_ptr<O::Product>> clients;
    auto addClient = [&](const int i) {
        End client = GameEnd(N::NetRole::Client, rules);
        client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
        client.net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
        client.target.server = N::MemoryLink::EndpointA();
        client.target.userId = static_cast<std::uint64_t>(i + 1);
        client.net.clock = reader;
        auto built = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        clients.push_back(std::move(*built));
    };
    addClient(0);
    addClient(1);
    JoinRun run;
    auto& serverWorld = match.World("Net").GetWorld();
    for (int frame = 0; frame < 160; ++frame) {
        if (frame == 30 + delay) addClient(2);
        const std::size_t peersBefore = PeersOf(serverWorld).size();
        const std::uint64_t tickBefore = serverWorld.Resource<W::Tick>()->value;
        REQUIRE(static_cast<bool>(match.RunFrame()));
        if (peersBefore == 2 && PeersOf(serverWorld).size() == 3) {
            run.joinedWithoutStep = serverWorld.Resource<W::Tick>()->value == tickBefore;
        }
        for (auto& client : clients) REQUIRE(static_cast<bool>(client->RunFrame()));
        for (auto& link : links) link.first.Advance();
        *clock += Ms(16);
    }
    // Each client gets the message of every join from its own on.
    const auto& joins = serverWorld.Resource<Joins>()->list;
    REQUIRE(joins.size() == 3);
    for (std::uint64_t user = 1; user <= 3; ++user) {
        std::uint64_t own = 0, expected = 0;
        for (const auto& [frame, id] : joins) {
            if (id == user) own = frame;
        }
        for (const auto& [frame, id] : joins) expected += frame >= own ? 1 : 0;
        run.expected.push_back(expected);
        std::uint64_t sent = 0;
        for (const K::EntityId id : serverWorld.Entities()) {
            const auto* peer = serverWorld.Get<N::Peer>(id);
            if (peer != nullptr && peer->userId == user) sent = serverWorld.Get<N::PeerLedger>(id)->serverMessagesSent;
        }
        run.sentToPeer.push_back(sent);
        run.received.push_back(clients[user - 1]->World("Net").GetWorld().Resource<ClientLog>()->messages);
    }
    return run;
}

} // namespace

TEST_CASE("scenario: a client connecting in a frame without a fixed step: every client gets the messages of that frame (R1)") {
    bool sawWithoutStep = false;
    for (const int delay : {0, 1}) {
        CAPTURE(delay);
        const JoinRun run = RunJoins(delay);
        sawWithoutStep = sawWithoutStep || run.joinedWithoutStep;
        for (std::size_t i = 0; i < 3; ++i) {
            CAPTURE(i);
            CHECK_MESSAGE(run.sentToPeer[i] == run.expected[i], "R1: each Peer is sent every server message written since it joined");
            CHECK(run.received[i] == run.expected[i]);
        }
    }
    CHECK_MESSAGE(sawWithoutStep, "one of the two runs has the third client join in a frame without a step");
}
