// World (spec A, F): storage, the checks of every access, the sync point's
// application of deferred records (spec F.2), hooks, the message cursors and
// trim (event cursor declaration 3.2), the boundary comparison (spec A.6 layer
// 2) and the World summary (spec D-9).

#include "engine/kernel/World.hpp"

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/IdIssuer.hpp"
#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace GYO::Kernel {

namespace Detail {

// The kernel's only way to change a Children value (Relationship.hpp: its
// mutators are private; DI-KS 29).
struct RelationshipStore {
    static void Append(Children& children, EntityId child) { children.Append(child); }
    static bool Erase(Children& children, EntityId child) noexcept { return children.Erase(child); }
    static Children Of(EntityId child) {
        Children made;
        made.Append(child);
        return made;
    }
};

} // namespace Detail

namespace {

bool SameError(const KernelError& a, const KernelError& b) {
    return a.code == b.code && a.rule == b.rule && a.message == b.message && a.detail == b.detail;
}

// Whether error, or any of its siblings at any depth, is the same error as wanted.
bool ContainsError(const KernelError& error, const KernelError& wanted) {
    if (SameError(error, wanted)) return true;
    return std::any_of(error.siblings.begin(), error.siblings.end(),
                       [&](const KernelError& sibling) { return ContainsError(sibling, wanted); });
}

// A hook names the sync point step that ran it, or the step of the exclusive
// System whose immediate change ran it; an observer is named as one.
std::string CallText(const Detail::SystemContext& call) {
    const bool syncPoint = call.hook && !call.immediateChange;
    return std::string(call.hook ? "hook '" : call.observer ? "observer '" : "System '") + std::string(call.name) + "' (schedule '" +
           std::string(call.schedule) + "', " + (syncPoint ? "sync point " : "") + "step " +
           std::to_string(call.planPos) + (call.immediateChange ? ", an immediate change" : "") + ")";
}

} // namespace

// ---- Construction

World::World(const Registry& registry, std::string name, WorldOptions options)
    : registry_(&registry), frozen_(&registry.Frozen()), data_(registry.data_.get()), name_(std::move(name)),
      options_(options) {
    // DI-KS 26: not while a round is pending (it would attach to a version it
    // never saw registered).
    GYO_ASSERT(registry.pending_ == nullptr);
    AttachVersion();
    // The kernel's own resource is present from the start (the Host or a System may overwrite it),
    // put in like the Host's SetResource: a mount's first run sees it changed (DI-KS 28).
    auto& settings = static_cast<Detail::ResourceSlot<KernelSettings>&>(*resources_[data_->kernelSettings]);
    settings.value = KernelSettings{};
    settings.changed = WriteStamp();
    ++data_->liveWorlds;
    if (!options_.dynamicInsert) ++data_->refusingWorlds;
}

void World::Sync() const {
    if (version_ != data_->version) const_cast<World&>(*this).AttachVersion();
}

// Declaration 3.2 rule 5: ids only grow, so new storage is appended; the hooks'
// access descriptions are all made again (a later round changes no existing
// one, rule 6, but nothing here relies on it). Made while no hook records, so
// a hook queue's address never changes while a hook records into it.
void World::AttachVersion() {
    GYO_ASSERT(state_ != WorldState::Running && state_ != WorldState::Applying);
    const Detail::RegistryData& data = *data_;
    GYO_ASSERT(data.columnMakers.size() == data.components.size());
    GYO_ASSERT(data.resourceMakers.size() == data.resources.size());
    GYO_ASSERT(data.messageMakers.size() == data.messages.size());
    for (std::size_t c = columns_.size(); c < data.columnMakers.size(); ++c) {
        GYO_ASSERT(data.columnMakers[c] != nullptr);
        columns_.push_back(data.columnMakers[c]());
    }
    for (std::size_t r = resources_.size(); r < data.resourceMakers.size(); ++r) resources_.push_back(data.resourceMakers[r]());
    for (std::size_t e = messages_.size(); e < data.messageMakers.size(); ++e) {
        messages_.push_back(data.messageMakers[e]());
        GYO_ASSERT(data.messages[e].capacity >= 1);
        messages_.back()->capacity = data.messages[e].capacity;
    }
    droppedEntry_.resize(messages_.size());
    // Event cursor declaration 3.2 (L5): a removed System's readers are gone
    // with the version that removed it.
    std::erase_if(cursors_, [&](const Detail::MessageCursor& cursor) {
        // An observer's cursor stays: observers are not removed.
        if (cursor.mount == Detail::kObserverMount) return false;
        return cursor.system >= data.systems.size() || data.systems[cursor.system].removed;
    });
    excluded_.resize(columns_.size(), false);
    hookAccess_.clear();
    for (const Detail::HookRecord& hook : data.hooks) hookAccess_.push_back(Detail::MakeAccessDesc(hook.access));
    observerAccess_.clear();
    for (const Detail::ObserverRecord& observer : data.observers) {
        observerAccess_.push_back(Detail::MakeAccessDesc(observer.access));
    }
    observerState_.resize(data.observers.size());
    for (const Detail::ScheduleRecord& schedule : data.schedules) {
        const bool known = std::any_of(hookQueues_.begin(), hookQueues_.end(),
                                       [&](const HookQueue& queue) { return queue.schedule == schedule.options.name; });
        if (!known) hookQueues_.push_back(HookQueue{schedule.options.name, {}, 0});
    }
    // Every table indexed by a registry id has the new version's size (a missed
    // one would be read past its end later; RC 17).
    GYO_ASSERT(messages_.size() == data.messages.size());
    GYO_ASSERT(excluded_.size() == data.components.size());
    GYO_ASSERT(hookAccess_.size() == data.hooks.size());
    GYO_ASSERT(observerAccess_.size() == data.observers.size() && observerState_.size() == data.observers.size());
    GYO_ASSERT(hookQueues_.size() == data.schedules.size());
    version_ = data.version;
}

World::HookQueue& World::HookQueueOf(std::string_view schedule) {
    for (HookQueue& queue : hookQueues_) {
        if (queue.schedule == schedule) return queue;
    }
    GYO_ASSERT(false && "no hook queue for this schedule");
    return hookQueues_.front();
}

// Spec W-7: columns in ascending ComponentId order, each in ascending EntityId order (Column's destructor).
// DI-KS 14: a thread-bound resource's value is destroyed only on its thread.
World::~World() {
    for (std::size_t r = 0; r < resources_.size(); ++r) CheckThread(static_cast<ResourceId>(r));
    for (auto& column : columns_) column.reset();
    --data_->liveWorlds;
    if (!options_.dynamicInsert) --data_->refusingWorlds;
}

void World::CheckThread(ResourceId id) const {
    if (!data_->resources[id].threadBound) return;
    const Detail::ResourceBase& slot = *resources_[id];
    GYO_ASSERT(slot.thread == 0 || slot.thread == Engine::Base::CurrentThreadSerial());
}

void World::BindThread(ResourceId id) {
    if (data_->resources[id].threadBound) resources_[id]->thread = Engine::Base::CurrentThreadSerial();
}

// ---- Public reads and Host writes

bool World::Alive(EntityId id) const {
    Sync();
    if (state_ == WorldState::Running || state_ == WorldState::Applying) {
        GYO_ASSERT(Detail::CurrentCall(const_cast<World&>(*this)) != nullptr);  // W-6
    }
    return std::binary_search(entities_.begin(), entities_.end(), id);
}

std::span<const EntityId> World::Entities() const {
    Sync();
    if (state_ == WorldState::Running || state_ == WorldState::Applying) {
        GYO_ASSERT(Detail::CurrentCall(const_cast<World&>(*this)) != nullptr);  // W-6
    }
    return entities_;
}

bool World::Despawn(EntityId id) {
    GYO_ASSERT(state_ == WorldState::Ready);  // W-3
    Sync();
    if (!std::binary_search(entities_.begin(), entities_.end(), id)) return false;
    auto& childOf = TypedColumn<ChildOf>(data_->childOf);
    auto& children = TypedColumn<Children>(data_->children);
    // DI-KS 32: the entity, then each child's whole subtree in turn (depth first).
    std::vector<EntityId> pending{id};
    while (!pending.empty()) {
        const EntityId one = pending.back();
        pending.pop_back();
        if (!std::binary_search(entities_.begin(), entities_.end(), one)) continue;
        std::vector<EntityId> kids;
        if (const Detail::Cell<Children>* cell = children.Find(one)) {
            for (EntityId kid : cell->value) {
                const Detail::Cell<ChildOf>* of = childOf.Find(kid);
                if (of != nullptr && of->value.parent == one) kids.push_back(kid);
            }
        }
        std::optional<EntityId> parent;
        if (const Detail::Cell<ChildOf>* cell = childOf.Find(one)) parent = cell->value.parent;
        for (std::size_t c = 0; c < columns_.size(); ++c) {
            if (columns_[c]->Contains(one)) {
                Access(Detail::EntryKind::Component, static_cast<std::uint32_t>(c), Detail::kModeWrite, true);
                columns_[c]->Erase(one);
            }
        }
        entities_.erase(std::lower_bound(entities_.begin(), entities_.end(), one));
        ++entitiesEpoch_;
        // DI-KS 30: the parent forgets it (no hook on the Host's path).
        if (parent) HostDetach(one, *parent);
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) pending.push_back(*it);
    }
    return true;
}

