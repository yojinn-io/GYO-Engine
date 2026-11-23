#pragma once

// A small 2D sample game for the replication tests (declaration section 8, G-5 and the scenario; the user allowed
// self-made samples, v8-arch-gyo2-migration-needs-consent). Synthetic only: positions that move, health that comes
// and goes, targets that point at other entities (EntityId inside a component), doors that only the server spawns
// (map entities, OnServer()) and toggles, with the server message DoorOpened. Every rule is a function of the tick,
// so a run is the same every time.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ModuleTestSupport.hpp"
#include "engine/net/Replication.hpp"
#include "engine/world_time/Sampling.hpp"

namespace GYO::Test::Net {

struct Position {
    std::int32_t x = 0;
    std::int32_t y = 0;
    friend bool operator==(const Position&, const Position&) = default;
};
struct Health {
    std::int32_t hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
struct Target {
    K::EntityId who = 0;
    friend bool operator==(const Target&, const Target&) = default;
};
struct Door {
    std::uint8_t open = 0;
    friend bool operator==(const Door&, const Door&) = default;
};
inline void HashValue(K::Hasher& h, const Position& v) {
    HashValue(h, v.x);
    HashValue(h, v.y);
}
inline void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }
inline void HashValue(K::Hasher& h, const Target& v) { HashValue(h, v.who); }
inline void HashValue(K::Hasher& h, const Door& v) { HashValue(h, v.open); }

// The server message: a door opened or closed.
struct DoorOpened {
    K::EntityId door = 0;
    std::uint8_t open = 0;
};

inline N::Codec<Position> PositionCodec() {
    return {[](N::NetWriter& w, const Position& v) {
                w.Value(v.x);
                w.Value(v.y);
            },
            [](N::NetReader& r, Position& v) { return r.Value(v.x) && r.Value(v.y); }, 1};
}
inline N::Codec<Health> HealthCodec() {
    return {[](N::NetWriter& w, const Health& v) { w.Value(v.hp); }, [](N::NetReader& r, Health& v) { return r.Value(v.hp); }, 1};
}
inline N::Codec<Target> TargetCodec() {
    return {[](N::NetWriter& w, const Target& v) { w.Value(v.who); }, [](N::NetReader& r, Target& v) { return r.Value(v.who); }, 1};
}
inline N::Codec<Door> DoorCodec() {
    return {[](N::NetWriter& w, const Door& v) { w.Value(v.open); }, [](N::NetReader& r, Door& v) { return r.Value(v.open); }, 1};
}
inline N::Codec<DoorOpened> DoorOpenedCodec() {
    return {[](N::NetWriter& w, const DoorOpened& v) {
                w.Value(v.door);
                w.Value(v.open);
            },
            [](N::NetReader& r, DoorOpened& v) { return r.Value(v.door) && r.Value(v.open); }, 1};
}

// The game's rules stop after this many ticks (the server stops changing).
struct GameRules {
    std::uint64_t lastTick = 400;
    // false: only the scenario's two players and one door.
    bool busy = true;
};
inline void HashValue(K::Hasher& h, const GameRules& v) {
    HashValue(h, v.lastTick);
    HashValue(h, v.busy);
}

// What the client saw: server messages and whether their door existed when they arrived; Changed<Position>.
struct ClientLog {
    std::uint64_t messages = 0;
    std::uint64_t messagesBeforeEntity = 0;
    std::uint64_t positionsChanged = 0;
    std::uint32_t replacedGeneration = 0;
};
inline void HashValue(K::Hasher&, const ClientLog&) {}

inline bool Playing(const GameRules& rules, const W::Tick& tick) { return tick.value >= 1 && tick.value <= rules.lastTick; }

inline K::SystemResult SpawnMap(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Spawn<K::Bundle<Door>> spawn,
                                K::Spawn<K::Bundle<Position>> players) {
    // A busy game adds a door every 25 ticks: it is announced in the same frame (AnnounceDoors).
    if (rules->busy && Playing(*rules, *tick) && tick->value % 25 == 0) {
        if (auto id = spawn.New(Door{0}); !id) return GYO::Engine::Base::Err(std::move(id).error());
    }
    if (tick->value != 1) return {};
    for (int i = 0; i < (rules->busy ? 3 : 1); ++i) {
        if (auto id = spawn.New(Door{0}); !id) return GYO::Engine::Base::Err(std::move(id).error());
    }
    if (!rules->busy) {
        for (int i = 0; i < 2; ++i) {
            if (auto id = players.New(Position{i * 10, 0}); !id) return GYO::Engine::Base::Err(std::move(id).error());
        }
    }
    return {};
}
inline K::SystemResult MoveAll(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Query<K::Write<Position>> positions) {
    if (!Playing(*rules, *tick)) return {};
    positions.ForEach([&](K::EntityId id, K::Mut<Position> p) {
        p->x += 1;
        p->y = static_cast<std::int32_t>((tick->value + id) % 7);
    });
    return {};
}
inline K::SystemResult SpawnSome(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Spawn<K::Bundle<Position, Health>> spawn) {
    if (!rules->busy || !Playing(*rules, *tick) || tick->value % 3 != 0) return {};
    if (auto id = spawn.New(Position{0, 0}, Health{100}); !id) return GYO::Engine::Base::Err(std::move(id).error());
    return {};
}
inline K::SystemResult DespawnSome(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Query<K::Read<Position>> movers,
                                   K::Despawn despawn) {
    if (!rules->busy || !Playing(*rules, *tick) || tick->value % 5 != 0) return {};
    const auto ids = movers.Entities();
    if (ids.size() > 6) despawn.Entity(ids[(tick->value / 5) % ids.size()]);
    return {};
}
inline K::SystemResult AddTarget(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Query<K::Read<Position>> movers,
                                 K::CommandsFor<K::Inserts<Target, Health>, K::Removes<>> commands) {
    if (!rules->busy || !Playing(*rules, *tick)) return {};
    const auto ids = movers.Entities();
    if (ids.size() < 2) return {};
    if (tick->value % 4 == 0) commands.Insert(ids[tick->value % ids.size()], Target{ids[(tick->value + 1) % ids.size()]});
    if (tick->value % 9 == 0) commands.Insert(ids[(tick->value / 9) % ids.size()], Health{50});
    return {};
}
inline K::SystemResult RemoveSome(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Query<K::Read<Position>> movers,
                                  K::CommandsFor<K::Inserts<>, K::Removes<Health, Target>> commands) {
    if (!rules->busy || !Playing(*rules, *tick)) return {};
    const auto ids = movers.Entities();
    if (ids.empty()) return {};
    if (tick->value % 7 == 0) commands.Remove<Health>(ids[(tick->value / 7) % ids.size()]);
    if (tick->value % 11 == 0) commands.Remove<Target>(ids[(tick->value / 11) % ids.size()]);
    return {};
}
inline K::SystemResult ToggleDoor(K::Res<W::Tick> tick, K::Res<GameRules> rules, K::Query<K::Write<Door>> doors,
                                  K::MessageWriter<DoorOpened> opened) {
    if (!Playing(*rules, *tick)) return {};
    const std::uint64_t every = rules->busy ? 6 : 30;
    if (tick->value % every != 0) return {};
    const auto ids = doors.Entities();
    if (ids.empty()) return {};
    const K::EntityId door = ids[(tick->value / every) % ids.size()];
    auto cell = doors.GetMut<Door>(door);
    cell->open = static_cast<std::uint8_t>(1 - cell->open);
    opened.Send(DoorOpened{door, cell->open});
    return {};
}

// Main/Update at the server: a door that appeared this frame is announced with DoorOpened in the same frame, so the
// message and the door's spawn go out in one tick (the message must not overtake the structure).
inline K::SystemResult AnnounceDoors(K::Res<GameRules> rules, K::Query<K::Read<Door>, K::Added<Door>> added,
                                     K::MessageWriter<DoorOpened> opened) {
    if (!rules->busy) return {};
    added.ForEach([&](K::EntityId id, const Door& door) { opened.Send(DoorOpened{id, door.open}); });
    return {};
}

inline K::SystemResult LogClient(K::MessageReader<DoorOpened> opened, K::Query<K::Read<Door>> doors,
                                 K::Query<K::Read<Position>, K::Changed<Position>> changed, K::ResMut<ClientLog> log) {
    for (const DoorOpened* message : opened.Read()) {
        ++log->messages;
        if (!doors.Contains(message->door)) ++log->messagesBeforeEntity;
    }
    log->positionsChanged += changed.Count();
    return {};
}

inline K::SystemOptions Fixed(std::string name, bool onServer) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = "FixedUpdate";
    mount.stage = "Update";
    if (onServer) mount.conditions.push_back(N::OnServer());
    options.mounts.push_back(std::move(mount));
    return options;
}

