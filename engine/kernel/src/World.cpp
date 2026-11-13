// World (spec A, F): storage, the checks of every access, the sync point's
// application of deferred records (spec F.2), hooks, event trim, the boundary
// comparison (spec A.6 layer 2) and the World summary (spec D-9).

#include "engine/kernel/World.hpp"

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace GYO::Kernel {

namespace {

std::size_t DomainIndex(Domain domain) noexcept {
    return static_cast<std::size_t>(domain);
}

bool SameError(const KernelError& a, const KernelError& b) {
    return a.code == b.code && a.rule == b.rule && a.message == b.message && a.detail == b.detail;
}

// Whether error, or any of its siblings at any depth, is the same error as wanted.
bool ContainsError(const KernelError& error, const KernelError& wanted) {
    if (SameError(error, wanted)) return true;
    return std::any_of(error.siblings.begin(), error.siblings.end(),
                       [&](const KernelError& sibling) { return ContainsError(sibling, wanted); });
}

std::string CallText(const Detail::SystemContext& call) {
    return std::string(call.hook ? "hook '" : "System '") + std::string(call.name) + "' (schedule '" +
           std::string(call.schedule) + "', " + (call.hook ? "sync point " : "") + "step " +
           std::to_string(call.planPos) + ")";
}

} // namespace

// ---- Construction

World::World(const Registry& registry, std::string name)
    : registry_(&registry), frozen_(&registry.Frozen()), data_(registry.data_.get()), name_(std::move(name)) {
    const Detail::RegistryData& data = *data_;
    GYO_ASSERT(data.columnMakers.size() == data.components.size());
    GYO_ASSERT(data.resourceMakers.size() == data.resources.size());
    GYO_ASSERT(data.eventMakers.size() == data.events.size());
    for (Detail::MakeColumnFn make : data.columnMakers) {
        GYO_ASSERT(make != nullptr);
        columns_.push_back(make());
    }
    for (Detail::MakeResourceFn make : data.resourceMakers) resources_.push_back(make());
    for (Detail::MakeEventsFn make : data.eventMakers) events_.push_back(make());
    // The kernel's own resource is present from the start (the Host overwrites it before the first Run).
    static_cast<Detail::ResourceSlot<KernelSettings>&>(*resources_[data.kernelSettings]).value = KernelSettings{};
    excluded_.assign(columns_.size(), false);
    for (const Detail::HookRecord& hook : data.hooks) hookAccess_.push_back(Detail::MakeAccessDesc(hook.access));
}

// Spec W-7: columns in ascending ComponentId order, each in ascending EntityId order (Column's destructor).
World::~World() {
    for (auto& column : columns_) column.reset();
}

// ---- Public reads and Host writes

bool World::Alive(EntityId id) const {
    if (state_ == WorldState::Running || state_ == WorldState::Applying) {
        GYO_ASSERT(Detail::CurrentCall(const_cast<World&>(*this)) != nullptr);  // W-6
    }
    return std::binary_search(entities_.begin(), entities_.end(), id);
}

std::span<const EntityId> World::Entities() const {
    if (state_ == WorldState::Running || state_ == WorldState::Applying) {
        GYO_ASSERT(Detail::CurrentCall(const_cast<World&>(*this)) != nullptr);  // W-6
    }
    return entities_;
}

bool World::Despawn(EntityId id) {
    GYO_ASSERT(state_ == WorldState::Ready);  // W-3
    if (!std::binary_search(entities_.begin(), entities_.end(), id)) return false;
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        if (columns_[c]->Contains(id)) {
            Access(Detail::EntryKind::Component, static_cast<std::uint32_t>(c), Detail::kModeWrite, true);
            columns_[c]->Erase(id);
        }
    }
    entities_.erase(std::lower_bound(entities_.begin(), entities_.end(), id));
    ++entitiesEpoch_;
    return true;
}

std::uint64_t World::MembershipEpoch() const noexcept {
    std::uint64_t sum = entitiesEpoch_;
    for (const auto& column : columns_) sum += column->epoch;
    return sum;
}

std::uint64_t World::Tick(Domain domain) const noexcept {
    return ticks_[DomainIndex(domain)];
}