World::Detached World::DetachChild(EntityId child, EntityId parent, ChangeStamp stamp) {
    Detail::Cell<Children>* cell = TypedColumn<Children>(data_->children).Find(parent);
    if (cell == nullptr || !Detail::RelationshipStore::Erase(cell->value, child)) return Detached::No;
    cell->changed = stamp;
    return cell->value.empty() ? Detached::Emptied : Detached::Yes;
}

bool World::AttachChild(EntityId child, EntityId parent, ChangeStamp stamp) {
    auto& children = TypedColumn<Children>(data_->children);
    if (Detail::Cell<Children>* cell = children.Find(parent)) {
        Detail::RelationshipStore::Append(cell->value, child);
        cell->changed = stamp;
        return false;
    }
    children.Put(parent, Detail::RelationshipStore::Of(child), stamp);
    return true;
}

void World::HostDetach(EntityId child, EntityId parent) {
    const Detached detached = DetachChild(child, parent, WriteStamp());
    if (detached == Detached::No) return;
    MarkApiWritten(Detail::EntryKind::Component, data_->children);
    if (detached == Detached::Emptied) TypedColumn<Children>(data_->children).Erase(parent);
}

RejectedRelationship World::RejectedOf(EntityId entity, EntityId target, RelationshipPath path) const {
    RejectedRelationship rejected;
    rejected.entity = entity;
    rejected.target = target;
    rejected.reason = target == entity ? RelationshipRejectReason::SelfReference : RelationshipRejectReason::TargetNotAlive;
    rejected.relationship = data_->components[data_->childOf].name;
    rejected.syncEpoch = syncEpoch_;
    rejected.path = path;
    return rejected;
}

void World::HostInsertChildOf(EntityId child, ChildOf value) {
    auto& childOf = TypedColumn<ChildOf>(data_->childOf);
    const ChangeStamp stamp = WriteStamp();
    Detail::Cell<ChildOf>* existing = childOf.Find(child);
    if (value.parent == child || !std::binary_search(entities_.begin(), entities_.end(), value.parent)) {
        // DI-KS 31: the relationship does not hold; one that held before goes (no hook on this path).
        RejectedRelationship rejected = RejectedOf(child, value.parent, RelationshipPath::Host);
        rejected.emitter = "Host";
        RecordRejectedRelationship(std::move(rejected));
        if (existing != nullptr) {
            const EntityId old = existing->value.parent;
            childOf.Erase(child);
            HostDetach(child, old);
        }
        return;
    }
    if (existing != nullptr) {
        if (existing->value.parent == value.parent) {
            existing->changed = stamp;
            return;
        }
        HostDetach(child, existing->value.parent);
    }
    const EntityId parent = value.parent;
    childOf.Put(child, std::move(value), stamp);
    AttachChild(child, parent, stamp);
    MarkApiWritten(Detail::EntryKind::Component, data_->children);
}

bool World::HostRemoveChildOf(EntityId child) {
    auto& childOf = TypedColumn<ChildOf>(data_->childOf);
    const Detail::Cell<ChildOf>* existing = childOf.Find(child);
    if (existing == nullptr) return false;
    const EntityId parent = existing->value.parent;
    childOf.Erase(child);
    HostDetach(child, parent);
    return true;
}

std::uint64_t World::MembershipEpoch() const noexcept {
    std::uint64_t sum = entitiesEpoch_;
    for (const auto& column : columns_) sum += column->epoch;
    return sum;
}

void World::MakeAlive(EntityId id) {
    const auto at = std::lower_bound(entities_.begin(), entities_.end(), id);
    if (at != entities_.end() && *at == id) return;
    entities_.insert(at, id);
    ++entitiesEpoch_;
}

void World::Access(Detail::EntryKind kind, std::uint32_t id, std::uint8_t mode, bool structural) const {
    World& self = const_cast<World&>(*this);
    Detail::SystemContext* call = Detail::CurrentCall(self);
    switch (state_) {
    case WorldState::Running:
    case WorldState::Applying: {
        // W-6: in a run, only a System or hook of this World on this thread touches it.
        GYO_ASSERT(call != nullptr);
        // W-3: membership changes only at sync points.
        GYO_ASSERT(!structural);
        if (!verification_) return;
        const Detail::AccessDesc& access = *call->access;
        bool declared = false;
        // ExclusiveWorld declares every component and resource value (DI-KS 1); events are not in it.
        if (access.wholeWorld && kind != Detail::EntryKind::Message) declared = true;
        const auto entry = std::lower_bound(access.entries.begin(), access.entries.end(), std::make_pair(kind, id),
                                            [](const Detail::AccessEntry& e, const std::pair<Detail::EntryKind, std::uint32_t>& k) {
                                                return e.kind != k.first ? e.kind < k.first : e.id < k.second;
                                            });
        if (entry != access.entries.end() && entry->kind == kind && entry->id == id) {
            // A declared write also allows a read of the same storage.
            if ((entry->modes & mode) == mode || (mode == Detail::kModeRead && (entry->modes & Detail::kModeWrite) != 0)) {
                declared = true;
            }
        }
        if (declared) return;
        const char* what = kind == Detail::EntryKind::Component ? "component"
                           : kind == Detail::EntryKind::Resource ? "resource"
                                                                 : "message";
        const std::string& name = kind == Detail::EntryKind::Component ? data_->components[id].name
                                  : kind == Detail::EntryKind::Resource ? data_->resources[id].name
                                                                        : data_->messages[id].name;
        KernelError error(KernelErrorCode::AccessViolation,
                          std::string(mode == Detail::kModeWrite ? "write" : "read") + " of " + what + " '" + name +
                              "' through the World API, not declared by " + CallText(*call),
                          "id " + std::to_string(id));
        error.rule = "W-5";
        error.world = name_;
        error.Wrap(CallText(*call));
        if (breakOnKernelError_ && breakHook_) breakHook_(error);
        call->errors.push_back(std::move(error));
        return;
    }
    case WorldState::Interrupted:
        // After an Err the Host may read (diagnostics, the summary); writes need a rebuilt World.
        GYO_ASSERT(mode == Detail::kModeRead);
        return;
    case WorldState::Ready:
        if (mode == Detail::kModeRead) return;
        MarkApiWritten(kind, id);
        return;
    }
}

void World::MarkApiWritten(Detail::EntryKind kind, std::uint32_t id) const {
    std::vector<Snapshot>& snapshots = kind == Detail::EntryKind::Component  ? columnSnapshots_
                                       : kind == Detail::EntryKind::Resource ? resourceSnapshots_
                                                                             : messageSnapshots_;
    if (id < snapshots.size()) snapshots[id].apiWritten = true;
}

// ---- Handles' operations (Context.hpp)

namespace Detail {

SystemContext* CurrentCall(World& world) noexcept {
    SystemContext* call = CurrentThreadCall();
    return call != nullptr && call->world == &world ? call : nullptr;
}

SystemContext& RequireCall(World& world) {
    SystemContext* call = CurrentCall(world);
    GYO_ASSERT(call != nullptr);  // W-6
    return *call;
}

// The type lookups are every typed entry's first step (World API and handles),
// so they attach the World to a new Registry version (declaration 3.2 rule 5).
ComponentId ComponentIdOf(const World& world, TypeKey key) {
    world.Sync();
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Component);
    return entry->id;
}

ResourceId ResourceIdOf(const World& world, TypeKey key) {
    world.Sync();
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Resource);
    return entry->id;
}

MessageTypeId MessageIdOf(const World& world, TypeKey key) {
    world.Sync();
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Message);
    return entry->id;
}

EventTypeId EventIdOf(const World& world, TypeKey key) {
    world.Sync();
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Event);  // Registrar::Event
    return entry->id;
}

namespace {
void RequireRunningCall(World& world) {
    if (world.State() == WorldState::Running || world.State() == WorldState::Applying) {
        GYO_ASSERT(CurrentCall(world) != nullptr);  // W-6
    }
}
} // namespace

