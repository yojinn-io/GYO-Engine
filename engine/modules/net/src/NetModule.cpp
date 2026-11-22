#include "engine/net/NetModule.hpp"

#include <random>
#include <string>
#include <utility>

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/World.hpp"
#include "engine/net/UdpLink.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/world_time/TimeTypes.hpp"

namespace GYO::Engine::Net {
namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

std::uint64_t RandomSeed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ device();
}

Stamp StampOf(const WorldTime::Tick& tick) { return Stamp{static_cast<std::uint32_t>(tick.value), 0}; }

HandshakeFacts Facts(const NetWorldLink& link, const WorldTime::FixedTime& fixed, const IdRange& range) {
    HandshakeFacts facts;
    facts.protocolHash = NetProtocolHash();
    facts.stepNanoseconds = fixed.accumulator.Step();
    facts.idRange = link.idRange.value_or(range);
    facts.fingerprint = link.fingerprint;
    return facts;
}

// Main/First (the apply point "in", O-12): every waiting datagram into the connection layer; a connection made
// spawns its Peer and sends PeerConnected, one ended despawns it and sends PeerDisconnected.
SystemResult ReceiveNet(Kernel::ResMut<NetWorldLink> link, Kernel::Spawn<Kernel::Bundle<Peer>> spawn, Kernel::Despawn despawn,
                        Kernel::MessageWriter<PeerConnected> connected, Kernel::MessageWriter<PeerDisconnected> disconnected) {
    NetWorldLink& world = *link;
    NetService& service = *world.service;
    service.Pump(service.Now());
    if (ServerHost* server = service.Server()) {
        for (const HostEvent& event : server->TakeEvents()) {
            if (event.kind == HostEventKind::Connected) {
                auto peer = spawn.New(Peer{event.connectionId, event.userId, {}});
                if (!peer) return Base::Err(std::move(peer).error());
                world.peers[event.connectionId] = *peer;
                connected.Send(PeerConnected{*peer, event.userId});
                continue;
            }
            const auto found = world.peers.find(event.connectionId);
            Kernel::EntityId entity = 0;
            if (found != world.peers.end()) {
                entity = found->second;
                despawn.Entity(entity);
                world.peers.erase(found);
            }
            disconnected.Send(PeerDisconnected{entity, event.userId, event.reason, {}});
        }
        return {};
    }
    const ClientSession* client = service.Client();
    if (client == nullptr) return {};
    const ClientState state = client->State();
    if ((state == ClientState::Connected || state == ClientState::Interrupted) && world.clientPeer == 0) {
        auto peer = spawn.New(Peer{client->Accepted()->connectionId, client->Request().userId, {}});
        if (!peer) return Base::Err(std::move(peer).error());
        world.clientPeer = *peer;
        world.clientEndReported = false;
        connected.Send(PeerConnected{*peer, client->Request().userId});
    } else if (state == ClientState::Disconnected && !world.clientEndReported) {
        const Kernel::EntityId entity = world.clientPeer;
        if (entity != 0) despawn.Entity(entity);
        world.clientPeer = 0;
        world.clientEndReported = true;
        disconnected.Send(PeerDisconnected{entity, client->Request().userId, client->Ended()->reason, client->Ended()->detail});
    }
    return {};
}

// What both SendNet signatures do: the World's facts, then the connection layer's datagrams, then the Peers'
// drop counts.
void Send(NetWorldLink& world, const WorldTime::Tick& tick, const WorldTime::FixedTime& fixed, const IdRange& range,
          Kernel::Query<Kernel::Write<Peer>>& peers) {
    NetService& service = *world.service;
    const NetTime now = service.Now();
    service.SetFacts(Facts(world, fixed, range), now, false);
    service.Flush(now, StampOf(tick));
    if (ServerHost* server = service.Server()) {
        for (const auto& [id, entity] : world.peers) {
            const Connection* connection = server->Find(id);
            auto peer = peers.GetMut<Peer>(entity);
            if (connection != nullptr && peer) peer->drops = connection->Stats().drops;
        }
    } else if (ClientSession* client = service.Client(); client != nullptr && world.clientPeer != 0) {
        auto peer = peers.GetMut<Peer>(world.clientPeer);
        if (const Connection* connection = client->Transport(); connection != nullptr && peer) peer->drops = connection->Stats().drops;
    }
}

// Main/Last (the apply point "out", O-12).
SystemResult SendNet(Kernel::ResMut<NetWorldLink> link, Kernel::Res<WorldTime::Tick> tick, Kernel::Res<WorldTime::FixedTime> fixed,
                     Kernel::Res<EntityIds::RangeAllotter> allotter, Kernel::Query<Kernel::Write<Peer>> peers) {
    Send(*link, *tick, *fixed, IdRange{allotter->first, allotter->last}, peers);
    return {};
}
// The same when the author gives the range (NetWorldOptions::idRange): RangeAllotter need not be registered.
SystemResult SendNetGivenRange(Kernel::ResMut<NetWorldLink> link, Kernel::Res<WorldTime::Tick> tick,
                               Kernel::Res<WorldTime::FixedTime> fixed, Kernel::Query<Kernel::Write<Peer>> peers) {
    Send(*link, *tick, *fixed, IdRange{}, peers);
    return {};
}

