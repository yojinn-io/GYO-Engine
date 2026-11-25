// The bandwidth judgement of the net Module's PR-2 gate 2 (net-module-pr2-gates.zh-Hant.md 2.3; the user's choice
// (b) on 2026-11-23: each Client's downlink <= 512 kbps on average in the typical scenario). Declared before the
// results and not loosened after them. Bytes are counted, not timed: the case runs in the cpu label.
//
// The scenario (fixed seed): a 500 m x 500 m map on two floors, in centimetres. 8 players crowd on floor 0 and wander;
// 20 AI patrol (5 near the crowd, the others on both floors); a boss walks a square in a room on floor 0; 200 items
// (18 near the crowd), 50 doors and 129 other entities stand still (both floors). The rule each Peer's PeerVisibility is filled with: the same floor and within 100 m of
// its player. One Match and 8 display clients over memory links without loss; 2 000 ticks, one a frame. Every
// datagram the Match sends to a client counts (values, structure updates, acks, keepalives).
//   Judgement: each client's average bytes per tick, at 64 Hz, <= 512 kbps; two runs give the same bytes, tick by tick.
//   Recorded only: the maximum, and the dense scenario (the same entities, all moving: about 50 seen, all changing).
//   The scenario is checked against 2.1 (S4 of PR-2's review): seen about 50 (40..60), moving about 14 (10..18).
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/net/MemoryLink.hpp"

namespace N = GYO::Engine::Net;
using namespace GYO::Test::Net;