void CheckHandle(World& world, std::uint64_t callSerial, std::uint64_t syncEpoch, std::string_view what) {
    RequireRunningCall(world);
    if (!world.verification_) return;
    SystemContext* call = CurrentCall(world);
    if (call == nullptr) return;  // the Host in Ready: no call to report to; W-6 covers a run
    if (call->callSerial == callSerial && world.syncEpoch_ == syncEpoch) return;
    KernelError error(KernelErrorCode::StaleHandle,
                      std::string(what) + " handle used outside the call that fetched it" +
                          (world.syncEpoch_ != syncEpoch ? " (a sync point has passed since)" : ""),
                      "fetched in call " + std::to_string(callSerial) + ", used in call " +
                          std::to_string(call->callSerial));
    error.rule = "W-4";
    error.world = world.name_;
    error.Wrap(CallText(*call));
    if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
    call->errors.push_back(std::move(error));
}

ColumnBase& ColumnAt(World& world, ComponentId id) {
    RequireRunningCall(world);
    return *world.columns_[id];
}

ResourceBase& ResourceAt(World& world, ResourceId id) {
    RequireRunningCall(world);
    world.CheckThread(id);
    return *world.resources_[id];
}

// DI-KS 36: an extract source, read through const access while it is Ready.
const ColumnBase& SourceColumnAt(const World& source, ComponentId id) {
    return *source.columns_[id];
}

const ResourceBase& SourceResourceAt(const World& source, ResourceId id) {
    source.CheckThread(id);
    return *source.resources_[id];
}

MessageBufferBase& MessagesAt(World& world, MessageTypeId id) {
    RequireRunningCall(world);
    return *world.messages_[id];
}

ChangeStamp CurrentStamp(const World& world) noexcept {
    return world.stamp_;
}

std::span<const EntityId> AliveEntities(const World& world) noexcept {
    return world.entities_;
}

std::size_t ComponentCount(const World& world) noexcept {
    return world.columns_.size();
}

std::size_t ResourceCount(const World& world) noexcept {
    return world.resources_.size();
}

std::vector<ComponentId> SetMembersOf(const World& world, TypeKey setTag) {
    world.Sync();
    const TypeEntry* entry = world.data_->FindType(setTag);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::SetTag);
    return world.data_->namedSets[entry->id].members;
}

namespace {

bool CursorBefore(const MessageCursor& a, const MessageCursor& b) {
    return std::tie(a.message, a.system, a.mount, a.slot) < std::tie(b.message, b.system, b.mount, b.slot);
}

} // namespace

std::uint64_t MessageCursorOf(World& world, const SystemContext& call, MessageTypeId id) {
    RequireRunningCall(world);
    GYO_ASSERT(call.reader.valid);  // a hook is no reader (Registrar::Hook rejects MessageReader)
    const MessageCursor key{id, call.reader.system, call.reader.mount, call.reader.slot, 0};
    const auto at = std::lower_bound(world.cursors_.begin(), world.cursors_.end(), key, CursorBefore);
    if (at != world.cursors_.end() && !CursorBefore(key, *at)) return at->read;
    // The reader's first run: from the oldest record kept (every record before
    // it was trimmed or dropped, so the seq before it is where it stands).
    const MessageBufferBase& buffer = *world.messages_[id];
    const std::uint64_t start = buffer.Size() > 0 ? buffer.SeqAt(0) - 1 : buffer.nextSeq - 1;
    world.cursors_.insert(at, MessageCursor{id, call.reader.system, call.reader.mount, call.reader.slot, start});
    return start;
}

void AdvanceMessageCursor(World& world, const ReaderKey& reader, MessageTypeId id, std::uint64_t read) {
    const MessageCursor key{id, reader.system, reader.mount, reader.slot, 0};
    const auto at = std::lower_bound(world.cursors_.begin(), world.cursors_.end(), key, CursorBefore);
    GYO_ASSERT(at != world.cursors_.end() && !CursorBefore(key, *at));  // made by MessageCursorOf
    if (read > at->read) at->read = read;
}

void RecordDroppedMessage(World& world, MessageTypeId id, std::uint64_t seq) {
    DiagnosticsBase* found = FindDiagnostics(world, KeyOf<DroppedMessages>());
    if (found == nullptr) {
        auto made = std::make_unique<DiagnosticsBuffer<DroppedMessages>>();
        found = made.get();
        AddDiagnostics(world, KeyOf<DroppedMessages>(), std::move(made));
    }
    auto& entries = static_cast<DiagnosticsBuffer<DroppedMessages>*>(found)->entries;
    World::DroppedEntry& dropped = world.droppedEntry_[id];
    std::optional<std::size_t>& entry = dropped.entry;
    if (!entry) {
        DroppedMessages record;
        record.message = world.data_->messages[id].name;
        record.firstSeq = seq;
        // The World's sync point number at the first drop of the Run (L8).
        entries.push_back(DiagnosticsBuffer<DroppedMessages>::Entry{world.syncEpoch_, std::move(record)});
        entry = entries.size() - 1;
    }
    DroppedMessages& record = entries[*entry].record;
    ++record.count;
    record.lastSeq = seq;
    // The readers that held seq unread, merged with this Run's earlier ones in
    // cursor order (registrationSeq, mount, slot).
    for (const MessageCursor& cursor : world.cursors_) {
        if (cursor.message != id || cursor.read >= seq) continue;
        const auto at = std::lower_bound(dropped.readers.begin(), dropped.readers.end(), cursor, CursorBefore);
        if (at == dropped.readers.end() || CursorBefore(cursor, *at)) dropped.readers.insert(at, cursor);
    }
    record.readers.clear();
    for (const MessageCursor& reader : dropped.readers) record.readers.push_back(world.ReaderInfoOf(reader));
}

void Push(SystemContext& call, Record record) {
    World& world = *call.world;
    GYO_ASSERT(world.state_ == WorldState::Running || world.state_ == WorldState::Applying);
    // F-5: while Applying only hooks and observers record (into the next window).
    GYO_ASSERT(world.state_ != WorldState::Applying || call.hook || call.observer);
    record.planPos = call.planPos;
    record.recSeq = (*call.recSeq)++;
    record.emitter = call.name;
    record.emitterHook = call.hook;
    record.emitterKey = call.access;
    call.queue->push_back(std::move(record));
}

KernelError CallError(SystemContext& call, KernelErrorCode code, std::string rule, std::string message) {
    World& world = *call.world;
    KernelError error(code, std::move(message));
    error.rule = std::move(rule);
    error.world = world.name_;
    error.Wrap(CallText(call));
    if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
    call.returned.push_back(error);
    return error;
}

KernelError CallerError(SystemContext& call, KernelErrorCode code, std::string rule, std::string message) {
    KernelError error(code, std::move(message));
    error.rule = std::move(rule);
    error.world = call.world->name_;
    error.Wrap(CallText(call));
    return error;
}

std::uint64_t CurrentSyncEpoch(const World& world) noexcept {
    return world.syncEpoch_;
}

ChangeStamp NextStamp(World& world) noexcept {
    return ++world.stamp_;
}

ResourceBase* AllotterSlot(World& world) noexcept {
    return world.data_->allotter ? world.resources_[*world.data_->allotter].get() : nullptr;
}

std::optional<EntityId> Allot(World& world) {
    ResourceBase* slot = AllotterSlot(world);
    GYO_ASSERT(slot != nullptr && world.data_->allotterOps.allot != nullptr);  // D-E3 at Finish
    slot->changed = world.stamp_;  // W-11: the allotter is written
    return world.data_->allotterOps.allot(*slot);
}

DiagnosticsBase* FindDiagnostics(World& world, TypeKey key) noexcept {
    for (auto& [k, buffer] : world.diagnostics_) {
        if (k == key) return buffer.get();
    }
    return nullptr;
}

void AddDiagnostics(World& world, TypeKey key, std::unique_ptr<DiagnosticsBase> buffer) {
    world.diagnostics_.emplace_back(key, std::move(buffer));
}

// The Executor's merge of one call's errors (spec B.4 point 7, E.2 step 5):
// violations, boundary differences, kernel errors the System dropped and the
// System's own Err, as one sibling list (one error alone is returned as is).
std::optional<KernelError> CombineCall(SystemContext& call, Engine::Base::Result<void, KernelError> result,
                                       const std::vector<std::string>& changed) {
    World& world = *call.world;
    std::vector<KernelError> list = std::move(call.errors);
    for (const std::string& storage : changed) {
        KernelError error(KernelErrorCode::BoundaryChange,
                          "'" + storage + "' changed during " + CallText(call) + ", which does not declare writing it");
        error.rule = "A.6";
        error.world = world.name_;
        error.Wrap(CallText(call));
        if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
        list.push_back(std::move(error));
    }
    if (result) {
        for (KernelError& dropped : call.returned) list.push_back(std::move(dropped));
    } else {
        KernelError own = std::move(result).error();
        for (KernelError& returned : call.returned) {
            if (!ContainsError(own, returned)) list.push_back(std::move(returned));
        }
        list.push_back(std::move(own));
    }
    if (list.empty()) return std::nullopt;
    if (list.size() == 1) return std::move(list.front());
    KernelError siblings(KernelErrorCode::Siblings, std::to_string(list.size()) + " errors in " + CallText(call));
    siblings.world = world.name_;
    siblings.siblings = std::move(list);
    return siblings;
}

} // namespace Detail

