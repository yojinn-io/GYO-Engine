#include "engine/net/Replication.hpp"

#include <algorithm>
#include <limits>
#include <bit>
#include <cmath>
#include <map>
#include <span>
#include <vector>
#include <string>
#include <utility>

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/world_time/FixedStep.hpp"
#include "engine/world_time/TimeTypes.hpp"

namespace GYO::Engine::Net {

std::uint64_t NetRegistry::Hash() const {
    // /3: the value parts carry the replicated Resources (Q8 (b)).
    std::string text = "gyo.net.registrations/3;";
    for (const auto& entry : components) text += "c:" + entry->name + ":" + std::to_string(entry->version) + ";";
    for (const auto& entry : resources) text += "r:" + entry->name + ":" + std::to_string(entry->version) + ";";
    for (const auto& entry : messages) text += "m:" + entry->name + ":" + std::to_string(entry->version) + ";";
    for (const auto& entry : inputs) {
        text += "i:" + entry->name + ":" + std::to_string(entry->version) + ":" + std::to_string(entry->redundancy) + ";";
    }
    return Base::Fnv1a64(text);
}

namespace Detail {
void MarkOneEnd(Orchestration::WorldSetup& setup, const std::string& system, const std::string& reason) {
    const auto* authority = setup.Options<Authority::AuthorityOptions>();
    if (authority == nullptr || authority->strategy == nullptr) return; // K1: the authority Module is not touched
    if (setup.Buffer().Find<Authority::AuthorityRegistrations>() == nullptr) return;
    Authority::AuthorityEntry(setup).OneEndOnly(system, reason);
}
} // namespace Detail

NetEntry::NetEntry(Orchestration::WorldSetup& setup) : setup_(&setup), table_(setup.Buffer().Find<NetRegistrations>()) {
    GYO_ASSERT(table_ != nullptr && "NetEntry: the net Module is not built into this World");
}

namespace Detail {

bool IsServer(Kernel::Res<NetWorldRole> role) { return role->role == NetRole::Server; }
bool IsClient(Kernel::Res<NetWorldRole> role) { return role->role == NetRole::Client; }

namespace {

// The kinds of replication's messages: the first byte of each. Structure, Confirm, ServerMessage and Resync go on the
// reliable channel, Values on the values channel. After the kind: the server World's generation and the client's
// resync epoch (u32 each).
enum class Kind : std::uint8_t { Structure = 1, Confirm = 2, ServerMessage = 3, Resync = 4, Values = 5, Inputs = 6 };

// A value part's payload budget: one part fits one packet without fragments at the default mtu (1170 bytes).
constexpr std::size_t kValuePartBudget = 1100;
// Value parts kept unconfirmed per Peer, by tick: older ones are forgotten (their entities are sent again).
constexpr std::uint32_t kPendingTicks = 256;

void Head(NetWriter& writer, const Kind kind, const std::uint32_t generation, const std::uint32_t epoch) {
    writer.Value(static_cast<std::uint8_t>(kind));
    writer.Value(generation);
    writer.Value(epoch);
}

// The replicated components of the World this frame (taken once, read for every Peer): the entities by id, each with
// its mask and the change stamp of each of its components, in contiguous arrays. A component's value is encoded the
// first time a Peer needs it and the bytes are shared by every Peer of the frame (the World does not change while
// ReplicateOut runs), so what goes on the wire is what encoding it for each Peer would give.
struct Snapshot {
    struct Entity {
        Kernel::EntityId id = 0;
        std::uint64_t mask = 0;
        // The index of its first slot: one slot per component it has, in wire order.
        std::uint32_t first = 0;
    };
    enum class Encoding : std::uint8_t { NotYet, Absent, Done };
    std::vector<Entity> entities;
    std::vector<Kernel::ChangeStamp> stamps;
    std::vector<Encoding> encoding;
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> lengths;
    std::vector<std::byte> bytes;

