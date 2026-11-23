// The client that simulates and rolls back (K3; net-module declaration D-5, D-7, step 4): judgement G-10 as declared
// in section 8 (not loosened after the results), the server running three steps a frame (U4: only the last one is
// sent, the confirmed point jumps), and the authority removing a whole entity (Exited).
//
// The sample: a deterministic 2D game. Bodies move by their Speed every step and bounce at +-200; the player's Speed
// follows its input (Push). The server alone spawns a crate every 10 steps from step 30 to 90 and removes the first
// crate at step 100 (OnServer: registered at both ends, so the consistency fingerprints agree). Body and Speed are
// decision components that roll back; Push is the decision input. The player entity is put at both ends by the
// prepare function with a fixed id outside both allotter ranges.
#include <doctest/doctest.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "GameTestSupport.hpp"
#include "NetTestSupport.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"

namespace N = GYO::Engine::Net;
namespace R = GYO::Engine::Rollback;
using namespace GYO::Test::Net;

namespace {

struct Body {
    std::int32_t x = 0;
    std::int32_t y = 0;
    friend bool operator==(const Body&, const Body&) = default;
};
inline void HashValue(K::Hasher& h, const Body& v) {
    HashValue(h, v.x);
    HashValue(h, v.y);
}
struct Speed {
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    friend bool operator==(const Speed&, const Speed&) = default;
};
inline void HashValue(K::Hasher& h, const Speed& v) {
    HashValue(h, v.dx);
    HashValue(h, v.dy);
}
struct Push {
    std::int32_t dx = 0;
    friend bool operator==(const Push&, const Push&) = default;
};
inline void HashValue(K::Hasher& h, const Push& v) { HashValue(h, v.dx); }

// A crate's colour: replicated, not decision data, does not roll back (K3 writes it directly).
struct Tint {
    std::int32_t rgb = 0;
    friend bool operator==(const Tint&, const Tint&) = default;
};
inline void HashValue(K::Hasher& h, const Tint& v) { HashValue(h, v.rgb); }
// The crates spawned so far: a decision Resource only the server's step writes, replicated (K3: ResourceValue).
struct Wave {
    std::int32_t count = 0;
    friend bool operator==(const Wave&, const Wave&) = default;
};
inline void HashValue(K::Hasher& h, const Wave& v) { HashValue(h, v.count); }

N::Codec<Body> BodyCodec() {
    return {[](N::NetWriter& w, const Body& v) {
                w.Value(v.x);
                w.Value(v.y);
            },
            [](N::NetReader& r, Body& v) { return r.Value(v.x) && r.Value(v.y); }, 1};
}
N::Codec<Speed> SpeedCodec() {
    return {[](N::NetWriter& w, const Speed& v) {
                w.Value(v.dx);
                w.Value(v.dy);
            },
            [](N::NetReader& r, Speed& v) { return r.Value(v.dx) && r.Value(v.dy); }, 1};
}
N::InputOptions<Push> PushOptions() {
    N::InputOptions<Push> options;
    options.codec = {[](N::NetWriter& w, const Push& v) { w.Value(v.dx); }, [](N::NetReader& r, Push& v) { return r.Value(v.dx); }, 1};
    return options;
}

constexpr K::EntityId kPlayer = 900'001;

K::SystemResult Steer(K::Query<K::Read<Push>, K::Write<Speed>> players) {
    players.ForEach([](K::EntityId, const Push& push, K::Mut<Speed> speed) { speed->dx = push.dx; });
    return {};
}
K::SystemResult MoveBodies(K::Query<K::Write<Body>, K::Write<Speed>> bodies) {
    bodies.ForEach([](K::EntityId, K::Mut<Body> body, K::Mut<Speed> speed) {
        body->x += speed->dx;
        body->y += speed->dy;
        if (body->x > 200 || body->x < -200) speed->dx = -speed->dx;
        if (body->y > 200 || body->y < -200) speed->dy = -speed->dy;
    });
    return {};
}
K::SystemResult SpawnCrates(K::Res<W::Tick> tick, K::Spawn<K::Bundle<Body, Speed, Tint>> spawn, K::ResMut<Wave> wave) {
    const std::uint64_t t = tick->value;
    if (t < 30 || t > 90 || t % 10 != 0) return {};
    auto id = spawn.New(Body{static_cast<std::int32_t>(t), 0}, Speed{1, static_cast<std::int32_t>(t % 3) - 1},
                        Tint{static_cast<std::int32_t>(t * 1000)});
    if (!id) return GYO::Engine::Base::Err(std::move(id).error());
    ++wave->count;
    return {};
}
K::SystemResult RemoveFirstCrate(K::Res<W::Tick> tick, K::Query<K::Read<Body>> bodies, K::Despawn despawn) {
    if (tick->value != 100) return {};
    for (const K::EntityId id : bodies.Entities()) {
        if (id == kPlayer) continue;
        despawn.Entity(id);
        break;
    }
    return {};
}

// The review of PR-2 (B1, S1, S5): the server hides a crate from the Peer for a while and takes components off
// crates, by tick. Filled by the server's System; read by the test at the end.
struct Reshape {
    bool on = false;
    bool blink = false;               // the lossy case: the blinking crate is hidden 2 ticks of every 6 in [150, 250)
    bool gate = false;                // the gated case (the recheck's N1): the crate is hidden in [150, 152)
    bool gateChange = false;          // the gated case: its Tint changes at 153 (the adversarial check's A6)
    K::EntityId withoutTintSoon = 0;  // spawned at 60, its Tint taken off at 61 (before the client has it in the World)
    K::EntityId withoutSpeed = 0;     // spawned at 50, its Speed taken off at 110 while seen (Removed)
    K::EntityId hidden = 0;           // spawned at 40, hidden in [120, 140); its Speed and Tint taken off at 130
                                      // (blinking: its Speed taken off at 240, while hidden)
    K::EntityId hiddenToo = 0;        // blinking: spawned at 70, blinks with it; its Tint taken off at 240
    K::EntityId tintAgain = 0;        // seen: spawned at 90, its Tint taken off at 160 and put on again at 200
};
Reshape& Reshaped() {
    static Reshape reshape;
    return reshape;
}

K::EntityId CrateTinted(K::ExclusiveWorld<>& world, const std::int32_t rgb) {
    for (const K::EntityId id : world.Entities()) {
        auto tint = world.Read<Tint>(id, [](const Tint& t) { return t.rgb; });
        if (tint && tint->has_value() && **tint == rgb) return id;
    }
    return 0;
}

bool HiddenAt(const Reshape& reshape, const std::uint64_t t) {
    if (reshape.gate) return t >= 150 && t < 152;
    if (reshape.blink) return t >= 150 && t < 250 && t % 6 < 2;
    return t >= 120 && t < 140;
}

// At the server, after the crates are spawned and removed: the takings-off and each Peer's PeerVisibility.
K::SystemResult ReshapeAtServer(K::ExclusiveWorld<> world) {
    Reshape& reshape = Reshaped();
    const std::uint64_t t = world.Read<W::Tick>([](const W::Tick& tick) { return tick.value; })->value_or(0);
    if (t == 61) {
        reshape.withoutTintSoon = CrateTinted(world, 60'000);
        if (auto removed = world.Remove<Tint>(reshape.withoutTintSoon); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    }
    if (t == 110) {
        reshape.withoutSpeed = CrateTinted(world, 50'000);
        if (auto removed = world.Remove<Speed>(reshape.withoutSpeed); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    }
    if (t == 119 || ((reshape.blink || reshape.gate) && t == 149)) reshape.hidden = CrateTinted(world, 40'000);
    if (reshape.blink && t == 149) reshape.hiddenToo = CrateTinted(world, 70'000);
    if (t == 130 && !reshape.blink && !reshape.gate) {
        if (auto removed = world.Remove<Speed>(reshape.hidden); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
        if (auto removed = world.Remove<Tint>(reshape.hidden); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    }
    const bool seen = !reshape.blink && !reshape.gate;
    if (reshape.gateChange && t == 153) {
        // While the crate is gone at the client (the rollback to 151 removed it; it comes back at a point >= 152), its
        // Tint changes: the value must wait for it (the adversarial check's A6).
        if (auto set = world.Insert(reshape.hidden, Tint{40'001}); !set) return GYO::Engine::Base::Err(std::move(set).error());
    }
    if (seen && t == 160) {
        reshape.tintAgain = CrateTinted(world, 90'000);
        if (auto removed = world.Remove<Tint>(reshape.tintAgain); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    }
    if (seen && t == 200) {
        if (auto set = world.Insert(reshape.tintAgain, Tint{9}); !set) return GYO::Engine::Base::Err(std::move(set).error());
    }
    if (t == 240 && reshape.blink) {
        if (auto removed = world.Remove<Speed>(reshape.hidden); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
        if (auto removed = world.Remove<Tint>(reshape.hiddenToo); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    }
    const bool hide = reshape.hidden != 0 && HiddenAt(reshape, t);
    for (const K::EntityId peer : world.Entities()) {
        auto isPeer = world.Read<N::Peer>(peer, [](const N::Peer&) { return true; });
        if (!isPeer || !isPeer->has_value()) continue;
        N::PeerVisibility visibility;
        visibility.everything = !hide;
        if (hide) {
            for (const K::EntityId id : world.Entities()) {
                auto body = world.Read<Body>(id, [](const Body&) { return true; });
                if (body && body->has_value() && id != reshape.hidden && id != reshape.hiddenToo) visibility.Show(id);
            }
        }
        if (auto inserted = world.Insert(peer, std::move(visibility)); !inserted) return GYO::Engine::Base::Err(std::move(inserted).error());
    }
    return {};
}

K::SystemOptions FixedAt(std::string name, std::string stage, std::vector<std::string> after, bool onServer) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = "FixedUpdate";
    mount.stage = std::move(stage);
    mount.after = std::move(after);
    if (onServer) mount.conditions.push_back(N::OnServer());
    options.mounts.push_back(std::move(mount));
    return options;
}

void RegisterSample(O::WorldSetup& s, const std::int64_t framesPerStep, const std::uint32_t valueBudget = 0, const bool reshape = false) {
    auto& r = s.Registrar();
    A::AuthorityEntry authority(s);
    authority.Component<Body>("Body");
    authority.Component<Speed>("Speed");
    authority.Component<Push>("Push", {.input = true});
    authority.Resource<Wave>("Wave");
    r.Component<Tint>({.name = "Tint"});
    N::NetEntry net(s);
    net.Replicate<Body>("Body", BodyCodec());
    net.Replicate<Speed>("Speed", SpeedCodec());
    net.Replicate<Tint>("Tint", {[](N::NetWriter& w, const Tint& v) { w.Value(v.rgb); },
                                 [](N::NetReader& r, Tint& v) { return r.Value(v.rgb); }, 1});
    N::NetEntry::ResourceOptions<Wave> waveOptions;
    waveOptions.codec = {[](N::NetWriter& w, const Wave& v) { w.Value(v.count); }, [](N::NetReader& r, Wave& v) { return r.Value(v.count); }, 1};
    net.ReplicateResource<Wave>("Wave", std::move(waveOptions));
    net.ValueBytesPerTick(valueBudget);
    net.Input<Push>("Push", PushOptions());
    R::RollbackEntry(s).StepSchedule();
    r.AddSystem<&Steer>(FixedAt("Steer", "Update", {}, false));
    r.AddSystem<&MoveBodies>(FixedAt("MoveBodies", "Update", {"Steer"}, false));
    r.AddSystem<&SpawnCrates>(FixedAt("SpawnCrates", "PostUpdate", {}, true));
    r.AddSystem<&RemoveFirstCrate>(FixedAt("RemoveFirstCrate", "PostUpdate", {"SpawnCrates"}, true));
    if (reshape) r.AddSystem<&ReshapeAtServer>(FixedAt("ReshapeAtServer", "PostUpdate", {"RemoveFirstCrate"}, true));
    s.Prepare([framesPerStep](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{15'625'000 * framesPerStep, W::SamplingMode::ManualDelta});
        world.Insert(kPlayer, Body{0, 0});
        world.Insert(kPlayer, Speed{0, 0});
        world.Insert(kPlayer, Push{0});
        world.SetResource(Wave{});
        return {};
    });
}

struct Sample {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    // How far the server's clock is ahead of the client's (the gated case lets only the server's time pass).
    std::shared_ptr<N::NetTime> serverAhead = std::make_shared<N::NetTime>(0);
    std::pair<N::MemoryLink, N::MemoryLink> links;
    std::unique_ptr<O::Product> server, client;

    Sample(const N::MemoryLinkSettings& link, const std::int64_t serverStepsPerFrame, const std::uint32_t valueBudget = 0,
           const bool reshape = false, const std::function<std::shared_ptr<N::Link>(N::MemoryLink)>& serverLink = {})
        : links(N::MemoryLink::Pair(link)) {
        auto reader = [now = clock] { return *now; };
        auto serverReader = [now = clock, ahead = serverAhead] { return *now + *ahead; };
        End s = GameEnd(N::NetRole::Server, {});
        s.authority = std::make_shared<A::ServerAuthorityStrategy>();
        s.build = [serverStepsPerFrame, valueBudget, reshape](O::WorldSetup& setup) {
            RegisterSample(setup, serverStepsPerFrame, valueBudget, reshape);
        };
        s.net.link = serverLink ? serverLink(links.first) : std::make_shared<N::MemoryLink>(links.first);
        s.net.clock = serverReader;
        s.net.seed = 31;
        End c = GameEnd(N::NetRole::Client, {});
        c.authority = std::make_shared<A::ServerAuthorityStrategy>();
        c.build = [reshape](O::WorldSetup& setup) { RegisterSample(setup, 1, 0, reshape); };
        c.net.link = std::make_shared<N::MemoryLink>(links.second);
        c.net.serverEndpoint = N::MemoryLink::EndpointA();
        c.net.userId = 9;
        c.net.clock = reader;
        auto builtServer = O::Product::Build(Describe(s), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtServer), Reported(builtServer));
        server = std::move(*builtServer);
        auto description = Describe(c);
        description.worlds[0].options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>()});
        auto builtClient = O::Product::Build(std::move(description), Units(true));
        REQUIRE_MESSAGE(static_cast<bool>(builtClient), Reported(builtClient));
        client = std::move(*builtClient);
    }
    K::World& ServerWorld() { return server->World("Net").GetWorld(); }
    K::World& ClientWorld() { return client->World("Net").GetWorld(); }
    std::uint64_t ServerTick() { return ServerWorld().Resource<W::Tick>()->value; }
    std::uint64_t ClientTick() { return ClientWorld().Resource<W::Tick>()->value; }
    const N::NetReplicaState& Replica() { return *ClientWorld().Resource<N::NetReplicaState>(); }
    K::EntityId Peer(K::World& world) {
        for (const K::EntityId id : world.Entities()) {
            if (world.Get<N::Peer>(id) != nullptr) return id;
        }
        return 0;
    }
};

std::uint64_t DigestOf(K::World& world) {
    auto digest = GYO::Engine::Authority::Digest(world);
    REQUIRE(static_cast<bool>(digest));
    return *digest;
}

// The crates (every Body but the player's) of a World.
std::set<K::EntityId> Crates(K::World& world) {
    std::set<K::EntityId> crates;
    for (const K::EntityId id : world.Entities()) {
        if (id != kPlayer && world.Get<Body>(id) != nullptr) crates.insert(id);
    }
    return crates;
}

// The client's input: a push for a while, then the default value for the last segment (G-10's premise).
std::int32_t PushAt(const std::uint64_t tick, const std::uint64_t quietFrom) {
    if (tick >= quietFrom) return 0;
    return static_cast<std::int32_t>((tick * 5) % 7) - 3;
}

struct Run {
    std::uint64_t clientTick = 0;
    std::uint64_t clientDigest = 0;
    std::optional<std::uint64_t> serverDigest;
    std::size_t serverCrates = 0, clientCrates = 0;
    std::uint64_t crateLost = 0;            // a crate seen at the client that was gone at a later frame, not exited
    std::set<K::EntityId> exitedAtServer;   // crates the server removed
    std::set<K::EntityId> stillAtClient;    // of those, still at the client at the end
    std::uint64_t confirmedJumps = 0;       // frames where the inbox's confirmed point moved by more than 1
    N::NetReplicaState replica;
    std::uint64_t serverEntered = 0;        // entities the server spawned and sent (the crates and the player)
    std::uint64_t tintsWrong = 0;           // client crates whose Tint is not the server's
    std::int32_t serverWave = 0, clientWave = 0;
    std::uint64_t serverDeferred = 0;       // values the server deferred to a later tick for the client's Peer
    std::uint32_t longestWait = 0;          // the most ticks in a row an entity's values waited at the server
    std::optional<std::uint64_t> rollbackConfirmed;  // the confirmed point the rollback Module's inbox has
    // The review of PR-2 (Reshape): what the client has at the end of the crates the server reshaped.
    bool speedOffWhileSeen = false;  // the crate whose Speed was taken off while seen has none at the client
    bool hiddenBack = false;         // the hidden crate is back at the client
    bool hiddenBodySame = false;     // with the server's Body
    bool hiddenSpeed = false;        // it has a Speed at the client (the server took it off while hidden)
    bool hiddenTint = false;         // it has a Tint at the client (the server took it off while hidden)
    bool tintSoon = false;           // the crate whose Tint was taken off at 61 has a Tint at the client
    bool hiddenTintSame = false;     // the hidden crate has the server's Tint at the client (the blinking case keeps it)
    bool hiddenTooTint = false;      // blinking: the second blinking crate has a Tint at the client (the server took it off)
    std::uint64_t rollbacksForEntries = 0;  // U3: rollbacks an entity said to enter took part in
    std::uint64_t notSentLeftOut = 0;       // entities the client's rollback left out of a comparison (NotSent)
    bool tintAddedAgain = false;            // the crate whose Tint was put on again at 200 has it at the client
    std::uint32_t playerLongestWait = 0;    // the most ticks in a row the player's values waited at the server
    std::uint32_t othersLongestWait = 0;    // the same for every other entity
};

enum class Shape { None, Seen, Blink };

// G-10's run: connect, give the player, push for a while, then the default input for the last segment; at the end
// the client's step P and its digest; the server runs on until it records step P.
Run RunSample(const N::MemoryLinkSettings& link, const std::int64_t serverStepsPerFrame, const int frames, const std::uint32_t valueBudget = 0,
              const Shape shape = Shape::None, const std::uint32_t playerWeight = 0) {
    Reshaped() = Reshape{shape != Shape::None, shape == Shape::Blink};
    Sample sample(link, serverStepsPerFrame, valueBudget, shape != Shape::None);
    for (int i = 0; i < 60 && (sample.Peer(sample.ServerWorld()) == 0 || sample.Peer(sample.ClientWorld()) == 0); ++i) {
        REQUIRE(static_cast<bool>(sample.server->RunFrame()));
        REQUIRE(static_cast<bool>(sample.client->RunFrame()));
        sample.links.first.Advance();
        *sample.clock += Ms(16);
    }
    REQUIRE(sample.Peer(sample.ServerWorld()) != 0);
    REQUIRE(sample.Peer(sample.ClientWorld()) != 0);
    sample.ServerWorld().Insert(sample.Peer(sample.ServerWorld()), N::PeerControls{kPlayer});
    sample.ClientWorld().Insert(sample.Peer(sample.ClientWorld()), N::PeerControls{kPlayer});
    if (playerWeight > 0) sample.ServerWorld().Insert(kPlayer, N::NetPriority{playerWeight});

    Run run;
    std::map<std::uint64_t, std::uint64_t> serverDigests;
    std::set<K::EntityId> seen;
    std::optional<std::uint32_t> lastConfirmed;
    const std::uint64_t quietFrom = static_cast<std::uint64_t>(frames) - 80;  // the last 80 client steps: default input
    for (int i = 0; i < frames; ++i) {
        const std::uint64_t target = sample.ClientTick() + 1;
        N::LocalInput<Push>(sample.ClientWorld())->Set(static_cast<std::uint32_t>(target), Push{PushAt(target, quietFrom)},
                                                      N::ViewTime{static_cast<std::uint32_t>(target), 0});
        // The server's frame comes every serverStepsPerFrame client frames and runs that many steps (the same tick
        // rate: a server at a lower frame rate).
        if (i % serverStepsPerFrame == 0) {
            REQUIRE(static_cast<bool>(sample.server->RunFrame()));
            serverDigests[sample.ServerTick()] = DigestOf(sample.ServerWorld());
        }
        REQUIRE(static_cast<bool>(sample.client->RunFrame()));
        sample.links.first.Advance();
        *sample.clock += Ms(16);
        // A crate the client has had stays, until the server removed it.
        const std::set<K::EntityId> now = Crates(sample.ClientWorld());
        for (const K::EntityId id : seen) {
            if (now.count(id) == 0 && Crates(sample.ServerWorld()).count(id) != 0) ++run.crateLost;
        }
        for (const K::EntityId id : now) seen.insert(id);
        if (const auto* ledger = sample.ServerWorld().Get<N::PeerLedger>(sample.Peer(sample.ServerWorld()))) {
            for (const auto& [entity, waited] : ledger->waitedTicks) {
                std::uint32_t& longest = entity == kPlayer ? run.playerLongestWait : run.othersLongestWait;
                longest = std::max(longest, waited);
            }
        }
        const auto& replica = sample.Replica();
        if (replica.inboxConfirmed && lastConfirmed && *replica.inboxConfirmed > *lastConfirmed + 1) ++run.confirmedJumps;
        lastConfirmed = replica.inboxConfirmed;
    }
    // What the server has sent so far reaches the client: the client runs alone a few frames (the server is stopped),
    // so the counts of what was sent and what arrived can be put side by side.
    if (const auto* ledger = sample.ServerWorld().Get<N::PeerLedger>(sample.Peer(sample.ServerWorld()))) {
        run.serverDeferred = ledger->valuesDeferred;
        run.longestWait = ledger->longestWait;
    }
    for (int i = 0; i < 6; ++i) {
        REQUIRE(static_cast<bool>(sample.client->RunFrame()));
        sample.links.first.Advance();
        *sample.clock += Ms(16);
    }
    run.clientTick = sample.ClientTick();
    run.clientDigest = DigestOf(sample.ClientWorld());
    run.replica = sample.Replica();
    run.rollbackConfirmed = sample.ClientWorld().Resource<R::RollbackInbox>()->Confirmed();
    // The server runs on alone, one step a frame, until it has run step P.
    sample.ServerWorld().SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
    for (int i = 0; i < 200 && sample.ServerTick() < run.clientTick; ++i) {
        REQUIRE(static_cast<bool>(sample.server->RunFrame()));
        serverDigests[sample.ServerTick()] = DigestOf(sample.ServerWorld());
    }
    if (const auto found = serverDigests.find(run.clientTick); found != serverDigests.end()) run.serverDigest = found->second;
    run.serverCrates = Crates(sample.ServerWorld()).size();
    for (const K::EntityId id : Crates(sample.ClientWorld())) {
        const Tint* there = sample.ClientWorld().Get<Tint>(id);
        const Tint* wanted = sample.ServerWorld().Get<Tint>(id);
        if ((there == nullptr) != (wanted == nullptr) || (there != nullptr && !(*there == *wanted))) ++run.tintsWrong;
    }
    run.serverWave = sample.ServerWorld().Resource<Wave>()->count;
    run.clientWave = sample.ClientWorld().Resource<Wave>()->count;
    run.clientCrates = Crates(sample.ClientWorld()).size();
    for (const K::EntityId id : seen) {
        if (Crates(sample.ServerWorld()).count(id) == 0) {
            run.exitedAtServer.insert(id);
            if (Crates(sample.ClientWorld()).count(id) != 0) run.stillAtClient.insert(id);
        }
    }
    // The server sent the player and the seven crates as spawns (each once on this connection).
    run.serverEntered = 1 + 7;
    const Reshape& reshape = Reshaped();
    K::World& client = sample.ClientWorld();
    K::World& server = sample.ServerWorld();
    if (shape != Shape::None) {
        run.speedOffWhileSeen = reshape.withoutSpeed != 0 && client.Alive(reshape.withoutSpeed) && client.Get<Speed>(reshape.withoutSpeed) == nullptr;
        run.hiddenBack = reshape.hidden != 0 && client.Get<Body>(reshape.hidden) != nullptr;
        run.hiddenBodySame = run.hiddenBack && server.Get<Body>(reshape.hidden) != nullptr &&
                             *client.Get<Body>(reshape.hidden) == *server.Get<Body>(reshape.hidden);
        run.hiddenSpeed = run.hiddenBack && client.Get<Speed>(reshape.hidden) != nullptr;
        run.hiddenTint = run.hiddenBack && client.Get<Tint>(reshape.hidden) != nullptr;
        run.tintSoon = reshape.withoutTintSoon != 0 && client.Get<Tint>(reshape.withoutTintSoon) != nullptr;
        run.hiddenTooTint = reshape.hiddenToo != 0 && client.Get<Tint>(reshape.hiddenToo) != nullptr;
        run.hiddenTintSame = run.hiddenTint && server.Get<Tint>(reshape.hidden) != nullptr &&
                             *client.Get<Tint>(reshape.hidden) == *server.Get<Tint>(reshape.hidden);
    }
    run.rollbacksForEntries = client.Resource<R::RollbackHistory>()->rollbacksForEntries;
    run.notSentLeftOut = client.Resource<R::RollbackHistory>()->notSentLeftOut;
    if (shape == Shape::Seen) {
        const Tint* again = client.Get<Tint>(reshape.tintAgain);
        run.tintAddedAgain = again != nullptr && *again == Tint{9};
    }
    return run;
}

std::string Counts(const Run& run) {
    const auto& r = run.replica;
    std::string drops;
    for (std::size_t i = 0; i < N::kReplicaDropCount; ++i) drops += (i ? "," : "") + std::to_string(r.drops[i]);
    return "client step P=" + std::to_string(run.clientTick) + " | inbox values=" + std::to_string(r.inboxValues) +
           " removals=" + std::to_string(r.inboxRemovals) + " entries=" + std::to_string(r.inboxEntries) +
           " exits=" + std::to_string(r.inboxExits) + " confirmed=" + std::to_string(r.inboxConfirmed.value_or(0)) +
           " | value parts received=" + std::to_string(r.valuePartsReceived) + " processed=" + std::to_string(r.valuePartsProcessed) +
           " pending=" + std::to_string(r.pendingValues.size()) + " | structures applied=" + std::to_string(r.structuresApplied) +
           " | drops=" + drops + " | crates server=" + std::to_string(run.serverCrates) + " client=" + std::to_string(run.clientCrates) +
           " | confirmed point jumps=" + std::to_string(run.confirmedJumps);
}

N::MemoryLinkSettings Slow() {
    N::MemoryLinkSettings settings;
    settings.seed = 0x5EED0010u;
    settings.aToB = {0, 0, 0, 0, 3};
    settings.bToA = {0, 0, 0, 0, 3};
    return settings;
}

} // namespace

TEST_CASE("k3: G-10 the client that rolls back ends with the Match's digest at its step P; spawned entities stay") {
    const Run run = RunSample(Slow(), 1, 300);
    MESSAGE("G-10: " << Counts(run));
    REQUIRE_MESSAGE(run.serverDigest.has_value(), "G-10: the Match recorded the client's step P");
    CHECK_MESSAGE(run.clientDigest == *run.serverDigest, "G-10: at its step P the client's Digest equals the Match's");
    CHECK_MESSAGE(run.crateLost == 0, "G-10: an entity the network spawned is not removed by a rollback (Entered)");
    CHECK_MESSAGE(run.clientCrates == run.serverCrates, "K3: the server's spawns reach the client (Entered)");
    CHECK_MESSAGE(run.rollbackConfirmed.has_value(), "K3: the confirmed point reaches the rollback Module");
    CHECK_MESSAGE(run.replica.unchangedNotWritten == 0, "K3: no rolling-back value is written directly in First");
    CHECK_MESSAGE(run.replica.resourceValuesApplied == 0, "K3: a rolling-back Resource goes into the inbox, not through its apply");
    CHECK_MESSAGE(run.tintsWrong == 0, "K3: a replicated component that does not roll back is written directly, once the entity is there");
    CHECK_MESSAGE(run.clientWave == run.serverWave, "K3: a replicated Resource that rolls back reaches the client through the inbox");
    // The counts: every spawn and the one removal reached the inbox; nothing unreadable or dropped beyond the depth.
    CHECK_MESSAGE(run.replica.inboxEntries == run.serverEntered, "G-10: entries in the inbox == entities the server spawned");
    CHECK_MESSAGE(run.replica.inboxExits == 1, "G-10: exits in the inbox == entities the server removed");
    CHECK(run.replica.drops[static_cast<std::size_t>(N::ReplicaDrop::Malformed)] == 0);
    CHECK(run.replica.valuePartsReceived == run.replica.valuePartsProcessed + run.replica.pendingValues.size());
}

TEST_CASE("k3: with the server running three steps a frame the confirmed point jumps, and G-10 still holds") {
    const Run run = RunSample(Slow(), 3, 300);
    MESSAGE("G-10 three steps a frame: " << Counts(run));
    CHECK_MESSAGE(run.confirmedJumps > 0, "the confirmed point jumps (only the last step of a frame is sent, U4)");
    REQUIRE(run.serverDigest.has_value());
    CHECK_MESSAGE(run.clientDigest == *run.serverDigest, "G-10 three steps a frame: at its step P the client's Digest equals the Match's");
    CHECK(run.crateLost == 0);
    CHECK(run.clientCrates == run.serverCrates);
}

TEST_CASE("k3: an entity the authority removes (Exited) is removed at the client at the confirmed point") {
    const Run run = RunSample(Slow(), 1, 300);
    REQUIRE_MESSAGE(run.exitedAtServer.size() == 1, "the server removed one crate the client had");
    CHECK_MESSAGE(run.stillAtClient.empty(), "an entity the authority removed is gone at the client");
}

TEST_CASE("k3: G-11 values the server defers over its budget are NotSent at the client, not taken as unchanged") {
    // 90 value bytes a tick for the Peer: two or three of the moving bodies a tick, the others wait (step 5).
    const Run run = RunSample(Slow(), 1, 300, 90);
    MESSAGE("G-11 NotSent: " << Counts(run) << " | deferred at the server=" << run.serverDeferred << " NotSent at the client="
                             << run.replica.inboxNotSent);
    REQUIRE_MESSAGE(run.serverDeferred > 0, "step 5: values over the budget wait for a later tick");
    CHECK_MESSAGE(run.longestWait <= 8, "step 5: the accumulated priority lets no entity wait long (longest " << run.longestWait << " ticks)");
    CHECK_MESSAGE(run.replica.inboxNotSent == run.serverDeferred, "G-11: every deferred value is NotSent at the simulating client");
    CHECK_MESSAGE(run.notSentLeftOut > 0, "G-11: the deferred entities are left out of the client's rollback comparison (" << run.notSentLeftOut << ")");
    REQUIRE(run.serverDigest.has_value());
    CHECK_MESSAGE(run.clientDigest == *run.serverDigest,
                  "G-11: the simulating client does not take NotSent as unchanged (its Digest at P is the Match's)");
}

TEST_CASE("k3: a budget smaller than one entity still sends one entity a tick") {
    // net 14: the first candidate is always sent. With 1 byte a tick nothing fits; without that rule nothing would
    // ever be sent. The bound 32 is written before the run (about 8 bodies move; one a tick).
    const Run run = RunSample(Slow(), 1, 300, 1);
    MESSAGE("budget 1: longest wait " << run.longestWait << ", deferred " << run.serverDeferred);
    REQUIRE(run.serverDeferred > 0);
    CHECK_MESSAGE(run.longestWait <= 32, "step 5: a budget smaller than one entity still sends one entity a tick (longest wait "
                                             << run.longestWait << ")");
}

TEST_CASE("k3: an entity of a heavier NetPriority waits less") {
    // net 14: the score is the accumulated priority plus the weight. The player weighs 8, the other bodies 1.
    const Run run = RunSample(Slow(), 1, 300, 90, Shape::None, 8);
    MESSAGE("weight 8: the player waited at most " << run.playerLongestWait << " ticks, the others " << run.othersLongestWait);
    REQUIRE(run.othersLongestWait > 0);
    CHECK_MESSAGE(run.playerLongestWait < run.othersLongestWait, "step 5: a heavier entity waits less");
}

TEST_CASE("k3: an entity hidden and seen again comes back with the server's components; components taken off reach the client") {
    // The review of PR-2: B1 (a component taken off while hidden must not come back), S1 (a component that does not
    // roll back taken off before the entity is in the World), S5 (Removed of a rolling-back component while seen).
    const Run run = RunSample(Slow(), 1, 300, 0, Shape::Seen);
    MESSAGE("K3 reshaped: " << Counts(run) << " | rollbacks for entries=" << run.rollbacksForEntries);
    CHECK_MESSAGE(run.speedOffWhileSeen, "K3: a rolling-back component the server takes off is gone at the client (Removed)");
    REQUIRE_MESSAGE(run.hiddenBack, "K3 visibility: the hidden crate is back at the client under its id");
    CHECK_MESSAGE(run.hiddenBodySame, "K3 visibility: it comes back with the server's values");
    CHECK_MESSAGE(!run.hiddenSpeed, "B1: a rolling-back component taken off while hidden does not come back with the entity");
    CHECK_MESSAGE(!run.hiddenTint, "B1: a component that does not roll back taken off while hidden is not at the client");
    CHECK_MESSAGE(!run.tintSoon, "S1 a: a component that does not roll back taken off before the entity is in the World is not written");
    CHECK_MESSAGE(run.tintAddedAgain, "K3: a component that does not roll back added to a seen entity reaches the client");
    CHECK_MESSAGE(run.replica.inboxEntries == run.serverEntered + 1, "K3 visibility: the crate seen again is an entry again (Entered)");
    CHECK_MESSAGE(run.rollbacksForEntries > 0, "U3: the rollbacks entries take part in are counted");
    REQUIRE(run.serverDigest.has_value());
    CHECK_MESSAGE(run.clientDigest == *run.serverDigest, "K3 visibility: at its step P the client's Digest equals the Match's");
}

TEST_CASE("k3: with losses, an entity seen again before its exit reaches the confirmed point keeps its components") {
    // S1 b: the spawn comes while the entity of before is still in the World (the confirmed point is before its exit);
    // a later rollback to a point between the exit and the entry removes it, and the entry puts it back. The crate
    // blinks (hidden 2 ticks of every 6 in [150, 250)) with a second one, and at 240, while they are hidden, the server
    // takes the first one's Speed and the second one's Tint off; the server-to-client direction loses and reorders.
    // The seeds are the ones of 1..24 where the case happened with the fix taken out (S1 b: 1, 18, 24; B1's Removed
    // on a spawn seen again and its taking-off of the components that do not roll back: 1, 18; the review fixes'
    // dev_log).
    N::MemoryLinkSettings lossy = Slow();
    lossy.aToB.lossPer10000 = 1000;
    lossy.aToB.delayPer10000 = 3000;
    lossy.aToB.maxDelaySteps = 4;
    for (const std::uint64_t seed : {1u, 18u, 24u}) {
        lossy.seed = seed;
        const Run run = RunSample(lossy, 1, 360, 0, Shape::Blink);
        CAPTURE(seed);
        MESSAGE("K3 blinking with losses, seed " << seed << ": " << Counts(run) << " | spawns while still in the World="
                                                  << run.replica.entriesWhileAlive << " | tints wrong=" << run.tintsWrong);
        REQUIRE_MESSAGE(run.replica.entriesWhileAlive > 0, "S1 b: a spawn came while the entity was still in the World");
        REQUIRE_MESSAGE(run.hiddenBack, "S1 b: the blinking crate is at the client at the end");
        CHECK_MESSAGE(run.hiddenTintSame, "S1 b: the blinking crate keeps its Tint (a component that does not roll back)");
        CHECK_MESSAGE(!run.hiddenSpeed, "B1: a rolling-back component taken off while hidden does not come back, its exit not yet forgotten");
        CHECK_MESSAGE(!run.hiddenTooTint, "B1: a component that does not roll back taken off while hidden is taken off the entity of before");
        CHECK_MESSAGE(run.tintsWrong == 0, "S1 b: every crate at the client has the server's Tint, or none where the server has none");
    }
}

namespace {

// The server's Link for the recheck's N1: the test decides, frame by frame, what the server sends. Pass sends at once;
// Hold keeps a datagram in one of two queues to send later; Drop loses it.
class Gate final : public N::Link {
public:
    enum class Mode { Pass, HoldEarly, Drop, HoldResend };
    explicit Gate(N::MemoryLink inner) : inner_(std::move(inner)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        if (mode == Mode::Pass) return inner_.Send(to, datagram);
        if (mode == Mode::Drop) return {};
        auto& queue = mode == Mode::HoldEarly ? early : resend;
        queue.emplace_back(to, std::vector<std::byte>(datagram.begin(), datagram.end()));
        return {};
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override { return inner_.Receive(buffer); }
    // Sends what queue holds now, in order.
    void Release(std::vector<std::pair<N::Endpoint, std::vector<std::byte>>>& queue) {
        for (const auto& [to, bytes] : queue) REQUIRE(static_cast<bool>(inner_.Send(to, bytes)));
        queue.clear();
    }
    Mode mode = Mode::Pass;
    std::vector<std::pair<N::Endpoint, std::vector<std::byte>>> early, resend;

private:
    N::MemoryLink inner_;
};

struct Gated {
    bool between = false;      // the confirmed point fell in [exit, entry) after a spawn came while the entity was there
    std::uint64_t entriesWhileAlive = 0;
    bool tintThere = false;    // at the end the crate has a Tint at the client
    bool tintSame = false;     // and it is the server's
    std::uint64_t confirmedAtRelease = 0;
    bool removedAtRelease = false;
};

// S1 b made to happen (the recheck's N1). The crate of 40 is hidden at the server's ticks 150 and 151 (exit 150, entry
// 152). The datagrams of ticks 150 and 151 are held, the one of tick 152 is lost, and the server pauses (no new tick,
// so nothing but resends of the reliable structure updates goes out; those are held too). Then:
//   sameFrame: everything held arrives in one client frame: the exit and the entry are applied while the crate of
//              before is still in the World, and the values of 150 and 151 make the confirmed point 151;
//   laterFrame: the resends (the exit and the entry) arrive first, a frame passes, then the values of 150 and 151.
Gated RunGated(const bool sameFrame, const bool changeTint = false) {
    Reshaped() = Reshape{true, false, true, changeTint};
    Gate* gate = nullptr;
    Sample sample(
        N::MemoryLinkSettings{}, 1, 0, true,
        [&gate](N::MemoryLink inner) {
            auto made = std::make_shared<Gate>(std::move(inner));
            gate = made.get();
            return made;
        });
    REQUIRE(gate != nullptr);
    auto clientFrame = [&] {
        const std::uint64_t target = sample.ClientTick() + 1;
        if (auto* input = N::LocalInput<Push>(sample.ClientWorld()); input != nullptr && sample.Peer(sample.ClientWorld()) != 0) {
            input->Set(static_cast<std::uint32_t>(target), Push{0}, N::ViewTime{static_cast<std::uint32_t>(target), 0});
        }
        REQUIRE(static_cast<bool>(sample.client->RunFrame()));
    };
    auto both = [&] {
        REQUIRE(static_cast<bool>(sample.server->RunFrame()));
        clientFrame();
        sample.links.first.Advance();
        *sample.clock += Ms(16);
    };
    for (int i = 0; i < 60 && (sample.Peer(sample.ServerWorld()) == 0 || sample.Peer(sample.ClientWorld()) == 0); ++i) both();
    REQUIRE(sample.Peer(sample.ServerWorld()) != 0);
    sample.ServerWorld().Insert(sample.Peer(sample.ServerWorld()), N::PeerControls{kPlayer});
    sample.ClientWorld().Insert(sample.Peer(sample.ClientWorld()), N::PeerControls{kPlayer});
    while (sample.ServerTick() + 1 < 150) both();
    Gated gated;
    const std::uint64_t before = sample.Replica().entriesWhileAlive;
    // Ticks 150 and 151: held; 152: lost. The client runs on, receiving nothing.
    for (const Gate::Mode mode : {Gate::Mode::HoldEarly, Gate::Mode::HoldEarly, Gate::Mode::Drop}) {
        gate->mode = mode;
        both();
    }
    REQUIRE(sample.ServerTick() == 152);
    // The server pauses; its resends are held.
    sample.ServerWorld().SetResource(W::TimeSampling{0, W::SamplingMode::ManualDelta});
    gate->mode = Gate::Mode::HoldResend;
    // Only the server's time passes (the client's time sync is not moved by the pause).
    for (int i = 0; i < 40; ++i) {
        REQUIRE(static_cast<bool>(sample.server->RunFrame()));
        sample.links.first.Advance();
        *sample.serverAhead += Ms(16);
    }
    REQUIRE(sample.ServerTick() == 152);
    REQUIRE_MESSAGE(!gate->resend.empty(), "N1: the paused server resent its structure updates");
    auto confirmed = [&] { return sample.Replica().inboxConfirmed.value_or(0); };
    REQUIRE(confirmed() < 150);
    if (sameFrame) {
        gate->Release(gate->early);
        gate->Release(gate->resend);
        sample.links.first.Advance();
        clientFrame();
    } else {
        gate->Release(gate->resend);
        sample.links.first.Advance();
        clientFrame();
        REQUIRE_MESSAGE(confirmed() < 150, "N1 later frame: the structures alone do not move the confirmed point");
        clientFrame();
        gate->Release(gate->early);
        sample.links.first.Advance();
        clientFrame();
    }
    gated.entriesWhileAlive = sample.Replica().entriesWhileAlive - before;
    // The rollback to the confirmed point in [exit, entry) has run (its decide stage is in the client's frame): the
    // crate of before is gone from the World.
    gated.confirmedAtRelease = confirmed();
    gated.removedAtRelease = !sample.ClientWorld().Alive(Reshaped().hidden);
    gated.between = gated.entriesWhileAlive > 0 && gated.confirmedAtRelease >= 150 && gated.confirmedAtRelease < 152 && gated.removedAtRelease;
    // The server resumes; the crate comes back at the confirmed point after 152.
    sample.ServerWorld().SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
    gate->mode = Gate::Mode::Pass;
    for (int i = 0; i < 40; ++i) both();
    const K::EntityId crate = Reshaped().hidden;
    const Tint* there = sample.ClientWorld().Get<Tint>(crate);
    const Tint* wanted = sample.ServerWorld().Get<Tint>(crate);
    gated.tintThere = there != nullptr;
    gated.tintSame = there != nullptr && wanted != nullptr && *there == *wanted;
    MESSAGE("N1 " << std::string(sameFrame ? "same frame" : "later frame") << ": spawns while still in the World=" << gated.entriesWhileAlive
                  << " confirmed after the release=" << gated.confirmedAtRelease << " crate of before removed=" << gated.removedAtRelease << " client tick=" << sample.ClientTick() << " | " << sample.Replica().inboxEntries << " entries, "
                  << sample.Replica().inboxExits << " exits");
    return gated;
}

} // namespace

TEST_CASE("k3: S1 b made to happen: the confirmed point falls between the exit and the entry in the spawn's frame") {
    const Gated gated = RunGated(true);
    REQUIRE_MESSAGE(gated.between, "N1: the spawn came while the crate of before was there, then the confirmed point fell in [exit, entry) and its rollback removed it");
    CHECK_MESSAGE(gated.tintThere, "S1 b same frame: a spawn's values wait while the entity of before is still in the World");
    CHECK(gated.tintSame);
}

TEST_CASE("k3: S1 b made to happen: the confirmed point falls between the exit and the entry two frames after the spawn") {
    const Gated gated = RunGated(false);
    REQUIRE_MESSAGE(gated.between, "N1: the spawn came while the crate of before was there, then the confirmed point fell in [exit, entry) and its rollback removed it");
    CHECK_MESSAGE(gated.tintThere, "S1 b later frame: a spawn's values are written only after the rollback decided at or after the spawn's tick");
    CHECK(gated.tintSame);
}

TEST_CASE("k3: a value of a component that does not roll back, come while its entity is gone at the client, waits for it") {
    // The adversarial check's A6, on the gated case of the same frame: at 153 the server changes the crate's Tint to
    // 40 001; the client gets it while the crate is gone (the rollback to 151 removed it, the one to a point >= 152
    // puts it back). The server resends it only until the client confirms the part, which is before the crate is back.
    const Gated gated = RunGated(true, true);
    REQUIRE(gated.between);
    CHECK_MESSAGE(gated.tintSame, "K3: a value of a component that does not roll back, come before the entity is in the World, waits for it");
}