// ---- Hooks (spec F.4)

Engine::Base::Result<void, KernelError> World::RunHook(std::size_t hook, EntityId entity, ComponentId component,
                                                       std::uint32_t planPos, std::string_view schedule, ChangeStamp stamp) {
    const Detail::HookRecord& record = data_->hooks[hook];
    Detail::SystemContext call;
    call.world = this;
    call.executor = executor_;
    call.access = &hookAccess_[hook];
    call.name = record.name;
    call.schedule = schedule;
    call.planPos = planPos;
    call.hook = true;
    call.stamp = stamp;
    call.lastRun = 0;
    call.callSerial = ++callSerial_;
    call.syncEpoch = syncEpoch_;
    // S5: a hook an immediate change ran records into that change's own queue.
    call.immediateChange = hookTarget_ != nullptr;
    HookQueue& queue = hookTarget_ != nullptr ? *hookTarget_ : HookQueueOf(schedule);
    call.queue = &queue.records;
    call.recSeq = &queue.recSeq;
    const std::size_t recordsBefore = queue.records.size();
    call.hookEntity = entity;
    call.hookComponent = component;
    // Layer 2 around a hook (spec A.6): a hook writes no storage directly (its
    // writes are deferred records, events and diagnostics), so every storage but
    // the event buffers it declares must be unchanged by the call.
    if (verification_) TakeSnapshots();
    Engine::Base::Result<void, KernelError> result = [&]() -> Engine::Base::Result<void, KernelError> {
        Detail::CallScope scope(call);
        return record.run(call);
    }();
    if (trackRoundRecorders_ && queue.records.size() > recordsBefore) roundRecorders_.push_back(hook);
    std::vector<std::string> changed;
    if (verification_) changed = CompareAfter(hookAccess_[hook]);
    std::optional<KernelError> error = Detail::CombineCall(call, std::move(result), changed);
    if (!error) return {};
    // One more layer of the context chain (vertical), not a sibling: the leaves keep their codes and rules.
    error->Wrap("hook '" + record.name + "' at " +
                (call.immediateChange ? "step " + std::to_string(planPos) + " (an immediate change)"
                                      : "sync point step " + std::to_string(planPos)) +
                " of schedule '" + std::string(schedule) + "'");
    return Engine::Base::Err(std::move(*error));
}

// Per (entity, component), the lifecycle reactions of one cell (time-events
// declaration 4.2; flecs and Bevy order them so): a new cell's OnAdd hooks,
// then its Add observers; a leaving cell's Remove observers, then its OnRemove
// hooks (the cell still readable by both).
Engine::Base::Result<void, KernelError> World::RunHooks(HookTrigger trigger, EntityId entity, ComponentId component,
                                                        std::uint32_t planPos, std::string_view schedule,
                                                        ChangeStamp stamp) {
    if (trigger == HookTrigger::OnRemove) {
        auto observed = RunObservers(ObserverTrigger::Remove, component, nullptr, entity, planPos, schedule, stamp, false);
        if (!observed) return observed;
    }
    for (std::size_t h = 0; h < data_->hooks.size(); ++h) {
        const Detail::HookRecord& hook = data_->hooks[h];
        if (hook.trigger != trigger || !hook.componentId || *hook.componentId != component) continue;
        auto result = RunHook(h, entity, component, planPos, schedule, stamp);
        if (!result) return result;
    }
    if (trigger == HookTrigger::OnAdd) {
        return RunObservers(ObserverTrigger::Add, component, nullptr, entity, planPos, schedule, stamp, false);
    }
    return {};
}

// ---- Observers (event cursor declaration 3.3)

Engine::Base::Result<void, KernelError> World::RunObservers(ObserverTrigger trigger, std::uint32_t id, const void* payload,
                                                            std::optional<EntityId> target, std::uint32_t planPos,
                                                            std::string_view schedule, ChangeStamp stamp, bool ownStamp) {
    for (const std::uint32_t index : data_->observerOrder) {
        const Detail::ObserverRecord& record = data_->observers[index];
        if (record.trigger != trigger || !record.observedId || *record.observedId != id) continue;
        ObserverState& state = observerState_[index];
        Detail::SystemContext call;
        call.world = this;
        call.executor = executor_;
        call.access = &observerAccess_[index];
        call.name = record.name;
        call.schedule = schedule;
        call.planPos = planPos;
        call.observer = true;
        call.immediateChange = (hookTarget_ != nullptr);
        call.eventKey = record.event;
        call.eventPayload = payload;
        call.eventTarget = target;
        call.stamp = ownStamp ? ++stamp_ : stamp;
        call.lastRun = state.lastRun;
        call.callSerial = ++callSerial_;
        call.syncEpoch = syncEpoch_;
        // An observer is a reader of its own (event cursor declaration 3.2).
        call.reader = Detail::ReaderKey{index, Detail::kObserverMount, 0, true};
        call.locals = &state.locals;
        HookQueue& into = call.immediateChange ? *hookTarget_ : HookQueueOf(schedule);
        call.queue = &into.records;
        call.recSeq = &into.recSeq;
        const std::size_t recordsBefore = into.records.size();
        // Layer 2 around the call (spec A.6), like a hook's.
        if (verification_) TakeSnapshots();
        Engine::Base::Result<void, KernelError> result = [&]() -> Engine::Base::Result<void, KernelError> {
            Detail::CallScope scope(call);
            return record.run(call);
        }();
        state.lastRun = call.stamp;
        if (trackRoundRecorders_ && into.records.size() > recordsBefore) roundObserverRecorders_.push_back(index);
        std::vector<std::string> changed;
        if (verification_) changed = CompareAfter(observerAccess_[index]);
        std::optional<KernelError> error = Detail::CombineCall(call, std::move(result), changed);
        if (!error) continue;
        const char* what = trigger == ObserverTrigger::Add ? "Add of component '"
                           : trigger == ObserverTrigger::Remove ? "Remove of component '"
                                                                : "event '";
        const std::string& subject =
            trigger == ObserverTrigger::Event ? data_->events[id].name : data_->components[id].name;
        error->Wrap("observer '" + record.name + "' of " + what + subject + "' at " +
                    (call.immediateChange ? "step " + std::to_string(planPos) + " (outside a sync point)"
                                          : "sync point step " + std::to_string(planPos)) +
                    " of schedule '" + std::string(schedule) + "'");
        return Engine::Base::Err(std::move(*error));
    }
    return {};
}

// ---- The sync point (spec F.2)

namespace {

struct NewCell {
    EntityId entity;
    ComponentId component;
};

} // namespace

