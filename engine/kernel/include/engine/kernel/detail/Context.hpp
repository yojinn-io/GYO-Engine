#pragma once

// Internal to the kernel (namespace Detail): what a running System call is
// (spec B.1 runtime, E.2) and the World operations the parameter handles use
// (Params.hpp). The World is only forward-declared here: handles reach it
// through the functions below, defined in World.cpp, so Params.hpp does not
// need World.hpp (spec B.4 point 8: the World's typed API is detail, open only
// to the handles).

#include "engine/base/Result.hpp"
#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/detail/Storage.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Kernel {

class World;
class WorldExecutor;
struct AccessSet;
// The kernel's own diagnostics record of a dropped spawn (World.hpp; spec F-13).
struct RejectedSpawn;

namespace Detail {

// One entry of an access description (spec A.6 layer 1): an id of one kind
// and the modes declared for it. Kept sorted by (kind, id).
enum AccessModeBits : std::uint8_t {
    kModeRead = 1,
    kModeWrite = 2,
};

enum class EntryKind : std::uint8_t {
    Component,
    Resource,
    Event,
};

struct AccessEntry {
    EntryKind kind;
    std::uint32_t id;
    std::uint8_t modes;
};

// The access description a System and a hook share (spec F.6: tlCurrent points
// to it for both). Derived from the Registry's AccessSet when the WorldExecutor is made.
struct AccessDesc {
    std::vector<AccessEntry> entries;
    // ExclusiveWorld: reads and writes every column and resource (DI-KS 1; but
    // a resource whose affinity the runner does not satisfy: S-16); the event
    // buffers are not in it. Its Writes<...> annotation adds no entry.
    bool wholeWorld = false;
};

// The record kinds of the deferred queue (spec F.1).
enum class RecordKind : std::uint8_t {
    Spawn,
    Insert,
    Remove,
    Despawn,
    ResourceInsert,
    ResourceRemove,
};

struct BundleValue {
    std::uint32_t component;
    std::unique_ptr<ValueBox> value;
};

// One deferred record (spec F.1). (planPos, recSeq) is unique in one sync window.
struct Record {
    RecordKind kind = RecordKind::Insert;
    EntityId entity = 0;
    // Component id (Insert, Remove) or resource id (Resource*).
    std::uint32_t id = 0;
    std::uint32_t planPos = 0;
    std::uint32_t recSeq = 0;
    // The emitter's zone (RejectedSpawn::emitterZone; goes with the zones, a later batch).
    Zone emitterZone = Zone::B;
    // SpawnWithId (no allotter) as opposed to Spawn.
    bool withId = false;
    // Who recorded it (spec F-13's RejectedSpawn): the System's or hook's name (the Registry's string).
    std::string_view emitter;
    bool emitterHook = false;
    // One per System and per hook (its access description); compared for equality only
    // (spec F-13: records of one id from one emitter keep the first).
    const void* emitterKey = nullptr;
    std::unique_ptr<ValueBox> value;
    std::vector<BundleValue> bundle;