namespace {

struct Floor {
    std::uint8_t level = 0;
};
inline void HashValue(K::Hasher& h, const Floor& v) { HashValue(h, v.level); }

// How an entity moves (the server's only; not replicated). kind 0: still; 1: player; 2: patrol; 3: boss.
struct Mover {
    std::uint8_t kind = 0;
    std::int32_t vx = 0, vy = 0;
    std::int32_t originX = 0, originY = 0;
    std::uint64_t seed = 0;
};
inline void HashValue(K::Hasher& h, const Mover& v) { HashValue(h, v.kind); }

std::uint64_t Next(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
std::int32_t Between(std::uint64_t& state, std::int32_t low, std::int32_t high) {
    return low + static_cast<std::int32_t>(Next(state) % static_cast<std::uint64_t>(high - low + 1));
}

constexpr std::int32_t kMap = 50'000;       // 500 m
constexpr std::int32_t kSight = 10'000;     // 100 m
constexpr K::EntityId kFirst = 60'001;
constexpr int kPlayers = 8;

K::SystemResult MoveAround(K::Res<W::Tick> tick, K::Query<K::Write<Position>, K::Write<Mover>> movers) {
    const std::uint64_t t = tick->value;
    movers.ForEach([t](K::EntityId, K::Mut<Position> p, K::Mut<Mover> m) {
        if (m->kind == 0) return;
        if (m->kind == 1 && t % 64 == 0) {
            // A player turns every second, staying within 30 m of the crowd's centre.
            m->vx = Between(m->seed, -8, 8);
            m->vy = Between(m->seed, -8, 8);
            if (p->x > m->originX + 3'000) m->vx = -std::abs(m->vx);
            if (p->x < m->originX - 3'000) m->vx = std::abs(m->vx);
            if (p->y > m->originY + 3'000) m->vy = -std::abs(m->vy);
            if (p->y < m->originY - 3'000) m->vy = std::abs(m->vy);
        }
        if (m->kind == 2 && std::abs(p->x + m->vx - m->originX) > 1'000) m->vx = -m->vx;  // patrol: back and forth over 20 m
        if (m->kind == 3 && t % 128 == 0) {                                              // boss: a 5 m square
            const std::int32_t vx = m->vx;
            m->vx = -m->vy;
            m->vy = vx;
        }
        p->x += m->vx;
        p->y += m->vy;
    });
    return {};
}

// At the server, Main/Update: each Peer sees what is on its player's floor within 100 m.
K::SystemResult SeeAround(K::ExclusiveWorld<> world) {
    std::vector<std::pair<K::EntityId, std::pair<Position, std::uint8_t>>> placed;
    std::vector<K::EntityId> peers;
    for (const K::EntityId id : world.Entities()) {
        auto position = world.Read<Position>(id, [](const Position& p) { return p; });
        auto floor = world.Read<Floor>(id, [](const Floor& f) { return f.level; });
        if (position && position->has_value() && floor && floor->has_value()) placed.push_back({id, {**position, **floor}});
        auto controls = world.Read<N::PeerControls>(id, [](const N::PeerControls& c) { return c.entity; });
        if (controls && controls->has_value()) peers.push_back(id);
    }
    for (const K::EntityId peer : peers) {
        const K::EntityId player = **world.Read<N::PeerControls>(peer, [](const N::PeerControls& c) { return c.entity; });
        const auto self = std::find_if(placed.begin(), placed.end(), [player](const auto& e) { return e.first == player; });
        if (self == placed.end()) continue;
        N::PeerVisibility visibility;
        visibility.everything = false;
        for (const auto& [id, where] : placed) {
            if (where.second != self->second.second) continue;
            const std::int64_t dx = where.first.x - self->second.first.x;
            const std::int64_t dy = where.first.y - self->second.first.y;
            if (dx * dx + dy * dy <= std::int64_t{kSight} * kSight) visibility.entities.push_back(id);
        }
        if (auto inserted = world.Insert(peer, std::move(visibility)); !inserted) return GYO::Engine::Base::Err(std::move(inserted).error());
    }
    return {};
}

void RegisterWorld(O::WorldSetup& s) {
    auto& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Floor>({.name = "Floor"});
    r.Component<Mover>({.name = "Mover"});
    N::NetEntry net(s);
    net.Replicate<Position>("Position", PositionCodec());
    net.Replicate<Floor>("Floor", {[](N::NetWriter& w, const Floor& v) { w.Value(v.level); },
                                   [](N::NetReader& rd, Floor& v) { return rd.Value(v.level); }, 1});
    r.AddSystem<&MoveAround>(Fixed("MoveAround", true));
    K::SystemOptions see = N::Detail::MountedAt("SeeAround", "Update");
    see.mounts.front().conditions.push_back(N::OnServer());
    r.AddSystem<&SeeAround>(std::move(see));
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
        return {};
    });
}

// The Match's entities: the typical scenario of gate 2's 2.1 (a client sees about 50, about 14 of them change each
// tick: 8 players, about 5 AI, the boss), or the dense one (the same entities, every one of them moving: about 50 seen,
// all changing). The review of PR-2 (S4): the first scenario of step 5 scattered every AI and still entity evenly and
// was lighter than 2.1 (about 38 seen, about 10 changing); now 5 AI patrol near the crowd and 18 items lie near it.
void Populate(K::World& world, const bool dense) {
    std::uint64_t seed = 0x5EED0B0Bu;
    K::EntityId next = kFirst;
    const std::int32_t cx = 25'000, cy = 25'000;
    auto put = [&](std::int32_t x, std::int32_t y, std::uint8_t level, Mover mover) {
        if (dense && mover.kind == 0) mover = Mover{2, 5, 0, x, 0, 0};
        world.Insert(next, Position{x, y});
        world.Insert(next, Floor{level});
        world.Insert(next, mover);
        ++next;
    };
    for (int i = 0; i < kPlayers; ++i) {
        Mover m{1, 0, 0, cx, cy, Next(seed)};
        put(cx + Between(seed, -2'000, 2'000), cy + Between(seed, -2'000, 2'000), 0, m);
    }
    for (int i = 0; i < 20; ++i) {
        if (i < 5) {
            const std::int32_t x = cx + Between(seed, -6'000, 6'000);
            put(x, cy + Between(seed, -6'000, 6'000), 0, Mover{2, 5, 0, x, 0, 0});
            continue;
        }
        const std::int32_t x = Between(seed, 1'000, kMap - 1'000);
        put(x, Between(seed, 0, kMap), static_cast<std::uint8_t>(i % 2), Mover{2, 5, 0, x, 0, 0});
    }
    put(cx + 2'000, cy, 0, Mover{3, 3, 0, 0, 0, 0});
    for (int i = 0; i < 200 + 50; ++i) {
        if (i < 18) {
            put(cx + Between(seed, -6'000, 6'000), cy + Between(seed, -6'000, 6'000), 0, Mover{});
            continue;
        }
        put(Between(seed, 0, kMap), Between(seed, 0, kMap), static_cast<std::uint8_t>(Next(seed) % 2), Mover{});
    }
    for (int i = 0; i < 129; ++i) put(Between(seed, 0, kMap), Between(seed, 0, kMap), static_cast<std::uint8_t>(Next(seed) % 2), Mover{});
}

class CountingHub final : public N::Link {
public:
    CountingHub(std::vector<N::MemoryLink> ends, std::shared_ptr<std::vector<std::uint64_t>> bytes)
        : ends_(std::move(ends)), bytes_(std::move(bytes)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        const auto index = static_cast<std::size_t>(to.value - 100);
        (*bytes_)[index] += datagram.size();
        auto& end = ends_[index];
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
    std::shared_ptr<std::vector<std::uint64_t>> bytes_;
};

struct Measured {
    // Per client, the bytes of each measured tick.
    std::vector<std::vector<std::uint64_t>> perTick;
    std::vector<double> meanKbps;
    std::vector<std::uint64_t> maxBytes;
    std::vector<std::size_t> seenAtEnd;
    // The scenario itself, over the measured ticks (S4: checked against 2.1): per client per tick, the entities its
    // Peer sees and of those the ones that move.
    double meanSeen = 0;
    double meanMoving = 0;
};

Measured Measure(const bool dense, const int ticks) {
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    auto bytes = std::make_shared<std::vector<std::uint64_t>>(kPlayers, 0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::vector<N::MemoryLink> serverEnds;
    for (int i = 0; i < kPlayers; ++i) {
        N::MemoryLinkSettings settings;
        settings.seed = static_cast<std::uint64_t>(300 + i);
        links.push_back(N::MemoryLink::Pair(settings));
        serverEnds.push_back(links.back().first);
    }
    End server = GameEnd(N::NetRole::Server, {});
    server.build = [dense](O::WorldSetup& s) {
        RegisterWorld(s);
        s.Prepare([dense](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            Populate(world, dense);
            return {};
        });
    };
    server.net.link = std::make_shared<CountingHub>(serverEnds, bytes);
    server.net.clock = reader;
    server.net.seed = 8;
    auto builtServer = O::Product::Build(Describe(server), Units(true));
    REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
    std::vector<std::unique_ptr<O::Product>> clients;
    for (int i = 0; i < kPlayers; ++i) {
        End client = GameEnd(N::NetRole::Client, {});
        client.build = &RegisterWorld;
        client.range = E::EntityIdsOptions{static_cast<K::EntityId>(200'001 + i * 100'000), static_cast<K::EntityId>(300'000 + i * 100'000)};
        client.net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
        client.target.server = N::MemoryLink::EndpointA();
        client.target.userId = static_cast<std::uint64_t>(i + 1);
        client.net.clock = reader;
        client.net.seed = static_cast<std::uint64_t>(500 + i);
        auto built = O::Product::Build(Describe(client), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        clients.push_back(std::move(*built));
    }
    auto& match = **builtServer;
    K::World& world = match.World("Net").GetWorld();
    auto frame = [&] {
        REQUIRE(static_cast<bool>(match.RunFrame()));
        for (auto& c : clients) REQUIRE(static_cast<bool>(c->RunFrame()));
        for (auto& [a, b] : links) a.Advance();
        *clock += Ms(16);
    };
    // Connect, then each Peer controls the player of its user.
    for (int i = 0; i < 40 && PeersOf(world).size() < kPlayers; ++i) frame();
    REQUIRE(PeersOf(world).size() == kPlayers);
    for (const K::EntityId id : world.Entities()) {
        if (const N::Peer* peer = world.Get<N::Peer>(id)) world.Insert(id, N::PeerControls{kFirst + peer->userId - 1});
    }
    Measured measured;
    measured.perTick.assign(kPlayers, {});
    std::uint64_t seenSum = 0, movingSum = 0, samples = 0;
    for (int t = 0; t < ticks; ++t) {
        std::fill(bytes->begin(), bytes->end(), 0);
        frame();
        for (int i = 0; i < kPlayers; ++i) measured.perTick[static_cast<std::size_t>(i)].push_back((*bytes)[static_cast<std::size_t>(i)]);
        for (const K::EntityId id : world.Entities()) {
            const N::PeerVisibility* visibility = world.Get<N::PeerVisibility>(id);
            if (visibility == nullptr) continue;
            ++samples;
            seenSum += visibility->entities.size();
            for (const K::EntityId seen : visibility->entities) {
                const Mover* mover = world.Get<Mover>(seen);
                movingSum += mover != nullptr && mover->kind != 0 ? 1 : 0;
            }
        }
    }
    if (samples > 0) {
        measured.meanSeen = static_cast<double>(seenSum) / static_cast<double>(samples);
        measured.meanMoving = static_cast<double>(movingSum) / static_cast<double>(samples);
    }
    for (int i = 0; i < kPlayers; ++i) {
        const auto& series = measured.perTick[static_cast<std::size_t>(i)];
        std::uint64_t sum = 0, most = 0;
        for (const std::uint64_t b : series) {
            sum += b;
            most = std::max(most, b);
        }
        measured.meanKbps.push_back(static_cast<double>(sum) / static_cast<double>(series.size()) * 8.0 * 64.0 / 1000.0);
        measured.maxBytes.push_back(most);
        std::size_t seen = 0;
        for (const K::EntityId id : clients[static_cast<std::size_t>(i)]->World("Net").GetWorld().Entities()) {
            seen += clients[static_cast<std::size_t>(i)]->World("Net").GetWorld().Get<Position>(id) != nullptr ? 1 : 0;
        }
        measured.seenAtEnd.push_back(seen);
    }
    return measured;
}

std::string Text(const Measured& m) {
    std::string out = " seen on average " + std::to_string(m.meanSeen).substr(0, 5) + ", moving " + std::to_string(m.meanMoving).substr(0, 5);
    for (std::size_t i = 0; i < m.meanKbps.size(); ++i) {
        out += " | client " + std::to_string(i + 1) + ": mean " + std::to_string(m.meanKbps[i]).substr(0, 6) + " kbps, max " +
               std::to_string(m.maxBytes[i]) + " B/tick, sees " + std::to_string(m.seenAtEnd[i]);
    }
    return out;
}

} // namespace

TEST_CASE("bandwidth: gate 2 the typical scenario: each client's downlink averages at most 512 kbps, the same in two runs") {
    constexpr int kTicks = 2'000;
    const Measured first = Measure(false, kTicks);
    MESSAGE("bandwidth typical:" << Text(first));
    // The scenario is 2.1's (S4 of PR-2's review; the ranges are written before the run): seen about 50, about 14 moving.
    CHECK_MESSAGE((first.meanSeen >= 40.0 && first.meanSeen <= 60.0), "S4: a client sees about 50 entities (2.1): " << first.meanSeen);
    CHECK_MESSAGE((first.meanMoving >= 10.0 && first.meanMoving <= 18.0), "S4: about 14 of them move each tick (2.1): " << first.meanMoving);
    for (std::size_t i = 0; i < first.meanKbps.size(); ++i) {
        CAPTURE(i);
        CHECK_MESSAGE(first.meanKbps[i] <= 512.0, "gate 2: each client's average downlink is at most 512 kbps");
    }
    const Measured second = Measure(false, kTicks);
    CHECK_MESSAGE(first.perTick == second.perTick, "gate 2: two runs send the same bytes, tick by tick and client by client");
    // Recorded only: the dense scenario.
    const Measured dense = Measure(true, kTicks);
    MESSAGE("bandwidth dense (recorded only):" << Text(dense));
}