    [[nodiscard]] const Entity* Find(const Kernel::EntityId id) const {
        const auto it = std::lower_bound(entities.begin(), entities.end(), id,
                                         [](const Entity& entity, const Kernel::EntityId key) { return entity.id < key; });
        return it != entities.end() && it->id == id ? &*it : nullptr;
    }
    [[nodiscard]] static std::uint32_t Slot(const Entity& entity, const std::uint16_t wire) {
        const std::uint64_t below = wire == 0 ? 0 : (entity.mask & (~std::uint64_t{0} >> (64 - wire)));
        return entity.first + static_cast<std::uint32_t>(std::popcount(below));
    }
    // The encoded value of one component of one entity; false when the entity does not have it (nothing is sent).
    bool Encode(ReplicaWorld& world, const ComponentEntry& entry, const Entity& entity) {
        if (((entity.mask >> entry.wire) & 1u) == 0) return false;
        const std::uint32_t slot = Slot(entity, entry.wire);
        if (encoding[slot] == Encoding::NotYet) {
            const std::size_t start = bytes.size();
            NetWriter writer(bytes);
            if (entry.encode(world, entity.id, writer)) {
                encoding[slot] = Encoding::Done;
                offsets[slot] = static_cast<std::uint32_t>(start);
                lengths[slot] = static_cast<std::uint32_t>(bytes.size() - start);
            } else {
                encoding[slot] = Encoding::Absent;
                bytes.resize(start);
            }
        }
        return encoding[slot] == Encoding::Done;
    }
};

Snapshot Take(ReplicaWorld& world, const NetRegistry& registry) {
    struct Visited {
        Kernel::EntityId id;
        std::uint16_t wire;
        Kernel::ChangeStamp stamp;
    };
    std::vector<Visited> visited;
    for (const auto& entry : registry.components) {
        entry->visit(world, [&](const Kernel::EntityId id, const Kernel::ChangeStamp stamp) {
            visited.push_back(Visited{id, entry->wire, stamp});
        });
    }
    std::sort(visited.begin(), visited.end(),
              [](const Visited& a, const Visited& b) { return a.id != b.id ? a.id < b.id : a.wire < b.wire; });
    Snapshot snapshot;
    snapshot.stamps.reserve(visited.size());
    for (const Visited& one : visited) {
        if (snapshot.entities.empty() || snapshot.entities.back().id != one.id) {
            snapshot.entities.push_back(Snapshot::Entity{one.id, 0, static_cast<std::uint32_t>(snapshot.stamps.size())});
        }
        snapshot.entities.back().mask |= std::uint64_t{1} << one.wire;
        snapshot.stamps.push_back(one.stamp);
    }
    snapshot.encoding.assign(snapshot.stamps.size(), Snapshot::Encoding::NotYet);
    snapshot.offsets.assign(snapshot.stamps.size(), 0);
    snapshot.lengths.assign(snapshot.stamps.size(), 0);
    return snapshot;
}

// One entity's components as (wire u16, length u32, bytes) after a count u8.
void Components(ReplicaWorld& world, const NetRegistry& registry, Snapshot& snapshot, const Snapshot::Entity& entity,
                const std::uint64_t mask, NetWriter& writer) {
    std::uint8_t count = 0;
    for (const auto& entry : registry.components) {
        if (((mask >> entry->wire) & 1u) == 0) continue;
        if (snapshot.Encode(world, *entry, entity)) ++count;
    }
    writer.Value(count);
    for (const auto& entry : registry.components) {
        if (((mask >> entry->wire) & 1u) == 0 || ((entity.mask >> entry->wire) & 1u) == 0) continue;
        const std::uint32_t slot = Snapshot::Slot(entity, entry->wire);
        if (snapshot.encoding[slot] != Snapshot::Encoding::Done) continue;
        writer.Value(entry->wire);
        writer.Value(snapshot.lengths[slot]);
        writer.Bytes(std::span<const std::byte>(snapshot.bytes.data() + snapshot.offsets[slot], snapshot.lengths[slot]));
    }
}

void DropAt(PeerLedger& ledger, const ReplicaDrop reason) { ++ledger.drops[static_cast<std::size_t>(reason)]; }

// The structure update a message needs, on the wire: 0 when none was sent yet, else its tick + 1 (S1: tick 0 is a
// structure tick like any other).
std::uint32_t StructureOnWire(const std::optional<std::uint32_t> tick) { return tick ? *tick + 1 : 0; }
std::optional<std::uint32_t> StructureFromWire(const std::uint32_t value) {
    if (value == 0) return std::nullopt;
    return value - 1;
}
// Whether the structure a message needs is applied.
bool StructureApplied(const std::optional<std::uint32_t> needed, const std::optional<std::uint32_t> applied) {
    return !needed || (applied && *needed <= *applied);
}

// The client's messages on the reliable channel: confirmations and resyncs, read by the server before it sends.
// What cannot be read, is of another kind, generation or epoch is dropped and counted on the Peer (D-12).
void ReadClient(PeerLedger& ledger, Connection& connection, const std::uint32_t serverGeneration, ServerHost& server,
                const std::uint32_t connectionId) {
    while (auto message = connection.Pop(kReliableChannel)) {
        NetReader reader(*message);
        std::uint8_t kind = 0;
        std::uint32_t generation = 0, epoch = 0;
        if (!reader.Value(kind) || !reader.Value(generation) || !reader.Value(epoch)) {
            DropAt(ledger, ReplicaDrop::Malformed);
            continue;
        }
        if (kind == static_cast<std::uint8_t>(Kind::Resync)) {
            // The client's World was replaced: everything is sent again, under the new epoch.
            ledger.entities.clear();
            ledger.pending.clear();
            ledger.resources.clear();
            ledger.lastStructureTick.reset();
            // What waited is sent with the structure again: no accumulated priority carries over (M1 of PR-2's review).
            ledger.waiting.clear();
            ledger.waitedTicks.clear();
            ledger.epoch = epoch;
            server.SetEpoch(connectionId, epoch); // a later server World's ledger starts with it (R2)
            continue;
        }
        if (kind != static_cast<std::uint8_t>(Kind::Confirm)) {
            DropAt(ledger, ReplicaDrop::Malformed);
            continue;
        }
        if (generation != serverGeneration) {
            DropAt(ledger, ReplicaDrop::StaleGeneration);
            continue;
        }
        if (epoch != ledger.epoch) {
            DropAt(ledger, ReplicaDrop::StaleEpoch);
            continue;
        }
        std::uint32_t count = 0;
        if (!reader.Value(count)) {
            DropAt(ledger, ReplicaDrop::Malformed);
            continue;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t tick = 0;
            std::uint16_t part = 0;
            if (!reader.Value(tick) || !reader.Value(part)) {
                DropAt(ledger, ReplicaDrop::Malformed);
                break;
            }
            ++ledger.confirmationsReceived;
            const auto pending = ledger.pending.find((std::uint64_t{tick} << 16) | part);
            if (pending == ledger.pending.end()) continue;
            for (const Kernel::EntityId entity : pending->second.entities) {
                auto known = ledger.entities.find(entity);
                if (known != ledger.entities.end()) {
                    known->second.confirmedStamp = std::max(known->second.confirmedStamp, pending->second.stamp);
                }
            }
            for (const std::uint16_t wire : pending->second.resources) {
                if (ledger.resources.size() <= wire) ledger.resources.resize(wire + std::size_t{1});
                auto& confirmed = ledger.resources[wire];
                confirmed = std::max(confirmed.value_or(0), pending->second.stamp);
            }
            ledger.pending.erase(pending);
        }
    }
}

} // namespace

SystemResult ReplicateOut(ReplicaWorld world) {
    NetService* service = nullptr;
    std::shared_ptr<const NetRegistry> registry;
    std::map<std::uint32_t, Kernel::EntityId> peers;
    std::uint32_t valueBudget = 0;
    (void)world.Read<NetWorldLink>([&](const NetWorldLink& link) {
        service = link.service;
        registry = link.registry;
        peers = link.peers;
        valueBudget = link.valueBytesPerTick;
    });
    ServerHost* server = service != nullptr ? service->Server() : nullptr;
    if (server == nullptr || !registry) return {};
    // The private components' bits (C-VIS2).
    std::uint64_t privateMask = 0;
    for (const auto& entry : registry->components) {
        if (entry->privateComponent) privateMask |= std::uint64_t{1} << entry->wire;
    }
    const auto tick = static_cast<std::uint32_t>(
        world.Read<WorldTime::Tick>([](const WorldTime::Tick& t) { return t.value; })->value_or(0));
    const std::uint32_t generation = service->Generation();
    // The server messages go out with a tick the World has not sent them with yet (B2, R1): in a frame without a
    // fixed step they stay in the outbox for the next tick, also when a Peer connects in that frame (it sends its own
    // structure and values now; the messages reach it with the others at the next tick).
    std::vector<OutgoingServerMessage> outgoing;
    bool newTick = false;
    (void)world.Modify<NetOutbox>([&](NetOutbox& box) {
        newTick = box.sentTick != tick;
        if (!newTick) return;
        outgoing = std::move(box.messages);
        box.messages.clear();
        box.sentTick = tick;
    });

    Snapshot snapshot = Take(world, *registry);
    const Kernel::ChangeStamp now = world.Stamp();
    // Buffers kept across the Peers of this frame (their capacity is reused, their content is not).
    std::vector<std::byte> structure, ops, current, one, body;
    std::vector<Kernel::EntityId> restructured, currentEntities, despawned;
    std::vector<LedgerEntity> restructuredAs;
    std::vector<std::vector<std::byte>> parts;
    std::vector<std::vector<Kernel::EntityId>> partEntities;
    // The replicated Resources this frame (Q8 (b)): changed stamp and encoded value, shared by every Peer.
    struct TakenResource {
        std::optional<Kernel::ChangeStamp> stamp;
        bool sendable = false;
        std::vector<std::byte> bytes;
    };
    std::vector<TakenResource> resourcesNow(registry->resources.size());
    for (std::size_t r = 0; r < registry->resources.size(); ++r) {
        TakenResource& taken = resourcesNow[r];
        taken.stamp = registry->resources[r]->stamp(world);
        if (!taken.stamp) continue;
        NetWriter resourceWriter(taken.bytes);
        taken.sendable = registry->resources[r]->encode(world, resourceWriter);
    }
    std::vector<std::byte> resourceBytes;
    std::vector<std::uint16_t> firstPartResources;
    // Step 5: one Peer's entities with changed values (their bytes in candidateBytes) and the ones that wait.
    struct Candidate {
        Kernel::EntityId entity;
        std::size_t offset;
        std::size_t size;
        std::uint64_t score;
        bool sent;
    };
    std::vector<Candidate> candidates;
    std::vector<std::byte> candidateBytes;
    std::vector<Kernel::EntityId> deferred;

    for (const auto& [connectionId, peerEntity] : peers) {
        Connection* connection = server->Find(connectionId);
        if (connection == nullptr) continue;
        bool hasLedger = false;
        PeerLedger ledger;
        (void)world.Modify<PeerLedger>(peerEntity, [&](PeerLedger& kept) {
            ledger = std::move(kept);
            hasLedger = true;
        });
        if (!hasLedger) continue;
        ReadClient(ledger, *connection, generation, *server, connectionId);
        // What this Peer sees (C-VIS1, C-VIS2): an entity it does not see has no components for it, and a private
        // component only when its PeerVisibility shows it.
        std::optional<PeerVisibility> visibility;
        (void)world.Read<PeerVisibility>(peerEntity, [&](const PeerVisibility& kept) { visibility = kept; });
        const auto peerMask = [&](const Snapshot::Entity& taken) -> std::uint64_t {
            if (visibility && !visibility->Sees(taken.id)) return 0;
            std::uint64_t mask = taken.mask;
            const std::uint64_t hidden = mask & privateMask;
            if (hidden == 0) return mask;
            for (const auto& entry : registry->components) {
                if (((hidden >> entry->wire) & 1u) == 0) continue;
                if (!visibility || !visibility->SeesPrivate(taken.id, entry->key)) mask &= ~(std::uint64_t{1} << entry->wire);
            }
            return mask;
        };

        if (ledger.lastSentTick != tick) {
            // The structure update: what this client has against what the World has.
            structure.clear();
            NetWriter writer(structure);
            Head(writer, Kind::Structure, generation, ledger.epoch);
            writer.Value(tick);
            ops.clear();
            NetWriter opWriter(ops);
            std::uint32_t opCount = 0;
            // Ascending: the snapshot is walked by id. The ledger takes the changes only once the connection took the
            // update (S3: a refused reliable send does not move the ledger on; the update is made again next tick).
            restructured.clear();
            despawned.clear();
            restructuredAs.clear();
            for (auto it = ledger.entities.begin(); it != ledger.entities.end(); ++it) {
                // Gone from the World, or no longer seen by this Peer (only WhileVisible): removed at that client.
                const Snapshot::Entity* still = snapshot.Find(it->first);
                if (still != nullptr && peerMask(*still) != 0) continue;
                opWriter.Value(it->first);
                opWriter.Value(std::uint8_t{2}); // despawn
                opWriter.Value(std::uint8_t{0});
                opWriter.Value(std::uint8_t{0});
                ++opCount;
                despawned.push_back(it->first);
            }
            for (const Snapshot::Entity& taken : snapshot.entities) {
                const Kernel::EntityId entity = taken.id;
                const std::uint64_t mask = peerMask(taken);
                if (mask == 0) continue;
                auto known = ledger.entities.find(entity);
                const bool spawn = known == ledger.entities.end();
                if (!spawn && known->second.mask == mask) continue;
                opWriter.Value(entity);
                opWriter.Value(static_cast<std::uint8_t>(spawn ? 1 : 0));
                const std::uint64_t removed = spawn ? 0 : (known->second.mask & ~mask);
                std::uint8_t removedCount = 0;
                for (std::uint16_t wire = 0; wire < 64; ++wire) removedCount += ((removed >> wire) & 1u) ? 1 : 0;
                opWriter.Value(removedCount);
                for (std::uint16_t wire = 0; wire < 64; ++wire) {
                    if ((removed >> wire) & 1u) opWriter.Value(wire);
                }
                // The values of an entity whose structure changed go with the structure (all of its components).
                Components(world, *registry, snapshot, taken, mask, opWriter);
                restructuredAs.push_back(LedgerEntity{mask, now});
                restructured.push_back(entity);
                ++opCount;
            }
            if (opCount > 0) {
                writer.Value(opCount);
                writer.Bytes(ops);
                if (connection->Send(kReliableChannel, structure)) {
                    // An entity removed at the client, or sent whole with its structure, waits no more: its accumulated
                    // priority does not carry over to a later change (M1 of PR-2's review).
                    for (const Kernel::EntityId gone : despawned) {
                        ledger.entities.erase(gone);
                        ledger.waiting.erase(gone);
                        ledger.waitedTicks.erase(gone);
                    }
                    for (std::size_t i = 0; i < restructured.size(); ++i) {
                        ledger.entities[restructured[i]] = restructuredAs[i];
                        ledger.waiting.erase(restructured[i]);
                        ledger.waitedTicks.erase(restructured[i]);
                    }
                    ledger.lastStructureTick = tick;
                    ++ledger.structuresSent;
                } else {
                    ++ledger.reliableRejected;
                    restructured.clear();
                }
            }

            // The server messages of this frame, after the structure they may point into.
            for (const OutgoingServerMessage& message : outgoing) {
                if (message.recipient && *message.recipient != peerEntity) continue;
                std::vector<std::byte> body;
                NetWriter messageWriter(body);
                Head(messageWriter, Kind::ServerMessage, generation, ledger.epoch);
                messageWriter.Value(message.wire);
                messageWriter.Value(StructureOnWire(ledger.lastStructureTick));
                messageWriter.Value(static_cast<std::uint32_t>(message.bytes.size()));
                messageWriter.Bytes(message.bytes);
                if (connection->Send(kReliableChannel, body)) {
                    ++ledger.serverMessagesSent;
                } else {
                    ++ledger.reliableRejected;
                }
            }

            // The values changed since the client last confirmed them, in parts of whole entities.
            // The Resources written since this Peer last confirmed them go in the first part (before its entities).
            resourceBytes.clear();
            firstPartResources.clear();
            NetWriter resourceWriter(resourceBytes);
            for (std::size_t r = 0; r < resourcesNow.size(); ++r) {
                const TakenResource& taken = resourcesNow[r];
                if (!taken.sendable) continue;
                const auto wire = static_cast<std::uint16_t>(r);
                const bool confirmed = wire < ledger.resources.size() && ledger.resources[wire] && *taken.stamp <= *ledger.resources[wire];
                if (confirmed) continue;
                resourceWriter.Value(wire);
                resourceWriter.Value(static_cast<std::uint32_t>(taken.bytes.size()));
                resourceWriter.Bytes(taken.bytes);
                firstPartResources.push_back(wire);
                ++ledger.resourceValuesSent;
            }
            std::size_t partCountSoFar = 0;
            current.clear();
            currentEntities.clear();
            partEntities.clear();
            std::uint32_t currentCount = 0;
            auto close = [&] {
                if (parts.size() == partCountSoFar) parts.emplace_back();
                const std::size_t partIndex = partCountSoFar++;
                std::vector<std::byte>& part = parts[partIndex];
                part.clear();
                NetWriter partWriter(part);
                // Each part: the Resources (only the first has any), then the entities.
                if (partIndex == 0) {
                    partWriter.Value(static_cast<std::uint8_t>(firstPartResources.size()));
                    partWriter.Bytes(resourceBytes);
                } else {
                    partWriter.Value(std::uint8_t{0});
                }
                // Then the entities whose changed values wait for a later tick (only the first part has any; step 5:
                // a simulating client writes NotSent for them).
                if (partIndex == 0) {
                    partWriter.Value(static_cast<std::uint16_t>(deferred.size()));
                    for (const Kernel::EntityId waits : deferred) partWriter.Value(waits);
                } else {
                    partWriter.Value(std::uint16_t{0});
                }
                partWriter.Value(currentCount);
                partWriter.Bytes(current);
                partEntities.push_back(std::move(currentEntities));
                currentEntities = {};
                current.clear();
                currentCount = 0;
            };
            // The entities with values changed since the client confirmed them, each encoded once into candidates.
            candidates.clear();
            candidateBytes.clear();
            for (const auto& [entity, known] : ledger.entities) {
                if (std::binary_search(restructured.begin(), restructured.end(), entity)) continue;
                const Snapshot::Entity* taken = snapshot.Find(entity);
                if (taken == nullptr) continue;
                std::uint64_t changedMask = 0;
                for (const auto& entry : registry->components) {
                    if (((known.mask >> entry->wire) & 1u) == 0 || ((taken->mask >> entry->wire) & 1u) == 0) continue;
                    if (snapshot.stamps[Snapshot::Slot(*taken, entry->wire)] > known.confirmedStamp) {
                        changedMask |= std::uint64_t{1} << entry->wire;
                    }
                }
                if (changedMask == 0) continue;
                const std::size_t start = candidateBytes.size();
                NetWriter oneWriter(candidateBytes);
                oneWriter.Value(entity);
                Components(world, *registry, snapshot, *taken, changedMask, oneWriter);
                candidates.push_back(Candidate{entity, start, candidateBytes.size() - start, 0, true});
            }
            // Over the Peer's budget (step 5): the highest accumulated priority first (lightyear's accumulator: each
            // tick an entity waits, its weight is added); the others wait, their accumulated priority grows.
            deferred.clear();
            if (valueBudget > 0 && candidateBytes.size() > valueBudget) {
                for (Candidate& candidate : candidates) {
                    std::uint32_t weight = 1;
                    (void)world.Read<NetPriority>(candidate.entity, [&](const NetPriority& p) { weight = p.weight; });
                    const auto waited = ledger.waiting.find(candidate.entity);
                    candidate.score = (waited != ledger.waiting.end() ? waited->second : 0) + weight;
                }
                std::vector<std::size_t> order(candidates.size());
                for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
                std::stable_sort(order.begin(), order.end(),
                                 [&](std::size_t a, std::size_t b) { return candidates[a].score > candidates[b].score; });
                std::size_t used = 0;
                for (const std::size_t i : order) {
                    Candidate& candidate = candidates[i];
                    candidate.sent = used + candidate.size <= valueBudget || used == 0;
                    if (candidate.sent) used += candidate.size;
                }
                for (Candidate& candidate : candidates) {
                    if (candidate.sent) {
                        ledger.waiting.erase(candidate.entity);
                        ledger.waitedTicks.erase(candidate.entity);
                    } else {
                        ledger.waiting[candidate.entity] = candidate.score;
                        deferred.push_back(candidate.entity);
                        ++ledger.valuesDeferred;
                        const std::uint32_t waited = ++ledger.waitedTicks[candidate.entity];
                        ledger.longestWait = std::max(ledger.longestWait, waited);
                    }
                }
            } else {
                for (const Candidate& candidate : candidates) {
                    ledger.waiting.erase(candidate.entity);
                    ledger.waitedTicks.erase(candidate.entity);
                }
            }
            for (const Candidate& candidate : candidates) {
                if (!candidate.sent) continue;
                const std::span<const std::byte> one(candidateBytes.data() + candidate.offset, candidate.size);
                // An entity's data is never split: a part closes before an entity that would not fit.
                const std::size_t resourcesHere = partCountSoFar == 0 ? resourceBytes.size() + 2 + 8 * deferred.size() : 0;
                if ((currentCount > 0 || resourcesHere > 0) && current.size() + resourcesHere + one.size() > kValuePartBudget) close();
                current.insert(current.end(), one.begin(), one.end());
                currentEntities.push_back(candidate.entity);
                ++currentCount;
            }
            // A tick with nothing changed still sends one empty part (C-RP4).
            if (currentCount > 0 || partCountSoFar == 0) close();
            const auto partCount = static_cast<std::uint16_t>(partCountSoFar);
            for (std::uint16_t index = 0; index < partCount; ++index) {
                body.clear();
                NetWriter valueWriter(body);
                Head(valueWriter, Kind::Values, generation, ledger.epoch);
                valueWriter.Value(tick);
                valueWriter.Value(StructureOnWire(ledger.lastStructureTick));
                valueWriter.Value(index);
                valueWriter.Value(partCount);
                valueWriter.Bytes(parts[index]);
                (void)connection->Send(kValuesChannel, body);
                ledger.pending[(std::uint64_t{tick} << 16) | index] = PendingPart{now, std::move(partEntities[index])};
                if (index == 0) ledger.pending[std::uint64_t{tick} << 16].resources = firstPartResources;
                ++ledger.valuePartsSent;
            }
            while (!ledger.pending.empty() && (ledger.pending.begin()->first >> 16) + kPendingTicks < tick) {
                ledger.pending.erase(ledger.pending.begin());
            }
            ledger.lastSentTick = tick;
        }
        (void)world.Modify<PeerLedger>(peerEntity, [&](PeerLedger& kept) { kept = std::move(ledger); });
    }
    return {};
}

namespace {

void Drop(NetReplicaState& state, const ReplicaDrop reason) { ++state.drops[static_cast<std::size_t>(reason)]; }

// Applies one structure update whole; Err only for a kernel error (the World cannot go on).
// K3 (D-5): what a client World with a rollback strategy needs to know of the rollback Module (nullptr: K1, K2).
struct RollingBack {
    const std::vector<bool>* components = nullptr;
    const std::vector<bool>* resources = nullptr;
    std::uint32_t depth = 0;
    // The entities alive now (ascending; K3 neither spawns nor despawns in First, so it stays right).
    const std::vector<Kernel::EntityId>* alive = nullptr;
    [[nodiscard]] bool Alive(const Kernel::EntityId entity) const { return std::binary_search(alive->begin(), alive->end(), entity); }
    [[nodiscard]] bool Component(const std::uint16_t wire) const { return wire < components->size() && (*components)[wire]; }
    [[nodiscard]] bool Resource(const std::uint16_t wire) const { return wire < resources->size() && (*resources)[wire]; }
};

void Inbox(ReplicaWorld& world, const std::function<void(Rollback::RollbackInbox&)>& write) {
    (void)world.Modify<Rollback::RollbackInbox>([&](Rollback::RollbackInbox& inbox) { write(inbox); });
}

// K3: a value of a component that does not roll back for an entity not in the World yet waits for it (newest per wire).
void Wait(NetReplicaState& state, const Kernel::EntityId entity, const std::uint16_t wire, std::span<const std::byte> bytes) {
    auto& waiting = state.waitingDirect[entity].values;
    for (auto& [kept, value] : waiting) {
        if (kept == wire) {
            value.assign(bytes.begin(), bytes.end());
            return;
        }
    }
    waiting.emplace_back(wire, std::vector<std::byte>(bytes.begin(), bytes.end()));
}

SystemResult ApplyStructure(ReplicaWorld& world, const NetRegistry& registry, NetReplicaState& state, NetReader& reader,
                            const RollingBack* rolling) {
    std::uint32_t tick = 0, opCount = 0;
    if (!reader.Value(tick) || !reader.Value(opCount)) {
        Drop(state, ReplicaDrop::Malformed);
        return {};
    }
    for (std::uint32_t op = 0; op < opCount; ++op) {
        Kernel::EntityId entity = 0;
        std::uint8_t flags = 0, removedCount = 0, setCount = 0;
        if (!reader.Value(entity) || !reader.Value(flags) || !reader.Value(removedCount)) {
            Drop(state, ReplicaDrop::Malformed);
            return {};
        }
        std::vector<std::uint16_t> removed(removedCount);
        for (auto& wire : removed) {
            if (!reader.Value(wire)) {
                Drop(state, ReplicaDrop::Malformed);
                return {};
            }
        }
        if (!reader.Value(setCount)) {
            Drop(state, ReplicaDrop::Malformed);
            return {};
        }
        if (flags == 2) {
            if (rolling != nullptr) {
                // K3: the entity's end goes to the rollback Module (Exited, R-8 A); it is removed at the confirmed point.
                Inbox(world, [&](Rollback::RollbackInbox& inbox) { inbox.Exited(entity, tick); });
                ++state.inboxExits;
                state.waitingDirect.erase(entity);
            } else {
                auto despawned = world.Despawn(entity);
                if (!despawned) return Base::Err(std::move(despawned).error());
            }
            for (auto it = state.written.lower_bound({entity, 0}); it != state.written.end() && it->first.first == entity;) {
                it = state.written.erase(it);
            }
            continue;
        }
        const bool spawnK3 = rolling != nullptr && flags == 1;
        bool knownBefore = false;
        if (spawnK3) {
            // K3: the server's spawn goes to the rollback Module (Entered, R-8 A): the entity is put back, with its id,
            // by the rollback at the confirmed point, and later rollbacks keep it. It is not spawned here.
            Inbox(world, [&](Rollback::RollbackInbox& inbox) {
                knownBefore = inbox.AliveAt(entity, std::numeric_limits<std::uint64_t>::max()).has_value();
                inbox.Entered(entity, tick);
            });
            ++state.inboxEntries;
            if (rolling->Alive(entity)) ++state.entriesWhileAlive;
            // Its components that do not roll back wait for the entity of this spawn (S1 b, B1 of PR-2's review).
            auto& waiting = state.waitingDirect[entity];
            waiting = NetReplicaState::WaitingDirect{};
            waiting.notBefore = tick;
            waiting.whole = true;
        }
        for (const std::uint16_t wire : removed) {
            if (rolling != nullptr && rolling->Component(wire)) {
                registry.components[wire]->removedToInbox(world, entity, tick);
                ++state.inboxRemovals;
            } else if (wire < registry.components.size()) {
                if (rolling != nullptr) {
                    // K3: a waiting value of it is not written later (S1 a of PR-2's review).
                    if (const auto waiting = state.waitingDirect.find(entity); waiting != state.waitingDirect.end()) {
                        std::erase_if(waiting->second.values, [wire](const auto& value) { return value.first == wire; });
                    }
                    if (rolling->Alive(entity)) (void)registry.components[wire]->remove(world, entity);
                } else {
                    (void)registry.components[wire]->remove(world, entity);
                }
            }
            state.written[{entity, wire}] = tick;
        }
        std::vector<std::uint16_t> spawnedRolling;
        bool first = flags == 1;
        for (std::uint8_t i = 0; i < setCount; ++i) {
            std::uint16_t wire = 0;
            std::uint32_t length = 0;
            std::span<const std::byte> bytes;
            if (!reader.Value(wire) || !reader.Value(length) || !reader.Bytes(length, bytes) || wire >= registry.components.size()) {
                Drop(state, ReplicaDrop::Malformed);
                return {};
            }
            if (rolling != nullptr) {
                // K3: a component that rolls back goes to the inbox (not written in First: R-2 premise 5); one that
                // does not is written directly, or waits until the entity is in the World.
                if (rolling->Component(wire)) {
                    if (!registry.components[wire]->toInbox(world, entity, bytes, tick)) {
                        Drop(state, ReplicaDrop::Malformed);
                    } else {
                        ++state.inboxValues;
                    }
                    if (spawnK3) spawnedRolling.push_back(wire);
                } else if (!rolling->Alive(entity) || state.waitingDirect.count(entity) != 0) {
                    Wait(state, entity, wire, bytes);
                } else {
                    const ApplyOutcome direct = registry.components[wire]->apply(world, entity, bytes, ApplyMode::Set);
                    if (direct == ApplyOutcome::Failed) {
                        return Base::Err(Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed,
                                                             "the net Module could not apply a structure update"));
                    }
                    if (direct == ApplyOutcome::Malformed) Drop(state, ReplicaDrop::Malformed);
                }
                state.written[{entity, wire}] = tick;
                continue;
            }
            const ApplyOutcome outcome =
                registry.components[wire]->apply(world, entity, bytes, first ? ApplyMode::Spawn : ApplyMode::Set);
            if (outcome == ApplyOutcome::Failed) {
                return Base::Err(Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed,
                                                     "the net Module could not apply a structure update"));
            }
            if (outcome == ApplyOutcome::Rejected) Drop(state, ReplicaDrop::SpawnRejected);
            if (outcome == ApplyOutcome::Malformed) Drop(state, ReplicaDrop::Malformed);
            if (outcome == ApplyOutcome::Missing) Drop(state, ReplicaDrop::MissingTarget);
            if (outcome == ApplyOutcome::Unchanged) ++state.unchangedNotWritten;
            first = false;
            state.written[{entity, wire}] = tick;
        }
        if (spawnK3 && knownBefore) {
            // K3, an entity seen again (B1 of PR-2's review): a spawn is the whole set, so a rolling-back component it
            // does not carry was taken off while the entity was away; the inbox's older value must not put it back.
            for (std::uint16_t wire = 0; wire < registry.components.size(); ++wire) {
                if (!rolling->Component(wire) || std::find(spawnedRolling.begin(), spawnedRolling.end(), wire) != spawnedRolling.end()) continue;
                registry.components[wire]->removedToInbox(world, entity, tick);
                ++state.inboxRemovals;
            }
        }
    }
    state.appliedStructureTick = tick;
    state.maxAppliedTick = std::max(state.maxAppliedTick, tick);
    ++state.structuresApplied;
    return {};
}

SystemResult ApplyValues(ReplicaWorld& world, const NetRegistry& registry, NetReplicaState& state, const NetReplicaState::Part& part,
                        const RollingBack* rolling) {
    // K3: a value older than the confirmed point by more than the rollback depth cannot be compared: dropped, counted.
    const bool beyondDepth = rolling != nullptr && rolling->depth > 0 && state.latestConfirmed &&
                             part.tick + rolling->depth < *state.latestConfirmed;
    NetReader reader(part.entries);
    // The replicated Resources (Q8 (b)), written with their registration's apply.
    std::uint8_t resourceCount = 0;
    if (!reader.Value(resourceCount)) {
        Drop(state, ReplicaDrop::Malformed);
        return {};
    }
    for (std::uint8_t r = 0; r < resourceCount; ++r) {
        std::uint16_t wire = 0;
        std::uint32_t length = 0;
        std::span<const std::byte> bytes;
        if (!reader.Value(wire) || !reader.Value(length) || !reader.Bytes(length, bytes) || wire >= registry.resources.size()) {
            Drop(state, ReplicaDrop::Malformed);
            return {};
        }
        if (rolling != nullptr && rolling->Resource(wire) && registry.resources[wire]->toInbox) {
            // K3: a Resource that rolls back goes to the inbox at the part's tick (late ones too: MC-1).
            if (beyondDepth) {
                Drop(state, ReplicaDrop::BeyondDepth);
            } else if (!registry.resources[wire]->toInbox(world, bytes, part.tick)) {
                Drop(state, ReplicaDrop::Malformed);
            } else {
                ++state.inboxValues;
            }
            continue;
        }
        const auto written = state.resourcesWritten.find(wire);
        if (written != state.resourcesWritten.end() && written->second > part.tick) {
            Drop(state, ReplicaDrop::StaleValue);
            continue;
        }
        auto outcome = registry.resources[wire]->apply(world, bytes);
        if (!outcome) return Base::Err(std::move(outcome).error());
        if (*outcome == ApplyOutcome::Failed) {
            return Base::Err(Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed, "the net Module could not apply a Resource"));
        }
        if (*outcome == ApplyOutcome::Malformed) {
            Drop(state, ReplicaDrop::Malformed);
            continue;
        }
        ++state.resourceValuesApplied;
        if (*outcome == ApplyOutcome::Unchanged) ++state.resourceValuesUnchanged;
        state.resourcesWritten[wire] = part.tick;
    }
    // The entities whose changed values wait for a later tick (step 5): the snapshot of this tick does not carry them.
    // K3 writes NotSent (R-8's third state: not compared at that C, not inferred unchanged); K1 and K2 wait.
    std::uint16_t deferredCount = 0;
    if (!reader.Value(deferredCount)) {
        Drop(state, ReplicaDrop::Malformed);
        return {};
    }
    for (std::uint16_t d = 0; d < deferredCount; ++d) {
        Kernel::EntityId waits = 0;
        if (!reader.Value(waits)) {
            Drop(state, ReplicaDrop::Malformed);
            return {};
        }
        if (rolling != nullptr) {
            Inbox(world, [&](Rollback::RollbackInbox& inbox) { inbox.NotSent(waits, part.tick); });
            ++state.inboxNotSent;
        }
    }
    std::uint32_t count = 0;
    if (!reader.Value(count)) {
        Drop(state, ReplicaDrop::Malformed);
        return {};
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        Kernel::EntityId entity = 0;
        std::uint8_t components = 0;
        if (!reader.Value(entity) || !reader.Value(components)) {
            Drop(state, ReplicaDrop::Malformed);
            return {};
        }
        for (std::uint8_t c = 0; c < components; ++c) {
            std::uint16_t wire = 0;
            std::uint32_t length = 0;
            std::span<const std::byte> bytes;
            if (!reader.Value(wire) || !reader.Value(length) || !reader.Bytes(length, bytes) || wire >= registry.components.size()) {
                Drop(state, ReplicaDrop::Malformed);
                return {};
            }
            if (rolling != nullptr && rolling->Component(wire)) {
                // K3: into the inbox at the part's tick, late ones too (MC-1: the rollback compares the last value at
                // or before C), unless older than the confirmed point by more than the depth.
                if (beyondDepth) {
                    Drop(state, ReplicaDrop::BeyondDepth);
                } else if (!registry.components[wire]->toInbox(world, entity, bytes, part.tick)) {
                    Drop(state, ReplicaDrop::Malformed);
                } else {
                    ++state.inboxValues;
                }
                continue;
            }
            const auto written = state.written.find({entity, wire});
            if (written != state.written.end() && written->second > part.tick) {
                Drop(state, ReplicaDrop::StaleValue);
                continue;
            }
            if (rolling != nullptr && (!rolling->Alive(entity) || state.waitingDirect.count(entity) != 0)) {
                // K3: the entity comes with the rollback that puts it back; the value waits for it.
                Wait(state, entity, wire, bytes);
                state.written[{entity, wire}] = part.tick;
                continue;
            }
            const ApplyOutcome outcome = registry.components[wire]->apply(world, entity, bytes, ApplyMode::SetIfPresent);
            if (outcome == ApplyOutcome::Failed) {
                return Base::Err(Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed, "the net Module could not apply a value"));
            }
            if (outcome == ApplyOutcome::Missing) Drop(state, ReplicaDrop::MissingTarget);
            if (outcome == ApplyOutcome::Malformed) Drop(state, ReplicaDrop::Malformed);
            if (outcome == ApplyOutcome::Unchanged) ++state.unchangedNotWritten;
            if (outcome == ApplyOutcome::Applied || outcome == ApplyOutcome::Unchanged) state.written[{entity, wire}] = part.tick;
        }
    }
    return {};
}

