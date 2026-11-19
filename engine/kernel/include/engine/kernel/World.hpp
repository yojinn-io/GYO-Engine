#pragma once

// World (spec A; target/world.zh-Hant.md; target/kernel DI-KS 2, 3, 4, 6, 9,
// 10, 11, 14).
//
// One WorldExecutor's data: columns, resources, message buffers and their
// readers' cursors, the deferred queue, change stamps, the sync point number and its state (no clock and no tick:
// v8-arch-gyo2-clock-out-of-kernel). Made from a Registry that has
// built its Plans; its lifetime is one generation (spec A.1: a generation
// change is a new World, made from the same Registry).
//
// Versions (declaration v8-arch-gyo2-kernel-subtract-structure 3.2 rule 5):
// the World remembers the Registry version it is attached to. Every entry that
// uses a table indexed by a registry id (the type lookups of the public
// functions and handles, Alive, Entities, Despawn, Summary, WorldExecutor::Run)
// first compares it with the Registry's and, when they differ, attaches:
// storage for the new components, resources and message buffers and a hook queue
// for each new schedule are added, the verification exclusions grow, the
// hooks' access descriptions are made again, and the message cursors of a
// removed System are dropped (event cursor declaration 3.2: what only it held
// is trimmed when the next outermost Run ends). Entries that use no such table
// (State, Name, Options, Stamp, SyncEpoch, ...) do not compare. The Registry keeps
// no list of its Worlds, only counters: two this class changes (live Worlds,
// and those that declare no dynamic insert) and one WorldExecutor::Run changes
// (Worlds inside a Run); so making, destroying and running a World, like
// registering, is on the thread that registers, or ordered by the caller
// (DI-KS 26).
//
// The public functions below are the Host's (state Ready) and, inside a running
// System, a path that does not go through a handle (a World* kept in a global,
// for example). Every one checks:
//   W-6   Running or Applying with no System or hook of this World running on
//         this thread: GYO_ASSERT (always on);
//   W-3   a structural change (insert, remove, despawn, resource presence)
//         outside Ready: GYO_ASSERT (always on);
//   W-5   inside a System or hook, with verification on: an undeclared
//         (id, mode) is a violation recorded in the call and returned as Err by
//         the Executor (spec A.6 layer 1). The access itself still happens:
//         verification reports, it does not block.
// The thread half of W-6 (ownerThread) arrives with the threading batch.

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Plan.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/detail/Context.hpp"
#include "engine/kernel/detail/Storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Kernel {

class WorldExecutor;

// What a World declares when it is made (DI-KS 26, L5). dynamicInsert: whether
// Registry::Register may add to the Registry it was made from while it lives;
// false makes every Register on that Registry return DynamicInsertRefused.
// Fixed for the World's life.
struct WorldOptions {
    bool dynamicInsert = true;
};

// Spec A.1 state (Building is the Registry's: a World is made only from a frozen Registry).
enum class WorldState : std::uint8_t {
    Ready,
    Running,
    Applying,
    // After an Err or an exception: no further Run until the World is rebuilt
    // (spec E.5; Restore arrives with rollback-extract).
    Interrupted,
};

// What a "verification on" report carries (target/system-contract 2b condition 2).
struct VerificationReport {
    std::string world;
    bool accessVerification = false;
    bool breakOnKernelError = false;
    // Columns left out of the boundary comparison (spec A.6 layer 2).
    std::vector<std::string> excludedColumns;
};

// Spec F-13: why a spawn record was dropped at a sync point.
enum class SpawnRejectReason : std::uint8_t {
    IdInAllotterRange,
    IdSpawnedTwice,
    IdAlive,
};

