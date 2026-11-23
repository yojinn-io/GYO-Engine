// Relevance (net-module declaration C-VIS1, C-VIS2, F4, F19; step 5): judgement G-11 as declared in section 8 (not
// loosened after the results). A server and two display clients (K1) over memory links. Three entities move every
// step; a server System fills each Peer's PeerVisibility by a rule the test gives:
//   - user 1 sees E1 and E3 always, E2 except while the server's tick is in [40, 80), and E3's private Secret;
//   - user 2 sees E2 and E3, never E1, and no private component.
// Each frame records, per client, the entities it has and whether E3 carries Secret. (The simulating client's NotSent
// is in RollbackClientTests.cpp, G-11's last item.)
#include <doctest/doctest.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

struct Secret {
    std::int32_t code = 0;
    friend bool operator==(const Secret&, const Secret&) = default;
};
inline void HashValue(K::Hasher& h, const Secret& v) { HashValue(h, v.code); }

// A component E2 carries that never changes: seen again, the entity is sent whole, this too (S5 of PR-2's review).
struct Mark {
    std::int32_t value = 0;
    friend bool operator==(const Mark&, const Mark&) = default;
};
inline void HashValue(K::Hasher& h, const Mark& v) { HashValue(h, v.value); }

constexpr K::EntityId kE1 = 50'001, kE2 = 50'002, kE3 = 50'003;

// The server's tick from which user 1 no longer sees E3's Secret (G-11: never; S5's case: 100).
std::uint64_t& PrivateUntil() {
    static std::uint64_t until = ~std::uint64_t{0};
    return until;
}

class Hub final : public N::Link {
public:
    explicit Hub(std::vector<N::MemoryLink> ends) : ends_(std::move(ends)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        auto& end = ends_[static_cast<std::size_t>(to.value - 100)];
        return end.Send(end.Remote(), datagram);
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

// The rule: what user sees at the server's tick.
bool Sees(const std::uint64_t user, const K::EntityId entity, const std::uint64_t tick) {
    if (user == 1) return entity != kE2 || tick < 40 || tick >= 80;
    return entity != kE1;
}

// At the server, Main/Update: each Peer's PeerVisibility from the rule.
K::SystemResult See(K::ExclusiveWorld<> world) {
    const std::uint64_t tick = world.Read<W::Tick>([](const W::Tick& t) { return t.value; })->value_or(0);
    for (const K::EntityId peer : world.Entities()) {
        auto user = world.Read<N::Peer>(peer, [](const N::Peer& p) { return p.userId; });
        if (!user || !user->has_value()) continue;
        if (**user == 3) continue;  // user 3's Peer has no PeerVisibility: it sees every entity and no private component
        N::PeerVisibility visibility;
        visibility.everything = false;
        for (const K::EntityId entity : {kE1, kE2, kE3}) {
            if (Sees(**user, entity, tick)) visibility.Show(entity);
        }
        if (**user == 1 && tick < PrivateUntil()) visibility.ShowPrivate<Secret>(kE3);
        if (auto inserted = world.Insert(peer, std::move(visibility)); !inserted) return GYO::Engine::Base::Err(std::move(inserted).error());
    }
    return {};
}

K::SystemResult Drift(K::Query<K::Write<Position>> positions) {
    positions.ForEach([](K::EntityId, K::Mut<Position> p) { p->x += 1; });
    return {};
}

void RegisterVisible(O::WorldSetup& s) {
    auto& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Secret>({.name = "Secret"});
    r.Component<Mark>({.name = "Mark"});
    N::NetEntry net(s);
    net.Replicate<Position>("Position", PositionCodec());
    net.Replicate<Mark>("Mark", {[](N::NetWriter& w, const Mark& v) { w.Value(v.value); }, [](N::NetReader& rd, Mark& v) { return rd.Value(v.value); }, 1});
    net.Replicate<Secret>("Secret", {[](N::NetWriter& w, const Secret& v) { w.Value(v.code); },
                                     [](N::NetReader& rd, Secret& v) { return rd.Value(v.code); }, 1},
                          N::ReplicateOptions{.privateComponent = true});
    K::SystemOptions drift = Fixed("Drift", true);
    r.AddSystem<&Drift>(std::move(drift));
    K::SystemOptions see = N::Detail::MountedAt("See", "Update");
    see.mounts.front().conditions.push_back(N::OnServer());
    r.AddSystem<&See>(std::move(see));
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
        return {};
    });
}

} // namespace

