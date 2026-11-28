// Placed entities (interact-door declaration 4): an entity each end puts in under the same id before the first step.
// A server and two display clients over memory links. The server has kPlaced (Placed, a Position that moves every
// step, a Door); client 1 puts kPlaced in too (Placed, a stale Position, its own Local component), client 2 does not.
// A server System fills user 1's PeerVisibility from the script: kPlaced is hidden while the server's tick is in
// [hideFrom, hideTo). The script can take the Door off, or despawn kPlaced, at a server tick.
#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

// A component only the client has (never replicated): it stays through a hide.
struct Local {
    std::int32_t value = 0;
    friend bool operator==(const Local&, const Local&) = default;
};
inline void HashValue(K::Hasher& h, const Local& v) { HashValue(h, v.value); }

constexpr K::EntityId kPlaced = 50'001;
constexpr std::uint64_t kNever = ~std::uint64_t{0};

struct Script {
    std::uint64_t hideFrom = kNever;
    std::uint64_t hideTo = kNever;
    std::uint64_t takeDoorAt = kNever;
    std::uint64_t despawnAt = kNever;
    std::optional<N::HiddenRule> rule;
};
Script& Current() {
    static Script script;
    return script;
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

std::uint64_t TickOf(K::ExclusiveWorld<>& world) { return world.Read<W::Tick>([](const W::Tick& t) { return t.value; })->value_or(0); }

// At the server, Main/Update: user 1's PeerVisibility from the script; the Door taken off and kPlaced despawned at
// their ticks.
K::SystemResult RunScript(K::ExclusiveWorld<> world) {
    const std::uint64_t tick = TickOf(world);
    const Script& script = Current();
    for (const K::EntityId peer : world.Entities()) {
        auto user = world.Read<N::Peer>(peer, [](const N::Peer& p) { return p.userId; });
        if (!user || !user->has_value() || **user != 1) continue;
        N::PeerVisibility visibility;
        visibility.everything = !(tick >= script.hideFrom && tick < script.hideTo);
        if (auto inserted = world.Insert(peer, std::move(visibility)); !inserted) return GYO::Engine::Base::Err(std::move(inserted).error());
    }
    if (tick >= script.takeDoorAt) (void)world.Remove<Door>(kPlaced);
    const auto present = world.Read<N::Placed>(kPlaced, [](const N::Placed&) {});
    if (tick >= script.despawnAt && present && *present) (void)world.Despawn(kPlaced);
    return {};
}

K::SystemResult Drift(K::Query<K::Write<Position>> positions) {
    positions.ForEach([](K::EntityId, K::Mut<Position> p) { p->x += 1; });
    return {};
}

void RegisterPlaced(O::WorldSetup& s) {
    auto& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Door>({.name = "Door"});
    r.Component<Local>({.name = "Local"});
    N::NetEntry net(s);
    net.Replicate<Position>("Position", PositionCodec());
    net.Replicate<Door>("Door", DoorCodec());
    K::SystemOptions drift = Fixed("Drift", true);
    drift.mounts.front().conditions.push_back(N::OnServer());
    r.AddSystem<&Drift>(std::move(drift));
    K::SystemOptions script = N::Detail::MountedAt("RunScript", "Update");
    script.mounts.front().conditions.push_back(N::OnServer());
    r.AddSystem<&RunScript>(std::move(script));
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
        return {};
    });
}

struct PlacedRun {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::unique_ptr<O::Product> server;
    std::vector<std::unique_ptr<O::Product>> clients;

    PlacedRun() {
        auto reader = [now = clock] { return *now; };
        for (int i = 0; i < 2; ++i) {
            N::MemoryLinkSettings settings;
            settings.seed = static_cast<std::uint64_t>(90 + i);
            links.push_back(N::MemoryLink::Pair(settings));
        }
        End serverEnd = GameEnd(N::NetRole::Server, {});
        serverEnd.build = [](O::WorldSetup& s) {
            RegisterPlaced(s);
            s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                world.Insert(kPlaced, N::Placed{});
                world.Insert(kPlaced, Position{0, 7});
                world.Insert(kPlaced, Door{1});
                if (Current().rule) world.Insert(kPlaced, N::WhenHidden{*Current().rule});
                return {};
            });
        };
        serverEnd.net.link = std::make_shared<Hub>(std::vector<N::MemoryLink>{links[0].first, links[1].first});
        serverEnd.net.clock = reader;
        serverEnd.net.seed = 6;
        auto builtServer = O::Product::Build(Describe(serverEnd), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
        server = std::move(*builtServer);
        for (int i = 0; i < 2; ++i) {
            End client = GameEnd(N::NetRole::Client, {});
            if (i == 0) {
                client.build = [](O::WorldSetup& s) {
                    RegisterPlaced(s);
                    // The client's own copy of the scene's entity: a stale value, and a component of its own.
                    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                        world.Insert(kPlaced, N::Placed{});
                        world.Insert(kPlaced, Position{-1, -1});
                        world.Insert(kPlaced, Local{5});
                        return {};
                    });
                };
            } else {
                client.build = &RegisterPlaced;
            }
            client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
            client.net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
            client.target.server = N::MemoryLink::EndpointA();
            client.target.userId = static_cast<std::uint64_t>(i + 1);
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
    std::uint64_t ServerTick() { return Server().Resource<W::Tick>()->value; }
    const N::NetReplicaState& State(int i) { return *Client(i).Resource<N::NetReplicaState>(); }
    bool Receiving(int i) { return Client(i).Get<N::NotReceiving>(kPlaced) == nullptr; }
};