// Reads the generation and epoch after the kind; false when the message is to be dropped (counted). A newer
// generation resets what the client knows of the server's World and is noticed.
bool Current(NetReplicaState& state, NetReader& reader) {
    std::uint32_t generation = 0, epoch = 0;
    if (!reader.Value(generation) || !reader.Value(epoch)) {
        Drop(state, ReplicaDrop::Malformed);
        return false;
    }
    if (state.generation && generation < *state.generation) {
        Drop(state, ReplicaDrop::StaleGeneration);
        return false;
    }
    if (!state.generation || generation > *state.generation) {
        // B1, R2: the epoch is never taken from a message; a replaced server World's ledgers start with the
        // connection's epoch, the one this World has.
        if (state.generation) state.replacedGenerations.push_back(generation);
        state.generation = generation;
        state.appliedStructureTick.reset();
        state.pendingValues.clear();
        state.heldMessages.clear();
        state.ticks.clear();
        state.written.clear();
        state.resourcesWritten.clear();
        state.maxAppliedTick = 0;
        state.latestConfirmed.reset();
    }
    if (epoch < state.epoch) {
        Drop(state, ReplicaDrop::StaleEpoch);
        return false;
    }
    return true;
}

} // namespace

SystemResult ApplyReplication(ReplicaWorld world) {
    NetService* service = nullptr;
    std::shared_ptr<const NetRegistry> registry;
    RollingBack rollingBack;
    std::vector<bool> rollbackComponents, rollbackResources;
    bool rollsBack = false;
    bool resyncPending = false;
    (void)world.Read<NetWorldLink>([&](const NetWorldLink& link) {
        service = link.service;
        resyncPending = link.resyncPending;
        registry = link.registry;
        rollsBack = link.rollsBack;
        rollbackComponents = link.rollbackComponents;
        rollbackResources = link.rollbackResources;
        rollingBack.depth = link.rollbackDepth;
    });
    rollingBack.components = &rollbackComponents;
    rollingBack.resources = &rollbackResources;
    std::vector<Kernel::EntityId> alive;
    if (rollsBack) alive = world.Entities();
    rollingBack.alive = &alive;
    const RollingBack* rolling = rollsBack ? &rollingBack : nullptr;
    ClientSession* client = service != nullptr ? service->Client() : nullptr;
    Connection* connection = client != nullptr && client->State() != ClientState::Disconnected ? client->Transport() : nullptr;
    if (connection == nullptr || !registry) return {};
    NetReplicaState state;
    (void)world.Modify<NetReplicaState>([&](NetReplicaState& kept) { state = std::move(kept); });

    if (resyncPending) {
        // This World replaced an earlier one on the same connection: ask for everything again, under its epoch (set
        // when it was prepared).
        std::vector<std::byte> body;
        NetWriter writer(body);
        Head(writer, Kind::Resync, state.generation.value_or(0), state.epoch);
        (void)connection->Send(kReliableChannel, body);
        (void)world.Modify<NetWorldLink>([](NetWorldLink& link) { link.resyncPending = false; });
    }

    SystemResult result;
    // The reliable channel, in order: structure updates are applied as they come; a server message waits for the
    // structure before it, which is already applied when it is read.
    while (auto message = connection->Pop(kReliableChannel)) {
        NetReader reader(*message);
        std::uint8_t kind = 0;
        if (!reader.Value(kind)) {
            Drop(state, ReplicaDrop::Malformed);
            continue;
        }
        if (!Current(state, reader)) continue;
        if (kind == static_cast<std::uint8_t>(Kind::Structure)) {
            if (result = ApplyStructure(world, *registry, state, reader, rolling); !result) break;
        } else if (kind == static_cast<std::uint8_t>(Kind::ServerMessage)) {
            std::uint16_t wire = 0;
            std::uint32_t structureTick = 0, length = 0;
            std::span<const std::byte> bytes;
            if (!reader.Value(wire) || !reader.Value(structureTick) || !reader.Value(length) || !reader.Bytes(length, bytes)) {
                Drop(state, ReplicaDrop::Malformed);
                continue;
            }
            ++state.serverMessagesReceived;
            state.heldMessages.push_back(
                {StructureFromWire(structureTick), {wire, std::vector<std::byte>(bytes.begin(), bytes.end())}});
        } else {
            Drop(state, ReplicaDrop::Malformed);
        }
    }
    // A server message is handed out once the structure tick it needs is applied (C-MS1).
    for (auto it = state.heldMessages.begin(); it != state.heldMessages.end();) {
        if (!StructureApplied(it->requiredStructure, state.appliedStructureTick)) {
            ++it;
            continue;
        }
        state.readyMessages.push_back(std::move(it->message));
        it = state.heldMessages.erase(it);
    }
    while (auto message = connection->Pop(kValuesChannel)) {
        NetReader reader(*message);
        std::uint8_t kind = 0;
        NetReplicaState::Part part;
        if (!reader.Value(kind) || kind != static_cast<std::uint8_t>(Kind::Values)) {
            Drop(state, ReplicaDrop::Malformed);
            continue;
        }
        if (!Current(state, reader)) continue;
        std::span<const std::byte> rest;
        std::uint32_t structureTick = 0;
        if (!reader.Value(part.tick) || !reader.Value(structureTick) || !reader.Value(part.part) ||
            !reader.Value(part.partCount) || part.partCount == 0 || !reader.Bytes(reader.Remaining(), rest)) {
            Drop(state, ReplicaDrop::Malformed);
            continue;
        }
        part.requiredStructure = StructureFromWire(structureTick);
        part.entries.assign(rest.begin(), rest.end());
        ++state.valuePartsReceived;
        state.pendingValues.push_back(std::move(part));
    }

    // Values whose structure is applied, oldest tick first; the others wait.
    std::stable_sort(state.pendingValues.begin(), state.pendingValues.end(),
                     [](const auto& a, const auto& b) { return a.tick < b.tick; });
    std::vector<std::pair<std::uint32_t, std::uint16_t>> confirmations;
    std::vector<NetReplicaState::Part> waiting;
    for (NetReplicaState::Part& part : state.pendingValues) {
        if (!result || !StructureApplied(part.requiredStructure, state.appliedStructureTick)) {
            waiting.push_back(std::move(part));
            continue;
        }
        if (result = ApplyValues(world, *registry, state, part, rolling); !result) continue;
        ++state.valuePartsProcessed;
        state.maxAppliedTick = std::max(state.maxAppliedTick, part.tick);
        auto& tick = state.ticks[part.tick];
        tick.count = part.partCount;
        ++tick.received;
        confirmations.emplace_back(part.tick, part.part);
    }
    state.pendingValues = std::move(waiting);
    // D-7: a tick whose value parts are all processed (each needed its structure applied first).
    for (auto it = state.ticks.begin(); it != state.ticks.end();) {
        if (it->second.received >= it->second.count) {
            if (!state.latestConfirmed || it->first > *state.latestConfirmed) state.latestConfirmed = it->first;
            ++state.confirmedTicks;
            it = state.ticks.erase(it);
        } else if (state.latestConfirmed && it->first + kPendingTicks < *state.latestConfirmed) {
            it = state.ticks.erase(it);
        } else {
            ++it;
        }
    }
    if (rolling != nullptr) {
        // The confirmed point the rollback has decided at by now (written last frame; it decides in the frame's
        // decide stage, after this System).
        const std::optional<std::uint32_t> decided = state.inboxConfirmed;
        // K3: the confirmed point (D-7: every value part of C processed, the structure they need applied; it falls
        // only on ticks the server sent values for) goes to the rollback Module.
        if (state.latestConfirmed && state.inboxConfirmed != state.latestConfirmed) {
            const std::uint64_t point = *state.latestConfirmed;
            Inbox(world, [point](Rollback::RollbackInbox& inbox) { inbox.ConfirmedThrough(point); });
            state.inboxConfirmed = state.latestConfirmed;
        }
        // The values of components that do not roll back, for entities a rollback has put back since.
        for (auto it = state.waitingDirect.begin(); it != state.waitingDirect.end();) {
            const NetReplicaState::WaitingDirect& waiting = it->second;
            if (!rolling->Alive(it->first) || (waiting.notBefore != 0 && (!decided || *decided < waiting.notBefore))) {
                ++it;
                continue;
            }
            if (waiting.whole) {
                // The spawn's set is whole: a component that does not roll back and is not in it is taken off.
                for (std::uint16_t wire = 0; wire < registry->components.size(); ++wire) {
                    if (rolling->Component(wire)) continue;
                    const bool carried = std::any_of(waiting.values.begin(), waiting.values.end(),
                                                     [wire](const auto& value) { return value.first == wire; });
                    if (!carried) (void)registry->components[wire]->remove(world, it->first);
                }
            }
            for (const auto& [wire, bytes] : waiting.values) {
                if (registry->components[wire]->apply(world, it->first, bytes, ApplyMode::Set) == ApplyOutcome::Failed) {
                    result = Base::Err(Kernel::KernelError(Kernel::KernelErrorCode::SystemFailed, "the net Module could not apply a value"));
                }
            }
            it = state.waitingDirect.erase(it);
        }
    }
    // The confirmation: only after the parts were processed (written into the World).
    if (!confirmations.empty()) {
        std::vector<std::byte> body;
        NetWriter writer(body);
        Head(writer, Kind::Confirm, state.generation.value_or(0), state.epoch);
        writer.Value(static_cast<std::uint32_t>(confirmations.size()));
        for (const auto& [tick, index] : confirmations) {
            writer.Value(tick);
            writer.Value(index);
        }
        (void)connection->Send(kReliableChannel, body);
        state.confirmationsSent += confirmations.size();
    }
    (void)world.Modify<NetReplicaState>([&](NetReplicaState& kept) { kept = std::move(state); });
    return result;
}