Engine::Base::Result<void, KernelError> World::ApplySync(std::uint32_t planPos, std::string_view schedule,
                                                         ChangeStamp stamp, std::vector<Detail::Record>* immediate,
                                                         Detail::Derive derive, Detail::Relationships relationships) {
    using Detail::Record;
    using Detail::RecordKind;
    using Result = Engine::Base::Result<void, KernelError>;

    // Spec F-13: the ids spawned at this sync point (both phases), ascending, with who spawned each.
    std::vector<std::pair<EntityId, std::string_view>> spawnedHere;
    // The trigger records of both phases, in order (delivered by the last step).
    std::vector<Detail::Record*> triggers;

    // K0: the running level's records start at its windowBase; the records
    // before it are the outer levels', applied later at their own sync points.
    const std::size_t base = runStack_.empty() ? 0 : runStack_.back().windowBase;
    GYO_ASSERT(base <= window_.size());
    // The level being applied is the innermost one (its records are the tail).
    GYO_ASSERT(runStack_.empty() || runStack_.back().schedule == schedule);

    // One phase: step 0 applies the hook queue this way, steps 1 to 7 the window's records.
    auto phase = [&](std::vector<Record>& records) -> Result {
        // The triggers (Commands::Trigger) wait for the last step, in record order.
        for (Record& r : records) {
            if (r.kind == RecordKind::Trigger) triggers.push_back(&r);
        }
        // Relationships (DI-KS 30 to 33): the parents whose Children became empty in this phase (removed
        // at step 5b if still empty: the net change), and the ChildOf cells this phase added or gave a new
        // parent, attached at step 5b in (planPos, recSeq) order (the user's Q1 (A')).
        auto& childOf = TypedColumn<ChildOf>(data_->childOf);
        auto& children = TypedColumn<Children>(data_->children);
        const bool keep = relationships != Detail::Relationships::RestoreGiven;
        std::vector<EntityId> emptied;
        // Each Children this phase touches, as it was when first touched, with its changed stamp: one that
        // ends the phase as it began keeps its stamp (DI-KS 30: the stamp moves only when the list changes).
        std::map<EntityId, std::pair<ChangeStamp, Children>> before;
        auto touch = [&](EntityId parent) {
            if (before.contains(parent)) return;
            if (const Detail::Cell<Children>* cell = children.Find(parent)) {
                before.emplace(parent, std::make_pair(cell->changed, cell->value));
            }
        };
        struct Attach {
            std::uint32_t planPos;
            std::uint32_t recSeq;
            const Record* record;
        };
        std::map<EntityId, Attach> attaches;
        auto detach = [&](EntityId child, EntityId parent) {
            if (!keep) return;
            touch(parent);
            if (DetachChild(child, parent, stamp) == Detached::Emptied) emptied.push_back(parent);
        };
        auto alive = [&](EntityId id) { return std::binary_search(entities_.begin(), entities_.end(), id); };
        const RelationshipPath path = immediate == nullptr ? RelationshipPath::SyncPoint
                                      : derive == Detail::Derive::Nothing ? RelationshipPath::Restore
                                                                          : RelationshipPath::ImmediateChange;
        auto rejectRelationship = [&](const Record& record, EntityId entity, EntityId target) {
            RejectedRelationship rejected = RejectedOf(entity, target, path);
            rejected.emitter = std::string(record.emitter);
            rejected.emitterIsHook = record.emitterHook;
            rejected.schedule = std::string(schedule);
            rejected.step = record.planPos;
            rejected.appliedAt = planPos;
            RecordRejectedRelationship(std::move(rejected));
        };
        auto holds = [&](EntityId entity, EntityId target) { return target != entity && alive(target); };

        // Step 2: despawn, ascending EntityId, only entities alive now, idempotent.
        std::vector<EntityId> despawns;
        // The user's Q3: a child whose ChildOf this phase removes or replaces is not despawned with its
        // parent; its ChildOf stays until that record is applied (a replacement is then an overwrite).
        std::vector<EntityId> rewired;
        for (const Record& r : records) {
            if (r.kind == RecordKind::Despawn) despawns.push_back(r.entity);
            if ((r.kind == RecordKind::Remove || r.kind == RecordKind::Insert) && r.id == data_->childOf) {
                // The same parent again is no detach (R-1, the user 2026-11-23): the child goes with it.
                const Detail::Cell<ChildOf>* now = childOf.Find(r.entity);
                const bool sameParent = r.kind == RecordKind::Insert && now != nullptr &&
                                        static_cast<const Detail::TypedBox<ChildOf>&>(*r.value).value.parent == now->value.parent;
                if (!sameParent) rewired.push_back(r.entity);
            }
        }
        std::sort(despawns.begin(), despawns.end());
        despawns.erase(std::unique(despawns.begin(), despawns.end()), despawns.end());
        std::sort(rewired.begin(), rewired.end());
        const bool expandLinked = relationships == Detail::Relationships::Change;
        std::vector<EntityId> pending;
        for (EntityId root : despawns) {
            // DI-KS 32 (the user's Q4): an entity, then each of its Children's whole subtree in turn, depth first.
            pending.assign(1, root);
            while (!pending.empty()) {
                const EntityId id = pending.back();
                pending.pop_back();
                if (!alive(id)) continue;
                std::vector<EntityId> kids;
                if (const Detail::Cell<Children>* cell = children.Find(id)) kids.assign(cell->value.begin(), cell->value.end());
                std::optional<EntityId> parent;
                if (const Detail::Cell<ChildOf>* cell = childOf.Find(id)) parent = cell->value.parent;
                // OnRemove for every component first (ascending ComponentId), so each hook still reads the whole
                // entity; then destroy in ascending ComponentId (W-7). (Decided by the user, v8-arch-gyo2-despawn-notify-all-then-destroy.)
                for (std::size_t c = 0; c < columns_.size(); ++c) {
                    if (!columns_[c]->Contains(id)) continue;
                    auto hooked = RunHooks(HookTrigger::OnRemove, id, static_cast<ComponentId>(c), planPos, schedule, stamp);
                    if (!hooked) return hooked;
                }
                for (std::size_t c = 0; c < columns_.size(); ++c) {
                    columns_[c]->Erase(id);
                }
                entities_.erase(std::lower_bound(entities_.begin(), entities_.end(), id));
                ++entitiesEpoch_;
                if (parent) detach(id, *parent);
                std::vector<EntityId> linked;
                for (EntityId kid : kids) {
                    // Only a child whose ChildOf names this entity (a Children a restore gives may say otherwise).
                    const Detail::Cell<ChildOf>* cell = childOf.Find(kid);
                    if (cell == nullptr || cell->value.parent != id) continue;
                    if (std::binary_search(rewired.begin(), rewired.end(), kid)) continue;  // Q3: its record decides
                    if (expandLinked) {
                        linked.push_back(kid);
                        continue;
                    }
                    // A restore leaves it behind: its ChildOf goes now, after its OnRemove; one the restore
                    // despawns later in this step keeps it for its own despawn.
                    if (std::binary_search(despawns.begin(), despawns.end(), kid)) continue;
                    auto hooked = RunHooks(HookTrigger::OnRemove, kid, data_->childOf, planPos, schedule, stamp);
                    if (!hooked) return hooked;
                    childOf.Erase(kid);
                }
                for (auto it = linked.rbegin(); it != linked.rend(); ++it) pending.push_back(*it);
            }
        }

        // Step 3: remove, (EntityId, ComponentId) ascending; OnRemove while the cell is still readable.
        std::vector<std::pair<EntityId, ComponentId>> removes;
        for (const Record& r : records) {
            if (r.kind == RecordKind::Remove) removes.emplace_back(r.entity, r.id);
        }
        std::sort(removes.begin(), removes.end());
        removes.erase(std::unique(removes.begin(), removes.end()), removes.end());
        for (const auto& [id, component] : removes) {
            if (!columns_[component]->Contains(id)) continue;
            auto hooked = RunHooks(HookTrigger::OnRemove, id, component, planPos, schedule, stamp);
            if (!hooked) return hooked;
            std::optional<EntityId> parent;
            if (component == data_->childOf) parent = childOf.Find(id)->value.parent;
            columns_[component]->Erase(id);
            if (parent) detach(id, *parent);
        }

        std::vector<NewCell> added;

        // Step 4: spawn, (EntityId, planPos, recSeq) ascending, the key unique in a
        // phase (F.1, F.6). A record that cannot spawn is dropped and recorded (spec
        // F-13), not an error. Each record takes the first reason that holds of: its
        // id is in this World's allotter range (SpawnWithId only); the id is spawned
        // twice at this sync point; the id is alive. Of the records of one id left
        // after the range check, those of one emitter keep the first (recSeq order);
        // those of two or more emitters are all dropped, so the result never depends
        // on the registration order of Systems or hooks that do not conflict.
        std::vector<Record*> spawns;
        for (Record& r : records) {
            if (r.kind == RecordKind::Spawn) spawns.push_back(&r);
        }
        auto spawnKey = [](const Record* r) { return std::make_tuple(r->entity, r->planPos, r->recSeq); };
        std::sort(spawns.begin(), spawns.end(),
                  [&](const Record* a, const Record* b) { return spawnKey(a) < spawnKey(b); });
        for (std::size_t i = 1; i < spawns.size(); ++i) {
            GYO_ASSERT(spawnKey(spawns[i - 1]) != spawnKey(spawns[i]));  // the key is unique in a phase (F.1)
        }
        auto reject = [&](const Record& spawn, SpawnRejectReason reason, std::string_view other) {
            RejectedSpawn rejected;
            rejected.entity = spawn.entity;
            rejected.reason = reason;
            rejected.emitter = std::string(spawn.emitter);
            rejected.emitterIsHook = spawn.emitterHook;
            rejected.fromAllotter = !spawn.withId;
            rejected.schedule = std::string(schedule);
            rejected.step = spawn.planPos;
            rejected.appliedAt = planPos;
            rejected.syncEpoch = syncEpoch_;
            rejected.otherEmitter = std::string(other);
            RecordRejectedSpawn(std::move(rejected));
        };
        std::vector<Record*> accepted;
        for (std::size_t first = 0; first < spawns.size();) {
            const EntityId id = spawns[first]->entity;
            std::size_t end = first + 1;
            while (end < spawns.size() && spawns[end]->entity == id) ++end;
            // One verdict per record of the id; the dropped ones are recorded afterwards, in key order.
            std::vector<std::optional<std::pair<SpawnRejectReason, std::string_view>>> dropped(end - first);
            std::vector<std::size_t> candidates;
            for (std::size_t i = first; i < end; ++i) {
                if (spawns[i]->withId && !spawns[i]->restore && data_->allotter &&
                    data_->allotterOps.owns(*resources_[*data_->allotter], id)) {
                    dropped[i - first].emplace(SpawnRejectReason::IdInAllotterRange, std::string_view{});
                } else {
                    candidates.push_back(i);
                }
            }
            if (!candidates.empty()) {
                const auto here = std::lower_bound(spawnedHere.begin(), spawnedHere.end(), id,
                                                   [](const std::pair<EntityId, std::string_view>& s, EntityId key) {
                                                       return s.first < key;
                                                   });
                const bool spawnedAtThisSyncPoint = here != spawnedHere.end() && here->first == id;
                const Record& head = *spawns[candidates.front()];
                if (std::binary_search(entities_.begin(), entities_.end(), id)) {
                    for (std::size_t c : candidates) {
                        if (spawnedAtThisSyncPoint) {
                            dropped[c - first].emplace(SpawnRejectReason::IdSpawnedTwice, here->second);
                        } else {
                            dropped[c - first].emplace(SpawnRejectReason::IdAlive, std::string_view{});
                        }
                    }
                } else if (std::all_of(candidates.begin(), candidates.end(),
                                       [&](std::size_t c) { return spawns[c]->emitterKey == head.emitterKey; })) {
                    for (std::size_t k = 1; k < candidates.size(); ++k) {
                        dropped[candidates[k] - first].emplace(SpawnRejectReason::IdSpawnedTwice, head.emitter);
                    }
                    // An id spawned by step 0 and despawned by this phase is listed already.
                    if (!spawnedAtThisSyncPoint) spawnedHere.insert(here, {id, head.emitter});
                    accepted.push_back(spawns[candidates.front()]);
                } else {
                    for (std::size_t c : candidates) {
                        std::string_view other;
                        for (std::size_t o : candidates) {
                            if (spawns[o]->emitterKey != spawns[c]->emitterKey) {
                                other = spawns[o]->emitter;
                                break;
                            }
                        }
                        dropped[c - first].emplace(SpawnRejectReason::IdSpawnedTwice, other);
                    }
                }
            }
            for (std::size_t i = first; i < end; ++i) {
                if (dropped[i - first]) reject(*spawns[i], dropped[i - first]->first, dropped[i - first]->second);
            }
            first = end;
        }
        for (Record* candidate : accepted) {
            Record& spawn = *candidate;
            MakeAlive(spawn.entity);
            for (Detail::BundleValue& part : spawn.bundle) {
                if (part.value->PutInto(*columns_[part.component], spawn.entity, stamp)) {
                    added.push_back(NewCell{spawn.entity, part.component});
                }
                if (part.component == data_->childOf) attaches[spawn.entity] = Attach{spawn.planPos, spawn.recSeq, &spawn};
            }
        }

        // Step 5: insert, (EntityId, ComponentId, planPos, recSeq) ascending; the
        // largest (planPos, recSeq) of one (entity, component) wins.
        std::vector<Record*> inserts;
        for (Record& r : records) {
            if (r.kind == RecordKind::Insert) inserts.push_back(&r);
        }
        auto insertKey = [](const Record* r) { return std::make_tuple(r->entity, r->id, r->planPos, r->recSeq); };
        std::sort(inserts.begin(), inserts.end(),
                  [&](const Record* a, const Record* b) { return insertKey(a) < insertKey(b); });
        for (std::size_t i = 1; i < inserts.size(); ++i) {
            GYO_ASSERT(insertKey(inserts[i - 1]) != insertKey(inserts[i]));  // the key is unique in a window (F.1)
        }
        for (std::size_t i = 0; i < inserts.size();) {
            std::size_t end = i + 1;
            while (end < inserts.size() && inserts[end]->entity == inserts[i]->entity && inserts[end]->id == inserts[i]->id) {
                ++end;
            }
            Record& winner = *inserts[end - 1];
            if (!std::binary_search(entities_.begin(), entities_.end(), winner.entity)) {
                droppedMissingEntity_ += end - i;
                i = end;
                continue;
            }
            if (winner.id == data_->childOf) {
                // DI-KS 31: checked before it is written, so an OnRemove of a replaced ChildOf reads its old value.
                const EntityId target = static_cast<Detail::TypedBox<ChildOf>&>(*winner.value).value.parent;
                Detail::Cell<ChildOf>* existing = childOf.Find(winner.entity);
                // Put by this phase's spawn: never notified, not yet attached.
                const bool spawned = attaches.contains(winner.entity);
                if (!holds(winner.entity, target)) {
                    rejectRelationship(winner, winner.entity, target);
                    if (existing != nullptr && spawned) {
                        childOf.Erase(winner.entity);
                        std::erase_if(added, [&](const NewCell& c) {
                            return c.entity == winner.entity && c.component == data_->childOf;
                        });
                        attaches.erase(winner.entity);
                    } else if (existing != nullptr) {
                        // A ChildOf that held: removed, as Bevy removes a relationship whose target is gone.
                        const EntityId old = existing->value.parent;
                        auto hooked = RunHooks(HookTrigger::OnRemove, winner.entity, data_->childOf, planPos, schedule, stamp);
                        if (!hooked) return hooked;
                        childOf.Erase(winner.entity);
                        detach(winner.entity, old);
                    }
                    i = end;
                    continue;
                }
                if (existing != nullptr && !spawned) {
                    if (existing->value.parent == target) {
                        // The same parent again: written (Changed<ChildOf>), Children unchanged.
                        existing->changed = stamp;
                        i = end;
                        continue;
                    }
                    detach(winner.entity, existing->value.parent);
                }
                attaches[winner.entity] = Attach{winner.planPos, winner.recSeq, &winner};
            }
            if (winner.value->PutInto(*columns_[winner.id], winner.entity, stamp)) {
                added.push_back(NewCell{winner.entity, winner.id});
            }
            i = end;
        }

        const auto byCell = [](const NewCell& a, const NewCell& b) {
            return a.entity != b.entity ? a.entity < b.entity : a.component < b.component;
        };

        // Step 5b: relationships (DI-KS 30, 31), after insert and before the initial values, so an
        // initial value from ChildOf never rests on a ChildOf that does not hold, and one from Children
        // sees the Children cells made here. A spawned ChildOf is checked here (its parent may be spawned
        // later in step 4; one that does not hold goes without a hook: nothing saw it); an inserted one was
        // checked before it was written, and nothing since changed who is alive. Attached in (planPos,
        // recSeq) order (Q1 (A')).
        std::vector<std::tuple<std::uint32_t, std::uint32_t, EntityId>> order;
        for (const auto& [child, attach] : attaches) {
            const Detail::Cell<ChildOf>* cell = childOf.Find(child);
            GYO_ASSERT(cell != nullptr);
            if (!holds(child, cell->value.parent)) {
                rejectRelationship(*attach.record, child, cell->value.parent);
                childOf.Erase(child);
                std::erase_if(added, [&](const NewCell& c) { return c.entity == child && c.component == data_->childOf; });
                continue;
            }
            order.emplace_back(attach.planPos, attach.recSeq, child);
        }
        if (keep) {
            std::sort(order.begin(), order.end());
            for (const auto& [pos, seq, child] : order) {
                const EntityId parent = childOf.Find(child)->value.parent;
                touch(parent);
                if (AttachChild(child, parent, stamp)) added.push_back(NewCell{parent, data_->children});
            }
            // The net change: a parent that lost its last child and got none back loses its Children cell.
            std::sort(emptied.begin(), emptied.end());
            emptied.erase(std::unique(emptied.begin(), emptied.end()), emptied.end());
            for (EntityId parent : emptied) {
                const Detail::Cell<Children>* cell = children.Find(parent);
                if (!alive(parent) || cell == nullptr || !cell->value.empty()) continue;
                auto hooked = RunHooks(HookTrigger::OnRemove, parent, data_->children, planPos, schedule, stamp);
                if (!hooked) return hooked;
                children.Erase(parent);
            }
            // A Children that ends the phase as it began keeps its changed stamp.
            for (const auto& [parent, was] : before) {
                Detail::Cell<Children>* cell = children.Find(parent);
                if (cell != nullptr && cell->value == was.second) cell->changed = was.first;
            }
        }

        // Step 5a: initial values (Registrar::InitialValue). Every cell added by
        // steps 4 and 5 is looked at once, (EntityId, ComponentId) ascending; a
        // cell this step adds is looked at in the next round, so a chain of
        // declarations completes at this sync point. A Target the entity
        // already has is left as it is, so a round adds each cell at most once
        // and the rounds end. Only an application that derives (a change, not a restore: DI-KS 12).
        if (derive == Detail::Derive::InitialValues && !data_->initialValues.empty()) {
            const auto& rules = data_->initialValues;
            std::vector<NewCell> round = added;
            while (!round.empty()) {
                std::sort(round.begin(), round.end(), byCell);
                std::vector<NewCell> next;
                for (const NewCell& cell : round) {
                    auto rule = std::lower_bound(rules.begin(), rules.end(), cell.component,
                                                 [](const Detail::InitialValueRecord& r, ComponentId request) {
                                                     return r.requestId < request;
                                                 });
                    for (; rule != rules.end() && rule->requestId == cell.component; ++rule) {
                        if (rule->put(*columns_[rule->requestId], *columns_[rule->targetId], cell.entity, stamp)) {
                            next.push_back(NewCell{cell.entity, rule->targetId});
                        }
                    }
                }
                added.insert(added.end(), next.begin(), next.end());
                round = std::move(next);
            }
        }

        // Step 6: OnAdd for new cells, (EntityId, ComponentId) ascending; an overwrite is not an add.
        std::sort(added.begin(), added.end(), byCell);
        for (const NewCell& cell : added) {
            auto hooked = RunHooks(HookTrigger::OnAdd, cell.entity, cell.component, planPos, schedule, stamp);
            if (!hooked) return hooked;
        }

        // Step 7: resource operations, (ResourceId, planPos, recSeq) ascending.
        std::vector<Record*> resourceOps;
        for (Record& r : records) {
            if (r.kind == RecordKind::ResourceInsert || r.kind == RecordKind::ResourceRemove) resourceOps.push_back(&r);
        }
        std::sort(resourceOps.begin(), resourceOps.end(), [](const Record* a, const Record* b) {
            return std::make_tuple(a->id, a->planPos, a->recSeq) < std::make_tuple(b->id, b->planPos, b->recSeq);
        });
        for (Record* op : resourceOps) {
            Detail::ResourceBase& slot = *resources_[op->id];
            CheckThread(op->id);
            if (op->kind == RecordKind::ResourceInsert) {
                op->value->PutInto(slot);
                BindThread(op->id);
            } else {
                slot.Reset();
                slot.thread = 0;
            }
            slot.changed = stamp;
        }
        return {};
    };

    // The last step (time-events declaration 4.2): the observers of every
    // trigger record, step 0's first, each phase's in record order, after the
    // resource step and before the sync epoch moves, so they see every change
    // of this sync point. What they record goes to the next window.
    auto deliver = [&]() -> Result {
        for (const Record* r : triggers) {
            auto observed = RunObservers(ObserverTrigger::Event, r->id, r->value->Payload(),
                                         r->hasTarget ? std::optional<EntityId>(r->entity) : std::nullopt, planPos,
                                         schedule, stamp, false);
            if (!observed) return observed;
        }
        return {};
    };

    // S5 (declaration 3.5 point 1): an immediate change, or one round of what
    // the hooks it ran recorded; only those records. This level's window and the
    // schedule's hook queue wait for their sync point (DI-KS 19).
    if (immediate != nullptr) {
        if (Result applied = phase(*immediate); !applied) return applied;
        return deliver();
    }

    // Step 0: the previous window's hook records first; hooks they trigger record into the next window.
    HookQueue& queue = HookQueueOf(schedule);
    std::vector<Record> hookRecords = std::move(queue.records);
    queue.records.clear();
    queue.recSeq = 0;
    std::vector<Record> windowRecords(std::make_move_iterator(window_.begin() + static_cast<std::ptrdiff_t>(base)),
                                      std::make_move_iterator(window_.end()));
    window_.erase(window_.begin() + static_cast<std::ptrdiff_t>(base), window_.end());
    if (Result step0 = phase(hookRecords); !step0) return step0;
    // Steps 1 to 7.
    if (Result rest = phase(windowRecords); !rest) return rest;
    // The triggers.
    if (Result delivered = deliver(); !delivered) return delivered;
    // Step 8.
    ++syncEpoch_;
    return {};
}