// One frame's view of client 1's kPlaced.
struct PlacedView {
    std::uint64_t serverTick = 0;
    bool alive = false;
    bool receiving = false;
    std::optional<Position> position;
    bool door = false;
    bool local = false;
};

std::vector<PlacedView> Run(PlacedRun& placed, const int frames) {
    std::vector<PlacedView> seen;
    for (int frame = 0; frame < frames; ++frame) {
        placed.Frame();
        K::World& world = placed.Client(0);
        PlacedView now;
        now.serverTick = placed.ServerTick();
        now.alive = world.Get<N::Placed>(kPlaced) != nullptr;
        now.receiving = now.alive && placed.Receiving(0);
        if (const Position* p = world.Get<Position>(kPlaced)) now.position = *p;
        now.door = world.Get<Door>(kPlaced) != nullptr;
        now.local = world.Get<Local>(kPlaced) != nullptr;
        seen.push_back(now);
    }
    return seen;
}

} // namespace

TEST_CASE("placed: an entity both ends put in is attached, not spawned; it carries NotReceiving until then") {
    Current() = Script{};
    PlacedRun placed;
    // Before any frame: the client's own entity, not receiving yet.
    placed.Frame();
    CHECK_MESSAGE(!placed.Receiving(0), "[net-placed-before-attach] a Placed entity carries NotReceiving before its first attach");
    const std::vector<PlacedView> seen = Run(placed, 60);
    const N::NetReplicaState& state = placed.State(0);
    CHECK_MESSAGE(state.attaches >= 1, "[net-placed-attach] the server's first sending is an attach");
    CHECK_MESSAGE(state.drops[static_cast<std::size_t>(N::ReplicaDrop::SpawnRejected)] == 0, "[net-placed-attach] not a spawn: none rejected");
    CHECK_MESSAGE(state.drops[static_cast<std::size_t>(N::ReplicaDrop::MissingTarget)] == 0, "the attached entity was there");
    CHECK_MESSAGE(placed.Receiving(0), "[net-placed-attach] attached, the entity receives: NotReceiving is taken off");
    REQUIRE(placed.Client(0).Get<Position>(kPlaced) != nullptr);
    CHECK_MESSAGE(placed.Client(0).Get<Position>(kPlaced)->y == 7, "[net-placed-attach] the server's value is written on the client's entity");
    CHECK(placed.Client(0).Get<Position>(kPlaced)->x > 0);
    CHECK_MESSAGE(placed.Client(0).Get<Door>(kPlaced) != nullptr, "the whole set is attached");
    CHECK_MESSAGE((placed.Client(0).Get<Local>(kPlaced) != nullptr && *placed.Client(0).Get<Local>(kPlaced) == Local{5}),
                  "the client's own component stays");
    CHECK(seen.back().alive);
}

TEST_CASE("placed: a client without the entity counts MissingTarget and makes none") {
    Current() = Script{};
    PlacedRun placed;
    (void)Run(placed, 60);
    CHECK_MESSAGE(placed.State(1).drops[static_cast<std::size_t>(N::ReplicaDrop::MissingTarget)] >= 1,
                  "[net-placed-missing] an attach for an entity the client did not put in is MissingTarget");
    CHECK_MESSAGE(placed.Client(1).Get<Position>(kPlaced) == nullptr, "[net-placed-missing] no entity is made for it");
    CHECK_MESSAGE(placed.State(1).attaches == 0, "[net-placed-missing] an attach without the entity does not count as attached");
}