SystemResult NotifyReplica(Kernel::ResMut<NetReplicaState> state, Kernel::MessageWriter<ServerWorldReplaced> replaced) {
    if (state.Read().replacedGenerations.empty()) return {};
    for (const std::uint32_t generation : state->replacedGenerations) replaced.Send(ServerWorldReplaced{generation});
    state->replacedGenerations.clear();
    return {};
}

} // namespace Detail
} // namespace GYO::Engine::Net

namespace GYO::Engine::Net::Detail {

// Server, Main/First after ReceiveNet: each Peer's input messages (the values channel carries only those from a
// client) into its buffers, within the window of the server's tick.
SystemResult ReceiveInputs(ReplicaWorld world) {
    NetService* service = nullptr;
    std::shared_ptr<const NetRegistry> registry;
    std::map<std::uint32_t, Kernel::EntityId> peers;
    (void)world.Read<NetWorldLink>([&](const NetWorldLink& link) {
        service = link.service;
        registry = link.registry;
        peers = link.peers;
    });
    ServerHost* server = service != nullptr ? service->Server() : nullptr;
    if (server == nullptr || !registry) return {};
    const auto tick = static_cast<std::uint32_t>(
        world.Read<WorldTime::Tick>([](const WorldTime::Tick& t) { return t.value; })->value_or(0));
    for (const auto& [connectionId, peer] : peers) {
        Connection* connection = server->Find(connectionId);
        if (connection == nullptr) continue;
        for (const auto& input : registry->inputs) input->ensure(world, peer);
        // What cannot be read, or is of another server World, is dropped and counted on the Peer (D-12, D-4).
        std::array<std::uint64_t, kReplicaDropCount> dropped{};
        while (auto message = connection->Pop(kValuesChannel)) {
            NetReader reader(*message);
            std::uint8_t kind = 0;
            std::uint32_t generation = 0, epoch = 0;
            std::uint16_t wire = 0;
            if (!reader.Value(kind) || kind != static_cast<std::uint8_t>(Kind::Inputs) || !reader.Value(generation) ||
                !reader.Value(epoch)) {
                ++dropped[static_cast<std::size_t>(ReplicaDrop::Malformed)];
                continue;
            }
            if (generation != service->Generation()) {
                ++dropped[static_cast<std::size_t>(ReplicaDrop::StaleGeneration)];
                continue;
            }
            if (!reader.Value(wire) || wire >= registry->inputs.size()) {
                ++dropped[static_cast<std::size_t>(ReplicaDrop::Malformed)];
                continue;
            }
            if (!registry->inputs[wire]->fill(world, peer, reader, tick)) ++dropped[static_cast<std::size_t>(ReplicaDrop::Malformed)];
        }
        if (dropped != std::array<std::uint64_t, kReplicaDropCount>{}) {
            (void)world.Modify<PeerLedger>(peer, [&](PeerLedger& ledger) {
                for (std::size_t i = 0; i < kReplicaDropCount; ++i) ledger.drops[i] += dropped[i];
            });
        }
    }
    return {};
}

// Client, Main/Last before SendNet: once a tick, each input's last slots of its own Peer, Unreliable.
SystemResult SendInputs(ReplicaWorld world) {
    NetService* service = nullptr;
    std::shared_ptr<const NetRegistry> registry;
    Kernel::EntityId peer = 0;
    (void)world.Read<NetWorldLink>([&](const NetWorldLink& link) {
        service = link.service;
        registry = link.registry;
        peer = link.clientPeer;
    });
    ClientSession* client = service != nullptr ? service->Client() : nullptr;
    Connection* connection = client != nullptr && client->State() != ClientState::Disconnected ? client->Transport() : nullptr;
    if (connection == nullptr || !registry || peer == 0) return {};
    // One message a tick (D-11).
    const std::uint64_t tick = world.Read<WorldTime::Tick>([](const WorldTime::Tick& t) { return t.value; })->value_or(0);
    bool due = false;
    // The server World the inputs are for (small thing 2): the newest generation seen, else the one of the accept.
    std::uint32_t generation = client->Accepted() ? client->Accepted()->worldGeneration : 0;
    std::uint32_t epoch = 0;
    (void)world.Modify<NetReplicaState>([&](NetReplicaState& state) {
        due = !state.lastInputTick || *state.lastInputTick != tick;
        state.lastInputTick = tick;
        if (state.generation) generation = *state.generation;
        epoch = state.epoch;
    });
    if (!due) return {};
    for (const auto& entry : registry->inputs) {
        std::vector<std::byte> body;
        NetWriter writer(body);
        Head(writer, Kind::Inputs, generation, epoch);
        writer.Value(entry->wire);
        if (entry->encodeRecent(world, peer, writer)) (void)connection->Send(kValuesChannel, body);
    }
    return {};
}

} // namespace GYO::Engine::Net::Detail

