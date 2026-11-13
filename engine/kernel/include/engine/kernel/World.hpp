#pragma once

// World (spec A; target/world.zh-Hant.md; target/kernel DI-KS 2, 3, 4, 6, 9,
// 10, 11, 14).
//
// One App's data: columns, resources, event buffers, the deferred queue,
// change stamps, domain ticks and its state. Made from a frozen Registry; its
// lifetime is one generation (spec A.1: a generation change is a new World,
// made from the same frozen Registry).
//
// The public functions below are the Host's (state Ready) and, inside a running
// System, a path that does not go through a handle (a World* kept in a global,
// for example). Every one checks:
//   W-6   Running or Applying with no System or hook of this World running on
//         this thread: GYO_ASSERT (always on);
//   W-3   a structural change (insert, remove, despawn, resource presence)
//         outside Ready: GYO_ASSERT (always on);
//   W-6b  a Host write in Ready after the first Run to zone C data other than a
//         boundary (role Input) or external resource: GYO_ASSERT (always on);
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

class App;

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

class World {
public:
    // registry must be frozen and outlive the World.
    explicit World(const Registry& registry, std::string name = {});
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    [[nodiscard]] WorldState State() const noexcept { return state_; }
    [[nodiscard]] const std::string& Name() const noexcept { return name_; }
    [[nodiscard]] const FrozenRegistry& Types() const noexcept { return *frozen_; }
    // Whether an Executor has started a Run on this World (W-6b, J-50).
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
        Access(Detail::EntryKind::Resource, resource, Detail::kModeRead, false);
        const auto& slot = TypedResource<T>(resource);
        return slot.value ? &*slot.value : nullptr;
    }

    template <class T>
    [[nodiscard]] T* ResourceMut() {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, false);
        auto& slot = TypedResource<T>(resource);
        if (!slot.value) return nullptr;
        slot.changed = WriteStamp();
        return &*slot.value;
    }

    // Host, Ready only. KernelSettings may be written only before the first Run
    // (W-6b, J-50: the Executor reads it once, when the first Run starts).
    template <class T>
    void SetResource(T value) {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, true);
        auto& slot = TypedResource<T>(resource);
        slot.value = std::move(value);
        slot.changed = WriteStamp();
    }

    template <class T>
    void RemoveResource() {
        const ResourceId resource = Detail::ResourceIdOf(*this, KeyOf<T>());
        Access(Detail::EntryKind::Resource, resource, Detail::kModeWrite, true);
        TypedResource<T>(resource).value.reset();
    }

    // ---- Events (read-only views for the Host and tests: every record kept)

    template <class E>
    [[nodiscard]] std::vector<E> EventPayloads() const {
        const EventTypeId event = Detail::EventIdOf(*this, KeyOf<E>());
        Access(Detail::EntryKind::Event, event, Detail::kModeRead, false);
        std::vector<E> out;
        for (const auto& record : static_cast<const Detail::EventBuffer<E>&>(*events_[event]).records) {
            out.push_back(record.payload);
        }
        return out;
    }

    // ---- Diagnostics (B-13): the World end of DiagnosticsWriter<R>; kept when
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

    // ---- Stamps, ticks, counters

    // The last stamp taken (spec A.1).
    [[nodiscard]] ChangeStamp Stamp() const noexcept { return stamp_; }
    // +1 at every sync point, also an empty one (spec F-6).
    [[nodiscard]] std::uint64_t SyncEpoch() const noexcept { return syncEpoch_; }
    // The domain tick (spec section 0): advanced by the domain's clock schedule's Run.
    [[nodiscard]] std::uint64_t Tick(Domain domain) const noexcept;
    // The first stamp of a tick of a domain while it is in the bounded table (spec A.1 tickStamps).
    [[nodiscard]] std::optional<ChangeStamp> TickStamp(Domain domain, std::uint64_t tick) const noexcept;
    // Spec F.2 step 5: inserts dropped because the entity was gone (a result, not an error).
    [[nodiscard]] std::uint64_t DroppedMissingEntity() const noexcept { return droppedMissingEntity_; }

    // ---- Verification (spec A.6)

    // Leaves column T out of the boundary comparison (large columns); before
    // the first Run only. The list is part of every verification report.
    template <class T>
    void ExcludeFromVerification() {
        GYO_ASSERT(state_ == WorldState::Ready && !hasRun_);
        excluded_[Detail::ComponentIdOf(*this, KeyOf<T>())] = true;
    }

    // The World summary of spec D-9: keys and values of every column, resource
    // values and event payloads, without stamps, tick stamps or Plan positions.
    // Version 1 (Fingerprint.cpp). Readable in Ready and Interrupted.
    [[nodiscard]] Fingerprint Summary() const;

    // The break hook called where a KernelError is made when
    // KernelSettings::breakOnKernelError is on (target/system-contract 2b). It
    // returns; the Err then goes on as usual. Default: none (a debugger trap is
    // the Host's to install; the kernel has no platform header).
    void SetBreakHook(std::function<void(const KernelError&)> hook) { breakHook_ = std::move(hook); }