// The game's registration, the same at both ends (the logic runs only at the server: OnServer()). extra: one more
// replicated component (a protocol that differs).
inline void RegisterGame(O::WorldSetup& s, const GameRules rules, const bool extra = false) {
    auto& r = s.Registrar();
    r.Component<Position>({.name = "Position"});
    r.Component<Health>({.name = "Health"});
    r.Component<Target>({.name = "Target"});
    r.Component<Door>({.name = "Door"});
    r.Message<DoorOpened>({.name = "DoorOpened"});
    r.Resource<GameRules>({.name = "GameRules"});
    r.Resource<ClientLog>({.name = "ClientLog"});
    N::NetEntry net(s);
    net.Replicate<Position>("Position", PositionCodec());
    net.Replicate<Health>("Health", HealthCodec());
    net.Replicate<Target>("Target", TargetCodec());
    net.Replicate<Door>("Door", DoorCodec());
    if (extra) net.Replicate<std::int32_t>("Extra");
    net.ServerMessage<DoorOpened>("DoorOpened", DoorOpenedCodec());
    r.AddSystem<&SpawnMap>(Fixed("SpawnMap", true));
    r.AddSystem<&MoveAll>(Fixed("MoveAll", true));
    r.AddSystem<&SpawnSome>(Fixed("SpawnSome", true));
    r.AddSystem<&DespawnSome>(Fixed("DespawnSome", true));
    r.AddSystem<&AddTarget>(Fixed("AddTarget", true));
    r.AddSystem<&RemoveSome>(Fixed("RemoveSome", true));
    r.AddSystem<&ToggleDoor>(Fixed("ToggleDoor", true));
    K::SystemOptions announce;
    announce.name = "AnnounceDoors";
    K::Mount announceMount;
    announceMount.schedule = "Main";
    announceMount.stage = "Update";
    announceMount.conditions.push_back(N::OnServer());
    announce.mounts.push_back(std::move(announceMount));
    r.AddSystem<&AnnounceDoors>(std::move(announce));
    K::SystemOptions log;
    log.name = "LogClient";
    K::Mount mount;
    mount.schedule = "Main";
    mount.stage = "Update";
    mount.conditions.push_back(N::OnClient());
    log.mounts.push_back(std::move(mount));
    r.AddSystem<&LogClient>(std::move(log));
    s.Prepare([rules](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(rules);
        world.SetResource(ClientLog{});
        // One fixed step per frame, whatever the clock: the ticks are the frames.
        world.SetResource(W::TimeSampling{15'625'000, W::SamplingMode::ManualDelta});
        return {};
    });
}