namespace GYO::Engine::Net::Detail {

// Client, Main/Last before SendInputs: the clock sync (D-9; the formulas at ClockSync). The wall clock is read only
// here, in the IO layer, and never written into decision data: what changes the World is Tick (through the time
// Module's JumpFixedSteps) and TimeSyncScale.
SystemResult SyncClock(Kernel::ExclusiveWorld<Kernel::Writes<WorldTime::Tick, WorldTime::FixedTime, WorldTime::TimeSyncScale>> world) {
    NetService* service = nullptr;
    (void)world.Read<NetWorldLink>([&](const NetWorldLink& link) { service = link.service; });
    ClientSession* client = service != nullptr ? service->Client() : nullptr;
    const bool connected = client != nullptr &&
                           (client->State() == ClientState::Connected || client->State() == ClientState::Interrupted);
    ClockSync sync;
    (void)world.Modify<ClockSync>([&](ClockSync& kept) { sync = kept; });
    double scale = 1.0;
    if (!connected || client->Transport() == nullptr) {
        sync.synced = false;
        sync.consecutive = 0;
        sync.haveOffset = false;
    } else {
        const Connection& connection = *client->Transport();
        const auto step = static_cast<double>(
            world.Read<WorldTime::FixedTime>([](const WorldTime::FixedTime& t) { return t.accumulator.Step(); })->value_or(1));
        const auto overstep = static_cast<double>(
            world.Read<WorldTime::FixedTime>([](const WorldTime::FixedTime& t) { return t.accumulator.Overstep(); })->value_or(0));
        const auto tick = static_cast<double>(world.Read<WorldTime::Tick>([](const WorldTime::Tick& t) { return t.value; })->value_or(0));
        const NetTime now = service->Now();
        std::uint32_t base = 0;
        NetTime at{0};
        if (const auto stamp = connection.RemoteStamp()) {
            base = stamp->tick;
            at = connection.RemoteStampAt();
        } else {
            base = client->Accepted()->tick;
            at = client->AcceptedAt();
        }
        const auto rtt = static_cast<double>(connection.SmoothedRtt().count());
        const auto jitter = static_cast<double>(connection.Jitter().count());
        // A newer packet updates the smoothed offset between the Match's ticks and the local clock.
        if (!sync.haveOffset || base != sync.lastStampTick || at != sync.lastStampAt) {
            const double raw = static_cast<double>(base) + rtt / 2.0 / step - static_cast<double>(at.count()) / step;
            const double jitterMs = jitter / 1e6;
            const double position = std::clamp((jitterMs - sync.settings.lowJitterMs) / (sync.settings.highJitterMs - sync.settings.lowJitterMs), 0.0, 1.0);
            const double alpha = sync.settings.maxAlpha - position * (sync.settings.maxAlpha - sync.settings.minAlpha);
            sync.offset = sync.haveOffset ? sync.offset + alpha * (raw - sync.offset) : raw;
            sync.haveOffset = true;
            sync.lastStampTick = base;
            sync.lastStampAt = at;
        }
        const double remote = static_cast<double>(now.count()) / step + sync.offset;
        const double target = remote + (rtt / 2.0 + sync.settings.jitterMultiple * jitter) / step + sync.settings.marginTicks;
        const double error = target - (tick + overstep / step);
        sync.lastTarget = target;
        sync.lastError = error;
        if (!sync.synced || std::abs(error) > sync.settings.snapTicks) {
            const auto k = static_cast<std::int64_t>(std::llround(error));
            if (k != 0) WorldTime::JumpFixedSteps(world, k);
            sync.synced = true;
            sync.consecutive = 0;
            ++sync.jumps;
            sync.lastJump = k;
            sync.lastError = error - static_cast<double>(k);
        } else if (std::abs(error) > sync.settings.deadbandTicks) {
            const bool behind = error > 0.0;
            sync.consecutive = behind == sync.previousBehind ? static_cast<std::uint8_t>(sync.consecutive + 1) : 1;
            sync.previousBehind = behind;
            scale = sync.scale;
            if (sync.consecutive >= sync.settings.consecutive) scale = behind ? sync.settings.speedup : 1.0 / sync.settings.speedup;
        } else {
            sync.consecutive = 0;
        }
    }
    if (scale != sync.scale) ++sync.speedChanges;
    sync.scale = scale;
    (void)world.Modify<WorldTime::TimeSyncScale>([scale](WorldTime::TimeSyncScale& kept) { kept.scale = scale; });
    (void)world.Modify<ClockSync>([&](ClockSync& kept) { kept = sync; });
    return {};
}

} // namespace GYO::Engine::Net::Detail
