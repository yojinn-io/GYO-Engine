#pragma once

// WorldExecutor (spec section 0 "Runtime", E; kernel HANDOFF v8-arch-kernel-app-name,
// v8-arch-gyo2-kernel-world-exec-type):
// one World, the frozen Registry it was made from, and the Serial Executor
// that runs that Registry's schedules (target/kernel DI-KS 2, 9, 14; spec E.2).
//
// Run(schedule) follows spec E.2: for each Plan step in order,
//   System     (0) with verification on, the boundary comparison "before"
//              (a change found here is the previous step's or the Ready
//              window's: a kernel-level Err, the System is not called);
//              (1) tlCurrent set; (2) run conditions, in order, short-circuit;
//              (3) the step's stamp (also when a condition is false: X-5);
//              (4) the call; (5) with verification on, the comparison
//              "after" (also when (4) was skipped); every error of the call is
//              one sibling list with the System's own Err; (6) tlCurrent cleared;
//   SyncPoint  a stamp; the deferred records applied in the order of spec F.2;
//              with verification on, W-1; the sync epoch +1; after the last
//              sync point of the Plan, the hook rounds (spec F-12): the
//              schedule's hook queue applied round after round until it is
//              empty, each round with its own stamp and sync epoch, at most
//              the schedule's hookRoundLimit or else KernelSettings'
//              (HookRoundLimit Err past it); then W-1 again. Not a Plan step.
// When the outermost Run ends (after its hook rounds; not a Plan step), every
// message record that every reader holding it has read is trimmed (event
// cursor declaration 3.2); a nested run and a Run that ends in an Err trim nothing.
// An Err from any source makes the World Interrupted, every record not yet
// applied is discarded, diagnostics are kept, and the outermost Run returns the
// Err once (spec E.5); the kernel logs nothing and has no error handler
// (target/error-handling DI 1, 2, 4). An exception from a System or hook is not
// caught: the World becomes Interrupted the same way and the exception goes on.
// Running an Interrupted World is a GYO_ASSERT (rebuild it: a new WorldExecutor).
//
// Nested runs (K0, v8-arch-gyo2-nested-schedule): a System holding
// ExclusiveWorld<...> may run another schedule (ExclusiveWorld::RunSchedule).
// The inner schedule runs whole, the same way (its own stamps, sync points,
// hook rounds; no message trim); the records the outer level has not
// applied stay at its own next sync point (W-3, F.2). An error inside: the World
// is Interrupted, the error is left for the outermost Run, which returns it once
// with the outer System's context; the
// Executor stops after every System that returns to an Interrupted World.
//
// Pushed events (event cursor declaration 3.3; time-events declaration 4.2):
// an observer runs where its event is triggered, never as a Plan step. Add<T>
// and Remove<T> observers run at the sync point that adds or removes the cell
// (per entity and component: OnAdd hooks, then Add observers; Remove
// observers, then OnRemove hooks; a despawned entity is destroyed after all of
// its cells ran theirs). Commands::Trigger runs its observers at the last step
// of the sync point that applies it, in record order. ExclusiveWorld::Trigger
// and Trigger below run them now. What an observer records goes the way of a
// hook's records (the hook queue, or the queue of the immediate change or
// trigger that ran it, applied round after round; F-12). The observers of one
// trigger run in a deterministic order (by name), which authors are not
// promised.
//
// The kernel knows no game initialization or startup schedule: which schedule
// runs when, and once, is the orchestration layer's (engine/orchestration,
// v8-arch-gyo2-orchestration-skeleton; DI-KS 15).
//
// v1 is Serial only (LIM-2). The WorldExecutor owns no thread (DI-KS 15): Run is called
// on one thread at a time by the caller, who decides which thread runs what
// (DI-KS 14); a thread-bound resource asserts when another thread touches it.

#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Kernel {

class WorldExecutor {
public:
    // registry must have built its Plans and outlive the WorldExecutor. worldName
    // identifies this WorldExecutor's World in errors and verification reports
    // (spec A.6); options is the World's declaration (WorldOptions, DI-KS 26).
    explicit WorldExecutor(const Registry& registry, std::string worldName = {}, WorldOptions options = {});
    ~WorldExecutor();
    WorldExecutor(const WorldExecutor&) = delete;
    WorldExecutor& operator=(const WorldExecutor&) = delete;