template <class R>
void World::RecordKernelDiagnostics(R record) {
    Detail::DiagnosticsBase* found = Detail::FindDiagnostics(*this, KeyOf<R>());
    if (found == nullptr) {
        auto made = std::make_unique<Detail::DiagnosticsBuffer<R>>();
        found = made.get();
        Detail::AddDiagnostics(*this, KeyOf<R>(), std::move(made));
    }
    const std::uint64_t epoch = record.syncEpoch;
    static_cast<Detail::DiagnosticsBuffer<R>*>(found)->entries.push_back(
        typename Detail::DiagnosticsBuffer<R>::Entry{epoch, std::move(record)});
}

void World::RecordRejectedSpawn(RejectedSpawn rejected) {
    ++rejectedSpawns_;
    RecordKernelDiagnostics(std::move(rejected));
}

void World::RecordRejectedRelationship(RejectedRelationship rejected) {
    ++rejectedRelationships_;
    RecordKernelDiagnostics(std::move(rejected));
}

std::optional<std::string> World::RelationshipProblem() const {
    const auto& childOf = TypedColumn<ChildOf>(data_->childOf);
    const auto& children = TypedColumn<Children>(data_->children);
    auto alive = [&](EntityId id) { return std::binary_search(entities_.begin(), entities_.end(), id); };
    std::size_t listed = 0;
    for (const auto& cell : children.cells) {
        if (cell.value.empty()) return "entity " + std::to_string(cell.id) + " has an empty Children";
        std::vector<EntityId> sorted(cell.value.begin(), cell.value.end());
        std::sort(sorted.begin(), sorted.end());
        if (const auto twice = std::adjacent_find(sorted.begin(), sorted.end()); twice != sorted.end()) {
            return "entity " + std::to_string(cell.id) + "'s Children lists " + std::to_string(*twice) + " twice";
        }
        for (const EntityId child : sorted) {
            const Detail::Cell<ChildOf>* source = childOf.Find(child);
            if (!alive(child) || source == nullptr || source->value.parent != cell.id) {
                return "entity " + std::to_string(cell.id) + "'s Children lists " + std::to_string(child) +
                       ", whose ChildOf does not name it";
            }
        }
        listed += cell.value.size();
    }
    // Every ChildOf names a live parent other than itself; with the checks above (each listed child's
    // ChildOf names that parent, no child twice) the counts being equal means each is listed exactly once.
    for (const auto& cell : childOf.cells) {
        if (cell.value.parent == cell.id || !alive(cell.value.parent) || children.Find(cell.value.parent) == nullptr) {
            return "entity " + std::to_string(cell.id) + "'s ChildOf names " + std::to_string(cell.value.parent) +
                   ", which does not list it in Children";
        }
    }
    if (listed != childOf.cells.size()) return std::string("Children and ChildOf do not count the same entities");
    return std::nullopt;
}

