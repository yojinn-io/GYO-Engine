// The Serial Executor (spec E.2, E.5; WorldExecutor.hpp). It logs nothing: an Err is
// returned once by the outermost Run or Trigger and its handler logs it
// (target/error-handling DI 1, 2; error levels D1).

#include "engine/kernel/WorldExecutor.hpp"

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Kernel {

namespace {

// Spec F-12's message: the observers that recorded during the last round
// (time-events declaration 4.2), after its hooks; sorted, each once; "" when none.
std::string ObserverRecorderNames(std::vector<std::size_t> observers, const Detail::RegistryData& data, bool afterHooks) {
    std::sort(observers.begin(), observers.end());
    observers.erase(std::unique(observers.begin(), observers.end()), observers.end());
    std::string names;
    for (std::size_t observer : observers) {
        names += (names.empty() ? (afterHooks ? "; observer '" : "observer '") : ", '") + data.observers[observer].name + "'";
    }
    return names;
}

} // namespace

WorldExecutor::WorldExecutor(const Registry& registry, std::string worldName, WorldOptions options)
    : registry_(&registry), world_(registry, std::move(worldName), options) {
    Attach();
    world_.executor_ = this;
}

WorldExecutor::~WorldExecutor() = default;

void WorldExecutor::Attach() {
    world_.Sync();
    const Detail::RegistryData& data = *world_.data_;
    if (version_ == data.version) return;
    std::vector<SystemState> rebuilt;
    rebuilt.reserve(data.systems.size());
    for (const Detail::SystemRecord& system : data.systems) {
        SystemState state;
        if (!system.removed) {
            state.access = Detail::MakeAccessDesc(system.access);
            // registrationSeq is the index: the same System as before, with the same mounts.
            if (system.seq < systems_.size() && systems_[system.seq].mounts.size() == system.options.mounts.size()) {
                state.mounts = std::move(systems_[system.seq].mounts);
            } else {
                state.mounts.resize(system.options.mounts.size());
            }
        }
        rebuilt.push_back(std::move(state));
    }
    systems_ = std::move(rebuilt);
    version_ = data.version;
}

VerificationReport WorldExecutor::Verification() const {
    VerificationReport report;
    report.world = world_.name_;
    report.accessVerification = world_.verification_;
    report.breakOnKernelError = world_.breakOnKernelError_;
    const Detail::RegistryData& data = *world_.data_;
    for (std::size_t c = 0; c < world_.excluded_.size(); ++c) {
        if (world_.excluded_[c]) report.excludedColumns.push_back(data.components[c].name);
    }
    return report;
}

Engine::Base::Result<void, KernelError> WorldExecutor::Fail(KernelError error, bool nested) {
    error.world = world_.name_;
    world_.IdentifyError(error);
    world_.state_ = WorldState::Interrupted;
    world_.DiscardDeferred();
    if (nested) {
        // K0 (spec E.5): reported once, by the outermost Run; the first error stays.
        if (!world_.leftError_) world_.leftError_ = error;
        return Engine::Base::Err(std::move(error));
    }
    if (!interruption_) interruption_ = error;
    return Engine::Base::Err(std::move(error));
}

namespace {

std::string StepText(const std::string& schedule, std::uint32_t position, const std::string& name) {
    return "step " + std::to_string(position) + " '" + name + "' of schedule '" + schedule + "'";
}

// Whether list holds error already: one error, wrapped by more levels, is the same
// error by identity (error levels D9); two errors of the same text are two.
bool Listed(const std::vector<KernelError>& list, const KernelError& error) {
    return std::any_of(list.begin(), list.end(),
                       [&](const KernelError& e) { return e.identity != 0 && e.identity == error.identity; });
}

// what: the nested run, as "schedule 'Name'" or "a schedule run by System 'Name'".
KernelError NestedAbortError(const World& world, const std::string& what) {
    KernelError error(KernelErrorCode::NestedRunAborted,
                      "an exception left the nested run of " + what +
                          " and an outer System caught it; the World is interrupted");
    error.rule = "X-16";
    error.world = world.Name();
    return error;
}

// where: the System and its Plan position, as "System 'Name' (schedule 'S', step N)".
KernelError ImmediateAbortError(const World& world, const std::string& where) {
    KernelError error(KernelErrorCode::ImmediateChangeAborted,
                      "an exception left an immediate change by " + where +
                          " and the System caught it; the World is interrupted");
    error.rule = "DI-KS 18";
    error.world = world.Name();
    return error;
}

// Declaration 3.2 rule 4: the Registry counts its Worlds inside a Run, raised
// when Run starts and lowered on every way out (an exception included), so
// Register, BuildPlans and DiscardRegistrations assert while anything a Run
// calls is on the stack. A nested RunSchedule does not come through Run and
// does not count again.
class RunningScope {
public:
    explicit RunningScope(std::uint32_t& running) noexcept : running_(running) { ++running_; }
    ~RunningScope() { --running_; }
    RunningScope(const RunningScope&) = delete;
    RunningScope& operator=(const RunningScope&) = delete;

private:
    std::uint32_t& running_;
};

} // namespace

// Spec E.5 and K0: one level of the run stack. An exception from a System or
// hook is not caught; on the way out the World becomes Interrupted and the
// records not yet applied are dropped; a nested level also leaves
// NestedRunAborted for the outermost Run (the outer System may catch the
// exception and return).
class WorldExecutor::LevelGuard {
public:
    LevelGuard(World& world, std::string_view schedule, bool nested)
        : world_(world), schedule_(schedule), nested_(nested), exceptions_(std::uncaught_exceptions()) {
        world_.runStack_.push_back(World::RunLevel{schedule, world_.window_.size(), world_.MembershipEpoch()});
    }
    ~LevelGuard() {
        world_.runStack_.pop_back();
        if (std::uncaught_exceptions() > exceptions_) {
            world_.state_ = WorldState::Interrupted;
            world_.DiscardDeferred();
            if (nested_ && !world_.leftError_) {
                world_.leftError_ = NestedAbortError(world_, "schedule '" + std::string(schedule_) + "'");
                world_.IdentifyError(*world_.leftError_);
            }
        }
    }
    LevelGuard(const LevelGuard&) = delete;
    LevelGuard& operator=(const LevelGuard&) = delete;

private:
    World& world_;
    std::string_view schedule_;
    bool nested_;
    int exceptions_;
};

Engine::Base::Result<void, KernelError> WorldExecutor::Run(std::string_view schedule) {
    World& world = world_;
    // E.5: an Interrupted World runs no more (X-9). A System reaching this
    // through a kept pointer finds the World Running: also a GYO_ASSERT (only
    // ExclusiveWorld::RunSchedule runs a schedule from inside a System, K0).
    GYO_ASSERT(world.state_ == WorldState::Ready);
    RunningScope running(world.data_->runningWorlds);
    Attach();
    const Detail::ScheduleRecord* record = world.data_->FindSchedule(std::string(schedule));
    GYO_ASSERT(record != nullptr);
    ReadSettings();
    // A new outermost Run: its own DroppedMessages records (event cursor declaration 3.2).
    for (World::DroppedEntry& dropped : world.droppedEntry_) dropped = World::DroppedEntry{};
    return RunPlan(record->options.name, false);
}

void WorldExecutor::ReadSettings() {
    World& world = world_;
    // KernelSettings is read once, when each Run call starts (J-50, DI-KS 24):
    // a change made by a System counts from the next Run. A nested RunSchedule
    // does not come here.
    const auto& settings =
        static_cast<const Detail::ResourceSlot<KernelSettings>&>(*world.resources_[world.data_->kernelSettings]);
    const bool verification = settings.value && settings.value->accessVerification;
    // Snapshots taken before the switch went off are stale (nothing was compared
    // since), and none were kept while it was off: a flip drops them all.
    if (verification != world.verification_) world.DropSnapshots();
    world.verification_ = verification;
    world.breakOnKernelError_ = settings.value && settings.value->breakOnKernelError;
    world.hookRoundLimit_ = settings.value ? settings.value->hookRoundLimit : KernelSettings{}.hookRoundLimit;
    // Spec F-12: a limit of 0 is a Programmer Error (the Host's or a System's write).
    GYO_ASSERT(world.hookRoundLimit_ >= 1);
    world.hasRun_ = true;
}

Engine::Base::Result<void, KernelError> WorldExecutor::RunPlan(std::string_view schedule, bool nested) {
    World& world = world_;
    const Detail::RegistryData& data = *world_.data_;
    const std::string scheduleName(schedule);
    const Plan* plan = registry_->Frozen().PlanOf(scheduleName);
    const Detail::ScheduleRecord* record = data.FindSchedule(scheduleName);
    GYO_ASSERT(plan != nullptr && record != nullptr);
    // No tick is advanced here: the kernel keeps no clock (the tick is the time
    // Module's, which runs a fixed step schedule nested; v8-arch-gyo2-clock-out-of-kernel).

    // The Ready window (spec A.6 layer 2, E.2 step 0): a change since the last
    // comparison that did not go through the World API is the Host's or a
    // reference kept outside a System; checked before any step, so a sync point
    // at the start of the Plan cannot hide it. A nested Run has no Ready window:
    // the entry compared the outer System's changes and took fresh snapshots.
    if (!nested && world.verification_) {
        const std::vector<std::string> changed = world.CompareBefore();
        if (!changed.empty()) {
            std::string list;
            for (const std::string& name : changed) list += (list.empty() ? "" : ", ") + name;
            KernelError error(KernelErrorCode::PreCallChange,
                              "changed outside the World API before Run of schedule '" + scheduleName + "': " + list);
            error.rule = "A.6";
            error.world = world.name_;
            error.Wrap("Ready window before Run of schedule '" + scheduleName + "'");
            if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
            return Fail(std::move(error), nested);
        }
    }

    // Spec F-12: a Run leaves its schedule's hook queue empty (Host writes run
    // no hook; an Err or a throw discards every queue), so the next one starts empty.
    GYO_ASSERT(world.HookQueueOf(scheduleName).records.empty());
    // Spec F-12: the hook rounds follow the last sync point of the Plan.
    std::uint32_t lastSync = static_cast<std::uint32_t>(plan->steps.size());
    for (std::uint32_t position = 0; position < plan->steps.size(); ++position) {
        if (plan->steps[position].kind == StepKind::SyncPoint) lastSync = position;
    }

    world.state_ = WorldState::Running;
    LevelGuard guard(world, record->options.name, nested);
    // This level's entry of the run stack (indexed: a nested Run grows the stack).
    const std::size_t level = world.runStack_.size() - 1;
    std::string previousStep;

    // W-1 with verification on, then fresh snapshots (after a sync point and after the hook rounds).
    auto checkStructure = [&](const std::string& text) -> std::optional<KernelError> {
        if (!world.verification_) return std::nullopt;
        if (std::optional<std::string> problem = world.StructureProblem()) {
            KernelError error(KernelErrorCode::StructureBroken, *problem);
            error.rule = "W-1";
            error.world = world.name_;
            error.Wrap("Executor: " + text);
            if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
            return error;
        }
        world.TakeSnapshots();
        return std::nullopt;
    };

    for (std::uint32_t position = 0; position < plan->steps.size(); ++position) {
        const Step& step = plan->steps[position];
        switch (step.kind) {
        case StepKind::System: {
            const Detail::SystemRecord& system = data.systems[step.system];
            SystemState& state = systems_[step.system];
            std::size_t mountIndex = 0;
            while (mountIndex < system.options.mounts.size() && system.options.mounts[mountIndex].schedule != scheduleName) {
                ++mountIndex;
            }
            GYO_ASSERT(mountIndex < system.options.mounts.size());
            const Mount& mount = system.options.mounts[mountIndex];
            MountState& mountState = state.mounts[mountIndex];

            // (0) The comparison before the call: a change here is not this System's.
            if (world.verification_) {
                const std::vector<std::string> changed = world.CompareBefore();
                if (!changed.empty()) {
                    std::string list;
                    for (const std::string& name : changed) list += (list.empty() ? "" : ", ") + name;
                    KernelError error(KernelErrorCode::PreCallChange,
                                      "changed outside the World API before " + StepText(scheduleName, position, system.options.name) +
                                          ": " + list + "; " + system.options.name + " was not called");
                    error.rule = "A.6";
                    error.world = world.name_;
                    error.Wrap(!previousStep.empty() ? "after " + previousStep
                               : nested          ? std::string("entry of the nested run of schedule '") + scheduleName + "'"
                                                 : std::string("Ready window before Run of schedule '") + scheduleName + "'");
                    if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                    return Fail(std::move(error), nested);
                }
            }

            std::uint32_t recSeq = 0;
            Detail::SystemContext call;
            call.world = &world;
            call.executor = this;
            call.access = &state.access;
            call.name = system.options.name;
            call.schedule = scheduleName;
            call.planPos = position;
            call.zone = system.options.zone;
            call.lastRun = mountState.lastRun;
            call.callSerial = ++world.callSerial_;
            call.syncEpoch = world.syncEpoch_;
            call.queue = &world.window_;
            call.recSeq = &recSeq;
            call.locals = &mountState.locals;
            // Event cursor declaration 3.2: each condition is a reader of its own
            // (slot 1, 2, ... in evaluation order), the System is slot 0.
            call.reader = Detail::ReaderKey{system.seq, static_cast<std::uint32_t>(mountIndex), 0, true};

            Engine::Base::Result<void, KernelError> result = [&]() -> Engine::Base::Result<void, KernelError> {
                // (1) tlCurrent: the conditions' reads are the System's (spec B-5).
                Detail::CallScope scope(call);
                // (2) Run conditions, in order, short-circuit: the mount's, then each set's (E.4).
                bool runs = true;
                std::uint32_t slot = 0;
                auto evaluate = [&](const ConditionRef& condition) {
                    ++slot;
                    if (!runs) return;
                    call.reader.slot = slot;
                    if (!condition.Evaluator()(call)) runs = false;
                };
                for (const ConditionRef& condition : mount.conditions) evaluate(condition);
                for (const std::string& setName : mount.sets) {
                    for (const Detail::SetRecord& set : data.sets) {
                        if (set.options.name != setName || set.options.schedule != scheduleName) continue;
                        for (const ConditionRef& condition : set.conditions) evaluate(condition);
                    }
                }
                call.reader.slot = 0;
                // (3) The step's stamp, also when skipped (X-5).
                call.stamp = ++world.stamp_;
                // (4)
                if (!runs) return {};
                mountState.lastRun = call.stamp;
                return system.run(call);
            }();
            // W-3: no System call changes membership; a nested run it made set a
            // new baseline when it returned (K0), so a change of its own before
            // or after that run is still caught.
            GYO_ASSERT(world.MembershipEpoch() == world.runStack_[level].structure);
            // (5) The comparison after the call, also when (4) was skipped.
            std::vector<std::string> changed;
            if (world.verification_) changed = world.CompareAfter(state.access);
            const bool interrupted = world.state_ == WorldState::Interrupted;
            if (interrupted && world.leftError_ && !Listed(call.returned, *world.leftError_)) {
                // K0: the error a nested run of this call left (failed, refused, or
                // left by an exception the System caught) is this call's error,
                // whatever the System returned; listed once.
                call.returned.push_back(*world.leftError_);
            }
            std::optional<KernelError> error = Detail::CombineCall(call, std::move(result), changed);
            // An Interrupted World stops here; a nested level always leaves its error, this is a guard.
            if (interrupted && !error) error = NestedAbortError(world, "a schedule run by System '" + system.options.name + "'");
            if (error) {
                error->Wrap("Executor: " + StepText(scheduleName, position, system.options.name));
                return Fail(std::move(*error), nested);
            }
            previousStep = StepText(scheduleName, position, system.options.name);
            break;
        }
        case StepKind::SyncPoint: {
            const ChangeStamp stamp = ++world.stamp_;
            world.state_ = WorldState::Applying;
            Engine::Base::Result<void, KernelError> applied = world.ApplySync(position, scheduleName, stamp);
            world.state_ = WorldState::Running;
            if (!applied) {
                KernelError error = std::move(applied).error();
                error.Wrap("Executor: " + StepText(scheduleName, position, step.name));
                return Fail(std::move(error), nested);
            }
            if (std::optional<KernelError> error = checkStructure(StepText(scheduleName, position, step.name))) {
                return Fail(std::move(*error), nested);
            }
            if (position == lastSync) {
                // Spec F-12: the hook rounds. Each applies the whole hook queue like a sync
                // point's step 0 (its own stamp, syncEpoch + 1); hooks it triggers record
                // into the queue for the next round. Not a Plan step.
                const std::uint32_t limit = record->options.hookRoundLimit.value_or(world.hookRoundLimit_);
                std::uint32_t round = 0;
                auto roundText = [&](std::uint32_t r) {
                    return "Executor: hook round " + std::to_string(r) + " at sync point step " + std::to_string(position) +
                           " of schedule '" + scheduleName + "'";
                };
                while (!world.HookQueueOf(scheduleName).records.empty()) {
                    if (round == limit) {
                        std::vector<std::size_t> recorders = world.roundRecorders_;
                        std::sort(recorders.begin(), recorders.end());
                        recorders.erase(std::unique(recorders.begin(), recorders.end()), recorders.end());
                        std::string names;
                        for (std::size_t hook : recorders) names += (names.empty() ? "hook '" : ", '") + data.hooks[hook].name + "'";
                        names += ObserverRecorderNames(world.roundObserverRecorders_, data, !names.empty());
                        KernelError error(KernelErrorCode::HookRoundLimit,
                                          "the hook rounds of schedule '" + scheduleName + "' did not empty its hook queue within " +
                                              std::to_string(limit) + " rounds (the limit of the " +
                                              (record->options.hookRoundLimit ? "schedule's declaration" : "World's KernelSettings") +
                                              "): " + std::to_string(world.HookQueueOf(scheduleName).records.size()) +
                                              " records remain, recorded in round " + std::to_string(round) + " by " + names);
                        error.rule = "F-12";
                        error.world = world.name_;
                        error.Wrap(roundText(round));
                        if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                        return Fail(std::move(error), nested);
                    }
                    ++round;
                    // This level's records are all applied (the outer levels' wait: K0).
                    GYO_ASSERT(world.window_.size() == world.runStack_[level].windowBase);
                    world.roundRecorders_.clear();
                    world.roundObserverRecorders_.clear();
                    world.trackRoundRecorders_ = true;
                    const ChangeStamp roundStamp = ++world.stamp_;
                    world.state_ = WorldState::Applying;
                    Engine::Base::Result<void, KernelError> applied = world.ApplySync(position, scheduleName, roundStamp);
                    world.state_ = WorldState::Running;
                    world.trackRoundRecorders_ = false;
                    if (!applied) {
                        KernelError error = std::move(applied).error();
                        error.Wrap(roundText(round));
                        return Fail(std::move(error), nested);
                    }
                }
                if (round > 0) {
                    if (std::optional<KernelError> error = checkStructure(roundText(round))) {
                        return Fail(std::move(*error), nested);
                    }
                }
            }
            world.runStack_[level].structure = world.MembershipEpoch();
            previousStep = StepText(scheduleName, position, step.name);
            break;
        }
        }
    }
    // Spec F-12: the hook rounds left nothing behind (and no System record of this level is pending).
    GYO_ASSERT(world.window_.size() == world.runStack_[level].windowBase && world.HookQueueOf(scheduleName).records.empty());
    // A nested Run returns to its outer System: the World is still running (K0).
    if (!nested) {
        // Event cursor declaration 3.2: the trim of what every reader has read,
        // when the outermost Run ends after its hook rounds; not a Plan step, not
        // after a nested run (its outer System's Run trims), not after an Err
        // (Fail returned above).
        world.TrimMessages();
        world.state_ = WorldState::Ready;
    }
    return {};
}

Engine::Base::Result<void, KernelError> WorldExecutor::RunNestedFrom(Detail::SystemContext& call, std::string_view schedule,
                                                               bool inClosure) {
    World& world = world_;
    const Detail::RegistryData& data = *world.data_;
    // Only ExclusiveWorld<...> reaches here: a System or a zone B hook holding it.
    GYO_ASSERT(call.access != nullptr && call.access->wholeWorld);
    const std::string who = std::string(call.hook ? "hook '" : "System '") + std::string(call.name) + "'";
    const std::string target = "schedule '" + std::string(schedule) + "'";
    // A refused call: an Err of this call (listed even when the System drops
    // it), the World interrupted, the error left for the outermost Run.
    auto refuse = [&](KernelErrorCode code, const char* rule, std::string message) {
        return Fail(Detail::CallError(call, code, rule, std::move(message)), true);
    };
    if (call.hook) {
        return refuse(KernelErrorCode::HookRunsSchedule, "X-13",
                      who + " runs " + target + ": a hook runs while a sync point applies its records, so it runs no schedule");
    }
    if (inClosure) {
        return refuse(KernelErrorCode::ClosureRunsSchedule, "X-14",
                      who + " runs " + target + " inside a Read or Modify closure: the value the closure holds would "
                                                "not survive the schedule's sync points");
    }
    const Detail::ScheduleRecord* record = data.FindSchedule(std::string(schedule));
    if (record == nullptr) {
        return refuse(KernelErrorCode::UnknownSchedule, "X-12", who + " runs " + target + ", which is not registered");
    }
    if (record->options.extract) {
        return refuse(KernelErrorCode::ExtractNotNestable, "X-15",
                      who + " runs extract " + target + ": an extract schedule runs with rollback-extract only");
    }
    for (const World::RunLevel& running : world.runStack_) {
        if (running.schedule == record->options.name) {
            return refuse(KernelErrorCode::ScheduleRunning, "X-11",
                          who + " runs " + target + ", which is running already: one schedule runs once at a time");
        }
    }
    if (world.state_ == WorldState::Interrupted) {
        // Spec E.5 (K0): an Interrupted World runs nothing; the error a nested
        // level left is returned (the Executor lists it once, at the boundary).
        return Engine::Base::Err(world.leftError_ ? *world.leftError_
                                                  : NestedAbortError(world, "schedule '" + std::string(schedule) + "'"));
    }
    // W-3: up to here the System changed no membership behind the World API; a
    // change found now would otherwise be hidden by the nested run's baseline.
    GYO_ASSERT(world.MembershipEpoch() == world.runStack_.back().structure);
    // Spec A.6 layer 2: the System's own changes so far are compared as at its
    // boundary, on its own account; then fresh snapshots, so the inner
    // schedule's first comparison does not take them for the inner step's.
    if (world.verification_) {
        const std::vector<std::string> changed = world.CompareAfter(*call.access);
        if (!changed.empty()) {
            std::vector<KernelError> list;
            for (const std::string& storage : changed) {
                KernelError error(KernelErrorCode::BoundaryChange,
                                  "'" + storage + "' changed during " + who + " (schedule '" + std::string(call.schedule) +
                                      "', step " + std::to_string(call.planPos) +
                                      ") before it ran " + target + ", which it does not declare writing; " + target +
                                      " was not run");
                error.rule = "A.6";
                error.world = world.name_;
                if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                list.push_back(std::move(error));
            }
            KernelError error = list.front();
            if (list.size() > 1) {
                error = KernelError(KernelErrorCode::Siblings, std::to_string(list.size()) + " errors before " + who +
                                                                   " ran " + target);
                error.world = world.name_;
                error.siblings = std::move(list);
            }
            return Fail(std::move(error), true);
        }
    }
    // On every way back (an exception the System may catch included): the outer
    // step's new membership baseline and fresh snapshots, since what the inner
    // schedule changed is not the outer System's.
    class Rebase {
    public:
        explicit Rebase(World& world) noexcept : world_(world) {}
        ~Rebase() {
            world_.runStack_.back().structure = world_.MembershipEpoch();
            if (world_.verification_) world_.TakeSnapshots();
        }
        Rebase(const Rebase&) = delete;
        Rebase& operator=(const Rebase&) = delete;

    private:
        World& world_;
    };
    Engine::Base::Result<void, KernelError> ran = [&] {
        Rebase rebase(world);
        return RunPlan(record->options.name, true);
    }();
    // A failed run left its error (Fail): the Executor lists it as this call's
    // error when the System returns, whatever the System does with it.
    return ran;
}

// S5 (declaration v8-arch-gyo2-kernel-subtract-structure 3.5; DI-KS 9, 18, 19,
// 21): the checks in the order of RunNestedFrom, then the change applied as a
// sync point applies its record, then the hook rounds of what its hooks recorded.
Engine::Base::Result<Detail::ImmediateOutcome, KernelError> WorldExecutor::ImmediateFrom(Detail::SystemContext& call,
                                                                                         Detail::Record record,
                                                                                         ChangeStamp stamp, bool closureOpen) {
    using Outcome = Engine::Base::Result<Detail::ImmediateOutcome, KernelError>;
    World& world = world_;
    const Detail::RegistryData& data = *world.data_;
    // Only ExclusiveWorld<...> reaches here: a System or a zone B hook holding it.
    GYO_ASSERT(call.access != nullptr && call.access->wholeWorld);
    const std::string who = std::string(call.hook ? "hook '" : "System '") + std::string(call.name) + "'";
    const std::string where = who + " (schedule '" + std::string(call.schedule) + "', step " + std::to_string(call.planPos) + ")";
    // A refused or failed change: the World interrupted, the error left for the
    // outermost Run (the Executor lists it as this call's error, whatever the
    // System does with it).
    auto stop = [&](KernelError error) -> Outcome {
        Engine::Base::Result<void, KernelError> failed = Fail(std::move(error), true);
        return Engine::Base::Err(std::move(failed).error());
    };
    if (call.hook) {
        return stop(Detail::CallError(call, KernelErrorCode::HookChangesMembership, "DI-KS 18",
                                      who + " changes membership now through ExclusiveWorld: a hook runs while records "
                                            "are applied, so it records its changes instead"));
    }
    if (closureOpen) {
        return stop(Detail::CallError(call, KernelErrorCode::ClosureChangesMembership, "DI-KS 21",
                                      who + " changes membership now inside a Read or Modify closure: the value the "
                                            "closure holds would not survive the change"));
    }
    if (world.state_ == WorldState::Interrupted) {
        // Spec E.5: an Interrupted World changes no more; inside a call it was
        // interrupted by a refusal or a nested level, which left its error.
        GYO_ASSERT(world.leftError_.has_value());
        return Engine::Base::Err(*world.leftError_);
    }
    // W-3 (declaration section 12 item 1, (a)): up to here nothing changed
    // membership behind the handle; such a change would be hidden by the new
    // baseline this change sets, so it is checked first.
    const std::uint64_t baseline = world.runStack_.back().structure;
    GYO_ASSERT(world.MembershipEpoch() == baseline);
    // Spec A.6 layer 2, as before a nested run: the System's own changes so far
    // are compared on its own account first, so the snapshots taken after the
    // change do not hide one it does not declare (declaration 3.5 point 2).
    if (world.verification_) {
        const std::vector<std::string> undeclared = world.CompareAfter(*call.access);
        if (!undeclared.empty()) {
            std::vector<KernelError> list;
            for (const std::string& storage : undeclared) {
                KernelError error(KernelErrorCode::BoundaryChange,
                                  "'" + storage + "' changed during " + where +
                                      " before an immediate change, which it does not declare writing; the change was "
                                      "not made");
                error.rule = "A.6";
                error.world = world.name_;
                if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                list.push_back(std::move(error));
            }
            KernelError error = list.front();
            if (list.size() > 1) {
                error = KernelError(KernelErrorCode::Siblings,
                                    std::to_string(list.size()) + " errors before an immediate change by " + who);
                error.world = world.name_;
                error.siblings = std::move(list);
            }
            return stop(std::move(error));
        }
    }

    Detail::ImmediateOutcome outcome;
    if (record.kind == Detail::RecordKind::Spawn && !record.withId) {
        // Like Spawn<Bundle<C...>>::New (W-9); D-E3 does not see at startup that an exclusive System spawns.
        if (Detail::AllotterSlot(world) == nullptr) {
            return Engine::Base::Err(Detail::CallError(call, KernelErrorCode::NoAllotter, "A.1",
                                                       who + " spawns through ExclusiveWorld, and the World declares "
                                                             "no allotter (Registrar::Allotter)"));
        }
        const std::optional<EntityId> id = Detail::Allot(world);
        if (!id) {
            return Engine::Base::Err(Detail::CallError(call, KernelErrorCode::AllotterExhausted, "W-9",
                                                       "the entity allotter's range is used up"));
        }
        record.entity = *id;
        outcome.entity = *id;
    }
    const bool alive = std::binary_search(world.entities_.begin(), world.entities_.end(), record.entity);
    switch (record.kind) {
    case Detail::RecordKind::Insert:
    case Detail::RecordKind::Despawn: outcome.changed = alive; break;
    case Detail::RecordKind::Remove: outcome.changed = world.columns_[record.id]->Contains(record.entity); break;
    case Detail::RecordKind::ResourceRemove: outcome.changed = world.resources_[record.id]->Present(); break;
    default: break;
    }
    record.planPos = call.planPos;
    record.recSeq = 0;
    record.emitterZone = call.zone;
    record.emitter = call.name;
    record.emitterHook = false;
    record.emitterKey = call.access;

    // From here, on every way back: the change's own queue is let go, the
    // World is Running again, and the step's new membership baseline and fresh
    // snapshots are taken, since this change and its hooks' are the System's
    // declared work (declaration 3.5 point 2). An exception that leaves the
    // change (a hook or an InitialValue function threw) interrupts the World at
    // once, like a level of the run stack (spec E.5): the records of every
    // level not yet applied are dropped, what was applied stays, and the error
    // is left for the outermost Run in case the System catches the exception
    // (DI-KS 18; like X-16). The Executor catches nothing. trackRoundRecorders_
    // is reset here for that path only (every other way back reset it).
    class Settle {
    public:
        Settle(World& world, const std::string& where) noexcept
            : subject_(world), where_(where), exceptions_(std::uncaught_exceptions()) {}
        ~Settle() {
            subject_.hookTarget_ = nullptr;
            subject_.trackRoundRecorders_ = false;
            if (std::uncaught_exceptions() > exceptions_) {
                subject_.state_ = WorldState::Interrupted;
                subject_.DiscardDeferred();
                if (!subject_.leftError_) {
                    subject_.leftError_ = ImmediateAbortError(subject_, where_);
                    subject_.IdentifyError(*subject_.leftError_);
                }
            } else if (subject_.state_ == WorldState::Applying) {
                subject_.state_ = WorldState::Running;
            }
            subject_.runStack_.back().structure = subject_.MembershipEpoch();
            if (subject_.verification_) subject_.TakeSnapshots();
        }
        Settle(const Settle&) = delete;
        Settle& operator=(const Settle&) = delete;

    private:
        World& subject_;
        const std::string& where_;
        int exceptions_;
    };
    // What the hooks this change runs record: applied below, before the change
    // returns; never the schedule's hook queue nor this level's window (DI-KS 19).
    World::HookQueue own{std::string(call.schedule), {}, 0};
    Settle settle(world, where);
    world.hookTarget_ = &own;
    // Spec F-5: while records are applied only hooks record (the System is waiting).
    world.state_ = WorldState::Applying;
    auto failed = [&](Engine::Base::Result<void, KernelError> applied, const std::string& text) -> Outcome {
        KernelError error = std::move(applied).error();
        error.Wrap(text);
        return stop(std::move(error));
    };

    // The change itself, with the handle's stamp: one phase of the sync point's steps 2 to 7.
    const std::uint64_t rejectedBefore = world.rejectedSpawns_;
    std::vector<Detail::Record> change;
    change.push_back(std::move(record));
    Engine::Base::Result<void, KernelError> applied = world.ApplySync(call.planPos, call.schedule, stamp, &change);
    if (!applied) return failed(std::move(applied), "Executor: immediate change by " + where);
    if (world.rejectedSpawns_ > rejectedBefore) {
        // Spec F-13: recorded in one place (World::RecordRejectedSpawn); read back from there.
        const auto* buffer = static_cast<const Detail::DiagnosticsBuffer<RejectedSpawn>*>(
            Detail::FindDiagnostics(world, KeyOf<RejectedSpawn>()));
        GYO_ASSERT(buffer != nullptr && !buffer->entries.empty());
        outcome.rejected = buffer->entries.back().record.reason;
    }

    // Spec F-12 for this change only (declaration section 12 item 2, (a)): round
    // after round, each a whole application with its own stamp and sync epoch
    // (F-6), until its hooks record nothing; the schedule's limit, or the World's.
    const Detail::ScheduleRecord* schedule = data.FindSchedule(std::string(call.schedule));
    GYO_ASSERT(schedule != nullptr);
    const std::uint32_t limit = schedule->options.hookRoundLimit.value_or(world.hookRoundLimit_);
    std::uint32_t round = 0;
    auto roundText = [&](std::uint32_t r) {
        return "Executor: hook round " + std::to_string(r) + " of an immediate change by " + where;
    };
    while (!own.records.empty()) {
        if (round == limit) {
            std::vector<std::size_t> recorders = world.roundRecorders_;
            std::sort(recorders.begin(), recorders.end());
            recorders.erase(std::unique(recorders.begin(), recorders.end()), recorders.end());
            std::string names;
            for (std::size_t hook : recorders) names += (names.empty() ? "hook '" : ", '") + data.hooks[hook].name + "'";
            names += ObserverRecorderNames(world.roundObserverRecorders_, data, !names.empty());
            KernelError error(KernelErrorCode::HookRoundLimit,
                              "the hook rounds of an immediate change by " + who + " did not empty within " +
                                  std::to_string(limit) + " rounds (the limit of the " +
                                  (schedule->options.hookRoundLimit ? "schedule's declaration" : "World's KernelSettings") +
                                  " of schedule '" + std::string(call.schedule) + "'): " + std::to_string(own.records.size()) +
                                  " records left after round " + std::to_string(round) + ", recorded by " + names);
            error.rule = "F-12";
            error.world = world.name_;
            error.Wrap(roundText(round));
            if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
            return stop(std::move(error));
        }
        ++round;
        std::vector<Detail::Record> recorded = std::move(own.records);
        own.records.clear();
        own.recSeq = 0;
        world.roundRecorders_.clear();
        world.roundObserverRecorders_.clear();
        world.trackRoundRecorders_ = true;
        const ChangeStamp roundStamp = ++world.stamp_;
        applied = world.ApplySync(call.planPos, call.schedule, roundStamp, &recorded);
        world.trackRoundRecorders_ = false;
        ++world.syncEpoch_;
        if (!applied) return failed(std::move(applied), roundText(round));
    }
    return outcome;
}

// Event cursor declaration 3.3, time-events declaration 4.2: the observers of
// a trigger outside a sync point, then the rounds of what they recorded.
Engine::Base::Result<void, KernelError> WorldExecutor::RunTriggered(EventTypeId id, const Detail::ValueBox& payload,
                                                                    std::optional<EntityId> target, std::uint32_t planPos,
                                                                    std::string_view schedule, std::uint32_t limit,
                                                                    const std::string& limitSource, const std::string& where) {
    World& world = world_;
    const Detail::RegistryData& data = *world.data_;
    const std::string event = "event '" + data.events[id].name + "'";
    // What the observers and the hooks of the rounds record: applied below,
    // before the trigger returns; never a schedule's hook queue nor a window.
    World::HookQueue pending{std::string(schedule), {}, 0};
    class Target {
    public:
        Target(World& world, World::HookQueue& queue) noexcept : owner_(world) { owner_.hookTarget_ = &queue; }
        ~Target() {
            owner_.hookTarget_ = nullptr;
            owner_.trackRoundRecorders_ = false;
        }
        Target(const Target&) = delete;
        Target& operator=(const Target&) = delete;

    private:
        World& owner_;
    } targetScope(world, pending);
    // Spec F-5: while the observers run and the records are applied, only observers and hooks record.
    world.state_ = WorldState::Applying;
    Engine::Base::Result<void, KernelError> ran =
        world.RunObservers(ObserverTrigger::Event, id, payload.Payload(), target, planPos, schedule, 0, true);
    if (!ran) {
        KernelError error = std::move(ran).error();
        error.Wrap("Executor: trigger of " + event + " by " + where);
        return Engine::Base::Err(std::move(error));
    }
    std::uint32_t round = 0;
    auto roundText = [&](std::uint32_t r) {
        return "Executor: round " + std::to_string(r) + " of the trigger of " + event + " by " + where;
    };
    while (!pending.records.empty()) {
        if (round == limit) {
            std::string names;
            std::vector<std::size_t> hooks = world.roundRecorders_;
            std::sort(hooks.begin(), hooks.end());
            hooks.erase(std::unique(hooks.begin(), hooks.end()), hooks.end());
            for (std::size_t hook : hooks) names += (names.empty() ? "hook '" : ", '") + data.hooks[hook].name + "'";
            names += ObserverRecorderNames(world.roundObserverRecorders_, data, !names.empty());
            KernelError error(KernelErrorCode::HookRoundLimit,
                              "the rounds of the trigger of " + event + " by " + where + " did not empty within " +
                                  std::to_string(limit) + " rounds (the limit of the " + limitSource + "): " +
                                  std::to_string(pending.records.size()) + " records left after round " +
                                  std::to_string(round) + ", recorded by " + names);
            error.rule = "F-12";
            error.world = world.name_;
            error.Wrap(roundText(round));
            if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
            return Engine::Base::Err(std::move(error));
        }
        ++round;
        std::vector<Detail::Record> recorded = std::move(pending.records);
        pending.records.clear();
        pending.recSeq = 0;
        world.roundRecorders_.clear();
        world.roundObserverRecorders_.clear();
        world.trackRoundRecorders_ = true;
        const ChangeStamp roundStamp = ++world.stamp_;
        // A whole application of the round's records (spec F.2 steps 2 to 7 and the triggers).
        Engine::Base::Result<void, KernelError> applied = world.ApplySync(planPos, schedule, roundStamp, &recorded);
        ++world.syncEpoch_;
        world.trackRoundRecorders_ = false;
        if (!applied) {
            KernelError error = std::move(applied).error();
            error.Wrap(roundText(round));
            return Engine::Base::Err(std::move(error));
        }
    }
    return {};
}

// ExclusiveWorld::Trigger: the checks in the order of ImmediateFrom; the
// observers' records go the way of an immediate change's hooks (time-events
// declaration 4.2: hookTarget_ and a round of its own).
Engine::Base::Result<void, KernelError> WorldExecutor::TriggerFrom(Detail::SystemContext& call, EventTypeId id,
                                                                   std::unique_ptr<Detail::ValueBox> payload,
                                                                   std::optional<EntityId> target, bool closureOpen) {
    World& world = world_;
    const Detail::RegistryData& data = *world.data_;
    // Only ExclusiveWorld<...> reaches here: a System or a zone B hook holding it.
    GYO_ASSERT(call.access != nullptr && call.access->wholeWorld);
    const std::string who = std::string(call.hook ? "hook '" : "System '") + std::string(call.name) + "'";
    const std::string where = who + " (schedule '" + std::string(call.schedule) + "', step " + std::to_string(call.planPos) + ")";
    auto halt = [&](KernelError error) { return Fail(std::move(error), true); };
    if (call.hook) {
        return halt(Detail::CallError(call, KernelErrorCode::HookChangesMembership, "DI-KS 18",
                                      who + " triggers event '" + data.events[id].name +
                                          "' now through ExclusiveWorld: a hook runs while records are applied, so it "
                                          "records the trigger instead (Commands::Trigger)"));
    }
    if (closureOpen) {
        return halt(Detail::CallError(call, KernelErrorCode::ClosureChangesMembership, "DI-KS 21",
                                      who + " triggers event '" + data.events[id].name +
                                          "' now inside a Read or Modify closure: the value the closure holds would not "
                                          "survive what the observers record"));
    }
    if (world.state_ == WorldState::Interrupted) {
        GYO_ASSERT(world.leftError_.has_value());
        return Engine::Base::Err(*world.leftError_);
    }
    // W-3: nothing changed membership behind the handle before the trigger.
    GYO_ASSERT(world.runStack_.back().structure == world.MembershipEpoch());
    // Spec A.6 layer 2: the System's own changes so far are compared on its own
    // account first, so the observers' snapshots do not hide one it does not declare.
    if (world.verification_) {
        const std::vector<std::string> ownChanges = world.CompareAfter(*call.access);
        if (!ownChanges.empty()) {
            std::vector<KernelError> list;
            for (const std::string& storage : ownChanges) {
                KernelError error(KernelErrorCode::BoundaryChange,
                                  "'" + storage + "' changed during " + where +
                                      " before a trigger, which it does not declare writing; no observer ran");
                error.rule = "A.6";
                error.world = world.name_;
                if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                list.push_back(std::move(error));
            }
            KernelError error = list.front();
            if (list.size() > 1) {
                error = KernelError(KernelErrorCode::Siblings, std::to_string(list.size()) + " errors before a trigger by " + who);
                error.world = world.name_;
                error.siblings = std::move(list);
            }
            return halt(std::move(error));
        }
    }
    // On every way back: Running again, the step's new membership baseline and
    // fresh snapshots (the observers' work and its rounds are this call's); an
    // exception interrupts the World at once and leaves ImmediateChangeAborted
    // for the outermost Run (like an immediate change, DI-KS 18).
    class Settle {
    public:
        Settle(World& world, const std::string& where) noexcept
            : owner_(world), where_(where), uncaught_(std::uncaught_exceptions()) {}
        ~Settle() {
            if (std::uncaught_exceptions() > uncaught_) {
                owner_.state_ = WorldState::Interrupted;
                owner_.DiscardDeferred();
                if (!owner_.leftError_) {
                    owner_.leftError_ = ImmediateAbortError(owner_, where_ + " (a trigger)");
                    owner_.IdentifyError(*owner_.leftError_);
                }
            } else if (owner_.state_ == WorldState::Applying) {
                owner_.state_ = WorldState::Running;
            }
            owner_.runStack_.back().structure = owner_.MembershipEpoch();
            if (owner_.verification_) owner_.TakeSnapshots();
        }
        Settle(const Settle&) = delete;
        Settle& operator=(const Settle&) = delete;

    private:
        World& owner_;
        const std::string& where_;
        int uncaught_;
    } settle(world, where);
    const Detail::ScheduleRecord* running = data.FindSchedule(std::string(call.schedule));
    GYO_ASSERT(running != nullptr);
    const std::uint32_t limit = running->options.hookRoundLimit.value_or(world.hookRoundLimit_);
    const std::string limitSource = std::string(running->options.hookRoundLimit ? "schedule's declaration" : "World's KernelSettings") +
                                    " of schedule '" + std::string(call.schedule) + "'";
    Engine::Base::Result<void, KernelError> ran =
        RunTriggered(id, *payload, target, call.planPos, call.schedule, limit, limitSource, where);
    if (!ran) return halt(std::move(ran).error());
    return {};
}

// WorldExecutor::Trigger (event cursor declaration 3.3, "outside the World,
// Ready"): a temporary queue applied to empty before it returns (time-events
// declaration 4.2), with the World's KernelSettings::hookRoundLimit (Ready has
// no schedule).
Engine::Base::Result<void, KernelError> WorldExecutor::TriggerFromHost(EventTypeId id,
                                                                       std::unique_ptr<Detail::ValueBox> payload,
                                                                       std::optional<EntityId> target) {
    World& world = world_;
    GYO_ASSERT(world.State() == WorldState::Ready);
    RunningScope inRun(world.data_->runningWorlds);
    Attach();
    // KernelSettings is read when the trigger starts, as when a Run starts (DI-KS 24): the verification
    // and break switches (a flip drops the snapshots, like a Run's) and the hook round limit.
    const auto& settings =
        static_cast<const Detail::ResourceSlot<KernelSettings>&>(*world.resources_[world.data_->kernelSettings]);
    const KernelSettings now = settings.value.has_value() ? *settings.value : KernelSettings{};
    if (now.accessVerification != world.verification_) world.DropSnapshots();
    world.verification_ = now.accessVerification;
    world.breakOnKernelError_ = now.breakOnKernelError;
    const std::uint32_t limit = now.hookRoundLimit;
    // Spec F-12: a limit of 0 is a Programmer Error.
    GYO_ASSERT(limit >= 1);
    // Its own DroppedMessages records, like a Run (the Host takes diagnostics between the two).
    std::fill(world.droppedEntry_.begin(), world.droppedEntry_.end(), World::DroppedEntry{});
    // Spec A.6 layer 2: a change since the last comparison that did not go
    // through the World API is not the observers'; checked before any of them runs.
    if (world.verification_) {
        const std::vector<std::string> changed = world.CompareBefore();
        if (!changed.empty()) {
            std::string list;
            for (const std::string& name : changed) list += (list.empty() ? "" : ", ") + name;
            KernelError error(KernelErrorCode::PreCallChange,
                              "changed outside the World API before the Host's trigger of event '" +
                                  world.data_->events[id].name + "': " + list);
            error.rule = "A.6";
            error.world = world.name_;
            error.Wrap("Ready window before the Host's trigger");
            if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
            return Fail(std::move(error), false);
        }
    }
    // On every way back: Ready again, or, when an exception leaves, interrupted
    // with every record dropped (spec E.5).
    class Settle {
    public:
        explicit Settle(World& world) noexcept : owner_(world), uncaught_(std::uncaught_exceptions()) {}
        ~Settle() {
            if (std::uncaught_exceptions() > uncaught_) {
                owner_.state_ = WorldState::Interrupted;
                owner_.DiscardDeferred();
            } else if (owner_.state_ == WorldState::Applying) {
                owner_.state_ = WorldState::Ready;
            }
        }
        Settle(const Settle&) = delete;
        Settle& operator=(const Settle&) = delete;

    private:
        World& owner_;
        int uncaught_;
    } settle(world);
    Engine::Base::Result<void, KernelError> ran =
        RunTriggered(id, *payload, target, 0, {}, limit, "World's KernelSettings", "the Host");
    if (!ran) return Fail(std::move(ran).error(), false);
    // The observers' declared writes and the rounds' changes are not changes outside the World API.
    if (world.verification_) world.TakeSnapshots();
    return {};
}

namespace Detail {

Engine::Base::Result<void, KernelError> RunNested(SystemContext& call, std::string_view schedule, bool inClosure) {
    GYO_ASSERT(call.executor != nullptr);
    return call.executor->RunNestedFrom(call, schedule, inClosure);
}

Engine::Base::Result<ImmediateOutcome, KernelError> ApplyImmediate(SystemContext& call, Record record, ChangeStamp stamp,
                                                                   bool inClosure) {
    GYO_ASSERT(call.executor != nullptr);
    return call.executor->ImmediateFrom(call, std::move(record), stamp, inClosure);
}

Engine::Base::Result<void, KernelError> TriggerNow(SystemContext& call, EventTypeId id, std::unique_ptr<ValueBox> payload,
                                                  std::optional<EntityId> target, bool inClosure) {
    GYO_ASSERT(call.executor != nullptr);
    return call.executor->TriggerFrom(call, id, std::move(payload), target, inClosure);
}

} // namespace Detail

} // namespace GYO::Kernel