std::optional<ChangeStamp> World::TickStamp(Domain domain, std::uint64_t tick) const noexcept {
    const TickStampSlot& slot = tickStamps_[DomainIndex(domain)][tick % kTickStampSlots];
    if (!slot.used || slot.tick != tick) return std::nullopt;
    return slot.stamp;
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
        if (access.wholeWorld && kind != Detail::EntryKind::Event) {
            if (mode == Detail::kModeRead) {
                declared = true;
            } else if (kind == Detail::EntryKind::Component) {
                const ColumnInfo& info = data_->components[id];
                declared = info.zone == Zone::B && info.writePolicy == WritePolicy::Shared;
            } else {
                const ResourceInfo& info = data_->resources[id];
                declared = info.zone == Zone::B && info.writePolicy == WritePolicy::Shared;
            }
        }
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
                                                                 : "event";
        const std::string& name = kind == Detail::EntryKind::Component ? data_->components[id].name
                                  : kind == Detail::EntryKind::Resource ? data_->resources[id].name
                                                                        : data_->events[id].name;
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
        if (kind == Detail::EntryKind::Component) {
            const ColumnInfo& info = data_->components[id];
            // W-6b: after the first Run the Host writes no zone C state.
            GYO_ASSERT(!hasRun_ || info.zone != Zone::C || info.role == Role::Input);
        } else if (kind == Detail::EntryKind::Resource) {
            const ResourceInfo& info = data_->resources[id];
            // W-6b, J-50: KernelSettings only before the first Run.
            GYO_ASSERT(!hasRun_ || id != data_->kernelSettings);
            GYO_ASSERT(!hasRun_ || info.zone != Zone::C || info.role == Role::Input || info.external);
        }
        MarkApiWritten(kind, id);
        return;
    }
}

void World::MarkApiWritten(Detail::EntryKind kind, std::uint32_t id) const {
    std::vector<Snapshot>& snapshots = kind == Detail::EntryKind::Component  ? columnSnapshots_
                                       : kind == Detail::EntryKind::Resource ? resourceSnapshots_
                                                                             : eventSnapshots_;
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

ComponentId ComponentIdOf(const World& world, TypeKey key) {
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Component);
    return entry->id;
}

ResourceId ResourceIdOf(const World& world, TypeKey key) {
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Resource);
    return entry->id;
}

EventTypeId EventIdOf(const World& world, TypeKey key) {
    const TypeEntry* entry = world.data_->FindType(key);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::Event);
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
    return *world.resources_[id];
}

EventBufferBase& EventsAt(World& world, EventTypeId id) {
    RequireRunningCall(world);
    return *world.events_[id];
}

ChangeStamp CurrentStamp(const World& world) noexcept {
    return world.stamp_;
}

std::uint64_t DomainTick(const World& world, Domain domain) noexcept {
    return world.ticks_[DomainIndex(domain)];
}

std::span<const EntityId> AliveEntities(const World& world) noexcept {
    return world.entities_;
}

Domain EventDomain(const World& world, EventTypeId id) {
    return world.data_->events[id].domain;
}

std::size_t ComponentCount(const World& world) noexcept {
    return world.columns_.size();
}

std::size_t ResourceCount(const World& world) noexcept {
    return world.resources_.size();
}

std::vector<ComponentId> SetMembersOf(const World& world, TypeKey setTag) {
    const TypeEntry* entry = world.data_->FindType(setTag);
    GYO_ASSERT(entry != nullptr && entry->role == TypeRole::SetTag);
    return world.data_->namedSets[entry->id].members;
}