    [[nodiscard]] World& GetWorld() noexcept { return world_; }
    [[nodiscard]] const World& GetWorld() const noexcept { return world_; }

    // Runs one schedule's Plan once (spec E.2); first attaches the World and
    // the Systems' state to a new Registry version (declaration 3.2 rule 5).
    // GYO_ASSERT: the World is Ready (so not from inside a System) and the
    // schedule exists and is not an extract schedule (that one runs through RunExtract).
    Engine::Base::Result<void, KernelError> Run(std::string_view schedule);
    // Runs an extract schedule's Plan once (DI-KS 35, v8-arch-gyo2-sdl-extract):
    // an outermost Run of this World like Run (the running count, the attach,
    // KernelSettings, its own DroppedMessages records, the Ready window, steps,
    // sync points, hook rounds, the trim of this World's messages), except that
    // the Extract parameters of the Plan's Systems and of their run conditions
    // read source, through const access only (DI-KS 36). A schedule a System
    // runs nested from it sees no source. The kernel keeps no pointer to source
    // (DI-KS 37): the caller gives it every time. GYO_ASSERT: this World is
    // Ready; the schedule exists and is an extract schedule; source was made
    // from the Registry this World's Registry set as its extract source, and is
    // not this World; source is Ready.
    Engine::Base::Result<void, KernelError> RunExtract(std::string_view schedule, const World& source);

    // The Host's trigger of event E while the World is Ready (event cursor
    // declaration 3.3, "outside the World, Ready"): its observers run now, each
    // like one System step (its own stamp), and what they record is applied
    // before this returns, as a whole sync point applies it (hooks, Add and
    // Remove observers, triggers), round after round until nothing is left, at
    // most the World's KernelSettings::hookRoundLimit rounds. KernelSettings
    // is read when it starts, as when a Run starts. Err (returned once, the World
    // interrupted, the rest dropped, the rounds applied kept): an observer's or a hook's Err (an access violation included),
    // HookRoundLimit past the limit, or, with verification on, a change outside
    // the World API since the last comparison (PreCallChange; no observer runs).
    // A direct write in Ready (World::Insert, Remove, Despawn) runs no hook and
    // no Add or Remove observer. GYO_ASSERT: the World is Ready and E is a
    // registered event type.
    template <class E>
    Engine::Base::Result<void, KernelError> Trigger(E event, std::optional<EntityId> target = std::nullopt) {
        return TriggerFromHost(Detail::EventIdOf(world_, KeyOf<E>()),
                               std::make_unique<Detail::EventBox<E>>(std::move(event)), target);
    }


    // The fields of a "verification on" report (target/system-contract 2b
    // condition 2): the KernelSettings the last Run started with.
    [[nodiscard]] VerificationReport Verification() const;

private:
    struct MountState {
        // Spec B-12: per mount.
        ChangeStamp lastRun = 0;
        std::vector<std::pair<TypeKey, std::shared_ptr<void>>> locals;
    };
    struct SystemState {
        Detail::AccessDesc access;
        std::vector<MountState> mounts;
    };

    friend Engine::Base::Result<void, KernelError> Detail::RunNested(Detail::SystemContext&, std::string_view, bool);
    friend Engine::Base::Result<Detail::ImmediateOutcome, KernelError> Detail::ApplyImmediate(Detail::SystemContext&,
                                                                                            Detail::Record, ChangeStamp,
                                                                                            bool);
    friend Engine::Base::Result<void, KernelError> Detail::TriggerNow(Detail::SystemContext&, EventTypeId,
                                                                      std::unique_ptr<Detail::ValueBox>,
                                                                      std::optional<EntityId>, bool);
    friend Engine::Base::Result<void, KernelError> Detail::ApplyRestore(Detail::SystemContext&,
                                                                        std::vector<Detail::Record>, ChangeStamp, bool,
                                                                        bool);

