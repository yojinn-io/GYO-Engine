// The Serial Executor (spec E.2, E.5; Product.hpp). The only kernel file that logs:
// its default handler writes a kernel error's whole chain to stderr once
// (target/error-handling DI 2; tests/common/kernel/kernel_guard.py K6 exempts
// files named Executor*).

#include "engine/kernel/Product.hpp"

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Kernel {

namespace {

void DefaultHandler(const KernelError& error) {
    const std::string text = error.Text();
    std::fprintf(stderr, "kernel error:\n%s", text.c_str());
    std::fflush(stderr);
}

} // namespace

Product::Product(const Registry& registry, std::string worldName)
    : registry_(&registry), world_(registry, std::move(worldName)), handler_(&DefaultHandler) {
    const Detail::RegistryData& data = *world_.data_;
    for (const Detail::SystemRecord& system : data.systems) {
        SystemState state;
        state.access = Detail::MakeAccessDesc(system.access);
        state.mounts.resize(system.options.mounts.size());
        systems_.push_back(std::move(state));
    }
    world_.executor_ = this;
    // Spec H-12: at most one (D-E21 at Finish).
    for (const Detail::ScheduleRecord& schedule : data.schedules) {
        if (schedule.options.startup) {
            startup_ = schedule.options.name;
            break;
        }
    }
}

Product::~Product() = default;

VerificationReport Product::Verification() const {
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

Engine::Base::Result<void, KernelError> Product::Fail(KernelError error, bool nested) {
    error.world = world_.name_;
    world_.state_ = WorldState::Interrupted;
    world_.DiscardDeferred();
    if (nested) {
        // K0 (spec E.5): reported once, by the outermost Run; the first error stays.
        if (!world_.leftError_) world_.leftError_ = error;
        return Engine::Base::Err(std::move(error));
    }
    if (handler_) handler_(error);
    return Engine::Base::Err(std::move(error));
}

namespace {

std::string StepText(const std::string& schedule, std::uint32_t position, const std::string& name) {
    return "step " + std::to_string(position) + " '" + name + "' of schedule '" + schedule + "'";
}

// Whether list holds error already (one error, wrapped by more levels, is the same error).
bool Listed(const std::vector<KernelError>& list, const KernelError& error) {
    return std::any_of(list.begin(), list.end(), [&](const KernelError& e) {
        return e.code == error.code && e.rule == error.rule && e.message == error.message && e.detail == error.detail;
    });
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

// Spec H-12: startupRunning_ is set for Startup's Run and cleared on every way out.
class StartupScope {
public:
    explicit StartupScope(bool& running) noexcept : running_(running) { running_ = true; }
    ~StartupScope() { running_ = false; }
    StartupScope(const StartupScope&) = delete;
    StartupScope& operator=(const StartupScope&) = delete;

private:
    bool& running_;
};

} // namespace

// Spec E.5 and K0: one level of the run stack. An exception from a System or
// hook is not caught; on the way out the World becomes Interrupted and the
// records not yet applied are dropped; a nested level also leaves
// NestedRunAborted for the outermost Run (the outer System may catch the
// exception and return).
class Product::LevelGuard {
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

Engine::Base::Result<void, KernelError> Product::Run(std::string_view schedule) {
    World& world = world_;
    // E.5: an Interrupted World runs no more (X-9). A System reaching this
    // through a kept pointer finds the World Running: also a GYO_ASSERT (only
    // ExclusiveWorld::RunSchedule runs a schedule from inside a System, K0).
    GYO_ASSERT(world.state_ == WorldState::Ready);
    const Detail::ScheduleRecord* record = world.data_->FindSchedule(std::string(schedule));
    GYO_ASSERT(record != nullptr);
    if (record->options.startup) {
        // Spec H-12: the main program may run Startup itself, once, before the kernel has.
        GYO_ASSERT(!world.startupRan_);
        return RunStartup(record->options.name);
    }
    // Spec H-12: Startup runs whole before the first Run of any other schedule
    // (before its tick advances), once.
    if (startup_ && !world.startupRan_) {
        if (Engine::Base::Result<void, KernelError> ran = RunStartup(*startup_); !ran) return ran;
    }
    return RunPlan(record->options.name, false);
}

Engine::Base::Result<void, KernelError> Product::RunStartup(std::string_view schedule) {
    world_.startupRan_ = true;
    StartupScope running(world_.startupRunning_);
    return RunPlan(schedule, false);
}

Engine::Base::Result<void, KernelError> Product::RunPlan(std::string_view schedule, bool nested) {
    World& world = world_;
    const Detail::RegistryData& data = *world_.data_;
    const std::string scheduleName(schedule);
    const Plan* plan = registry_->Frozen().PlanOf(scheduleName);
    const Detail::ScheduleRecord* record = data.FindSchedule(scheduleName);
    GYO_ASSERT(plan != nullptr && record != nullptr);
    const Domain domain = record->options.domain;

    // KernelSettings is read once, when the first Run starts (LIM-6, J-50).
    if (!world.hasRun_) {
        const auto& settings = static_cast<const Detail::ResourceSlot<KernelSettings>&>(*world.resources_[data.kernelSettings]);
        world.verification_ = settings.value && settings.value->accessVerification;
        world.breakOnKernelError_ = settings.value && settings.value->breakOnKernelError;
        world.hookRoundLimit_ = settings.value ? settings.value->hookRoundLimit : KernelSettings{}.hookRoundLimit;
        // Spec F-12: a limit of 0 is the Host's misuse (a precondition).
        GYO_ASSERT(world.hookRoundLimit_ >= 1);
        world.hasRun_ = true;
    }
    // The domain tick: advanced only by the domain's clock schedule's Run, first
    // (spec section 0, B5); a nested Run of it too (K0).
    if (record->options.clock) {
        const std::size_t d = static_cast<std::size_t>(domain);
        const std::uint64_t tick = ++world.ticks_[d];
        auto& slot = world.tickStamps_[d][tick % World::kTickStampSlots];
        slot = World::TickStampSlot{tick, world.stamp_ + 1, true};
    }

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
            call.domain = domain;
            call.tick = world.ticks_[static_cast<std::size_t>(domain)];
            call.lastRun = mountState.lastRun;
            call.callSerial = ++world.callSerial_;
            call.syncEpoch = world.syncEpoch_;
            call.queue = &world.window_;
            call.recSeq = &recSeq;
            call.locals = &mountState.locals;

            Engine::Base::Result<void, KernelError> result = [&]() -> Engine::Base::Result<void, KernelError> {
                // (1) tlCurrent: the conditions' reads are the System's (spec B-5).
                Detail::CallScope scope(call);
                // (2) Run conditions, in order, short-circuit: the mount's, then each set's (E.4).
                bool runs = true;
                for (const ConditionRef& condition : mount.conditions) {
                    if (runs && !condition.Evaluator()(call)) runs = false;
                }
                for (const std::string& setName : mount.sets) {
                    for (const Detail::SetRecord& set : data.sets) {
                        if (set.options.name != setName || set.options.schedule != scheduleName) continue;
                        for (const ConditionRef& condition : set.conditions) {
                            if (runs && !condition.Evaluator()(call)) runs = false;
                        }
                    }
                }
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
                        for (std::size_t hook : recorders) names += (names.empty() ? "'" : ", '") + data.hooks[hook].name + "'";
                        KernelError error(KernelErrorCode::HookRoundLimit,
                                          "the hook rounds of schedule '" + scheduleName + "' did not empty its hook queue within " +
                                              std::to_string(limit) + " rounds (the limit of the " +
                                              (record->options.hookRoundLimit ? "schedule's declaration" : "World's KernelSettings") +
                                              "): " + std::to_string(world.HookQueueOf(scheduleName).records.size()) +
                                              " records remain, recorded in round " + std::to_string(round) + " by hook " + names);
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
        case StepKind::EventTrim:
            world.TrimEvents(domain);
            if (world.verification_) world.TakeSnapshots();
            previousStep = StepText(scheduleName, position, step.name);
            break;
        }
    }
    // Spec F-12: the hook rounds left nothing behind (and no System record of this level is pending).
    GYO_ASSERT(world.window_.size() == world.runStack_[level].windowBase && world.HookQueueOf(scheduleName).records.empty());
    // A nested Run returns to its outer System: the World is still running (K0).
    if (!nested) world.state_ = WorldState::Ready;
    return {};
}

Engine::Base::Result<void, KernelError> Product::RunNestedFrom(Detail::SystemContext& call, std::string_view schedule,
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
    if (record->options.startup) {
        return refuse(KernelErrorCode::StartupNotNestable, "H-12",
                      who + " runs Startup " + target + ": the kernel runs Startup, once, before any other schedule");
    }
    if (record->options.extract) {
        return refuse(KernelErrorCode::ExtractNotNestable, "X-15",
                      who + " runs extract " + target + ": an extract schedule runs with rollback-extract only");
    }
    if (world.startupRunning_) {
        return refuse(KernelErrorCode::StartupRunning, "H-13",
                      who + " runs " + target + " while Startup schedule '" + std::string(world.runStack_.front().schedule) +
                          "' runs: Startup's Systems are not counted as writers, so Startup runs alone, start to end");
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

namespace Detail {

Engine::Base::Result<void, KernelError> RunNested(SystemContext& call, std::string_view schedule, bool inClosure) {
    GYO_ASSERT(call.executor != nullptr);
    return call.executor->RunNestedFrom(call, schedule, inClosure);
}

} // namespace Detail

} // namespace GYO::Kernel
