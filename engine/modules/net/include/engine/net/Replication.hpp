#pragma once

// Replication from the server to clients (declaration D-4, D-5, D-13, C-MS1; Design Invariants draft in
// docs/architecture/target/v2/net.zh-Hant.md, section 8).
//
//   NetEntry(setup).Replicate<T>(name, codec)        a component the server sends (the author registers T itself)
//   NetEntry(setup).ServerMessage<M>(name, codec)    a World message the server sends, bound to a structure tick
//   NetEntry(setup).ReplicateResource<R, V>(name, {codec, read, apply})
//                                                    a Resource the server sends as a value V (Q8 (b)); the client
//                                                    writes it with apply (by default SetResource<R>)
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

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/World.hpp"
#include "engine/net/Codec.hpp"
#include "engine/net/FlatMap.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/world_time/TimeTypes.hpp"

namespace GYO::Engine::Net {

using ReplicaWorld = Kernel::ExclusiveWorld<>;

// The World's net role, for the run conditions (registered only in a World with a role).
struct NetWorldRole final {
    NetRole role = NetRole::Server;
};
inline void HashValue(Kernel::Hasher& h, const NetWorldRole& v) { HashValue(h, v.role); }

// What applying one received value did.
// Unchanged: the value is the one the World has (compared through the codec's bytes; not written, so the cell's
// changed stamp stays: CCR-8).
enum class ApplyOutcome : std::uint8_t { Applied = 1, Malformed, Rejected, Missing, Failed, Unchanged };
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

// One replicated Resource (Q8 (b)), type-erased where its types are known (ReplicateResource<R, V>).
struct ResourceEntry final {
    std::string name;
    std::uint32_t version = 1;
    std::uint16_t wire = 0;
    Kernel::TypeKey key;
    // Server: the Resource's changed stamp; nullopt when it is absent.
    std::function<std::optional<Kernel::ChangeStamp>(ReplicaWorld&)> stamp;
    // Server: writes the value to send; false when there is none (absent, or the read gave none).
    std::function<bool(ReplicaWorld&, NetWriter&)> encode;
    // Client: decodes the value and writes it with the registration's apply.
    std::function<Base::Result<ApplyOutcome, Kernel::KernelError>(ReplicaWorld&, std::span<const std::byte>)> apply;
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

// One input type (D-11), type-erased where its type is known (NetEntry::Input<T>).
struct InputEntry final {
    std::string name;
    std::uint32_t version = 1;
    std::uint16_t wire = 0;
    Kernel::TypeKey key;
    std::uint16_t redundancy = 5;
    // Client: writes the Peer's last `redundancy` slots up to its newest tick into one input message; false when the
    // Peer has no input yet.
    std::function<bool(ReplicaWorld&, Kernel::EntityId peer, NetWriter&)> encodeRecent;
    // Server: reads one input message into the Peer's buffer, within the window of the server's tick.
    // false when the message cannot be read (counted Malformed on the Peer's ledger).
    std::function<bool(ReplicaWorld&, Kernel::EntityId peer, NetReader&, std::uint32_t serverTick)> fill;
    // Server: gives the Peer its buffer of T when it has none (from the first frame of the connection, so every step
    // the entry runs is counted, consumed or missing).
    std::function<void(ReplicaWorld&, Kernel::EntityId peer)> ensure;
    // The InputOptions<T> given (read back by ApplyInput<T>).
    std::shared_ptr<const void> options;
};

// The registrations of one World, in registration order.
struct NetRegistry final {
    std::vector<std::shared_ptr<const ComponentEntry>> components;
    std::vector<std::shared_ptr<const ResourceEntry>> resources;
    std::vector<std::shared_ptr<const MessageEntryBase>> messages;
    std::vector<std::shared_ptr<const InputEntry>> inputs;

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
    // Kept until a frame whose tick ReplicateOut has not sent yet takes them (B2: a frame without a fixed step sends
    // nothing, and its messages wait for the next tick).
    std::vector<OutgoingServerMessage> messages;
    // The tick the messages last went out with, one for the whole World (R1: a Peer that connects in a frame
    // without a step sends its own structure and values that frame, but no messages).
    std::optional<std::uint32_t> sentTick;
};
inline void HashValue(Kernel::Hasher&, const NetOutbox&) {}

// Why a replication message or value was dropped (D-12): at the client (NetReplicaState), and at the server for what
// a client sent (PeerLedger: Malformed, StaleEpoch, StaleGeneration).
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
    // The wire ids of the Resources this part carried.
    std::vector<std::uint16_t> resources{};
};
struct PeerLedger final {
    FlatMap<Kernel::EntityId, LedgerEntity> entities;
    // Value parts sent and not confirmed, by (tick << 16) | part.
    FlatMap<std::uint64_t, PendingPart> pending;
    // Per replicated Resource (by wire id): the World stamp of the newest value the client confirmed; nullopt before.
    std::vector<std::optional<Kernel::ChangeStamp>> resources;
    // The tick of the newest structure update sent; nullopt before the first (S1: tick 0 is a structure tick too).
    std::optional<std::uint32_t> lastStructureTick;
    std::optional<std::uint32_t> lastSentTick;
    // The client World's epoch, set by its resync (its World was replaced): older messages are dropped there.
    std::uint32_t epoch = 0;
    // Counts (G-5 item 4).
    std::uint64_t structuresSent = 0;
    std::uint64_t valuePartsSent = 0;
    std::uint64_t serverMessagesSent = 0;
    std::uint64_t confirmationsReceived = 0;
    // Replicated Resource values sent (Q8 (b)).
    std::uint64_t resourceValuesSent = 0;
    // Reliable sends the connection refused (queue full, too large): the ledger did not move on for them.
    std::uint64_t reliableRejected = 0;
    // What this client sent that the server dropped (confirmations, resyncs, inputs), by reason.
    std::array<std::uint64_t, kReplicaDropCount> drops{};
};
inline void HashValue(Kernel::Hasher&, const PeerLedger&) {}


// The client's receiving side (a Resource of a client World).
struct NetReplicaState final {
    std::optional<std::uint32_t> generation;
    // This client World's epoch: the NetService's count of client Worlds minus one (B1: it grows with every client
    // World of the Product and is never taken from a message).
    std::uint32_t epoch = 0;
    bool resyncNeeded = false;
    // The tick of the last structure update applied (they arrive in order); nullopt before the first.
    std::optional<std::uint32_t> appliedStructureTick;
    struct Part {
        std::uint32_t tick = 0;
        std::optional<std::uint32_t> requiredStructure;
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
    // The tick of the newest value written per replicated Resource (wire).
    std::map<std::uint16_t, std::uint32_t> resourcesWritten;
    struct Ready {
        std::uint16_t wire = 0;
        std::vector<std::byte> bytes;
    };
    std::vector<Ready> readyMessages;
    // Server messages that arrived before the structure tick they need (kept until it is applied).
    struct Held {
        std::optional<std::uint32_t> requiredStructure;
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
    // The client's tick of its last input message (one a tick).
    std::optional<std::uint64_t> lastInputTick;
    // Values that were the World's already: not written (CCR-8).
    std::uint64_t unchangedNotWritten = 0;
    // Replicated Resource values handed to their apply (Q8 (b)), and of those the default apply found unchanged.
    std::uint64_t resourceValuesApplied = 0;
    std::uint64_t resourceValuesUnchanged = 0;
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

// The time the player saw when it gave an input (C-IN2): a Match tick and the fraction towards the next one. The
// fraction is one byte, as lightyear's Overstep goes on the wire (crates/core/core/src/time.rs:25-33, 90-104: a
// U0F16 kept, a U0F8 sent; its note: about 0.2 % quantization for one byte).
struct ViewTime final {
    std::uint32_t tick = 0;
    std::uint8_t overstep = 0;
    friend constexpr bool operator==(const ViewTime&, const ViewTime&) noexcept = default;
};

// The counts of one Peer's input buffer (D-12; G-6).
struct InputCounts final {
    // Slots read from input messages, and what became of each.
    std::uint64_t slotsReceived = 0;
    std::uint64_t stored = 0;
    std::uint64_t duplicates = 0;
    // At or before the tick already consumed.
    std::uint64_t late = 0;
    // Beyond the server's tick + window.
    std::uint64_t outsideWindow = 0;
    // Steps the entry ran: with the slot of that tick, or holding the last one (missing).
    std::uint64_t consumed = 0;
    std::uint64_t missing = 0;
    // Stored and passed by the server's tick without the entry consuming them (no controlled entity yet).
    std::uint64_t expired = 0;
    // Input messages sent (client) or read (server).
    std::uint64_t messages = 0;
};

// One Peer's inputs of type T by tick (D-11; on the Peer entity at both ends: NT-10). The client's product puts its
// local input in with Set; the server fills it from the client's messages. The entry writes slot n into the
// controlled entity's T in step n. lightyear's InputBuffer (crates/inputs/inputs/src/input_buffer.rs) is the shape.
template <class T>
struct PeerInput final {
    struct Slot {
        T value{};
        ViewTime view;
    };
    FlatMap<std::uint32_t, Slot> slots;
    // The newest tick consumed (the server's window starts after it), and the last value written (held when a
    // slot is missing).
    std::optional<std::uint32_t> consumedThrough;
    std::optional<T> last;
    ViewTime lastView;
    std::uint32_t lastSentTick = 0;
    InputCounts counts;