TEST_CASE("placed: Detach takes the replicated components off a hidden entity, keeps it and its own; seen again it is attached") {
    Current() = Script{.hideFrom = 40, .hideTo = 80};
    PlacedRun placed;
    const std::vector<PlacedView> seen = Run(placed, 140);
    bool hiddenSeen = false, hiddenWrong = false;
    for (const PlacedView& s : seen) {
        if (!s.alive || s.receiving || s.serverTick < 45 || s.serverTick >= 80) continue;
        hiddenSeen = true;
        if (s.position || s.door || !s.local) hiddenWrong = true;
    }
    CHECK_MESSAGE(hiddenSeen, "[net-placed-detach] the entity stays while hidden and carries NotReceiving");
    CHECK_MESSAGE(!hiddenWrong, "[net-placed-detach] hidden: no replicated component, its own component kept");
    CHECK(placed.State(0).hides >= 1);
    CHECK_MESSAGE(placed.Receiving(0), "[net-placed-reattach] seen again, it receives");
    REQUIRE(placed.Client(0).Get<Position>(kPlaced) != nullptr);
    const std::int32_t behind = placed.Server().Get<Position>(kPlaced)->x - placed.Client(0).Get<Position>(kPlaced)->x;
    CHECK_MESSAGE((behind >= 0 && behind <= 8), "[net-placed-reattach] seen again, the server's value of now (a few ticks behind): " << behind);
    CHECK(placed.Client(0).Get<Door>(kPlaced) != nullptr);
    CHECK(placed.State(0).attaches >= 2);
}

TEST_CASE("placed: Retain keeps a hidden entity's last values, which no change reaches; seen again they are the server's") {
    Current() = Script{.hideFrom = 40, .hideTo = 80, .rule = N::HiddenRule::Retain};
    PlacedRun placed;
    const std::vector<PlacedView> seen = Run(placed, 140);
    std::optional<Position> frozen;
    bool hiddenSeen = false, moved = false, lost = false;
    for (const PlacedView& s : seen) {
        if (!s.alive || s.receiving || s.serverTick < 45 || s.serverTick >= 80) continue;
        hiddenSeen = true;
        if (!s.position || !s.door || !s.local) {
            lost = true;
            continue;
        }
        if (frozen && !(*frozen == *s.position)) moved = true;
        frozen = s.position;
    }
    CHECK_MESSAGE(hiddenSeen, "[net-placed-retain] the entity stays while hidden and carries NotReceiving");
    CHECK_MESSAGE(!lost, "[net-placed-retain] hidden: the last values stay");
    CHECK_MESSAGE(!moved, "[net-placed-retain] hidden: no change of the server reaches it");
    CHECK_MESSAGE(placed.Receiving(0), "[net-placed-reattach] seen again, it receives");
    REQUIRE(frozen.has_value());
    REQUIRE(placed.Client(0).Get<Position>(kPlaced) != nullptr);
    CHECK_MESSAGE(placed.Client(0).Get<Position>(kPlaced)->x > frozen->x, "[net-placed-reattach] seen again, the server's newer value");
}

TEST_CASE("placed: a replicated component the server took off while the entity was hidden is taken off when it is attached again") {
    Current() = Script{.hideFrom = 40, .hideTo = 80, .takeDoorAt = 60, .rule = N::HiddenRule::Retain};
    PlacedRun placed;
    const std::vector<PlacedView> seen = Run(placed, 140);
    bool doorWhileHidden = false;
    for (const PlacedView& s : seen) {
        if (s.alive && !s.receiving && s.serverTick >= 65 && s.serverTick < 80 && s.door) doorWhileHidden = true;
    }
    CHECK_MESSAGE(doorWhileHidden, "Retain: the Door the server took off is still at the client while hidden (nothing reaches it)");
    CHECK(placed.Server().Get<Door>(kPlaced) == nullptr);
    CHECK_MESSAGE(placed.Receiving(0), "seen again, it receives");
    CHECK_MESSAGE(placed.Client(0).Get<Door>(kPlaced) == nullptr,
                  "[net-placed-attach-whole] attached again, a component the server no longer has is taken off (P3 (a))");
    CHECK(placed.Client(0).Get<Local>(kPlaced) != nullptr);
}

TEST_CASE("placed: a despawn at the server removes the entity at the client, seen or hidden, under either rule") {
    struct Case {
        const char* what;
        Script script;
    };
    for (const Case& c : {Case{"seen", Script{.despawnAt = 30}},
                          Case{"hidden, Detach", Script{.hideFrom = 40, .hideTo = 200, .despawnAt = 60}},
                          Case{"hidden, Retain", Script{.hideFrom = 40, .hideTo = 200, .despawnAt = 60, .rule = N::HiddenRule::Retain}}}) {
        CAPTURE(c.what);
        Current() = c.script;
        PlacedRun placed;
        const std::vector<PlacedView> seen = Run(placed, 120);
        CHECK(placed.Server().Get<N::Placed>(kPlaced) == nullptr);
        CHECK_MESSAGE(placed.Client(0).Get<N::Placed>(kPlaced) == nullptr, "[net-placed-despawn] the server's despawn reaches the client");
        CHECK_MESSAGE(placed.Client(0).Get<Local>(kPlaced) == nullptr, "[net-placed-despawn] the whole entity is removed");
    }
}
