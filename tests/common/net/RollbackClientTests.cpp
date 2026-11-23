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
    bool blink = false;               // the lossy case: the blinking crate is hidden 6 ticks of every 12 in [150, 250)
    K::EntityId withoutTintSoon = 0;  // spawned at 60, its Tint taken off at 61 (before the client has it in the World)
    K::EntityId withoutSpeed = 0;     // spawned at 50, its Speed taken off at 110 while seen (Removed)
    K::EntityId hidden = 0;           // spawned at 40, hidden in [120, 140); its Speed and Tint taken off at 130
                                      // (blinking: its Speed taken off at 240, while hidden)
    K::EntityId hiddenToo = 0;        // blinking: spawned at 70, blinks with it; its Tint taken off at 240
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
    if (t == 119 || (reshape.blink && t == 149)) reshape.hidden = CrateTinted(world, 40'000);
    if (reshape.blink && t == 149) reshape.hiddenToo = CrateTinted(world, 70'000);
    if (t == 130 && !reshape.blink) {
        if (auto removed = world.Remove<Speed>(reshape.hidden); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
        if (auto removed = world.Remove<Tint>(reshape.hidden); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
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
    std::pair<N::MemoryLink, N::MemoryLink> links;
    std::unique_ptr<O::Product> server, client;

    Sample(const N::MemoryLinkSettings& link, const std::int64_t serverStepsPerFrame, const std::uint32_t valueBudget = 0,
           const bool reshape = false)
        : links(N::MemoryLink::Pair(link)) {
        auto reader = [now = clock] { return *now; };
        End s = GameEnd(N::NetRole::Server, {});
        s.authority = std::make_shared<A::ServerAuthorityStrategy>();
        s.build = [serverStepsPerFrame, valueBudget, reshape](O::WorldSetup& setup) {
            RegisterSample(setup, serverStepsPerFrame, valueBudget, reshape);
        };
        s.net.link = std::make_shared<N::MemoryLink>(links.first);
        s.net.clock = reader;
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
};

enum class Shape { None, Seen, Blink };

// G-10's run: connect, give the player, push for a while, then the default input for the last segment; at the end
// the client's step P and its digest; the server runs on until it records step P.
Run RunSample(const N::MemoryLinkSettings& link, const std::int64_t serverStepsPerFrame, const int frames, const std::uint32_t valueBudget = 0,
              const Shape shape = Shape::None) {
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
    REQUIRE(run.serverDigest.has_value());
    CHECK_MESSAGE(run.clientDigest == *run.serverDigest,
                  "G-11: the simulating client does not take NotSent as unchanged (its Digest at P is the Match's)");
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
