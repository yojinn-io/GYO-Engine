#pragma once

// Products for the net Module's tests: the time, rollback, authority, entity_ids and net Modules on the default
// skeleton, built by hand (no register file), with synthetic data only.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include "engine/authority/AuthorityModule.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace GYO::Test::Net {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace A = GYO::Engine::Authority;
namespace E = GYO::Engine::EntityIds;
namespace W = GYO::Engine::WorldTime;
namespace N = GYO::Engine::Net;

inline O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

// Keeps the frozen Registry of the World it is built into (the Product owns it).
class ProbeModule final : public O::Module {
public:
    explicit ProbeModule(const K::FrozenRegistry** out) : out_(out) {}
    void Build(O::WorldSetup&) override {}
    void Finish(const K::FrozenRegistry& registry, O::WorldFinish&) override { *out_ = &registry; }

private:
    const K::FrozenRegistry** out_;
};

inline O::RegisteredUnits Units(const bool withNet, const K::FrozenRegistry** probe = nullptr) {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(ModuleRow("entity_ids"));
    units.modules.push_back({"entity_ids", std::make_unique<E::EntityIdsModule>()});
    units.rows.push_back(ModuleRow("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<GYO::Engine::Rollback::RollbackModule>()});
    units.rows.push_back(ModuleRow("authority", {"world_time", "rollback"}));
    units.modules.push_back({"authority", std::make_unique<A::AuthorityModule>()});
    if (withNet) {
        units.rows.push_back(ModuleRow("net", {"world_time", "rollback", "authority", "entity_ids"}));
        units.modules.push_back({"net", std::make_unique<N::NetModule>()});
    }
    if (probe != nullptr) {
        units.rows.push_back(ModuleRow("probe"));
        units.modules.push_back({"probe", std::make_unique<ProbeModule>(probe)});
    }
    return units;
}

template <class Built>
std::string Reported(const Built& built) {
    if (built) return "built";
    std::string out;
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out.empty() ? built.error().detail : out;
}

// What the World's PeerConnected and PeerDisconnected messages said (a reader keeps them; the World trims what
// no reader holds).
struct Seen {
    std::vector<N::PeerConnected> connected;
    std::vector<N::PeerDisconnected> disconnected;
};
inline void HashValue(K::Hasher&, const Seen&) {}

inline K::SystemResult RememberPeers(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                                     K::ResMut<Seen> seen) {
    for (const N::PeerConnected* message : connected.Read()) seen->connected.push_back(*message);
    for (const N::PeerDisconnected* message : disconnected.Read()) seen->disconnected.push_back(*message);
    return {};
}

inline void RegisterSeen(O::WorldSetup& s) {
    s.Registrar().Resource<Seen>({.name = "Seen"});
    K::SystemOptions options;
    options.name = "RememberPeers";
    K::Mount mount;
    mount.schedule = "Main";
    mount.stage = "Update";
    options.mounts.push_back(std::move(mount));
    s.Registrar().AddSystem<&RememberPeers>(std::move(options));
    s.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Seen{});
        return {};
    });
}

inline const Seen& SeenIn(K::World& world) { return *world.Resource<Seen>(); }

// One end: a Product with one World "Net" in the given role.
struct End {
    N::NetProductOptions net;
    std::optional<N::NetWorldOptions> role;
    E::EntityIdsOptions range;
    std::shared_ptr<A::AuthorityStrategy> authority;
    W::WorldTimeOptions time;
    std::function<void(O::WorldSetup&)> build;
    bool productOptions = true;
};

inline O::ProductDescription Describe(const End& end) {
    O::ProductDescription d;
    d.name = "net-test";
    if (end.productOptions) d.options.Set(end.net);
    O::WorldDescription w;
    w.name = "Net";
    if (end.role) w.options.Set(*end.role);
    w.options.Set(end.range);
    w.options.Set(end.time);
    if (end.authority) {
        w.options.Set(A::AuthorityOptions{end.authority});
        w.dynamicInsert = false;
    }
    // The reader of the net messages joins a World with a role.
    w.build = [build = end.build, reads = end.role.has_value()](O::WorldSetup& s) {
        if (reads) RegisterSeen(s);
        if (build) build(s);
    };
    d.worlds.push_back(std::move(w));
    return d;
}

// The Peer entities of a World.
inline std::vector<N::Peer> PeersOf(K::World& world) {
    std::vector<N::Peer> peers;
    for (const K::EntityId id : world.Entities()) {
        if (const N::Peer* peer = world.Get<N::Peer>(id)) peers.push_back(*peer);
    }
    return peers;
}

} // namespace GYO::Test::Net