    // One schedule's Plan (spec E.2): the outermost level, or nested inside the
    // running exclusive System's call.
    Engine::Base::Result<void, KernelError> RunPlan(std::string_view schedule, bool nested);
    Engine::Base::Result<void, KernelError> RunNestedFrom(Detail::SystemContext& call, std::string_view schedule,
                                                          bool inClosure);
    // S5 (Detail::ApplyImmediate): an immediate structural change of the running exclusive System call.
    Engine::Base::Result<Detail::ImmediateOutcome, KernelError> ImmediateFrom(Detail::SystemContext& call,
                                                                              Detail::Record record, ChangeStamp stamp,
                                                                              bool closureOpen);
    // Detail::ApplyRestore: the records of ExclusiveWorld::Restore, as one immediate change.
    Engine::Base::Result<void, KernelError> RestoreFrom(Detail::SystemContext& call, std::vector<Detail::Record> records,
                                                        ChangeStamp stamp, bool closureOpen, bool targetsGiven);
    // The checks an immediate change and a restore share, in the order of
    // RunNestedFrom (hook, closure, Interrupted, W-3, A.6 layer 2). Err: refused.
    Engine::Base::Result<void, KernelError> RefuseImmediate(Detail::SystemContext& call, bool closureOpen,
                                                            const std::string& who, const std::string& where);
    // The change itself and its hook rounds, as one immediate change with stamp.
    // relationshipsAfterChange: when given, World::RejectedRelationships() right after the change itself,
    // before its hook rounds (an immediate insert of ChildOf reports only its own result, DI-KS 31).
    Engine::Base::Result<void, KernelError> ApplyImmediateRecords(Detail::Derive derive, Detail::Relationships relationships,
                                                                  Detail::SystemContext& call,
                                                                  std::vector<Detail::Record> change, ChangeStamp stamp,
                                                                  const std::string& who, const std::string& where,
                                                                  std::uint64_t* relationshipsAfterChange = nullptr);
    // Detail::TriggerNow: ExclusiveWorld::Trigger of the running exclusive System call.
    Engine::Base::Result<void, KernelError> TriggerFrom(Detail::SystemContext& call, EventTypeId id,
                                                        std::unique_ptr<Detail::ValueBox> payload,
                                                        std::optional<EntityId> target, bool closureOpen);
    Engine::Base::Result<void, KernelError> TriggerFromHost(EventTypeId id, std::unique_ptr<Detail::ValueBox> payload,
                                                            std::optional<EntityId> target);
    // The two triggers outside a sync point: the observers of event id, each
    // with a stamp of its own, recording into a queue of their own (hookTarget_),
    // then that queue applied round after round (each a new stamp and sync
    // epoch + 1), at most limit rounds (HookRoundLimit past it). The caller
    // sets and resets the World's state and hookTarget_ and fails the World on Err.
    Engine::Base::Result<void, KernelError> RunTriggered(EventTypeId id, const Detail::ValueBox& payload,
                                                         std::optional<EntityId> target, std::uint32_t planPos,
                                                         std::string_view schedule, std::uint32_t limit,
                                                         const std::string& limitSource, const std::string& where);
    // The outermost Run that Run and RunExtract share (DI-KS 35); source is the
    // extract source of this Run (nullptr for Run).
    Engine::Base::Result<void, KernelError> RunOutermost(std::string_view schedule, const World* source);
    // J-50 (DI-KS 24): reads KernelSettings, once per Run call.
    void ReadSettings();
    // Declaration 3.2 rule 5: attaches the World, then makes every System's
    // state again by registrationSeq when the Registry's version is not the
    // one attached: the access descriptions all made again; a kept System
    // keeps its mounts' lastRun and Local<T>; a removed one's state is dropped.
    void Attach();
    // Spec E.5: interrupts the World and drops every record. The outermost level
    // returns it; a nested one leaves the error instead (its first one).
    // origin: the error is produced at this level; false when it is a nested run's error passing through
    // (marked on the trace span on top of the thread's stack, trace declaration D4 (i)).
    Engine::Base::Result<void, KernelError> Fail(KernelError error, bool nested, bool origin = true);

    // One level of the run stack (World::runStack_): pushed for the Plan's
    // duration and popped on every way out; an exception through it interrupts
    // the World, drops the records and, nested, leaves NestedRunAborted.
    class LevelGuard;

    const Registry* registry_;
    World world_;
    // By registrationSeq; a removed System's entry is empty.
    std::vector<SystemState> systems_;
    // The Registry version systems_ matches (0: none yet).
    std::uint64_t version_ = 0;
    // The source World of the RunExtract under way (DI-KS 35), nullptr otherwise;
    // set and cleared by RunOutermost on every way out. Never kept past a call (DI-KS 37).
    const World* extractSource_ = nullptr;
};

} // namespace GYO::Kernel