void World::DiscardDeferred() noexcept {
    window_.clear();
    for (HookQueue& queue : hookQueues_) {
        queue.records.clear();
        queue.recSeq = 0;
    }
}

MessageReaderInfo World::ReaderInfoOf(const Detail::MessageCursor& cursor) const {
    if (cursor.mount == Detail::kObserverMount) return MessageReaderInfo{data_->observers[cursor.system].name, {}, {}, true};
    const Detail::SystemRecord& system = data_->systems[cursor.system];
    const Mount& mount = system.options.mounts[cursor.mount];
    MessageReaderInfo info{system.options.name, mount.schedule, {}};
    if (cursor.slot == 0) return info;
    // The Executor's evaluation order: the mount's conditions, then each set's (E.4).
    std::uint32_t slot = 0;
    for (const ConditionRef& condition : mount.conditions) {
        if (++slot == cursor.slot) info.condition = condition.Name();
    }
    for (const std::string& setName : mount.sets) {
        for (const Detail::SetRecord& set : data_->sets) {
            if (set.options.name != setName || set.options.schedule != mount.schedule) continue;
            for (const ConditionRef& condition : set.conditions) {
                if (++slot == cursor.slot) info.condition = condition.Name();
            }
        }
    }
    return info;
}

std::optional<std::string> World::StructureProblem() const {
    for (std::size_t i = 1; i < entities_.size(); ++i) {
        if (entities_[i - 1] >= entities_[i]) return std::string("the entity list is not strictly ascending");
    }
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        const Detail::ColumnBase& column = *columns_[c];
        if (!column.StrictlyAscending()) {
            return "column '" + data_->components[c].name + "' is not strictly ascending";
        }
        for (std::size_t i = 0; i < column.Size(); ++i) {
            if (!std::binary_search(entities_.begin(), entities_.end(), column.KeyAt(i))) {
                return "column '" + data_->components[c].name + "' has entity " + std::to_string(column.KeyAt(i)) +
                       ", which is not alive";
            }
        }
    }
    // DI-KS 30 (declaration v8-arch-gyo2-kernel-relationship 10.2 item 8).
    return RelationshipProblem();
}