// A server and two display clients over memory links (G-11's setup).
struct Visible {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::unique_ptr<O::Product> server;
    std::vector<std::unique_ptr<O::Product>> clients;

    Visible() {
        auto reader = [now = clock] { return *now; };
        for (int i = 0; i < 3; ++i) {
            N::MemoryLinkSettings settings;
            settings.seed = static_cast<std::uint64_t>(70 + i);
            links.push_back(N::MemoryLink::Pair(settings));
        }
        End serverEnd = GameEnd(N::NetRole::Server, {});
        serverEnd.build = [](O::WorldSetup& s) {
            RegisterVisible(s);
            s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                world.Insert(kE1, Position{0, 1});
                world.Insert(kE2, Position{0, 2});
                world.Insert(kE2, Mark{77});
                world.Insert(kE2, Secret{7});  // never shown to anyone
                world.Insert(kE3, Position{0, 3});
                world.Insert(kE3, Secret{4242});
                return {};
            });
        };
        serverEnd.net.link = std::make_shared<Hub>(std::vector<N::MemoryLink>{links[0].first, links[1].first, links[2].first});
        serverEnd.net.clock = reader;
        serverEnd.net.seed = 6;
        auto builtServer = O::Product::Build(Describe(serverEnd), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
        server = std::move(*builtServer);
        for (int i = 0; i < 3; ++i) {
            End client = GameEnd(N::NetRole::Client, {});
            client.build = &RegisterVisible;
            client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
            client.net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
            client.net.serverEndpoint = N::MemoryLink::EndpointA();
            client.net.userId = static_cast<std::uint64_t>(i + 1);
            client.net.clock = reader;
            auto built = O::Product::Build(Describe(client), Units(true));
            REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
            clients.push_back(std::move(*built));
        }
    }
    void Frame() {
        REQUIRE(static_cast<bool>(server->RunFrame()));
        for (auto& c : clients) REQUIRE(static_cast<bool>(c->RunFrame()));
        for (auto& [a, b] : links) a.Advance();
        *clock += Ms(16);
    }
    K::World& Client(int i) { return clients[static_cast<std::size_t>(i)]->World("Net").GetWorld(); }
    K::World& Server() { return server->World("Net").GetWorld(); }
};