void Push(SystemContext& call, Record record) {
    World& world = *call.world;
    GYO_ASSERT(world.state_ == WorldState::Running || world.state_ == WorldState::Applying);
    // F-5: while Applying only hooks record (into the next window).
    GYO_ASSERT(world.state_ != WorldState::Applying || call.hook);
    record.planPos = call.planPos;
    record.recSeq = (*call.recSeq)++;
    record.emitterZone = call.zone;
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

std::optional<KernelError> CheckUntypedCommand(SystemContext& call, ComponentId component, EntityId entity, bool insert) {
    const ColumnInfo& info = call.world->data_->components[component];
    if (info.zone == Zone::B && info.writePolicy == WritePolicy::Shared) return std::nullopt;
    return CallError(call, KernelErrorCode::UntypedWrite, "F-10",
                     std::string("untyped Commands ") + (insert ? "insert" : "remove") + " of component '" + info.name +
                         "' (ComponentId " + std::to_string(component) + ", " +
                         (info.zone == Zone::C ? "zone C" : "Exclusive") + ") on entity " + std::to_string(entity) +
                         ": untyped Commands write only zone B, Shared columns; use CommandsFor");
}

std::optional<KernelError> CheckDespawn(SystemContext& call, EntityId entity) {
    if (call.zone != Zone::B) return std::nullopt;
    World& world = *call.world;
    for (std::size_t c = 0; c < world.columns_.size(); ++c) {
        if (world.data_->components[c].zone == Zone::C && world.columns_[c]->Contains(entity)) {
            return CallError(call, KernelErrorCode::ZoneCDespawn, "Z5",
                             "a zone B call despawns entity " + std::to_string(entity) + ", which has zone C component '" +
                                 world.data_->components[c].name + "'");
        }
    }
    return std::nullopt;
}

std::optional<KernelError> CheckExclusiveWrite(SystemContext& call, ComponentId component, EntityId entity) {
    const ColumnInfo& info = call.world->data_->components[component];
    if (info.zone == Zone::B && info.writePolicy == WritePolicy::Shared) return std::nullopt;
    return CallError(call, KernelErrorCode::ExclusiveWorldWrite, "S-13",
                     "ExclusiveWorld write of component '" + info.name + "' (" +
                         (info.zone == Zone::C ? "zone C" : "Exclusive") + ") on entity " + std::to_string(entity) +
                         ": ExclusiveWorld writes only zone B, Shared data");
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
    const Detail::ScheduleRecord* scheduleRecord = data_->FindSchedule(std::string(schedule));
    Detail::SystemContext call;
    call.world = this;
    call.access = &hookAccess_[hook];
    call.name = record.name;
    call.schedule = schedule;
    call.planPos = planPos;
    call.zone = record.zone;
    call.hook = true;
    call.domain = scheduleRecord != nullptr ? scheduleRecord->options.domain : Domain::None;
    call.tick = ticks_[DomainIndex(call.domain)];
    call.stamp = stamp;
    call.lastRun = 0;
    call.callSerial = ++callSerial_;
    call.syncEpoch = syncEpoch_;
    call.queue = &hookQueue_;
    call.recSeq = &hookRecSeq_;
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
    std::vector<std::string> changed;
    if (verification_) changed = CompareAfter(hookAccess_[hook]);
    std::optional<KernelError> error = Detail::CombineCall(call, std::move(result), changed);
    if (!error) return {};
    // One more layer of the context chain (vertical), not a sibling: the leaves keep their codes and rules.
    error->Wrap("hook '" + record.name + "' at sync point step " + std::to_string(planPos) + " of schedule '" +
                std::string(schedule) + "'");
    return Engine::Base::Err(std::move(*error));
}

Engine::Base::Result<void, KernelError> World::RunHooks(HookTrigger trigger, EntityId entity, ComponentId component,
                                                        std::uint32_t planPos, std::string_view schedule,
                                                        ChangeStamp stamp) {
    for (std::size_t h = 0; h < data_->hooks.size(); ++h) {
        const Detail::HookRecord& hook = data_->hooks[h];
        if (hook.trigger != trigger || !hook.componentId || *hook.componentId != component) continue;
        auto result = RunHook(h, entity, component, planPos, schedule, stamp);
        if (!result) return result;
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
                                                         ChangeStamp stamp) {
    using Detail::Record;
    using Detail::RecordKind;
    using Result = Engine::Base::Result<void, KernelError>;

    auto kernelError = [&](KernelErrorCode code, std::string rule, std::string message) {
        KernelError error(code, std::move(message));
        error.rule = std::move(rule);
        error.world = name_;
        error.Wrap("sync point at step " + std::to_string(planPos) + " of schedule '" + std::string(schedule) + "'");
        if (breakOnKernelError_ && breakHook_) breakHook_(error);
        return error;
    };

    // One phase: step 0 applies the hook queue this way, steps 1 to 7 the window's records.
    // pending: records of the same window not applied by this phase (step 0
    // checks the window's zone C inserts too: a zone B hook's despawn recorded
    // before a zone C insert of the next window; K-S10).
    auto phase = [&](std::vector<Record>& records, const std::vector<Record>& pending) -> Result {
        // Z5 recheck (spec F.2 steps 0 and 2; B-11): before any despawn of this
        // phase, every zone B emitter's target is checked once; a hit applies none.
        for (const Record& despawn : records) {
            if (despawn.kind != RecordKind::Despawn || despawn.emitterZone != Zone::B) continue;
            const EntityId target = despawn.entity;
            std::string reason;
            for (std::size_t c = 0; c < columns_.size() && reason.empty(); ++c) {
                if (data_->components[c].zone == Zone::C && columns_[c]->Contains(target)) {
                    reason = "it has zone C component '" + data_->components[c].name + "'";
                }
            }
            for (const std::vector<Record>* list : {static_cast<const std::vector<Record>*>(&records), &pending}) {
                for (const Record& other : *list) {
                    if (!reason.empty()) break;
                    if (other.emitterZone != Zone::C || other.entity != target) continue;
                    if (other.kind == RecordKind::Insert || (other.kind == RecordKind::Spawn && other.withId)) {
                        reason = "a zone C call inserts into it in the same window";
                    }
                }
            }
            if (!reason.empty()) {
                return Engine::Base::Err(kernelError(KernelErrorCode::ZoneCDespawn, "Z5",
                                                     "a zone B despawn of entity " + std::to_string(target) +
                                                         " at the sync point: " + reason + "; no despawn applied"));
            }
        }

        // Step 2: despawn, ascending EntityId, only entities alive now, idempotent.
        std::vector<EntityId> despawns;
        for (const Record& r : records) {
            if (r.kind == RecordKind::Despawn) despawns.push_back(r.entity);
        }
        std::sort(despawns.begin(), despawns.end());
        despawns.erase(std::unique(despawns.begin(), despawns.end()), despawns.end());
        for (EntityId id : despawns) {
            const auto at = std::lower_bound(entities_.begin(), entities_.end(), id);
            if (at == entities_.end() || *at != id) continue;
            // OnRemove for every component first (ascending ComponentId), so each hook still reads the whole
            // entity; then destroy in ascending ComponentId (W-7). (Architect's recommendation (a), pending the user.)
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
            columns_[component]->Erase(id);
        }

        std::vector<NewCell> added;

        // Step 4: spawn, ascending EntityId.
        std::vector<Record*> spawns;
        for (Record& r : records) {
            if (r.kind == RecordKind::Spawn) spawns.push_back(&r);
        }
        std::sort(spawns.begin(), spawns.end(), [](const Record* a, const Record* b) { return a->entity < b->entity; });
        for (std::size_t i = 0; i < spawns.size(); ++i) {
            Record& spawn = *spawns[i];
            GYO_ASSERT(i == 0 || spawns[i - 1]->entity != spawn.entity);  // one id spawned twice in a window
            GYO_ASSERT(!std::binary_search(entities_.begin(), entities_.end(), spawn.entity));  // alive, not despawned
            if (spawn.withId && data_->allotter) {
                // SpawnWithId must not use an id of this World's allotter range (spec A.1).
                GYO_ASSERT(!data_->allotterOps.owns(*resources_[*data_->allotter], spawn.entity));
            }
            MakeAlive(spawn.entity);
            for (Detail::BundleValue& part : spawn.bundle) {
                if (part.value->PutInto(*columns_[part.component], spawn.entity, stamp)) {
                    added.push_back(NewCell{spawn.entity, part.component});
                }
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
            } else if (winner.value->PutInto(*columns_[winner.id], winner.entity, stamp)) {
                added.push_back(NewCell{winner.entity, winner.id});
            }
            i = end;
        }

        // Step 6: OnAdd for new cells, (EntityId, ComponentId) ascending; an overwrite is not an add.
        std::sort(added.begin(), added.end(), [](const NewCell& a, const NewCell& b) {
            return a.entity != b.entity ? a.entity < b.entity : a.component < b.component;
        });
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
            if (op->kind == RecordKind::ResourceInsert) {
                op->value->PutInto(slot);
            } else {
                slot.Reset();
            }
            slot.changed = stamp;
        }
        return {};
    };

    // Step 0: the previous window's hook records first; hooks they trigger record into the next window.
    std::vector<Record> hookRecords = std::move(hookQueue_);
    hookQueue_.clear();
    hookRecSeq_ = 0;
    std::vector<Record> windowRecords = std::move(window_);
    window_.clear();
    const std::vector<Record> none;
    if (Result step0 = phase(hookRecords, windowRecords); !step0) return step0;
    // Steps 1 to 7.
    if (Result rest = phase(windowRecords, none); !rest) return rest;
    // Step 8.
    ++syncEpoch_;
    return {};
}

void World::DiscardDeferred() noexcept {
    window_.clear();
    hookQueue_.clear();
    hookRecSeq_ = 0;
}

void World::TrimEvents(Domain domain) {
    for (std::size_t e = 0; e < events_.size(); ++e) {
        if (data_->events[e].domain == domain) events_[e]->Trim(ticks_[DomainIndex(domain)]);
    }
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
    return std::nullopt;
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
    eventSnapshots_.resize(events_.size());
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        if (!excluded_[c]) Take(columnSnapshots_[c], columns_[c]->StorageBytes());
    }
    for (std::size_t r = 0; r < resources_.size(); ++r) Take(resourceSnapshots_[r], resources_[r]->StorageBytes());
    for (std::size_t e = 0; e < events_.size(); ++e) Take(eventSnapshots_[e], events_[e]->StorageBytes());
}

