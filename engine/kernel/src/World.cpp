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
#include <iterator>
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
    // The kernel's own resource is present from the start (the Host or a System may overwrite it).
    static_cast<Detail::ResourceSlot<KernelSettings>&>(*resources_[data.kernelSettings]).value = KernelSettings{};
    excluded_.assign(columns_.size(), false);
    for (const Detail::HookRecord& hook : data.hooks) hookAccess_.push_back(Detail::MakeAccessDesc(hook.access));
    // Made up front: a bucket's address never changes while a hook records into it.
    for (const Detail::ScheduleRecord& schedule : data.schedules) hookQueues_.push_back(HookQueue{schedule.options.name, {}, 0});
}

World::HookQueue& World::HookQueueOf(std::string_view schedule) {
    for (HookQueue& queue : hookQueues_) {
        if (queue.schedule == schedule) return queue;
    }
    GYO_ASSERT(false && "no hook queue for this schedule");
    return hookQueues_.front();
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
        // ExclusiveWorld declares every component and resource value (DI-KS 1); events are not in it.
        if (access.wholeWorld && kind != Detail::EntryKind::Event) declared = true;
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

std::optional<KernelError> CheckExclusiveRead(SystemContext& call, ResourceId resource) {
    const World& world = *call.world;
    const ResourceInfo& info = world.data_->resources[resource];
    const Detail::ScheduleRecord* schedule = world.data_->FindSchedule(std::string(call.schedule));
    const std::string runner = schedule != nullptr ? schedule->options.runner : std::string();
    if (!info.affinity.IsAny() && info.affinity.role != runner) {
        // X-3 at run time: a System's other reads are checked at startup; this one is not in its signature.
        return CallError(call, KernelErrorCode::UnreadableResource, "S-16",
                         "ExclusiveWorld read of resource '" + info.name + "' by System '" + std::string(call.name) +
                             "': the resource needs thread role '" + info.affinity.role + "' but schedule '" +
                             std::string(call.schedule) + "' runs on '" + runner + "'");
    }
    return std::nullopt;
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
    const Detail::ScheduleRecord* scheduleRecord = data_->FindSchedule(std::string(schedule));
    Detail::SystemContext call;
    call.world = this;
    call.executor = executor_;
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
    HookQueue& queue = HookQueueOf(schedule);
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

    // Spec F-13: the ids spawned at this sync point (both phases), ascending, with who spawned each.
    std::vector<std::pair<EntityId, std::string_view>> spawnedHere;
    // The schedule's domain tick for a RejectedSpawn record, looked up only when one is dropped.
    std::optional<std::uint64_t> rejectTick;
    auto tickOfSchedule = [&]() {
        if (!rejectTick) {
            const Detail::ScheduleRecord* scheduleRecord = data_->FindSchedule(std::string(schedule));
            rejectTick = ticks_[DomainIndex(scheduleRecord != nullptr ? scheduleRecord->options.domain : Domain::None)];
        }
        return *rejectTick;
    };

    // K0: the running level's records start at its windowBase; the records
    // before it are the outer levels', applied later at their own sync points.
    const std::size_t base = runStack_.empty() ? 0 : runStack_.back().windowBase;
    GYO_ASSERT(base <= window_.size());
    // The level being applied is the innermost one (its records are the tail).
    GYO_ASSERT(runStack_.empty() || runStack_.back().schedule == schedule);

    // One phase: step 0 applies the hook queue this way, steps 1 to 7 the window's records.
    auto phase = [&](std::vector<Record>& records) -> Result {
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
            rejected.emitterZone = spawn.emitterZone;
            rejected.fromAllotter = !spawn.withId;
            rejected.schedule = std::string(schedule);
            rejected.step = spawn.planPos;
            rejected.appliedAt = planPos;
            rejected.tick = tickOfSchedule();
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
                if (spawns[i]->withId && data_->allotter && data_->allotterOps.owns(*resources_[*data_->allotter], id)) {
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

        const auto byCell = [](const NewCell& a, const NewCell& b) {
            return a.entity != b.entity ? a.entity < b.entity : a.component < b.component;
        };

        // Step 5a: initial values (Registrar::InitialValue). Every cell added by
        // steps 4 and 5 is looked at once, (EntityId, ComponentId) ascending; a
        // cell this step adds is looked at in the next round, so a chain of
        // declarations completes at this sync point. A Target the entity
        // already has is left as it is, so a round adds each cell at most once
        // and the rounds end.
        if (!data_->initialValues.empty()) {
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
    // Step 8.
    ++syncEpoch_;
    return {};
}

void World::RecordRejectedSpawn(RejectedSpawn rejected) {
    ++rejectedSpawns_;
    Detail::DiagnosticsBase* found = Detail::FindDiagnostics(*this, KeyOf<RejectedSpawn>());
    if (found == nullptr) {
        auto made = std::make_unique<Detail::DiagnosticsBuffer<RejectedSpawn>>();
        found = made.get();
        Detail::AddDiagnostics(*this, KeyOf<RejectedSpawn>(), std::move(made));
    }
    const std::uint64_t tick = rejected.tick;
    static_cast<Detail::DiagnosticsBuffer<RejectedSpawn>*>(found)->entries.push_back(
        Detail::DiagnosticsBuffer<RejectedSpawn>::Entry{tick, false, std::move(rejected)});
}

void World::DiscardDeferred() noexcept {
    window_.clear();
    for (HookQueue& queue : hookQueues_) {
        queue.records.clear();
        queue.recSeq = 0;
    }
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

void World::DropSnapshots() noexcept {
    columnSnapshots_.clear();
    resourceSnapshots_.clear();
    eventSnapshots_.clear();
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
        // ExclusiveWorld declares every component and resource value (DI-KS 1); events are not in it.
        if (access.wholeWorld && kind != Detail::EntryKind::Event) return true;
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