    // The client's product: the input of tick (a Match tick) and the time the player saw.
    void Set(const std::uint32_t tick, T value, const ViewTime view = {}) { slots[tick] = Slot{std::move(value), view}; }
};
template <class T>
inline void HashValue(Kernel::Hasher&, const PeerInput<T>&) {}
static_assert(Kernel::Detail::kStorable<PeerInput<int>>, "PeerInput<T> is an ECS component: nothrow movable (W-8)");

// The entity a Peer controls (written by the product; the network does not know the player, P12).
struct PeerControls final {
    Kernel::EntityId entity = 0;
};
inline void HashValue(Kernel::Hasher& h, const PeerControls& v) { HashValue(h, v.entity); }

// W-8 for every ECS component of the net Module, checked where they are defined. libc++ and libstdc++ move std::map
// without throwing and MSVC does not, so a node container in a component compiles here and fails only on Windows:
// the components hold FlatMap and std::vector instead (FlatMap.hpp), and these asserts keep the rule visible.
static_assert(Kernel::Detail::kStorable<PeerLedger>, "PeerLedger is an ECS component: nothrow movable (W-8)");
static_assert(Kernel::Detail::kStorable<PeerControls>, "PeerControls is an ECS component: nothrow movable (W-8)");
static_assert(Kernel::Detail::kStorable<FlatMap<std::uint32_t, int>>, "FlatMap must stay nothrow movable (W-8)");

// The options of an input type.
template <class T>
struct InputOptions final {
    Codec<T> codec = DefaultCodec<T>();
    // Slots repeated in each message (lightyear crates/inputs/inputs/src/config.rs:16-19, 50: packet_redundancy 5).
    std::uint16_t redundancy = 5;
    // The server takes slots in (consumed, tick + window] (the rollback Module's InputHistoryLength::Long; lightyear
    // keeps +-64 ticks).
    std::uint32_t window = 64;
    // Applied to the last value when a slot is missing and it is held: clears the one-shot fields (a press) so a
    // held input does not fire again (C-IN3). None: held as it is.
    std::function<void(T&)> held;
};

namespace Detail {

using SystemResult = Base::Result<void, Kernel::KernelError>;

// Prunes a buffer to the window behind its newest slot (both ends).
template <class T>
void PruneInputs(PeerInput<T>& input, const std::uint32_t keep) {
    if (input.slots.empty()) return;
    const std::uint32_t newest = input.slots.rbegin()->first;
    while (!input.slots.empty() && input.slots.begin()->first + keep < newest) input.slots.erase(input.slots.begin());
}

// The entry System of input T (D-11): the same name, access and mount at both ends (so the consistency fingerprints
// agree). In step n it writes each Peer's slot n into the entity the Peer controls; a missing slot holds the last
// value (counted). Added through authority.Ingress where the World has an authority strategy (it is skipped while
// resimulating; the rollback Module writes the recorded inputs back), with AddSystem otherwise (K1).
template <class T>
SystemResult ApplyInput(Kernel::Query<Kernel::Write<PeerInput<T>>, Kernel::Read<PeerControls>> peers,
                        Kernel::Query<Kernel::Write<T>> targets, Kernel::Res<WorldTime::Tick> tick,
                        Kernel::Res<NetWorldLink> link) {
    const auto now = static_cast<std::uint32_t>(tick->value);
    const InputOptions<T>* options = nullptr;
    for (const auto& input : link->registry->inputs) {
        if (input->key == Kernel::KeyOf<T>()) options = static_cast<const InputOptions<T>*>(input->options.get());
    }
    peers.ForEach([&](Kernel::EntityId, Kernel::Mut<PeerInput<T>> input, const PeerControls& controls) {
        if (controls.entity == 0 || !targets.Contains(controls.entity)) return;
        auto& buffer = input.Get();
        T value{};
        const auto slot = buffer.slots.find(now);
        if (slot != buffer.slots.end()) {
            value = slot->second.value;
            buffer.lastView = slot->second.view;
            ++buffer.counts.consumed;
        } else {
            // Hold the last value, its one-shot fields cleared (a press fires once: C-IN3).
            if (buffer.last) value = *buffer.last;
            if (options != nullptr && options->held) options->held(value);
            ++buffer.counts.missing;
        }
        buffer.last = value;
        buffer.consumedThrough = now;
        *targets.template GetMut<T>(controls.entity) = value;
    });
    return {};
}


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
        entry->apply = [decode = codec.decode, encode = codec.encode](ReplicaWorld& world, Kernel::EntityId id,
                                                                      std::span<const std::byte> bytes, ApplyMode mode) -> ApplyOutcome {
            T value{};
            NetReader reader(bytes);
            if (!decode(reader, value) || reader.Remaining() != 0) return ApplyOutcome::Malformed;
            if (mode != ApplyMode::Spawn) {
                // CCR-8: the same value sent again is not written, so the cell's changed stamp says only that the
                // server changed it. Compared through the codec's bytes: T needs no ==.
                auto same = world.template Read<T>(id, [&](const T& now) {
                    std::vector<std::byte> current;
                    NetWriter currentWriter(current);
                    encode(currentWriter, now);
                    return current.size() == bytes.size() && std::equal(current.begin(), current.end(), bytes.begin());
                });
                if (same && same->has_value() && **same) return ApplyOutcome::Unchanged;
            }
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

    // A Resource R the server replicates as a value V (net-module declaration Q8 (b)); R is registered as a Resource by
    // the author at both ends. Every tick the server sends V when R was written since that Peer last confirmed it
    // (its changed stamp), in the first value part, under the same confirmation as the components. The client
    // writes it with apply once the structure the part needs is applied (K1, K2; K3 is NT-E5 until step 4).
    //   codec  how V goes on the wire (DefaultCodec<V> for a number or an enum);
    //   read   server: the value of R to send, nullopt for none; empty when V is R (R itself);
    //   apply  client: writes the value; empty when V is R: SetResource<R>, not written when the codec gives the
    //          same bytes as the value there (CCR-8). An apply of the author's own compares first as well (CCR-8).
    // The time Module's Resources are written every frame (their Changed() is always true): they are not replicated
    // and an author does not register them (each end keeps its own time, D-9).
    template <class R, class V = R>
    struct ResourceOptions final {
        Codec<V> codec = DefaultCodec<V>();
        std::function<std::optional<V>(const R&)> read;
        std::function<Kernel::SystemResult(ReplicaWorld&, const V&)> apply;
    };
    template <class R, class V = R>
    void ReplicateResource(std::string name, ResourceOptions<R, V> options = {}) {
        static_assert(std::is_default_constructible_v<V>, "NetEntry::ReplicateResource: the value type is decoded into a V{}");
        GYO_ASSERT(options.codec.encode && options.codec.decode &&
                   "NetEntry::ReplicateResource: give the value's codec (encode and decode); no byte copy");
        if constexpr (!std::is_same_v<R, V>) {
            GYO_ASSERT(options.read && options.apply &&
                       "NetEntry::ReplicateResource<R, V>: a value type other than the Resource needs read and apply");
        }
        GYO_ASSERT(table_->registry.resources.size() < 255 && "NetEntry::ReplicateResource: at most 255 replicated Resources");
        auto entry = std::make_shared<ResourceEntry>();
        entry->name = std::move(name);
        entry->version = options.codec.version;
        entry->wire = static_cast<std::uint16_t>(table_->registry.resources.size());
        entry->key = Kernel::KeyOf<R>();
        entry->stamp = [](ReplicaWorld& world) { return world.template ResourceStamp<R>(); };
        entry->encode = [encode = options.codec.encode, read = options.read](ReplicaWorld& world, NetWriter& writer) {
            auto written = world.template Read<R>([&](const R& resource) {
                if constexpr (std::is_same_v<R, V>) {
                    if (!read) {
                        encode(writer, resource);
                        return true;
                    }
                }
                const std::optional<V> value = read(resource);
                if (!value) return false;
                encode(writer, *value);
                return true;
            });
            return written && written->has_value() && **written;
        };
        entry->apply = [codec = options.codec, apply = options.apply](ReplicaWorld& world, std::span<const std::byte> bytes)
            -> Base::Result<ApplyOutcome, Kernel::KernelError> {
            std::optional<V> value{std::in_place};
            NetReader reader(bytes);
            if (!codec.decode(reader, *value) || reader.Remaining() != 0) return ApplyOutcome::Malformed;
            if (apply) {
                if (auto applied = apply(world, *value); !applied) return Base::Err(std::move(applied).error());
                return ApplyOutcome::Applied;
            }
            if constexpr (std::is_same_v<R, V>) {
                // CCR-8: compared through the codec's bytes, so R needs no ==.
                auto same = world.template Read<R>([&](const R& now) {
                    std::vector<std::byte> current;
                    NetWriter currentWriter(current);
                    codec.encode(currentWriter, now);
                    return current.size() == bytes.size() && std::equal(current.begin(), current.end(), bytes.begin());
                });
                if (!same) return Base::Err(std::move(same).error());
                if (same->has_value() && **same) return ApplyOutcome::Unchanged;
                if (same->has_value()) {
                    auto modified = world.template Modify<R>([&value](R& cell) { cell = std::move(*value); });
                    if (!modified) return Base::Err(std::move(modified).error());
                    return ApplyOutcome::Applied;
                }
                if (auto set = world.SetResource(std::move(*value)); !set) return Base::Err(std::move(set).error());
                return ApplyOutcome::Applied;
            } else {
                return ApplyOutcome::Failed;
            }
        };
        table_->registry.resources.push_back(std::move(entry));
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

    // An input (D-11): T is a component on the player's entity at both ends, written by the entry System
    // ApplyInput:<name> in step n from the slot n of the Peer that controls the entity (PeerControls). The client's
    // product puts its inputs into its own Peer's PeerInput<T> (LocalInput<T>); the client sends one message a tick
    // with the last `redundancy` slots, Unreliable; the server keeps each (Peer, tick) once. T is registered by the
    // author (with authority.Component<T>(name, {.input = true}) for a decision input).
    template <class T>
    void Input(std::string name, InputOptions<T> options = {}) {
        GYO_ASSERT(options.codec.encode && options.codec.decode && "NetEntry::Input<T>: give T's codec (encode and decode)");
        auto shared = std::make_shared<InputOptions<T>>(std::move(options));
        auto entry = std::make_shared<InputEntry>();
        entry->name = name;
        entry->version = shared->codec.version;
        entry->wire = static_cast<std::uint16_t>(table_->registry.inputs.size());
        entry->key = Kernel::KeyOf<T>();
        entry->redundancy = shared->redundancy;
        entry->options = shared;
        const InputOptions<T>* typed = shared.get();
        entry->encodeRecent = [typed](ReplicaWorld& world, Kernel::EntityId peer, NetWriter& writer) {
            auto written = world.template Modify<PeerInput<T>>(peer, [&](PeerInput<T>& input) {
                if (input.slots.empty()) return false;
                const std::uint32_t end = input.slots.rbegin()->first;
                const std::uint16_t count = typed->redundancy;
                writer.Value(end);
                writer.Value(count);
                for (std::uint32_t back = count; back-- > 0;) {
                    const auto slot = input.slots.find(end - back);
                    writer.Value(static_cast<std::uint8_t>(slot != input.slots.end() ? 1 : 0));
                    if (slot == input.slots.end()) continue;
                    writer.Value(slot->second.view.tick);
                    writer.Value(slot->second.view.overstep);
                    std::vector<std::byte> bytes;
                    NetWriter valueWriter(bytes);
                    typed->codec.encode(valueWriter, slot->second.value);
                    writer.Value(static_cast<std::uint16_t>(bytes.size()));
                    writer.Bytes(bytes);
                }
                ++input.counts.messages;
                Detail::PruneInputs(input, 2 * typed->window);
                return true;
            });
            return written.has_value() && written->has_value() && **written;
        };
        entry->ensure = [](ReplicaWorld& world, Kernel::EntityId peer) {
            const auto has = world.template Read<PeerInput<T>>(peer, [](const PeerInput<T>&) {});
            if (!(has && *has)) (void)world.Insert(peer, PeerInput<T>{});
        };
        entry->fill = [typed](ReplicaWorld& world, Kernel::EntityId peer, NetReader& reader, std::uint32_t serverTick) {
            const auto has = world.template Read<PeerInput<T>>(peer, [](const PeerInput<T>&) {});
            if (!(has && *has)) (void)world.Insert(peer, PeerInput<T>{});
            bool readable = true;
            (void)world.template Modify<PeerInput<T>>(peer, [&](PeerInput<T>& input) {
                std::uint32_t end = 0;
                std::uint16_t count = 0;
                if (!reader.Value(end) || !reader.Value(count)) {
                    readable = false;
                    return;
                }
                ++input.counts.messages;
                for (std::uint32_t back = count; back-- > 0;) {
                    std::uint8_t present = 0;
                    if (!reader.Value(present)) {
                        readable = false;
                        return;
                    }
                    if (present == 0) continue;
                    typename PeerInput<T>::Slot slot;
                    std::uint16_t length = 0;
                    std::span<const std::byte> bytes;
                    if (!reader.Value(slot.view.tick) || !reader.Value(slot.view.overstep) || !reader.Value(length) ||
                        !reader.Bytes(length, bytes)) {
                        readable = false;
                        return;
                    }
                    NetReader valueReader(bytes);
                    if (!typed->codec.decode(valueReader, slot.value)) {
                        readable = false;
                        return;
                    }
                    const std::uint32_t tick = end - back;
                    ++input.counts.slotsReceived;
                    // The window (consumed, serverTick + window]; each (Peer, tick) kept once (C-IN1, C-IN3).
                    if (input.consumedThrough && tick <= *input.consumedThrough) {
                        ++input.counts.late;
                    } else if (tick > serverTick + typed->window) {
                        ++input.counts.outsideWindow;
                    } else if (!input.slots.emplace(tick, std::move(slot)).second) {
                        ++input.counts.duplicates;
                    } else {
                        ++input.counts.stored;
                    }
                }
                if (input.consumedThrough) {
                    while (!input.slots.empty() && input.slots.begin()->first <= *input.consumedThrough) {
                        input.slots.erase(input.slots.begin());
                    }
                } else {
                    // Nothing consumed yet (no controlled entity, small thing 1): the slots the server's tick passed
                    // are expired, counted (stored = consumed + expired + still ahead).
                    while (!input.slots.empty() && input.slots.begin()->first <= serverTick) {
                        input.slots.erase(input.slots.begin());
                        ++input.counts.expired;
                    }
                }
            });
            return readable;
        };
        table_->registry.inputs.push_back(std::move(entry));
        if (!table_->role) return;
        setup_->Registrar().Component<PeerInput<T>>({.name = "PeerInput:" + name});
        Kernel::SystemOptions apply;
        apply.name = "ApplyInput:" + name;
        Kernel::Mount mount;
        mount.schedule = fixedSchedule_;
        mount.stage = std::string(Orchestration::DefaultSchedules::first);
        apply.mounts.push_back(std::move(mount));
        const auto* authority = setup_->Options<Authority::AuthorityOptions>();
        if (authority != nullptr && authority->strategy != nullptr &&
            setup_->Buffer().Find<Authority::AuthorityRegistrations>() != nullptr) {
            Authority::AuthorityEntry(*setup_).Ingress<&Detail::ApplyInput<T>>(std::move(apply));
        } else {
            setup_->Registrar().AddSystem<&Detail::ApplyInput<T>>(std::move(apply));
        }
    }

private:
    Orchestration::WorldSetup* setup_;
    NetRegistrations* table_;
    std::string fixedSchedule_ = std::string(Orchestration::DefaultSchedules::fixed);
};

// The client's own Peer's buffer of input T, made when it is not there yet (between two Runs, or from the product's
// code with the World); nullptr before the client's Peer exists.
template <class T>
[[nodiscard]] PeerInput<T>* LocalInput(Kernel::World& world) {
    const NetWorldLink* link = world.Resource<NetWorldLink>();
    if (link == nullptr || link->clientPeer == 0 || !world.Alive(link->clientPeer)) return nullptr;
    if (world.Get<PeerInput<T>>(link->clientPeer) == nullptr) world.Insert(link->clientPeer, PeerInput<T>{});
    return world.GetMut<PeerInput<T>>(link->clientPeer);
}

// The clock sync of a client World (D-9; 3b). Its constants follow lightyear's SyncConfig defaults
// (crates/core/sync/src/timeline/sync.rs:210-220) and its objective lightyear's input timeline
// (crates/core/sync/src/timeline/input.rs:628-629); the remote estimate is lightyear's RemoteTimeline
// (crates/core/sync/src/timeline/remote.rs:109-221): an offset between the Match's ticks and the local clock,
// smoothed by an EMA whose weight goes from 0.10 (jitter <= 1 ms) down to 0.02 (jitter >= 10 ms):
//   raw     = (Match tick of a newer packet + RTT/2 / step) - its arrival time / step
//   remote  = now / step + offset (offset: raw the first time, then offset + alpha x (raw - offset))
//   target  = remote + (RTT/2 + 4 x jitter) / step + 1 (jitter margin) + 1 + 1 (error margin)  [ticks]
//   error   = target - (Tick + overstep / step)
//   |error| > 10: JumpFixedSteps(round(error)) (also the first sync after connecting);
//   |error| > 1 three times in a row with one sign: speed 1.05 (behind) or 1 / 1.05 (ahead);
//   |error| <= 1: speed 1.
struct ClockSyncSettings final {
    double snapTicks = 10.0;
    double deadbandTicks = 1.0;
    std::uint8_t consecutive = 3;
    double speedup = 1.05;
    double jitterMultiple = 4.0;
    // jitter margin (1 tick) + the input's own tick (1) + the error margin (1 tick).
    double marginTicks = 3.0;
    // The EMA weights of the remote estimate and the jitter (ms) they go between.
    double maxAlpha = 0.10;
    double minAlpha = 0.02;
    double lowJitterMs = 1.0;
    double highJitterMs = 10.0;
};
struct ClockSync final {
    ClockSyncSettings settings;
    bool synced = false;
    std::uint8_t consecutive = 0;
    bool previousBehind = true;
    double lastError = 0.0;
    double lastTarget = 0.0;
    double scale = 1.0;
    std::uint64_t jumps = 0;
    std::int64_t lastJump = 0;
    std::uint64_t speedChanges = 0;
    // The remote estimate: the smoothed offset (ticks), and the last packet it took.
    bool haveOffset = false;
    double offset = 0.0;
    std::uint32_t lastStampTick = 0;
    NetTime lastStampAt{0};
};
inline void HashValue(Kernel::Hasher&, const ClockSync&) {}

namespace Detail {
// The Systems NetModule registers for replication and inputs.
SystemResult SyncClock(Kernel::ExclusiveWorld<Kernel::Writes<WorldTime::Tick, WorldTime::FixedTime, WorldTime::TimeSyncScale>> world);
SystemResult ReceiveInputs(ReplicaWorld world);
SystemResult SendInputs(ReplicaWorld world);
SystemResult ReplicateOut(ReplicaWorld world);
SystemResult ApplyReplication(ReplicaWorld world);
SystemResult NotifyReplica(Kernel::ResMut<NetReplicaState> state, Kernel::MessageWriter<ServerWorldReplaced> replaced);
} // namespace Detail

} // namespace GYO::Engine::Net
