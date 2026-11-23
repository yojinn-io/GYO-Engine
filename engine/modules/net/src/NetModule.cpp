#include "engine/net/NetModule.hpp"

#include <random>
#include <string>
#include <utility>

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/World.hpp"
#include "engine/net/UdpLink.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/rollback/RollbackStrategy.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace GYO::Engine::Net {
namespace {

using Detail::SystemResult;

std::uint64_t RandomSeed() {
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ device();
}

Stamp StampOf(const WorldTime::Tick& tick) { return Stamp{static_cast<std::uint32_t>(tick.value), 0}; }

HandshakeFacts Facts(const NetWorldLink& link, const WorldTime::FixedTime& fixed, const IdRange& range) {
    HandshakeFacts facts;
    facts.protocolHash = link.registry ? link.registry->Hash() : NetRegistry{}.Hash();
    facts.stepNanoseconds = fixed.accumulator.Step();
    facts.idRange = link.idRange.value_or(range);
    facts.fingerprint = link.fingerprint;
    return facts;
}

// Main/First (the apply point "in", O-12): every waiting datagram into the connection layer; every connection has
// its Peer entity (spawned with PeerConnected; after the server's World was replaced, the open connections get new
// ones and an empty ledger), and one that ended is despawned with PeerDisconnected. A Link failure since the last
// frame is told with NetLinkFailed.
SystemResult ReceiveNet(Kernel::ResMut<NetWorldLink> link, Kernel::Spawn<Kernel::Bundle<Peer, PeerLedger>> spawn,
                        Kernel::Despawn despawn, Kernel::MessageWriter<PeerConnected> connected,
                        Kernel::MessageWriter<PeerDisconnected> disconnected, Kernel::MessageWriter<NetLinkFailed> failed) {
    NetWorldLink& world = *link;
    NetService& service = *world.service;
    service.Pump(service.Now());
    if (service.LinkFailures() > world.reportedLinkFailures) {
        world.reportedLinkFailures = service.LinkFailures();
        const auto& last = service.LastLinkFailure();
        failed.Send(NetLinkFailed{service.LinkFailures(), last ? last->code : NetFailureCode::LinkFailed,
                                  last ? Base::Describe(*last) : std::string{}});
    }
    if (ServerHost* server = service.Server()) {
        for (const HostEvent& event : server->TakeEvents()) {
            if (event.kind != HostEventKind::Disconnected) continue;
            const auto found = world.peers.find(event.connectionId);
            Kernel::EntityId entity = 0;
            if (found != world.peers.end()) {
                entity = found->second;
                despawn.Entity(entity);
                world.peers.erase(found);
            }
            disconnected.Send(PeerDisconnected{entity, event.userId, event.reason, {}});
        }
        for (const std::uint32_t id : server->ConnectionIds()) {
            if (world.peers.count(id) != 0) continue;
            const std::uint64_t userId = server->UserIdOf(id);
            // The connection's epoch (R2): a new connection's, or the one a client World set with its resync before
            // this server World was made.
            PeerLedger ledger;
            ledger.epoch = server->EpochOf(id);
            auto peer = spawn.New(Peer{id, userId, {}}, std::move(ledger));
            if (!peer) return Base::Err(std::move(peer).error());
            world.peers[id] = *peer;
            connected.Send(PeerConnected{*peer, userId});
        }
        return {};
    }
    const ClientSession* client = service.Client();
    if (client == nullptr) return {};
    const ClientState state = client->State();
    if ((state == ClientState::Connected || state == ClientState::Interrupted) && world.clientPeer == 0) {
        auto peer = spawn.New(Peer{client->Accepted()->connectionId, client->Request().userId, {}}, PeerLedger{});
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

} // namespace

void NetModule::Configure(Orchestration::ProductSetup& setup) {
    const NetProductOptions* given = setup.Options<NetProductOptions>();
    if (given == nullptr) return; // G-1: no settings, nothing made, no socket opened
    NetProductOptions options = *given;
    // The transport's channels are the net Module's own: replication's reliable messages, then its values.
    options.session.transport.channels = {{ChannelKind::OrderedReliable, 1}, {ChannelKind::Unreliable, 0}};
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
    // The table NetEntry appends to, in every World the Module is built into (it registers nothing in the kernel).
    NetRegistrations& table = setup.Buffer().Create<NetRegistrations>();
    const NetWorldOptions* role = setup.Options<NetWorldOptions>();
    if (role == nullptr) return; // G-1: a World without a role registers nothing
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Peer>({.name = "Peer"});
    r.Component<PeerLedger>({.name = "PeerLedger"});
    r.Component<PeerControls>({.name = "PeerControls"});
    r.Component<PeerVisibility>({.name = "PeerVisibility"});
    r.Component<NetPriority>({.name = "NetPriority"});
    table.role = role->role;
    r.Resource<NetWorldLink>({.name = "NetWorldLink"});
    r.Resource<NetWorldRole>({.name = "NetWorldRole"});
    r.Message<PeerConnected>({.name = "PeerConnected"});
    r.Message<PeerDisconnected>({.name = "PeerDisconnected"});
    r.Message<NetLinkFailed>({.name = "NetLinkFailed"});
    r.AddSystem<&ReceiveNet>(Detail::MountedAt("ReceiveNet", Orchestration::DefaultSchedules::first));
    Kernel::SystemOptions send = Detail::MountedAt("SendNet", Orchestration::DefaultSchedules::last);
    if (role->idRange) {
        r.AddSystem<&SendNetGivenRange>(std::move(send));
    } else {
        r.AddSystem<&SendNet>(std::move(send));
    }
    const std::string reason = "the net Module's apply point: each end runs its own connection layer";
    Detail::MarkOneEnd(setup, "ReceiveNet", reason);
    Detail::MarkOneEnd(setup, "SendNet", reason);
    if (role->role == NetRole::Server) {
        r.Resource<NetOutbox>({.name = "NetOutbox"});
        Kernel::SystemOptions out = Detail::MountedAt("ReplicateOut", Orchestration::DefaultSchedules::last);
        out.mounts.front().before.push_back("SendNet");
        r.AddSystem<&Detail::ReplicateOut>(std::move(out));
        Detail::MarkOneEnd(setup, "ReplicateOut", "the net Module reads the server's World for its clients");
        Kernel::SystemOptions inputs = Detail::MountedAt("ReceiveInputs", Orchestration::DefaultSchedules::first);
        inputs.mounts.front().after.push_back("ReceiveNet");
        r.AddSystem<&Detail::ReceiveInputs>(std::move(inputs));
        Detail::MarkOneEnd(setup, "ReceiveInputs", "the net Module fills the Peers' input buffers at the server");
    } else {
        r.Resource<NetReplicaState>({.name = "NetReplicaState"});
        r.Message<ServerWorldReplaced>({.name = "ServerWorldReplaced"});
        Kernel::SystemOptions apply = Detail::MountedAt("ApplyReplication", Orchestration::DefaultSchedules::first);
        apply.mounts.front().after.push_back("ReceiveNet");
        r.AddSystem<&Detail::ApplyReplication>(std::move(apply));
        r.AddSystem<&Detail::NotifyReplica>(Detail::MountedAt("NotifyReplica", Orchestration::DefaultSchedules::preUpdate));
        // The clock sync (D-9): a Tick writer of the time Module's list (WT-5), at the client only.
        r.Resource<ClockSync>({.name = "ClockSync"});
        Kernel::SystemOptions clock = Detail::MountedAt("SyncClock", Orchestration::DefaultSchedules::last);
        clock.mounts.front().before.push_back("SendInputs");
        r.AddSystem<&Detail::SyncClock>(std::move(clock));
        WorldTime::WorldTimeModule::AddTickWriter(setup, "SyncClock");
        Detail::MarkOneEnd(setup, "SyncClock", "the net Module's clock sync moves the client's Tick toward the Match's");
        Kernel::SystemOptions inputs = Detail::MountedAt("SendInputs", Orchestration::DefaultSchedules::last);
        inputs.mounts.front().before.push_back("SendNet");
        r.AddSystem<&Detail::SendInputs>(std::move(inputs));
        Detail::MarkOneEnd(setup, "SendInputs", "the net Module sends the client's inputs");
        // It writes the replicated components (decision data at K2) at the client only.
        Detail::MarkOneEnd(setup, "ApplyReplication", "the net Module writes the server's values at the client");
    }

    NetService* service = setup.ProductBuffer().Find<NetService>();
    const NetRole worldRole = role->role;
    const std::optional<IdRange> givenRange = role->idRange;
    const auto* authority = setup.Options<Authority::AuthorityOptions>();
    const bool strategy = authority != nullptr && authority->strategy != nullptr;
    // K3 (D-5): a client World with a rollback strategy writes what rolls back into the rollback Module's inbox.
    const auto* rollbackOptions = setup.Options<Rollback::RollbackOptions>();
    const std::shared_ptr<Rollback::RollbackStrategy> rollbackStrategy =
        worldRole == NetRole::Client && rollbackOptions != nullptr ? rollbackOptions->strategy : nullptr;
    setup.Prepare([service, worldRole, givenRange, strategy, rollbackStrategy, &table](Kernel::World& world)
                      -> Base::Result<void, Orchestration::OrchestrationError> {
        // Check reported a World with a role and no NetService (NT-E1): no World is built then.
        NetWorldLink link;
        link.service = service;
        link.role = worldRole;
        link.idRange = givenRange;
        link.registry = std::make_shared<const NetRegistry>(table.registry);
        link.valueBytesPerTick = table.valueBytesPerTick;
        if (rollbackStrategy) {
            link.rollsBack = true;
            link.rollbackDepth = rollbackStrategy->DefaultInputLength();
            const auto* rollback = world.Resource<Rollback::RollbackRegistry>();
            for (const auto& entry : link.registry->components) {
                const Rollback::DataRecord* record = rollback != nullptr ? rollback->Data(entry->key) : nullptr;
                link.rollbackComponents.push_back(record != nullptr && record->kind == Rollback::DataKind::Component);
            }
            for (const auto& entry : link.registry->resources) {
                const Rollback::DataRecord* record = rollback != nullptr ? rollback->Data(entry->key) : nullptr;
                link.rollbackResources.push_back(record != nullptr && record->kind == Rollback::DataKind::Resource);
            }
        }
        if (strategy) {
            auto fingerprint = Authority::ConsistencyFingerprintOf(world);
            if (fingerprint) link.fingerprint = fingerprint->Text();
        }
        world.SetResource(NetWorldRole{worldRole});
        if (service != nullptr) {
            if (worldRole == NetRole::Server) {
                service->BeginServerWorld();
                world.SetResource(NetOutbox{});
            } else {
                NetReplicaState state;
                // B1, R2: the epoch grows with every client World of the Product. A new connection carries it in its
                // request; a World on a connection made for an earlier World asks for a resync (below).
                const std::uint32_t clientWorlds = service->BeginClientWorld();
                state.epoch = clientWorlds - 1;
                world.SetResource(std::move(state));
                world.SetResource(ClockSync{});
            }
            const auto* fixed = world.Resource<WorldTime::FixedTime>();
            const auto* allotter = world.Resource<EntityIds::RangeAllotter>();
            const IdRange range = allotter != nullptr ? IdRange{allotter->first, allotter->last} : IdRange{};
            if (fixed != nullptr) service->SetFacts(Facts(link, *fixed, range), service->Now(), true);
            if (worldRole == NetRole::Client) {
                const ClientSession* client = service->Client();
                auto* state = world.ResourceMut<NetReplicaState>();
                state->resyncNeeded = client != nullptr && client->Request().epoch != state->epoch;
            }
        }
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