Kernel::SystemOptions At(std::string name, std::string_view stage) {
    Kernel::SystemOptions options;
    options.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = std::string(Orchestration::DefaultSchedules::frame);
    mount.stage = std::string(stage);
    options.mounts.push_back(std::move(mount));
    return options;
}

} // namespace

std::uint64_t NetProtocolHash() { return Base::Fnv1a64("gyo.net.registrations/1;"); }

void NetModule::Configure(Orchestration::ProductSetup& setup) {
    const NetProductOptions* given = setup.Options<NetProductOptions>();
    if (given == nullptr) return; // G-1: no settings, nothing made, no socket opened
    NetProductOptions options = *given;
    std::shared_ptr<Link> link = options.link;
    Endpoint serverEndpoint = options.serverEndpoint;
    if (!link) {
        auto opened = UdpLink::Open(options.bind);
        if (!opened) {
            setup.Report("NT-E4", "the net Module could not open its UDP socket: " + Base::Describe(opened.error()));
            return;
        }
        link = std::make_shared<UdpLink>(std::move(*opened));
        serverEndpoint = UdpLink::ToEndpoint(options.server);
    }
    const std::uint64_t seed = options.seed.value_or(RandomSeed());
    setup.Buffer().Create<NetService>(std::move(options), std::move(link), serverEndpoint, seed,
                                      seed ^ 0xA0761D6478BD642Full);
}

void NetModule::Build(Orchestration::WorldSetup& setup) {
    const NetWorldOptions* role = setup.Options<NetWorldOptions>();
    if (role == nullptr) return; // G-1: a World without a role registers nothing
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Peer>({.name = "Peer"});
    r.Resource<NetWorldLink>({.name = "NetWorldLink"});
    r.Message<PeerConnected>({.name = "PeerConnected"});
    r.Message<PeerDisconnected>({.name = "PeerDisconnected"});
    r.AddSystem<&ReceiveNet>(At("ReceiveNet", Orchestration::DefaultSchedules::first));
    if (role->idRange) {
        r.AddSystem<&SendNetGivenRange>(At("SendNet", Orchestration::DefaultSchedules::last));
    } else {
        r.AddSystem<&SendNet>(At("SendNet", Orchestration::DefaultSchedules::last));
    }

    // With an authority strategy the two Systems are marked as one end's; without one (K1) the authority Module
    // is not touched, since a World with registrations and no strategy is AU-E2.
    const Authority::AuthorityOptions* authority = setup.Options<Authority::AuthorityOptions>();
    const bool strategy = authority != nullptr && authority->strategy != nullptr;
    if (strategy && setup.Buffer().Find<Authority::AuthorityRegistrations>() != nullptr) {
        Authority::AuthorityEntry entry(setup);
        entry.OneEndOnly("ReceiveNet", "the net Module's apply point in: each end runs its own connection layer");
        entry.OneEndOnly("SendNet", "the net Module's apply point out: each end runs its own connection layer");
    }

    NetService* service = setup.ProductBuffer().Find<NetService>();
    const NetRole worldRole = role->role;
    const std::optional<IdRange> givenRange = role->idRange;
    setup.Prepare([service, worldRole, givenRange, strategy](Kernel::World& world)
                      -> Base::Result<void, Orchestration::OrchestrationError> {
        // Check reported a World with a role and no NetService (NT-E1): no World is built then.
        NetWorldLink link;
        link.service = service;
        link.role = worldRole;
        link.idRange = givenRange;
        if (strategy) {
            auto fingerprint = Authority::ConsistencyFingerprintOf(world);
            if (fingerprint) link.fingerprint = fingerprint->Text();
        }
        const auto* fixed = world.Resource<WorldTime::FixedTime>();
        const auto* allotter = world.Resource<EntityIds::RangeAllotter>();
        const IdRange range = allotter != nullptr ? IdRange{allotter->first, allotter->last} : IdRange{};
        if (service != nullptr && fixed != nullptr) service->SetFacts(Facts(link, *fixed, range), service->Now(), true);
        world.SetResource(std::move(link));
        return {};
    });
}

void NetModule::Check(const Kernel::FrozenRegistry&, Orchestration::WorldCheck& context) {
    const NetWorldOptions* role = context.Options<NetWorldOptions>();
    if (role == nullptr) return;
    NetService* service = context.ProductBuffer().Find<NetService>();
    if (service == nullptr) {
        context.Report("NT-E1", "World '" + context.WorldName() +
                                    "' has a net role (NetWorldOptions) but the Product has no net settings (NetProductOptions)");
        return;
    }
    if (service->Role() != role->role) {
        context.Report("NT-E2", "World '" + context.WorldName() + "' has another net role than the Product's NetProductOptions");
        return;
    }
    if (!service->Bind(context.WorldName())) {
        context.Report("NT-E3", "World '" + context.WorldName() + "' has a net role, but another World of the Product has one");
    }
}

} // namespace GYO::Engine::Net
