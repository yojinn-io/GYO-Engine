#pragma once

// Replication from the server to clients (declaration D-4, D-5, D-13, C-MS1; Design Invariants draft in
// docs/architecture/target/v2/net.zh-Hant.md, section 8).
//
//   NetEntry(setup).Replicate<T>(name, codec)        a component the server sends (the author registers T itself)
//   NetEntry(setup).ServerMessage<M>(name, codec)    a World message the server sends, bound to a structure tick
//   OnServer(), OnClient()                           run conditions for logic of one end (D-13)
//
// The wire id is the registration order; the protocol hash (compared in the handshake) is made of the names, the
// order and the codec versions, so two ends that register differently do not connect (ProtocolMismatch).
//
// Every tick and every Peer the server sends (ReplicateOut, Main/Last):
//   - one structure update when something changed (spawn, despawn, a component added or removed; the values of an
//     entity whose structure changed go with it), Ordered Reliable, applied whole;
//   - the values changed since that Peer last confirmed them, Unreliable, in parts of whole entities, each with the
//     tick, "parts of this tick" and the structure tick it needs; an empty part when nothing changed (C-RP4).
// The client (ApplyReplication, Main/First) applies a structure update whole, writes values once the structure they
// need is applied, and confirms each value part it processed (Ordered Reliable). Only that confirmation moves the
// Peer's ledger on; the packet acks do not.

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/net/Codec.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Net {

using ReplicaWorld = Kernel::ExclusiveWorld<>;

// The World's net role, for the run conditions (registered only in a World with a role).
struct NetWorldRole final {
    NetRole role = NetRole::Server;
};
inline void HashValue(Kernel::Hasher& h, const NetWorldRole& v) { HashValue(h, v.role); }

// What applying one received value did.
enum class ApplyOutcome : std::uint8_t { Applied = 1, Malformed, Rejected, Missing, Failed };
enum class ApplyMode : std::uint8_t {
    // SpawnWithId with this value (the first component of a spawn).
    Spawn = 1,
    // Overwrite when present, insert when not (a structure update).
    Set,
    // Overwrite when present only (a value message: the structure decides what an entity has).
    SetIfPresent,
};

// One replicated component, type-erased where its type is known (Replicate<T>).
struct ComponentEntry final {
    std::string name;
    std::uint32_t version = 1;
    std::uint16_t wire = 0;
    Kernel::TypeKey key;
    // Every cell of the type: (entity, changed stamp).
    std::function<void(ReplicaWorld&, const std::function<void(Kernel::EntityId, Kernel::ChangeStamp)>&)> visit;
    // Writes the entity's value; false when it has none.
    std::function<bool(ReplicaWorld&, Kernel::EntityId, NetWriter&)> encode;
    // Decodes the value and writes it into the World.
    std::function<ApplyOutcome(ReplicaWorld&, Kernel::EntityId, std::span<const std::byte>, ApplyMode)> apply;
    // Removes the entity's component; false when it had none.
    std::function<bool(ReplicaWorld&, Kernel::EntityId)> remove;
};

struct MessageEntryBase {
    std::string name;
    std::uint32_t version = 1;
    std::uint16_t wire = 0;
    Kernel::TypeKey key;
    virtual ~MessageEntryBase() = default;
};

template <class M>
struct MessageEntry final : MessageEntryBase {
    Codec<M> codec;
    // The Peer entity to send one message to; nullopt: every Peer.
    std::function<std::optional<Kernel::EntityId>(const M&)> recipient;
};

// The registrations of one World, in registration order.
struct NetRegistry final {
    std::vector<std::shared_ptr<const ComponentEntry>> components;
    std::vector<std::shared_ptr<const MessageEntryBase>> messages;

    // The protocol hash: names, order and codec versions.
    [[nodiscard]] std::uint64_t Hash() const;
    template <class M>
    [[nodiscard]] const MessageEntry<M>* Message() const {
        for (const auto& entry : messages) {
            if (entry->key == Kernel::KeyOf<M>()) return static_cast<const MessageEntry<M>*>(entry.get());
        }
        return nullptr;
    }
};

// The table of one World build (in that build's ModuleBuffer; NetModule makes it in every World it is built into).
struct NetRegistrations final {
    std::optional<NetRole> role;
    NetRegistry registry;
};

// A message the server collected this frame (CollectServerMessages<M>), sent by ReplicateOut after the structure.
struct OutgoingServerMessage final {
    std::uint16_t wire = 0;
    std::optional<Kernel::EntityId> recipient;
    std::vector<std::byte> bytes;
};
struct NetOutbox final {
    std::vector<OutgoingServerMessage> messages;
};
inline void HashValue(Kernel::Hasher&, const NetOutbox&) {}

// The ledger of one Peer (declaration D-5; on the Peer entity, NT-10): what that client has been sent and has
// confirmed. It decides what to send; it is not a copy of the game's state.
struct LedgerEntity final {
    // The replicated components the client has (bit = wire id).
    std::uint64_t mask = 0;
    // The World stamp of the newest snapshot of this entity the client confirmed (or that went reliably).
    Kernel::ChangeStamp confirmedStamp = 0;
};
struct PendingPart final {
    Kernel::ChangeStamp stamp = 0;
    std::vector<Kernel::EntityId> entities;
};
struct PeerLedger final {
    std::map<Kernel::EntityId, LedgerEntity> entities;
    // Value parts sent and not confirmed, by (tick << 16) | part.
    std::map<std::uint64_t, PendingPart> pending;
    std::uint32_t lastStructureTick = 0;
    std::optional<std::uint32_t> lastSentTick;
    // Raised by the client's resync (its World was replaced): older messages are ignored there.
    std::uint32_t epoch = 0;
    // Counts (G-5 item 4).
    std::uint64_t structuresSent = 0;
    std::uint64_t valuePartsSent = 0;
    std::uint64_t serverMessagesSent = 0;
    std::uint64_t confirmationsReceived = 0;
};
inline void HashValue(Kernel::Hasher&, const PeerLedger&) {}

// Why the client dropped a replication message or value (D-12).
enum class ReplicaDrop : std::uint8_t {
    // Of an older server World (generation).
    StaleGeneration = 0,
    // Of an earlier resync of this client.
    StaleEpoch,
    // Cannot be read.
    Malformed,
    // SpawnWithId refused the id (it is in this World's own range, or alive).
    SpawnRejected,
    // A value for an entity or component this World does not have.
    MissingTarget,
    // A value older than one already written for that entity and component.
    StaleValue,
};
inline constexpr std::size_t kReplicaDropCount = 6;

// The client's receiving side (a Resource of a client World).
struct NetReplicaState final {
    std::optional<std::uint32_t> generation;
    std::uint32_t epoch = 0;
    bool resyncNeeded = false;
    // The tick of the last structure update applied (they arrive in order).
    std::uint32_t appliedStructureTick = 0;
    struct Part {
        std::uint32_t tick = 0;
        std::uint32_t requiredStructure = 0;
        std::uint16_t part = 0;
        std::uint16_t partCount = 0;
        std::vector<std::byte> entries;
    };
    std::vector<Part> pendingValues;
    struct TickParts {
        std::uint16_t count = 0;
        std::uint16_t received = 0;
    };
    std::map<std::uint32_t, TickParts> ticks;
    // The newest value or structure tick written into the World.
    std::uint32_t maxAppliedTick = 0;
    // The newest tick C for which D-7's two conditions hold (every value part of C processed, the structure they
    // need applied); nullopt before the first.
    std::optional<std::uint32_t> latestConfirmed;
    std::uint64_t confirmedTicks = 0;
    // The tick of the newest value or structure written per (entity, wire).
    std::map<std::pair<Kernel::EntityId, std::uint16_t>, std::uint32_t> written;
    struct Ready {
        std::uint16_t wire = 0;
        std::vector<std::byte> bytes;
    };
    std::vector<Ready> readyMessages;
    // Server messages that arrived before the structure tick they need (kept until it is applied).
    struct Held {
        std::uint32_t requiredStructure = 0;
        Ready message;
    };
    std::vector<Held> heldMessages;
    std::vector<std::uint32_t> replacedGenerations;
    // Counts (G-5 item 4).
    std::uint64_t structuresApplied = 0;
    std::uint64_t valuePartsProcessed = 0;
    std::uint64_t valuePartsReceived = 0;
    std::uint64_t serverMessagesReceived = 0;
    std::uint64_t serverMessagesDelivered = 0;
    std::uint64_t confirmationsSent = 0;
    std::array<std::uint64_t, kReplicaDropCount> drops{};
};
inline void HashValue(Kernel::Hasher&, const NetReplicaState&) {}

// A retained message on a client: the server's World was replaced (a new generation). Whether to replace the
// client World is the product's call; when it does, the new World asks the server for everything again.
struct ServerWorldReplaced final {
    std::uint32_t generation = 0;
};
inline void HashValue(Kernel::Hasher& h, const ServerWorldReplaced& v) { HashValue(h, v.generation); }

// A retained message: the Link failed (E2; the World goes on). failures is the running count.
struct NetLinkFailed final {
    std::uint64_t failures = 0;
    NetFailureCode code = NetFailureCode::LinkFailed;
    std::string message;
};
inline void HashValue(Kernel::Hasher& h, const NetLinkFailed& v) {
    HashValue(h, v.failures);
    HashValue(h, v.code);
    HashValue(h, v.message);
}

// The World's way to the Product's NetService, the registrations and the World side of the connections (a
// Resource the prepare function puts there: O-15).
struct NetWorldLink final {
    NetService* service = nullptr;
    NetRole role = NetRole::Server;
    std::shared_ptr<const NetRegistry> registry;
    std::optional<std::string> fingerprint;
    std::optional<IdRange> idRange;
    // Server: connection id -> Peer entity. Client: its one Peer entity (0: none yet) and whether its end was
    // reported.
    std::map<std::uint32_t, Kernel::EntityId> peers;
    Kernel::EntityId clientPeer = 0;
    bool clientEndReported = false;
    std::uint64_t reportedLinkFailures = 0;
};
inline void HashValue(Kernel::Hasher&, const NetWorldLink&) {}

namespace Detail {

using SystemResult = Base::Result<void, Kernel::KernelError>;

inline Kernel::SystemOptions MountedAt(std::string name, const std::string_view stage) {
    Kernel::SystemOptions options;
    options.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = std::string(Orchestration::DefaultSchedules::frame);
    mount.stage = std::string(stage);
    options.mounts.push_back(std::move(mount));
    return options;
}

// Server, Main/PostUpdate: reads the World's M with its own cursor and queues each for ReplicateOut.
template <class M>
SystemResult CollectServerMessages(Kernel::MessageReader<M> reader, Kernel::Res<NetWorldLink> link,
                                   Kernel::ResMut<NetOutbox> outbox) {
    const MessageEntry<M>* entry = link->registry->template Message<M>();
    for (const M* message : reader.Read()) {
        OutgoingServerMessage out;
        out.wire = entry->wire;
        if (entry->recipient) out.recipient = entry->recipient(*message);
        NetWriter writer(out.bytes);
        entry->codec.encode(writer, *message);
        outbox->messages.push_back(std::move(out));
    }
    return {};
}

// Client, Main/PreUpdate: writes the M that arrived (after the structure they need) into the World's Message<M>.
template <class M>
SystemResult DeliverServerMessages(Kernel::MessageWriter<M> writer, Kernel::Res<NetWorldLink> link,
                                   Kernel::ResMut<NetReplicaState> state) {
    const MessageEntry<M>* entry = link->registry->template Message<M>();
    auto& ready = state->readyMessages;
    for (auto it = ready.begin(); it != ready.end();) {
        if (it->wire != entry->wire) {
            ++it;
            continue;
        }
        M message{};
        NetReader reader(it->bytes);
        if (entry->codec.decode(reader, message) && reader.Remaining() == 0) {
            writer.Send(std::move(message));
            ++state->serverMessagesDelivered;
        } else {
            ++state->drops[static_cast<std::size_t>(ReplicaDrop::Malformed)];
        }
        it = ready.erase(it);
    }
    return {};
}

// Marks a System authority.OneEndOnly in a World with an authority strategy; nothing in one without (K1).
void MarkOneEnd(Orchestration::WorldSetup& setup, const std::string& system, const std::string& reason);

bool IsServer(Kernel::Res<NetWorldRole> role);
bool IsClient(Kernel::Res<NetWorldRole> role);

} // namespace Detail

// D-13: the run conditions of logic that runs at one end. Register the same System at both ends with the condition,
// so the consistency fingerprints stay the same; it does not run at the other end. Only in a World with a net role.
[[nodiscard]] inline Kernel::ConditionRef OnServer() { return Kernel::Condition<&Detail::IsServer>("net.OnServer"); }
[[nodiscard]] inline Kernel::ConditionRef OnClient() { return Kernel::Condition<&Detail::IsClient>("net.OnClient"); }

// The author's way in: made from the WorldSetup of any Build of a World the net Module is built into (the
// product's own included); without the Module it is a GYO_ASSERT. Registers at both ends the same way.
class NetEntry final {
public:
    explicit NetEntry(Orchestration::WorldSetup& setup);

    // A component the server replicates (T is registered by the author). The codec writes each field; there is no
    // byte copy. A type without a default codec (not a number or an enum) and no codec given is a GYO_ASSERT.
    template <class T>
    void Replicate(std::string name, Codec<T> codec = DefaultCodec<T>()) {
        GYO_ASSERT(codec.encode && codec.decode && "NetEntry::Replicate<T>: give T's codec (encode and decode); no byte copy");
        GYO_ASSERT(table_->registry.components.size() < 64 && "NetEntry::Replicate: at most 64 replicated components");
        auto entry = std::make_shared<ComponentEntry>();
        entry->name = std::move(name);
        entry->version = codec.version;
        entry->wire = static_cast<std::uint16_t>(table_->registry.components.size());
        entry->key = Kernel::KeyOf<T>();
        entry->visit = [](ReplicaWorld& world, const std::function<void(Kernel::EntityId, Kernel::ChangeStamp)>& f) {
            world.template ReadColumn<T>([&f](Kernel::EntityId id, Kernel::ChangeStamp, Kernel::ChangeStamp changed, const T&) {
                f(id, changed);
            });
        };
        entry->encode = [encode = codec.encode](ReplicaWorld& world, Kernel::EntityId id, NetWriter& writer) {
            auto read = world.template Read<T>(id, [&](const T& value) { encode(writer, value); });
            return read.has_value() && *read;
        };
        entry->apply = [decode = codec.decode](ReplicaWorld& world, Kernel::EntityId id, std::span<const std::byte> bytes,
                                               ApplyMode mode) -> ApplyOutcome {
            T value{};
            NetReader reader(bytes);
            if (!decode(reader, value) || reader.Remaining() != 0) return ApplyOutcome::Malformed;
            if (mode == ApplyMode::Spawn) {
                auto spawned = world.SpawnWithId(id, std::move(value));
                if (!spawned) return ApplyOutcome::Failed;
                return spawned->has_value() ? ApplyOutcome::Rejected : ApplyOutcome::Applied;
            }
            // Modify sets the cell's changed stamp, so the game's Changed<T> sees the write.
            auto modified = world.template Modify<T>(id, [&value](T& cell) { cell = std::move(value); });
            if (!modified) return ApplyOutcome::Failed;
            if (*modified) return ApplyOutcome::Applied;
            if (mode == ApplyMode::SetIfPresent) return ApplyOutcome::Missing;
            auto inserted = world.Insert(id, std::move(value));
            if (!inserted) return ApplyOutcome::Failed;
            return *inserted ? ApplyOutcome::Applied : ApplyOutcome::Missing;
        };
        entry->remove = [](ReplicaWorld& world, Kernel::EntityId id) {
            auto removed = world.template Remove<T>(id);
            return removed.has_value() && *removed;
        };
        table_->registry.components.push_back(std::move(entry));
    }

    // A World message M the server sends to every Peer (or to the one recipient names), after the structure update
    // of its tick; the client writes it into its Message<M> once that structure is applied. Not counted for the
    // confirmation point (D-7). M is registered as a Message by the author at both ends.
    template <class M>
    void ServerMessage(std::string name, Codec<M> codec,
                       std::function<std::optional<Kernel::EntityId>(const M&)> recipient = {}) {
        GYO_ASSERT(codec.encode && codec.decode && "NetEntry::ServerMessage<M>: give M's codec (encode and decode)");
        auto entry = std::make_shared<MessageEntry<M>>();
        entry->name = name;
        entry->version = codec.version;
        entry->wire = static_cast<std::uint16_t>(table_->registry.messages.size());
        entry->key = Kernel::KeyOf<M>();
        entry->codec = std::move(codec);
        entry->recipient = std::move(recipient);
        table_->registry.messages.push_back(entry);
        if (table_->role == NetRole::Server) {
            setup_->Registrar().AddSystem<&Detail::CollectServerMessages<M>>(
                Detail::MountedAt("CollectServerMessages:" + name, Orchestration::DefaultSchedules::postUpdate));
        } else if (table_->role == NetRole::Client) {
            setup_->Registrar().AddSystem<&Detail::DeliverServerMessages<M>>(
                Detail::MountedAt("DeliverServerMessages:" + name, Orchestration::DefaultSchedules::preUpdate));
            // It writes the game's message, which may be decision data, at the client only.
            Detail::MarkOneEnd(*setup_, "DeliverServerMessages:" + name, "the net Module writes the server's messages at the client");
        }
    }

private:
    Orchestration::WorldSetup* setup_;
    NetRegistrations* table_;
};

namespace Detail {
// The Systems NetModule registers for replication.
SystemResult ReplicateOut(ReplicaWorld world);
SystemResult ApplyReplication(ReplicaWorld world);
SystemResult NotifyReplica(Kernel::ResMut<NetReplicaState> state, Kernel::MessageWriter<ServerWorldReplaced> replaced);
} // namespace Detail

} // namespace GYO::Engine::Net