    [[nodiscard]] static Record Of(RecordKind kind, EntityId entity, std::uint32_t id = 0) {
        Record record;
        record.kind = kind;
        record.entity = entity;
        record.id = id;
        return record;
    }
};

// One System or hook call (spec B.1 runtime "SystemContext"). Lives on the
// Executor's stack for the duration of the call; handles never keep a pointer
// to it (a handle kept past the call would dangle), they keep the World and the
// call's serial number instead.
struct SystemContext {
    World* world = nullptr;
    // The Executor running this World's schedules: ExclusiveWorld::RunSchedule
    // runs a schedule through it (K0). One World per WorldExecutor today (T5 makes it
    // "this World's Executor").
    WorldExecutor* executor = nullptr;
    const AccessDesc* access = nullptr;
    std::string_view name;
    std::string_view schedule;
    std::uint32_t planPos = 0;
    Zone zone = Zone::B;
    bool hook = false;
    Domain domain = Domain::None;
    std::uint64_t tick = 0;
    // The stamp this step took (spec E.2 step 3) and the mount's last-run stamp (B-12).
    ChangeStamp stamp = 0;
    ChangeStamp lastRun = 0;
    std::uint64_t callSerial = 0;
    std::uint64_t syncEpoch = 0;
    // Access-verification violations of the call (W-4, W-5): always listed by
    // the Executor as siblings beside the System's own Err (spec B.4 point 7,
    // E.2 step 5); the System never sees them.
    std::vector<KernelError> errors;
    // Errors a kernel API returned to the System as Err (S-16, W-9, nested-run refusals).
    // The System should pass them on; one it drops is listed by the Executor all
    // the same (nothing is silent, target/error-handling).
    std::vector<KernelError> returned;
    // Where this call's deferred records go and the call's own record counter.
    std::vector<Record>* queue = nullptr;
    std::uint32_t* recSeq = nullptr;
    // Hooks only: the entity and component that triggered the hook (HookCtx).
    EntityId hookEntity = 0;
    ComponentId hookComponent = 0;
    // The mount's Local<T> storage (spec B-12: one per mount).
    std::vector<std::pair<TypeKey, std::shared_ptr<void>>>* locals = nullptr;
};

// The call running on this thread (spec A.6 layer 1: tlCurrent, Verification.cpp),
// set and cleared by the Executor around every System and hook call.
[[nodiscard]] SystemContext* CurrentThreadCall() noexcept;
void SetCurrentThreadCall(SystemContext* call) noexcept;

// Sets tlCurrent for one call and restores the previous value on exit, also
// when the call throws (spec E.5: the Executor does not catch).
class CallScope {
public:
    explicit CallScope(SystemContext& call) noexcept : previous_(CurrentThreadCall()) { SetCurrentThreadCall(&call); }
    ~CallScope() { SetCurrentThreadCall(previous_); }
    CallScope(const CallScope&) = delete;
    CallScope& operator=(const CallScope&) = delete;

private:
    SystemContext* previous_;
};

// The access description of an access set (spec A.6 layer 1), sorted by (kind, id).
[[nodiscard]] AccessDesc MakeAccessDesc(const AccessSet& access);

// ---- World operations for the handles (World.cpp)

// The call currently running on this thread in world, or nullptr.
[[nodiscard]] SystemContext* CurrentCall(World& world) noexcept;
// W-6: GYO_ASSERT that a System or hook of this World is running on this thread.
[[nodiscard]] SystemContext& RequireCall(World& world);

[[nodiscard]] ComponentId ComponentIdOf(const World& world, TypeKey key);
[[nodiscard]] ResourceId ResourceIdOf(const World& world, TypeKey key);
[[nodiscard]] EventTypeId EventIdOf(const World& world, TypeKey key);

// Spec W-4: with verification on, a handle whose call serial or sync epoch is
// not the current one records a violation in the current call.
void CheckHandle(World& world, std::uint64_t callSerial, std::uint64_t syncEpoch, std::string_view what);

[[nodiscard]] ColumnBase& ColumnAt(World& world, ComponentId id);
[[nodiscard]] ResourceBase& ResourceAt(World& world, ResourceId id);
[[nodiscard]] EventBufferBase& EventsAt(World& world, EventTypeId id);
[[nodiscard]] ChangeStamp CurrentStamp(const World& world) noexcept;
[[nodiscard]] std::uint64_t DomainTick(const World& world, Domain domain) noexcept;
[[nodiscard]] std::span<const EntityId> AliveEntities(const World& world) noexcept;
[[nodiscard]] Domain EventDomain(const World& world, EventTypeId id);
[[nodiscard]] std::size_t ComponentCount(const World& world) noexcept;
[[nodiscard]] std::size_t ResourceCount(const World& world) noexcept;
[[nodiscard]] std::vector<ComponentId> SetMembersOf(const World& world, TypeKey setTag);

// K0 (v8-arch-gyo2-nested-schedule): runs schedule from inside the running
// exclusive System call (ExclusiveWorld::RunSchedule; Executor.cpp). Refused
// with Err, the World interrupted and the error reported once by the outermost
// Run: from a hook (X-13), inside a Read or Modify closure (X-14), an unknown
// name (X-12), Startup (H-12) or while Startup runs (H-13), an extract schedule
// (X-15), a schedule running already at any level (X-11). An Interrupted World
// runs nothing: the error a nested level left is returned.
[[nodiscard]] Engine::Base::Result<void, KernelError> RunNested(SystemContext& call, std::string_view schedule,
                                                                bool inClosure);
// The World's sync epoch now (a handle renews its own after a nested run, W-4).
[[nodiscard]] std::uint64_t CurrentSyncEpoch(const World& world) noexcept;
// Takes a new stamp (+1; spec E.2: after a nested run, the exclusive System's
// later writes carry a stamp of their own).
[[nodiscard]] ChangeStamp NextStamp(World& world) noexcept;

// Records a deferred command of the current call (spec F.1). GYO_ASSERT when
// the World is Applying and the caller is not a hook (spec F-5).
void Push(SystemContext& call, Record record);

// Builds a kernel error for the current call (context: the call's name and Plan
// position, the World's name), records it in call.returned and returns it for
// the API to return as Err. Calls the World's break hook first when
// breakOnKernelError is on (target/system-contract 2b).
[[nodiscard]] KernelError CallError(SystemContext& call, KernelErrorCode code, std::string rule, std::string message);

// The always-on check of ExclusiveWorld (target/error-handling "the added
// row"): returns the error (already recorded in call.errors through CallError)
// or nothing.
//   S-16  an ExclusiveWorld read of a resource whose thread affinity the running
//         schedule's runner does not satisfy.
[[nodiscard]] std::optional<KernelError> CheckExclusiveRead(SystemContext& call, ResourceId resource);

// The allotter resource of the World (spec A.1), or nullptr when none is declared.
[[nodiscard]] ResourceBase* AllotterSlot(World& world) noexcept;
[[nodiscard]] std::optional<EntityId> Allot(World& world);

// The Executor's merge of one call's errors (spec B.4 point 7, E.2 step 5):
// violations, boundary differences (storage names), kernel errors the System
// dropped, and the System's own Err, as one sibling list; nothing when the call
// is clean. One error alone is returned as is.
[[nodiscard]] std::optional<KernelError> CombineCall(SystemContext& call, Engine::Base::Result<void, KernelError> result,
                                                     const std::vector<std::string>& changed);

// DiagnosticsWriter<T>'s World-side buffer (B-13).
[[nodiscard]] DiagnosticsBase* FindDiagnostics(World& world, TypeKey key) noexcept;
void AddDiagnostics(World& world, TypeKey key, std::unique_ptr<DiagnosticsBase> buffer);

} // namespace Detail

} // namespace GYO::Kernel
