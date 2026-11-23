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
// The kernel's own diagnostics record of dropped messages (World.hpp).
struct DroppedMessages;
// Why a spawn was dropped (World.hpp defines the values; spec F-13).
enum class SpawnRejectReason : std::uint8_t;

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
    Message,
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
    // ExclusiveWorld: reads and writes every column and resource (DI-KS 1); the
    // event buffers are not in it. Its Writes<...> annotation adds no entry.
    bool wholeWorld = false;
};

// The record kinds of the deferred queue (spec F.1).
// Whether an application of records derives data from the cells it adds (World::ApplySync).
enum class Derive : std::uint8_t {
    InitialValues,  // a change: step 5a fills each Registrar::InitialValue target of a new request cell
    Nothing,        // a restore: the records are put back as given (DI-KS 12)
};

// How an application keeps relationships (Relationship.hpp; DI-KS 30 to 33).
enum class Relationships : std::uint8_t {
    Change,        // keeps the targets (Children) and expands linked despawns
    Restore,       // keeps the targets; no linked expansion (a restore gives the whole alive set)
    RestoreGiven,  // the restore names the target type: put back as given, no upkeep, no linked expansion
};

enum class RecordKind : std::uint8_t {
    Spawn,
    Insert,
    Remove,
    Despawn,
    ResourceInsert,
    ResourceRemove,
    // Commands::Trigger (event cursor declaration 3.3): id is the event type id,
    // value its payload (EventBox), entity the target when hasTarget. Delivered
    // at the last step of the sync point, in record order (time-events
    // declaration 4.2).
    Trigger,
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
    // SpawnWithId (no allotter) as opposed to Spawn.
    bool withId = false;
    // A Spawn of ExclusiveWorld::Restore (primitive 3, authority-rollback
    // declaration 2.1): an entity put back alive with its old id, which may be
    // in this World's allotter range; that check is skipped, the others are not.
    bool restore = false;
    // Trigger: the event names a target entity (entity).
    bool hasTarget = false;
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

// The mount of an observer's reader key and cursor (event cursor declaration
// 3.2: an observer is a reader): system is then the observer's index
// (Registrar::Observer order), slot 0. No System has that many mounts.
inline constexpr std::uint32_t kObserverMount = 0xFFFFFFFFU;

// Which reader a call is (event cursor declaration 3.2; MessageCursor): a
// System's mount (slot 0) or that mount's run condition number slot (from 1),
// or an observer (mount kObserverMount). Set by the Executor before a System
// and before each of its conditions, and before an observer; a hook has none
// (it cannot hold a MessageReader).
struct ReaderKey {
    std::uint32_t system = 0;
    std::uint32_t mount = 0;
    std::uint32_t slot = 0;
    bool valid = false;
};

// One System or hook call (spec B.1 runtime "SystemContext"). Lives on the
// Executor's stack for the duration of the call; handles never keep a pointer
// to it (a handle kept past the call would dangle), they keep the World and the
// call's serial number instead.
struct SystemContext {
    World* world = nullptr;
    // The source World of a RunExtract's step (DI-KS 35): what Extract
    // parameters read, const only (DI-KS 36); nullptr outside one.
    const World* extractSource = nullptr;
    // The Executor running this World's schedules: ExclusiveWorld::RunSchedule
    // runs a schedule through it (K0). One World per WorldExecutor today (T5 makes it
    // "this World's Executor").
    WorldExecutor* executor = nullptr;
    const AccessDesc* access = nullptr;
    std::string_view name;
    std::string_view schedule;
    std::uint32_t planPos = 0;
    bool hook = false;
    // A hook an immediate change of ExclusiveWorld runs (S5): planPos is the
    // exclusive System's step, not a sync point's (the error text says so).
    bool immediateChange = false;
    // An observer (event cursor declaration 3.3): records like a hook (F-5) and
    // is given its event through On<E>: the observer's event type key, the
    // payload (nullptr for Add<T> and Remove<T>) and the target.
    bool observer = false;
    TypeKey eventKey{};
    const void* eventPayload = nullptr;
    std::optional<EntityId> eventTarget;
    // The stamp this step took (spec E.2 step 3) and the mount's last-run stamp (B-12).
    ChangeStamp stamp = 0;
    ChangeStamp lastRun = 0;
    std::uint64_t callSerial = 0;
    std::uint64_t syncEpoch = 0;
    // The reader this call is for MessageReader (not valid in a hook).
    ReaderKey reader;
    // Access-verification violations of the call (W-4, W-5): always listed by
    // the Executor as siblings beside the System's own Err (spec B.4 point 7,
    // E.2 step 5); the System never sees them.
    std::vector<KernelError> errors;
    // Errors a kernel API returned to the System as Err (W-9, nested-run refusals).
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
[[nodiscard]] MessageTypeId MessageIdOf(const World& world, TypeKey key);
[[nodiscard]] EventTypeId EventIdOf(const World& world, TypeKey key);

// Spec W-4: with verification on, a handle whose call serial or sync epoch is
// not the current one records a violation in the current call.
void CheckHandle(World& world, std::uint64_t callSerial, std::uint64_t syncEpoch, std::string_view what);

[[nodiscard]] ColumnBase& ColumnAt(World& world, ComponentId id);
[[nodiscard]] ResourceBase& ResourceAt(World& world, ResourceId id);
// Read-only storage of an extract source (DI-KS 36), for Extract parameters only: no
// running-call check (W-6; the source is not running) and no write.
[[nodiscard]] const ColumnBase& SourceColumnAt(const World& source, ComponentId id);
[[nodiscard]] const ResourceBase& SourceResourceAt(const World& source, ResourceId id);
[[nodiscard]] MessageBufferBase& MessagesAt(World& world, MessageTypeId id);
[[nodiscard]] ChangeStamp CurrentStamp(const World& world) noexcept;
[[nodiscard]] std::span<const EntityId> AliveEntities(const World& world) noexcept;
[[nodiscard]] std::size_t ComponentCount(const World& world) noexcept;
[[nodiscard]] std::size_t ResourceCount(const World& world) noexcept;
[[nodiscard]] std::vector<ComponentId> SetMembersOf(const World& world, TypeKey setTag);

// K0 (v8-arch-gyo2-nested-schedule): runs schedule from inside the running
// exclusive System call (ExclusiveWorld::RunSchedule; Executor.cpp). Refused
// with Err, the World interrupted and the error reported once by the outermost
// Run: from a hook (X-13), inside a Read or Modify closure (X-14), an unknown
// name (X-12), an extract schedule (X-15), a schedule running already at any level (X-11). An Interrupted World
// runs nothing: the error a nested level left is returned.
[[nodiscard]] Engine::Base::Result<void, KernelError> RunNested(SystemContext& call, std::string_view schedule,
                                                                bool inClosure);
// What an immediate structural change of ExclusiveWorld did (declaration
// v8-arch-gyo2-kernel-subtract-structure 3.5): entity, the id a Spawn took
// from the allotter; changed, whether there was something to change (Insert:
// the entity is alive; Remove: the cell was there; Despawn: the entity was
// alive; RemoveResource: it was present); rejected, why a spawn was dropped
// and recorded (spec F-13; for an allotter's id that is alive too, so
// ExclusiveWorld::Spawn returns no id).
struct ImmediateOutcome {
    EntityId entity = 0;
    bool changed = false;
    std::optional<SpawnRejectReason> rejected;
};

// S5 (DI-KS 18; Executor.cpp): applies one record of the running exclusive
// System call now, as a sync point applies it (spec F.2: the same steps, the
// hooks it triggers run now), with the handle's stamp; then, round after round,
// only what those hooks recorded (each round a new stamp and sync epoch + 1),
// up to the schedule's hook round limit (spec F-12). Neither the running
// level's records nor the schedule's hook queue are applied (DI-KS 19). A Spawn
// record with withId false takes its id from the allotter here. Refused with
// Err, the World interrupted and the error reported once by the outermost Run:
// from a hook (HookChangesMembership), inside a Read or Modify closure
// (ClosureChangesMembership), with verification on when the System changed
// what it does not declare before (A.6, compared first); an Interrupted World
// changes nothing and returns the left error. An exception that leaves the
// change (a hook or an InitialValue function threw) interrupts the World and
// leaves ImmediateChangeAborted for the outermost Run. Err, the change not
// made and the call going on (the Run ends in that Err when the System
// returns, DI-KS 2): no allotter (NoAllotter), the allotter used up
// (AllotterExhausted). GYO_ASSERT: the membership changed behind the handle
// before (W-3).
[[nodiscard]] Engine::Base::Result<ImmediateOutcome, KernelError> ApplyImmediate(SystemContext& call, Record record,
                                                                                ChangeStamp stamp, bool inClosure);

// ExclusiveWorld::Restore (primitives 2 and 3, authority-rollback declaration
// 2.1): applies the records of one restore now, all of them as one phase of a
// sync point with the handle's one stamp (despawns, removes, spawns, inserts,
// initial values, then OnAdd: a hook sees every cell put back), then the hook
// rounds, as ApplyImmediate; refused and failing as ApplyImmediate. Its Spawn
// records carry restore (no allotter range check) and an empty bundle.
// targetsGiven: the RestoreSet names Children (Relationships::RestoreGiven).
[[nodiscard]] Engine::Base::Result<void, KernelError> ApplyRestore(SystemContext& call, std::vector<Record> records,
                                                                   ChangeStamp stamp, bool inClosure, bool targetsGiven);

// Event cursor declaration 3.3 (time-events declaration 4.2): ExclusiveWorld::
// Trigger. Runs the observers of event id now, each like one nested System
// call (its own stamp; DI-KS 16 to 21); what they record goes the way of an
// immediate change (S5): into a queue of its own, applied round after round
// before the call returns, up to the schedule's hook round limit
// (HookRoundLimit, the World interrupted); this level's records and the
// schedule's hook queue wait (DI-KS 19). Refused with Err, the World
// interrupted and the error reported once by the outermost Run: from a hook
// (HookChangesMembership: the rounds would change membership now), inside a
// Read or Modify closure (ClosureChangesMembership). An observer's Err
// interrupts the World and is reported once. An Interrupted World runs no
// observer and returns the left error.
[[nodiscard]] Engine::Base::Result<void, KernelError> TriggerNow(SystemContext& call, EventTypeId id,
                                                                 std::unique_ptr<ValueBox> payload,
                                                                 std::optional<EntityId> target, bool inClosure);

// Event cursor declaration 3.2: the cursor of the call's reader in message id
// (the highest seq it has read). A reader's first run makes its cursor, at the
// seq before the oldest record kept (so it reads every record still kept); from
// then on it holds back every record past its cursor. GYO_ASSERT: the call is a
// reader (not a hook).
[[nodiscard]] std::uint64_t MessageCursorOf(World& world, const SystemContext& call, MessageTypeId id);
// Moves the reader's cursor in message id up to read (never back). Not a write:
// the cursor table is not in the boundary comparison (A.6 layer 2).
void AdvanceMessageCursor(World& world, const ReaderKey& reader, MessageTypeId id, std::uint64_t read);
// A write to a full buffer dropped the record seq of message id: counted into
// this outermost Run's DroppedMessages of that type, with the readers that held
// it unread.
void RecordDroppedMessage(World& world, MessageTypeId id, std::uint64_t seq);

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
// The same error with the same context, for the caller only: not recorded in
// call.returned (so the Run does not end in it) and no break hook (it is no
// kernel error, like a spawn a sync point drops: spec F-13).
[[nodiscard]] KernelError CallerError(SystemContext& call, KernelErrorCode code, std::string rule, std::string message);


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