// Spec F-13: one dropped spawn record, a diagnostics record of the kernel's own:
// read with World::Diagnostics<RejectedSpawn>() and taken with TakeDiagnostics
// between two Runs. A product System may not write this type.
struct RejectedSpawn {
    EntityId entity = 0;
    SpawnRejectReason reason = SpawnRejectReason::IdAlive;
    // The System or hook that recorded the spawn.
    std::string emitter;
    bool emitterIsHook = false;
    Zone emitterZone = Zone::B;
    // Recorded by Spawn (the allotter's id: Spawn<Bundle<C...>> or
    // ExclusiveWorld::Spawn), not SpawnWithId.
    bool fromAllotter = false;
    // The schedule whose sync point dropped it (for an immediate change of
    // ExclusiveWorld, the exclusive System's schedule).
    std::string schedule;
    // The record's Plan position: a System's step; for a hook, the sync point
    // that triggered it, or, when an immediate change of ExclusiveWorld ran the
    // hook, the exclusive System's step.
    std::uint32_t step = 0;
    // The sync point that dropped it (for a hook round, the last sync point of
    // the Plan); for an immediate change of ExclusiveWorld and the hook rounds
    // it runs, the exclusive System's step (no sync point applied it).
    std::uint32_t appliedAt = 0;
    // World::SyncEpoch() when it was dropped: the sync points completed before
    // the one (or the immediate change) that dropped it (L8; the kernel has no
    // tick, v8-arch-gyo2-clock-out-of-kernel).
    std::uint64_t syncEpoch = 0;
    // IdSpawnedTwice only: the other emitter (the one whose record was kept, or
    // another emitter of the same id when all were dropped).
    std::string otherEmitter;
};

// One outermost Run's dropped records of one message type (event cursor
// declaration 3.2: a write to a full buffer drops the oldest record), a
// diagnostics record of the kernel's own like RejectedSpawn: read with
// World::Diagnostics<DroppedMessages>() and taken with TakeDiagnostics between
// two Runs. A product System may not write this type. Dropping is a result,
// not an error: the Run goes on.
struct DroppedMessages {
    // The message type's name.
    std::string message;
    std::uint64_t count = 0;
    // The write sequence numbers of the first and last dropped record.
    std::uint64_t firstSeq = 0;
    std::uint64_t lastSeq = 0;
    // The readers that held a dropped record they had not read, in
    // (registrationSeq, mount, condition) order.
    std::vector<MessageReaderInfo> readers;
};

class World {
public:
    // registry must have built its Plans, have no pending round (GYO_ASSERT)
    // and outlive the World.
    explicit World(const Registry& registry, std::string name = {}, WorldOptions options = {});
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    [[nodiscard]] WorldState State() const noexcept { return state_; }
    [[nodiscard]] const std::string& Name() const noexcept { return name_; }
    [[nodiscard]] const FrozenRegistry& Types() const noexcept { return *frozen_; }
    // The declaration it was made with (it belongs to the World, not to the Registry).
    [[nodiscard]] const WorldOptions& Options() const noexcept { return options_; }
    // Whether an Executor has started a Run on this World.
    [[nodiscard]] bool HasRun() const noexcept { return hasRun_; }

    // ---- Entities and component values

    [[nodiscard]] bool Alive(EntityId id) const;
    [[nodiscard]] std::span<const EntityId> Entities() const;