// ---- Boundary comparison (spec A.6 layer 2)

namespace {

void Take(Detail::Snapshot& snapshot, std::span<const std::byte> bytes) {
    snapshot.bytes.assign(bytes.begin(), bytes.end());
    snapshot.valid = true;
    snapshot.apiWritten = false;
}

bool Differs(const Detail::Snapshot& snapshot, std::span<const std::byte> bytes) {
    return !std::equal(snapshot.bytes.begin(), snapshot.bytes.end(), bytes.begin(), bytes.end());
}

} // namespace

void World::TakeSnapshots() {
    columnSnapshots_.resize(columns_.size());
    resourceSnapshots_.resize(resources_.size());
    messageSnapshots_.resize(messages_.size());
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        if (!excluded_[c]) Take(columnSnapshots_[c], columns_[c]->StorageBytes());
    }
    for (std::size_t r = 0; r < resources_.size(); ++r) Take(resourceSnapshots_[r], resources_[r]->StorageBytes());
    for (std::size_t e = 0; e < messages_.size(); ++e) Take(messageSnapshots_[e], messages_[e]->StorageBytes());
}

void World::TrimMessages() {
    for (std::size_t e = 0; e < messages_.size(); ++e) {
        Detail::MessageBufferBase& buffer = *messages_[e];
        // Nobody holds a record no reader has run for: all are trimmed then.
        std::uint64_t upTo = buffer.nextSeq - 1;
        for (const Detail::MessageCursor& cursor : cursors_) {
            if (cursor.message == e && cursor.read < upTo) upTo = cursor.read;
        }
        if (!buffer.TrimThrough(upTo) || !verification_) continue;
        messageSnapshots_.resize(messages_.size());
        Take(messageSnapshots_[e], buffer.StorageBytes());
    }
}

void World::DropSnapshots() noexcept {
    columnSnapshots_.clear();
    resourceSnapshots_.clear();
    messageSnapshots_.clear();
}

std::vector<std::string> World::CompareBefore() {
    std::vector<std::string> changed;
    columnSnapshots_.resize(columns_.size());
    resourceSnapshots_.resize(resources_.size());
    messageSnapshots_.resize(messages_.size());
    auto check = [&changed](Snapshot& snapshot, std::span<const std::byte> bytes, const std::string& name) {
        if (snapshot.valid && !snapshot.apiWritten && Differs(snapshot, bytes)) changed.push_back(name);
        Take(snapshot, bytes);
    };
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        if (!excluded_[c]) check(columnSnapshots_[c], columns_[c]->StorageBytes(), "component " + data_->components[c].name);
    }
    for (std::size_t r = 0; r < resources_.size(); ++r) {
        check(resourceSnapshots_[r], resources_[r]->StorageBytes(), "resource " + data_->resources[r].name);
    }
    for (std::size_t e = 0; e < messages_.size(); ++e) {
        check(messageSnapshots_[e], messages_[e]->StorageBytes(), "message " + data_->messages[e].name);
    }
    return changed;
}

std::vector<std::string> World::CompareAfter(const Detail::AccessDesc& access) {
    std::vector<std::string> changed;
    auto declaredWrite = [&](Detail::EntryKind kind, std::uint32_t id) {
        // ExclusiveWorld declares every component and resource value (DI-KS 1); events are not in it.
        if (access.wholeWorld && kind != Detail::EntryKind::Message) return true;
        for (const Detail::AccessEntry& entry : access.entries) {
            if (entry.kind == kind && entry.id == id && (entry.modes & Detail::kModeWrite) != 0) return true;
        }
        return false;
    };
    auto check = [&](Snapshot& snapshot, std::span<const std::byte> bytes, Detail::EntryKind kind, std::uint32_t id,
                     const std::string& name) {
        if (!declaredWrite(kind, id) && snapshot.valid && Differs(snapshot, bytes)) changed.push_back(name);
        Take(snapshot, bytes);
    };
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        if (!excluded_[c]) {
            check(columnSnapshots_[c], columns_[c]->StorageBytes(), Detail::EntryKind::Component,
                  static_cast<std::uint32_t>(c), "component " + data_->components[c].name);
        }
    }
    for (std::size_t r = 0; r < resources_.size(); ++r) {
        check(resourceSnapshots_[r], resources_[r]->StorageBytes(), Detail::EntryKind::Resource,
              static_cast<std::uint32_t>(r), "resource " + data_->resources[r].name);
    }
    for (std::size_t e = 0; e < messages_.size(); ++e) {
        check(messageSnapshots_[e], messages_[e]->StorageBytes(), Detail::EntryKind::Message, static_cast<std::uint32_t>(e),
              "message " + data_->messages[e].name);
    }
    return changed;
}

// ---- Summary (spec D-9)

namespace {

void SummaryText(Hasher& h, std::string_view text) {
    h.U64(text.size());
    h.Bytes(std::as_bytes(std::span<const char>(text.data(), text.size())));
}

// 2: message records with their seq, each type's next write sequence number,
// and the message cursors (v8-arch-gyo2-time-events step 3).
// 3: each value through its type's hash function (Hash.hpp), no longer its
// object bytes (v8-arch-gyo2-authority-rollback step 1, declaration section 3).
// 4: no write sequence number or record seq (counts of the history: one more
// record written once changed every later summary) and no cursor table (whether
// a reader has run and where it read to are state, left out knowingly: the keys
// follow the registration order; kernel RC 25); v8-arch-gyo2-kernel-summary-messages,
// the user's (b).
constexpr std::uint32_t kSummaryVersion = 4;

} // namespace

// Format "gyo.kernel.world-summary" version 4, every number as 8 bytes
// little-endian (Hasher::U64): the format string (length, bytes) and version;
// the alive entities (count, each id); every column in ComponentId order (cell
// count, then each key and the value through HashValue); every resource in
// ResourceId order but KernelSettings (present flag, then the value through
// HashValue when present); every message buffer in MessageTypeId order (record
// count, then each record's payload through HashValue, in write order). No
// stamp, tick, Plan position, message sequence number or cursor is hashed.
// A type with no HashValue that has a value in the World (a cell, a present
// resource, a message record) makes the call an Err naming every such type;
// with no value it is not hashed and not reported (declaration 3.3).
Engine::Base::Result<Fingerprint, KernelError> World::Summary() const {
    GYO_ASSERT(state_ == WorldState::Ready || state_ == WorldState::Interrupted);
    Sync();
    std::vector<std::string> missing;
    for (std::size_t c = 0; c < columns_.size(); ++c)
        if (columns_[c]->Size() > 0 && !columns_[c]->HasHash()) missing.push_back("component " + data_->components[c].name);
    for (std::size_t r = 0; r < resources_.size(); ++r) {
        if (r == data_->kernelSettings) continue;
        CheckThread(static_cast<ResourceId>(r));
        if (resources_[r]->Present() && !resources_[r]->HasHash()) missing.push_back("resource " + data_->resources[r].name);
    }
    for (std::size_t m = 0; m < messages_.size(); ++m)
        if (messages_[m]->Size() > 0 && !messages_[m]->HasHash()) missing.push_back("message " + data_->messages[m].name);
    if (!missing.empty()) {
        std::string names;
        for (const std::string& name : missing) names += (names.empty() ? "" : ", ") + name;
        KernelError error(KernelErrorCode::HashMissing,
                          "World::Summary: a type with a value in the World has no HashValue", names);
        error.world = name_;
        return Engine::Base::Err(std::move(error));
    }
    Hasher h;
    SummaryText(h, "gyo.kernel.world-summary");
    h.U64(kSummaryVersion);
    h.U64(entities_.size());
    for (EntityId id : entities_) h.U64(id);
    h.U64(columns_.size());
    for (const auto& column : columns_) {
        h.U64(column->Size());
        for (std::size_t i = 0; i < column->Size(); ++i) {
            h.U64(column->KeyAt(i));
            column->HashAt(h, i);
        }
    }
    // KernelSettings is not simulation data (LIM-6: not in a digest): left out, so the
    // summary does not depend on the verification switch.
    h.U64(resources_.size() - 1);
    for (std::size_t r = 0; r < resources_.size(); ++r) {
        if (r == data_->kernelSettings) continue;
        const bool present = resources_[r]->Present();
        h.U64(present ? 1U : 0U);
        if (present) resources_[r]->Hash(h);
    }
    h.U64(messages_.size());
    for (const auto& buffer : messages_) {
        h.U64(buffer->Size());
        for (std::size_t i = 0; i < buffer->Size(); ++i) buffer->HashAt(h, i);
    }
    return Fingerprint{kSummaryVersion, Engine::Base::Sha256(h.Data())};
}

} // namespace GYO::Kernel