std::vector<std::string> World::CompareBefore() {
    std::vector<std::string> changed;
    columnSnapshots_.resize(columns_.size());
    resourceSnapshots_.resize(resources_.size());
    eventSnapshots_.resize(events_.size());
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
    for (std::size_t e = 0; e < events_.size(); ++e) {
        check(eventSnapshots_[e], events_[e]->StorageBytes(), "event " + data_->events[e].name);
    }
    return changed;
}

std::vector<std::string> World::CompareAfter(const Detail::AccessDesc& access) {
    std::vector<std::string> changed;
    auto declaredWrite = [&](Detail::EntryKind kind, std::uint32_t id) {
        if (access.wholeWorld && kind == Detail::EntryKind::Component) {
            const ColumnInfo& info = data_->components[id];
            if (info.zone == Zone::B && info.writePolicy == WritePolicy::Shared) return true;
        }
        if (access.wholeWorld && kind == Detail::EntryKind::Resource) {
            const ResourceInfo& info = data_->resources[id];
            if (info.zone == Zone::B && info.writePolicy == WritePolicy::Shared) return true;
        }
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
    for (std::size_t e = 0; e < events_.size(); ++e) {
        check(eventSnapshots_[e], events_[e]->StorageBytes(), Detail::EntryKind::Event, static_cast<std::uint32_t>(e),
              "event " + data_->events[e].name);
    }
    return changed;
}

// ---- Summary (spec D-9)

namespace {

class SummaryBytes {
public:
    void U8(std::uint8_t value) { bytes.push_back(static_cast<std::byte>(value)); }
    void U32(std::uint64_t value) {
        for (int shift = 0; shift < 32; shift += 8) U8(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
    void U64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) U8(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
    void Str(const std::string& text) {
        U32(text.size());
        for (char c : text) U8(static_cast<std::uint8_t>(c));
    }
    void Raw(std::span<const std::byte> raw) {
        U32(raw.size());
        bytes.insert(bytes.end(), raw.begin(), raw.end());
    }
    std::vector<std::byte> bytes;
};

constexpr std::uint32_t kSummaryVersion = 1;

} // namespace

// Format "gyo.kernel.world-summary" version 1: the format string and version;
// the alive entities (count, each u64); every column in ComponentId order
// (cell count, then each key as u64 and the value's object bytes as a list);
// every resource in ResourceId order but KernelSettings (present flag, object bytes); every event
// buffer in EventTypeId order (record count, each payload's bytes). No stamp,
// tick, Plan position or seq is hashed. A value's padding bytes are hashed as
// they are: the kernel tests use types without padding.
Fingerprint World::Summary() const {
    GYO_ASSERT(state_ == WorldState::Ready || state_ == WorldState::Interrupted);
    SummaryBytes out;
    out.Str("gyo.kernel.world-summary");
    out.U32(kSummaryVersion);
    out.U32(entities_.size());
    for (EntityId id : entities_) out.U64(id);
    out.U32(columns_.size());
    for (const auto& column : columns_) {
        out.U32(column->Size());
        for (std::size_t i = 0; i < column->Size(); ++i) {
            out.U64(column->KeyAt(i));
            out.Raw(column->ValueBytesAt(i));
        }
    }
    // KernelSettings is not simulation data (LIM-6: not in a digest): left out, so the
    // summary does not depend on the verification switch.
    out.U32(resources_.size() - 1);
    for (std::size_t r = 0; r < resources_.size(); ++r) {
        if (r == data_->kernelSettings) continue;
        out.U8(resources_[r]->Present() ? 1U : 0U);
        out.Raw(resources_[r]->StorageBytes());
    }
    out.U32(events_.size());
    for (const auto& buffer : events_) {
        out.U32(buffer->Size());
        for (std::size_t i = 0; i < buffer->Size(); ++i) out.Raw(buffer->PayloadBytesAt(i));
    }
    return Fingerprint{kSummaryVersion, Engine::Base::Sha256(out.bytes)};
}

} // namespace GYO::Kernel