    template <class T>
    [[nodiscard]] const T* Get(EntityId id) const {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeRead, false);
        const auto* cell = TypedColumn<T>(component).Find(id);
        return cell != nullptr ? &cell->value : nullptr;
    }

    // The keys of one column, ascending.
    template <class T>
    [[nodiscard]] std::vector<EntityId> Keys() const {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeRead, false);
        std::vector<EntityId> keys;
        for (const auto& cell : TypedColumn<T>(component).cells) keys.push_back(cell.id);
        return keys;
    }

    // The cell's change stamps (spec A.1), or nothing when absent.
    template <class T>
    [[nodiscard]] std::optional<std::pair<ChangeStamp, ChangeStamp>> Stamps(EntityId id) const {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeRead, false);
        const auto* cell = TypedColumn<T>(component).Find(id);
        if (cell == nullptr) return std::nullopt;
        return std::make_pair(cell->added, cell->changed);
    }

    // A pointer to write through: the Host in Ready (the column is then left
    // out of the next boundary comparison: a write through the World API) or a
    // System through the World API (checked as a write, W-5). The changed stamp
    // becomes the current stamp.
    template <class T>
    [[nodiscard]] T* GetMut(EntityId id) {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeWrite, false);
        auto* cell = TypedColumn<T>(component).Find(id);
        if (cell == nullptr) return nullptr;
        cell->changed = WriteStamp();
        return &cell->value;
    }

    // Host, Ready only: inserts or overwrites one value; the entity becomes alive.
    // The allotter's range is neither checked nor moved (DI-KS 18).
    template <class T>
    void Insert(EntityId id, T value) {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeWrite, true);
        MakeAlive(id);
        TypedColumn<T>(component).Put(id, std::move(value), WriteStamp());
    }

    // Host, Ready only.
    template <class T>
    bool Remove(EntityId id) {
        const ComponentId component = Detail::ComponentIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Component, component, Detail::kModeWrite, true);
        return columns_[component]->Erase(id);
    }

    // Host, Ready only: destroys every component of id in ascending
    // ComponentId order (spec W-7); no hook runs (hooks run at sync points).
    bool Despawn(EntityId id);

    // ---- Resources

    template <class T>
    [[nodiscard]] const T* Resource() const {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        CheckThread(resource);
        Access(Detail::EntryKind::Resource, resource, Detail::kModeRead, false);
        const auto& slot = TypedResource<T>(resource);
        return slot.value ? &*slot.value : nullptr;
    }

    template <class T>
    [[nodiscard]] T* ResourceMut() {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        CheckThread(resource);
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, false);
        auto& slot = TypedResource<T>(resource);
        if (!slot.value) return nullptr;
        slot.changed = WriteStamp();
        return &*slot.value;
    }

    // Host, Ready only. KernelSettings too, at any time in Ready: the Executor
    // reads it once, when each WorldExecutor::Run starts (J-50, DI-KS 24).
    template <class T>
    void SetResource(T value) {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        CheckThread(resource);
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, true);
        auto& slot = TypedResource<T>(resource);
        slot.value = std::move(value);
        slot.changed = WriteStamp();
        BindThread(resource);
    }

    template <class T>
    void RemoveResource() {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        CheckThread(resource);
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, true);
        TypedResource<T>(resource).value.reset();
        TypedResource<T>(resource).thread = 0;
    }

    // ---- Messages (a read-only view for the Host and tests: every record kept,
    // in write order; moves no cursor and is no reader)

    template <class M>
    [[nodiscard]] std::vector<M> MessagePayloads() const {
        const MessageTypeId message = Detail::MessageIdOf(*this, KeyOf<M>());
        Access(Detail::EntryKind::Message, message, Detail::kModeRead, false);
        std::vector<M> out;
        for (const auto& record : static_cast<const Detail::MessageBuffer<M>&>(*messages_[message]).records) {
            out.push_back(record.payload);
        }
        return out;
    }

    // ---- Diagnostics (B-13): the World end of DiagnosticsWriter<R>, and of the
    // kernel's own RejectedSpawn records (spec F-13); kept when
    // Interrupted. Readable in Ready and Interrupted.

    template <class R>
    [[nodiscard]] std::vector<R> Diagnostics() const {
        GYO_ASSERT(state_ == WorldState::Ready || state_ == WorldState::Interrupted);
        std::vector<R> out;
        for (const auto& [key, buffer] : diagnostics_) {
            if (key == KeyOf<R>()) {
                for (const auto& entry : static_cast<const Detail::DiagnosticsBuffer<R>&>(*buffer).entries) {
                    out.push_back(entry.record);
                }
            }
        }
        return out;
    }

    // Takes the World end of DiagnosticsWriter<R>: returns the records and empties
    // the buffer (the Host's exchange point; B-13). Ready and Interrupted.
    template <class R>
    [[nodiscard]] std::vector<R> TakeDiagnostics() {
        std::vector<R> out = Diagnostics<R>();
        for (auto& [key, buffer] : diagnostics_) {
            if (key == KeyOf<R>()) static_cast<Detail::DiagnosticsBuffer<R>&>(*buffer).entries.clear();
        }
        return out;
    }

    // ---- Stamps, counters

    // The last stamp taken (spec A.1).
    [[nodiscard]] ChangeStamp Stamp() const noexcept { return stamp_; }
    // +1 at every sync point, also an empty one (spec F-6).
    [[nodiscard]] std::uint64_t SyncEpoch() const noexcept { return syncEpoch_; }
    // Spec F.2 step 5: inserts dropped because the entity was gone (a result, not an error).
    [[nodiscard]] std::uint64_t DroppedMissingEntity() const noexcept { return droppedMissingEntity_; }
    // Spec F-13: spawn records dropped at sync points over the World's life (TakeDiagnostics does not reset it).
    [[nodiscard]] std::uint64_t RejectedSpawns() const noexcept { return rejectedSpawns_; }

    // ---- Verification (spec A.6)

    // Leaves column T out of the boundary comparison (large columns); before
    // the first Run only. The list is part of every verification report.
    template <class T>
    void ExcludeFromVerification() {
        GYO_ASSERT(state_ == WorldState::Ready && !hasRun_);
        excluded_[Detail::ComponentIdOf(*this, KeyOf<T>())] = true;
    }

    // The World summary of spec D-9: keys and values of every column, resource
    // values, every message record (seq and payload) with each type's next write
    // sequence number, and the message cursors; without stamps or
    // Plan positions. Version 2 (World.cpp). Readable in Ready and Interrupted. Values
    // are hashed as raw bytes: a type in the summary must have no padding (the
    // author's responsibility; v8-arch-gyo2-summary-padding-a-then-c).
    [[nodiscard]] Fingerprint Summary() const;

    // The break hook called where a KernelError is made when
    // KernelSettings::breakOnKernelError is on (target/system-contract 2b). It
    // returns; the Err then goes on as usual. Default: none (a debugger trap is
    // the Host's to install; the kernel has no platform header).
    void SetBreakHook(std::function<void(const KernelError&)> hook) { breakHook_ = std::move(hook); }