private:
    friend class App;
    friend Detail::SystemContext* Detail::CurrentCall(World&) noexcept;
    friend Detail::SystemContext& Detail::RequireCall(World&);
    friend ComponentId Detail::ComponentIdOf(const World&, TypeKey);
    friend ResourceId Detail::ResourceIdOf(const World&, TypeKey);
    friend EventTypeId Detail::EventIdOf(const World&, TypeKey);
    friend void Detail::CheckHandle(World&, std::uint64_t, std::uint64_t, std::string_view);
    friend Detail::ColumnBase& Detail::ColumnAt(World&, ComponentId);
    friend Detail::ResourceBase& Detail::ResourceAt(World&, ResourceId);
    friend Detail::EventBufferBase& Detail::EventsAt(World&, EventTypeId);
    friend ChangeStamp Detail::CurrentStamp(const World&) noexcept;
    friend std::uint64_t Detail::DomainTick(const World&, Domain) noexcept;
    friend std::span<const EntityId> Detail::AliveEntities(const World&) noexcept;
    friend Domain Detail::EventDomain(const World&, EventTypeId);
    friend std::size_t Detail::ComponentCount(const World&) noexcept;
    friend std::size_t Detail::ResourceCount(const World&) noexcept;
    friend std::vector<ComponentId> Detail::SetMembersOf(const World&, TypeKey);
    friend void Detail::Push(Detail::SystemContext&, Detail::Record);
    friend KernelError Detail::CallError(Detail::SystemContext&, KernelErrorCode, std::string, std::string);
    friend std::optional<KernelError> Detail::CheckUntypedCommand(Detail::SystemContext&, ComponentId, EntityId, bool);
    friend std::optional<KernelError> Detail::CheckDespawn(Detail::SystemContext&, EntityId);
    friend std::optional<KernelError> Detail::CheckExclusiveWrite(Detail::SystemContext&, ComponentId, EntityId);
    friend Detail::ResourceBase* Detail::AllotterSlot(World&) noexcept;
    friend std::optional<EntityId> Detail::Allot(World&);
    friend Detail::DiagnosticsBase* Detail::FindDiagnostics(World&, TypeKey) noexcept;
    friend void Detail::AddDiagnostics(World&, TypeKey, std::unique_ptr<Detail::DiagnosticsBase>);
    friend std::optional<KernelError> Detail::CombineCall(Detail::SystemContext&, Engine::Base::Result<void, KernelError>,
                                                          const std::vector<std::string>&);

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
    void MakeAlive(EntityId id);
    // The stamp a write through the World API carries: in a run, the current
    // step's; in Ready, the stamp the next step will take (a Host write belongs
    // to the next step, so every mount that has not run since sees it: B-12).
    [[nodiscard]] ChangeStamp WriteStamp() const noexcept { return state_ == WorldState::Ready ? stamp_ + 1 : stamp_; }
    void MarkApiWritten(Detail::EntryKind kind, std::uint32_t id) const;

    // ---- Executor side (App, World.cpp)

    // F.2: applies the window's deferred records and the hook queue; Err from a
    // hook or the Z5 recheck. stamp: the sync point's stamp.
    Engine::Base::Result<void, KernelError> ApplySync(std::uint32_t planPos, std::string_view schedule,
                                                      ChangeStamp stamp);
    // Spec E.5: discards every record not yet applied (hook queue included).
    void DiscardDeferred() noexcept;
    // F.3 trim of one domain's buffers: drops records before the current tick.
    void TrimEvents(Domain domain);
    // Spec W-3: the sum of every column's epoch and the entity list's; the
    // Executor checks that no System call changes it (always on, O(columns)).
    [[nodiscard]] std::uint64_t MembershipEpoch() const noexcept;
    // W-1 with verification on.
    [[nodiscard]] std::optional<std::string> StructureProblem() const;

    // Layer 2 (spec A.6): snapshots and comparisons.
    using Snapshot = Detail::Snapshot;
    void TakeSnapshots();
    // Names of storages changed outside the World API since the last snapshot.
    [[nodiscard]] std::vector<std::string> CompareBefore();
    // Names of storages access does not declare as written that changed during the call.
    [[nodiscard]] std::vector<std::string> CompareAfter(const Detail::AccessDesc& access);
    // Runs one hook (spec F.4) with tlCurrent set to its access description.
    Engine::Base::Result<void, KernelError> RunHook(std::size_t hook, EntityId entity, ComponentId component,
                                                    std::uint32_t planPos, std::string_view schedule, ChangeStamp stamp);
    // OnAdd / OnRemove hooks of one cell, in registration order.
    Engine::Base::Result<void, KernelError> RunHooks(HookTrigger trigger, EntityId entity, ComponentId component,
                                                     std::uint32_t planPos, std::string_view schedule, ChangeStamp stamp);

    const Registry* registry_;
    const FrozenRegistry* frozen_;
    const Detail::RegistryData* data_;
    std::string name_;
    WorldState state_ = WorldState::Ready;
    bool hasRun_ = false;
    bool verification_ = false;
    bool breakOnKernelError_ = false;

    std::vector<EntityId> entities_;
    std::vector<std::unique_ptr<Detail::ColumnBase>> columns_;
    std::vector<std::unique_ptr<Detail::ResourceBase>> resources_;
    std::vector<std::unique_ptr<Detail::EventBufferBase>> events_;
    std::vector<std::pair<TypeKey, std::unique_ptr<Detail::DiagnosticsBase>>> diagnostics_;

    // The current sync window's deferred records and the hook queue (records
    // of hooks, applied first at the next sync point: spec F.2 step 0).
    std::vector<Detail::Record> window_;
    std::vector<Detail::Record> hookQueue_;
    std::uint32_t hookRecSeq_ = 0;

    ChangeStamp stamp_ = 0;
    std::uint64_t syncEpoch_ = 0;
    std::uint64_t callSerial_ = 0;
    // +1 at every change of the alive-entity list (spec A.1 epoch, with the columns' own).
    std::uint64_t entitiesEpoch_ = 0;
    std::uint64_t droppedMissingEntity_ = 0;
    static_assert(static_cast<std::size_t>(Domain::Loading) + 1 == 4, "one tick per Domain value");
    std::array<std::uint64_t, 4> ticks_{};
    static constexpr std::size_t kTickStampSlots = 256;
    struct TickStampSlot {
        std::uint64_t tick = 0;
        ChangeStamp stamp = 0;
        bool used = false;
    };
    std::array<std::array<TickStampSlot, kTickStampSlots>, 4> tickStamps_{};

    // Layer 2: one snapshot per column, resource and event buffer; allocated
    // only with verification on (target/system-contract 2b condition 4).
    mutable std::vector<Snapshot> columnSnapshots_;
    mutable std::vector<Snapshot> resourceSnapshots_;
    mutable std::vector<Snapshot> eventSnapshots_;
    std::vector<bool> excluded_;

    std::vector<Detail::AccessDesc> hookAccess_;
    std::function<void(const KernelError&)> breakHook_;
};

} // namespace GYO::Kernel