TEST_CASE("visibility: G-11 an entity a Peer does not see is not sent, comes back under the same id; a private component reaches only its Peer") {
    PrivateUntil() = ~std::uint64_t{0};
    Visible visible;
    auto& clients = visible.clients;
    auto& match = *visible.server;
    // Per client: frames it had each entity; frames E3 carried Secret; whether user 1 lost E2 while hidden and got it back.
    std::map<std::pair<int, K::EntityId>, int> had;
    auto Had = [&had](int client, K::EntityId entity) -> int& { return had[std::make_pair(client, entity)]; };
    int secretAt1 = 0, secretAt2 = 0, secretE2At1 = 0, secretAt3 = 0, seenAt3 = 0;
    bool lostE2 = false, backE2 = false;
    for (int frame = 0; frame < 160; ++frame) {
        visible.Frame();
        for (int i = 0; i < 2; ++i) {
            K::World& world = clients[static_cast<std::size_t>(i)]->World("Net").GetWorld();
            for (const K::EntityId entity : {kE1, kE2, kE3}) Had(i, entity) += world.Get<Position>(entity) != nullptr ? 1 : 0;
            (i == 0 ? secretAt1 : secretAt2) += world.Get<Secret>(kE3) != nullptr ? 1 : 0;
        }
        K::World& third = clients[2]->World("Net").GetWorld();
        secretE2At1 += clients[0]->World("Net").GetWorld().Get<Secret>(kE2) != nullptr ? 1 : 0;
        secretAt3 += (third.Get<Secret>(kE2) != nullptr || third.Get<Secret>(kE3) != nullptr) ? 1 : 0;
        seenAt3 += (third.Get<Position>(kE1) != nullptr && third.Get<Position>(kE2) != nullptr && third.Get<Position>(kE3) != nullptr) ? 1 : 0;
        K::World& first = clients[0]->World("Net").GetWorld();
        if (Had(0, kE2) > 0 && first.Get<Position>(kE2) == nullptr) lostE2 = true;
        if (lostE2 && first.Get<Position>(kE2) != nullptr) backE2 = true;
    }
    K::World& serverWorld = match.World("Net").GetWorld();
    K::World& world1 = clients[0]->World("Net").GetWorld();
    K::World& world2 = clients[1]->World("Net").GetWorld();
    std::uint64_t deferred = 0;
    for (const K::EntityId id : serverWorld.Entities()) {
        if (const auto* ledger = serverWorld.Get<N::PeerLedger>(id)) deferred += ledger->valuesDeferred;
    }
    MESSAGE("G-11: client 1 had E1/E2/E3 in " << Had(0, kE1) << "/" << Had(0, kE2) << "/" << Had(0, kE3) << " frames, Secret in "
                                               << secretAt1 << "; client 2 had E1/E2/E3 in " << Had(1, kE1) << "/" << Had(1, kE2) << "/"
                                               << Had(1, kE3) << ", Secret in " << secretAt2 << "; E2 lost and back at client 1: "
                                               << lostE2 << "/" << backE2 << "; values deferred " << deferred);
    CHECK_MESSAGE(Had(1, kE1) == 0, "G-11: an entity a Peer does not see is not sent to it");
    CHECK_MESSAGE(lostE2, "G-11: an entity a Peer stops seeing is removed at that client (only WhileVisible)");
    CHECK_MESSAGE(backE2, "G-11: seen again, the entity comes back");
    REQUIRE(world1.Get<Position>(kE2) != nullptr);
    CHECK_MESSAGE(*world1.Get<Position>(kE2) == *serverWorld.Get<Position>(kE2), "G-11: it comes back whole, under the same id");
    // Whole: a component that did not change while it was hidden comes again too (S5 of PR-2's review).
    CHECK_MESSAGE((world1.Get<Mark>(kE2) != nullptr && *world1.Get<Mark>(kE2) == Mark{77}),
                  "G-11: seen again, it is sent whole: a component unchanged while hidden comes too");
    CHECK_MESSAGE(secretAt1 > 0, "G-11: the private component reaches the Peer that is shown it");
    CHECK_MESSAGE(secretAt2 == 0, "G-11: the private component reaches no other Peer");
    CHECK_MESSAGE(secretE2At1 == 0, "G-11: a private component of another entity is not sent (shown per entity)");
    MESSAGE("G-11 client 3 (no PeerVisibility): saw E1, E2 and E3 in " << seenAt3 << " frames, a Secret in " << secretAt3);
    CHECK_MESSAGE(seenAt3 > 0, "G-11: a Peer without PeerVisibility sees every entity");
    CHECK_MESSAGE(secretAt3 == 0, "G-11: a Peer without PeerVisibility gets no private component");
    CHECK(world2.Get<Position>(kE3) != nullptr);
    REQUIRE(world1.Get<Secret>(kE3) != nullptr);
    CHECK((*world1.Get<Secret>(kE3) == Secret{4242}));
    // The counts at the end (the rule is still after tick 80): each client has exactly what its Peer sees.
    const std::uint64_t tick = serverWorld.Resource<W::Tick>()->value;
    for (int i = 0; i < 2; ++i) {
        K::World& world = clients[static_cast<std::size_t>(i)]->World("Net").GetWorld();
        for (const K::EntityId entity : {kE1, kE2, kE3}) {
            CAPTURE(i);
            CAPTURE(entity);
            CHECK_MESSAGE((world.Get<Position>(entity) != nullptr) == Sees(static_cast<std::uint64_t>(i + 1), entity, tick),
                          "G-11: each client ends with exactly the entities its Peer sees");
        }
    }
}

TEST_CASE("visibility: a private component a Peer stops being shown is taken off at its client; the entity stays") {
    // S5 of PR-2's review: user 1 is shown E3's Secret until the server's tick 100, then not (HidePrivate's effect).
    PrivateUntil() = 100;
    Visible visible;
    int secretFrames = 0;
    for (int frame = 0; frame < 160; ++frame) {
        visible.Frame();
        secretFrames += visible.Client(0).Get<Secret>(kE3) != nullptr ? 1 : 0;
    }
    PrivateUntil() = ~std::uint64_t{0};
    MESSAGE("HidePrivate: client 1 had Secret in " << secretFrames << " frames");
    CHECK_MESSAGE(secretFrames > 0, "the private component reached the Peer while it was shown");
    CHECK_MESSAGE(visible.Client(0).Get<Secret>(kE3) == nullptr, "a private component no longer shown is taken off at that client");
    CHECK_MESSAGE(visible.Client(0).Get<Position>(kE3) != nullptr, "the entity itself stays (only the component's scope changed)");
    CHECK(visible.Server().Get<Secret>(kE3) != nullptr);
}