private:
    friend class WorldExecutor;
    friend Detail::SystemContext* Detail::CurrentCall(World&) noexcept;
    friend Detail::SystemContext& Detail::RequireCall(World&);
    friend ComponentId Detail::ComponentIdOf(const World&, TypeKey);
    friend ResourceId Detail::ResourceIdOf(const World&, TypeKey);
    friend MessageTypeId Detail::MessageIdOf(const World&, TypeKey);
    friend EventTypeId Detail::EventIdOf(const World&, TypeKey);
    friend void Detail::CheckHandle(World&, std::uint64_t, std::uint64_t, std::string_view);
    friend Detail::ColumnBase& Detail::ColumnAt(World&, ComponentId);
    friend Detail::ResourceBase& Detail::ResourceAt(World&, ResourceId);
    friend Detail::MessageBufferBase& Detail::MessagesAt(World&, MessageTypeId);
    friend ChangeStamp Detail::CurrentStamp(const World&) noexcept;
    friend std::span<const EntityId> Detail::AliveEntities(const World&) noexcept;
    friend std::uint64_t Detail::MessageCursorOf(World&, const Detail::SystemContext&, MessageTypeId);
    friend void Detail::AdvanceMessageCursor(World&, const Detail::ReaderKey&, MessageTypeId, std::uint64_t);
    friend void Detail::RecordDroppedMessage(World&, MessageTypeId, std::uint64_t);
    friend std::size_t Detail::ComponentCount(const World&) noexcept;
    friend std::size_t Detail::ResourceCount(const World&) noexcept;
    friend std::vector<ComponentId> Detail::SetMembersOf(const World&, TypeKey);
    friend void Detail::Push(Detail::SystemContext&, Detail::Record);
    friend KernelError Detail::CallError(Detail::SystemContext&, KernelErrorCode, std::string, std::string);
    friend KernelError Detail::CallerError(Detail::SystemContext&, KernelErrorCode, std::string, std::string);
    friend std::uint64_t Detail::CurrentSyncEpoch(const World&) noexcept;
    friend ChangeStamp Detail::NextStamp(World&) noexcept;
    friend Detail::ResourceBase* Detail::AllotterSlot(World&) noexcept;
    friend std::optional<EntityId> Detail::Allot(World&);
    friend Detail::DiagnosticsBase* Detail::FindDiagnostics(World&, TypeKey) noexcept;
    friend void Detail::AddDiagnostics(World&, TypeKey, std::unique_ptr<Detail::DiagnosticsBase>);
    friend std::optional<KernelError> Detail::CombineCall(Detail::SystemContext&, ::Engine::Base::Result<void, KernelError>,
                                                          const std::vector<std::string>&);

    // Attaches to the Registry's version when it is not the one attached
    // (declaration 3.2 rule 5). Cheap when equal: one comparison.
    void Sync() const;
    // GYO_ASSERT: not Running or Applying (a Registry never builds while one
    // of its Worlds runs: rule 4).
    void AttachVersion();

    template <class T>
    [[nodiscard]] Detail::Column<T>& TypedColumn(ComponentId id) const {
        return static_cast<Detail::Column<T>&>(*columns_[id]);
    }
    template <class T>
    [[nodiscard]] Detail::ResourceSlot<T>& TypedResource(ResourceId id) const {
        return static_cast<Detail::ResourceSlot<T>&>(*resources_[id]);
    }

    // The checks of every public access (see the file comment). structural:
    // insert, remove, despawn or resource presence.
    void Access(Detail::EntryKind kind, std::uint32_t id, std::uint8_t mode, bool structural) const;
    // DI-KS 14: a thread-bound resource that holds a value is touched only on the
    // thread that put it in (GYO_ASSERT); BindThread records this thread.
    void CheckThread(ResourceId id) const;
    void BindThread(ResourceId id);
    void MakeAlive(EntityId id);
    // The stamp a write through the World API carries: in a run, the current
    // step's; in Ready, the stamp the next step will take (a Host write belongs
    // to the next step, so every mount that has not run since sees it: B-12).
    [[nodiscard]] ChangeStamp WriteStamp() const noexcept { return state_ == WorldState::Ready ? stamp_ + 1 : stamp_; }
    void MarkApiWritten(Detail::EntryKind kind, std::uint32_t id) const;

    // ---- Executor side (WorldExecutor, World.cpp)

    // F.2: applies the running level's deferred records (window_ from its
    // windowBase; K0) and the schedule's hook queue (spawn records that cannot
    // spawn are dropped and recorded, spec F-13); Err from a hook. The outer
    // levels' records and their schedules' hook queues are not looked at.
    // stamp: the sync point's stamp. immediate (S5, WorldExecutor::ImmediateFrom):
    // applies only these records, as one phase of steps 2 to 7; the window, the
    // hook queue and the sync epoch are left alone.
    ::Engine::Base::Result<void, KernelError> ApplySync(std::uint32_t planPos, std::string_view schedule,
                                                      ChangeStamp stamp, std::vector<Detail::Record>* immediate = nullptr);
    // Spec F-13: counts one dropped spawn and appends its diagnostics record.
    void RecordRejectedSpawn(RejectedSpawn rejected);
    // Spec E.5: discards every record not yet applied (hook queue included).
    void DiscardDeferred() noexcept;
    // Event cursor declaration 3.2: when the outermost Run ends, after the hook
    // rounds (not after a nested run, not after an Err), drops every record that
    // every cursor of its type has read (all of them when no reader has run);
    // a changed buffer's boundary snapshot is taken again, so the next Run's
    // Ready window does not take the trim for a change outside the World API.
    void TrimMessages();
    // The reader's name for DroppedMessages and the Registry's reader list.
    [[nodiscard]] MessageReaderInfo ReaderInfoOf(const Detail::MessageCursor& cursor) const;
    // Spec W-3: the sum of every column's epoch and the entity list's; the
    // Executor checks that no System call changes it (always on, O(columns)).
    [[nodiscard]] std::uint64_t MembershipEpoch() const noexcept;
    // W-1 with verification on.
    [[nodiscard]] std::optional<std::string> StructureProblem() const;

    // Layer 2 (spec A.6): snapshots and comparisons.
    using Snapshot = Detail::Snapshot;
    void TakeSnapshots();
    // Drops every snapshot: the next comparison takes fresh ones and reports
    // nothing (the verification switch changed, J-50).
    void DropSnapshots() noexcept;
    // Names of storages changed outside the World API since the last snapshot.
    [[nodiscard]] std::vector<std::string> CompareBefore();
    // Names of storages access does not declare as written that changed during the call.
    [[nodiscard]] std::vector<std::string> CompareAfter(const Detail::AccessDesc& access);
    // Runs one hook (spec F.4) with tlCurrent set to its access description.
    ::Engine::Base::Result<void, KernelError> RunHook(std::size_t hook, EntityId entity, ComponentId component,
                                                    std::uint32_t planPos, std::string_view schedule, ChangeStamp stamp);
    // OnAdd / OnRemove hooks of one cell, in registration order.
    ::Engine::Base::Result<void, KernelError> RunHooks(HookTrigger trigger, EntityId entity, ComponentId component,
                                                     std::uint32_t planPos, std::string_view schedule, ChangeStamp stamp);
    // The observers of one trigger (event cursor declaration 3.3): those of
    // (trigger, id) in name order (RegistryData::observerOrder), each a System
    // call with tlCurrent set to its access description, its own reader key,
    // Local<T> and last-run stamp. What they record goes where a hook's goes
    // (hookTarget_, or the schedule's hook queue). stamp: the call's stamp, or,
    // when ownStamp, a new stamp for each call (a trigger outside a sync point:
    // each is like one System step). Err from an observer, wrapped once.
    ::Engine::Base::Result<void, KernelError> RunObservers(ObserverTrigger trigger, std::uint32_t id, const void* payload,
                                                         std::optional<EntityId> target, std::uint32_t planPos,
                                                         std::string_view schedule, ChangeStamp stamp, bool ownStamp);

    const Registry* registry_;
    const FrozenRegistry* frozen_;
    const Detail::RegistryData* data_;
    std::string name_;
    WorldOptions options_;
    // The Registry version the storage below matches (0: none yet).
    std::uint64_t version_ = 0;
    WorldState state_ = WorldState::Ready;
    bool hasRun_ = false;
    bool verification_ = false;
    bool breakOnKernelError_ = false;

    std::vector<EntityId> entities_;
    std::vector<std::unique_ptr<Detail::ColumnBase>> columns_;
    std::vector<std::unique_ptr<Detail::ResourceBase>> resources_;
    std::vector<std::unique_ptr<Detail::MessageBufferBase>> messages_;
    // Every reader's cursor (event cursor declaration 3.2), sorted by (message,
    // system, mount, slot); a reader is in it once it has run. Not in the
    // boundary comparison: moving a cursor is not a write.
    std::vector<Detail::MessageCursor> cursors_;
    // This outermost Run's DroppedMessages entry of each message type (an index
    // into the kernel's diagnostics buffer, or none) and the readers it lists
    // (their keys, sorted); cleared when an outermost Run starts (the Host takes
    // diagnostics only between Runs).
    struct DroppedEntry {
        std::optional<std::size_t> entry;
        std::vector<Detail::MessageCursor> readers;
    };
    std::vector<DroppedEntry> droppedEntry_;
    std::vector<std::pair<TypeKey, std::unique_ptr<Detail::DiagnosticsBase>>> diagnostics_;

    // The current sync window's deferred records. With nested runs (K0) one
    // container holds every level's records: a level's own records are the tail
    // from its windowBase (RunLevel), applied and cut off at that level's sync
    // points; the outer levels' records wait, untouched, for their own.
    std::vector<Detail::Record> window_;
    // K0: the schedules running now, outermost first (spec E.2; WorldExecutor::Run
    // pushes the first, ExclusiveWorld::RunSchedule the others). A schedule is on
    // it at most once (X-11).
    struct RunLevel {
        // The Registry's string.
        std::string_view schedule;
        // Where this level's deferred records start in window_.
        std::size_t windowBase = 0;
        // The membership epoch the running step started from (W-3): a System
        // call leaves it unchanged; a nested run sets a new one when it returns.
        std::uint64_t structure = 0;
    };
    std::vector<RunLevel> runStack_;
    // K0 (spec E.5): the first error of a nested level, kept for the outermost
    // Run, which reports once; a later RunSchedule of the Interrupted World returns it.
    std::optional<KernelError> leftError_;
    // One hook queue per schedule: a hook's records are applied at step 0 of the
    // next sync point of the Run that recorded them; those left at the last sync
    // point are applied by the hook rounds right after it (spec F-12), so the
    // queue is empty when a Run ends and when one starts. Per schedule, so a Run
    // never touches another schedule's queue (the K0 nesting relies on it).
    struct HookQueue {
        std::string schedule;
        std::vector<Detail::Record> records;
        std::uint32_t recSeq = 0;
    };
    std::vector<HookQueue> hookQueues_;
    [[nodiscard]] HookQueue& HookQueueOf(std::string_view schedule);
    // S5: while an immediate change of ExclusiveWorld applies, the queue its
    // hooks record into instead of the schedule's (WorldExecutor::ImmediateFrom
    // applies it before the change returns); nullptr otherwise.
    HookQueue* hookTarget_ = nullptr;
    // Spec F-12: the product's default hook round limit (KernelSettings, read
    // when each Run starts), and the hooks that recorded during the current
    // hook round (HookData index, call order; for the limit's message).
    std::uint32_t hookRoundLimit_ = 8;
    bool trackRoundRecorders_ = false;
    std::vector<std::size_t> roundRecorders_;

    ChangeStamp stamp_ = 0;
    std::uint64_t syncEpoch_ = 0;
    std::uint64_t callSerial_ = 0;
    // +1 at every change of the alive-entity list (spec A.1 epoch, with the columns' own).
    std::uint64_t entitiesEpoch_ = 0;
    std::uint64_t droppedMissingEntity_ = 0;
    std::uint64_t rejectedSpawns_ = 0;

    // Layer 2: one snapshot per column, resource and message buffer; allocated
    // only with verification on (target/system-contract 2b condition 4).
    mutable std::vector<Snapshot> columnSnapshots_;
    mutable std::vector<Snapshot> resourceSnapshots_;
    mutable std::vector<Snapshot> messageSnapshots_;
    std::vector<bool> excluded_;

    std::vector<Detail::AccessDesc> hookAccess_;
    // Per observer (Registrar::Observer order): its access description, its
    // last-run stamp (B-12) and Local<T> values; kept across versions (the
    // observers of a version are those of the last plus the new ones).
    struct ObserverState {
        ChangeStamp lastRun = 0;
        std::vector<std::pair<TypeKey, std::shared_ptr<void>>> locals;
    };
    std::vector<Detail::AccessDesc> observerAccess_;
    std::vector<ObserverState> observerState_;
    // The observers that recorded during the current hook round (their index;
    // for the limit's message, beside roundRecorders_).
    std::vector<std::size_t> roundObserverRecorders_;
    // The WorldExecutor running this World (set by it): a hook's call carries it too,
    // so ExclusiveWorld::RunSchedule in a hook reaches the refusal (X-13).
    WorldExecutor* executor_ = nullptr;
    std::function<void(const KernelError&)> breakHook_;
};

} // namespace GYO::Kernel