// The replicated part of a World: every entity with a replicated component, and each component's value.
struct GameState {
    std::set<K::EntityId> entities;
    std::map<K::EntityId, Position> positions;
    std::map<K::EntityId, Health> healths;
    std::map<K::EntityId, Target> targets;
    std::map<K::EntityId, Door> doors;
    friend bool operator==(const GameState&, const GameState&) = default;
};

inline GameState StateOf(K::World& world) {
    GameState state;
    for (const K::EntityId id : world.Entities()) {
        bool replicated = false;
        if (const auto* v = world.Get<Position>(id)) state.positions[id] = *v, replicated = true;
        if (const auto* v = world.Get<Health>(id)) state.healths[id] = *v, replicated = true;
        if (const auto* v = world.Get<Target>(id)) state.targets[id] = *v, replicated = true;
        if (const auto* v = world.Get<Door>(id)) state.doors[id] = *v, replicated = true;
        if (replicated) state.entities.insert(id);
    }
    return state;
}

// Where two states differ, in words (empty when equal).
inline std::string Difference(const GameState& client, const GameState& server) {
    std::ostringstream out;
    if (client.entities != server.entities) out << "entities " << client.entities.size() << " vs " << server.entities.size() << "; ";
    if (client.positions != server.positions) out << "positions differ; ";
    if (client.healths != server.healths) out << "healths differ; ";
    if (client.targets != server.targets) out << "targets differ; ";
    if (client.doors != server.doors) out << "doors differ; ";
    return out.str();
}

// A game end: the Product with the game registered and the net role.
inline End GameEnd(const N::NetRole role, const GameRules rules, const bool extra = false) {
    End end;
    end.net.role = role;
    end.role = N::NetWorldOptions{role, std::nullopt};
    end.range = role == N::NetRole::Server ? E::EntityIdsOptions{1, 100'000} : E::EntityIdsOptions{100'001, 200'000};
    end.build = [rules, extra](O::WorldSetup& s) { RegisterGame(s, rules, extra); };
    return end;
}

} // namespace GYO::Test::Net
