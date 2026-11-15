// The Serial Executor (spec E.2, E.5; Product.hpp). The only kernel file that logs:
// its default handler writes a kernel error's whole chain to stderr once
// (target/error-handling DI 2; tests/common/kernel/kernel_guard.py K6 exempts
// files named Executor*).

#include "engine/kernel/Product.hpp"

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cstdio>
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

Engine::Base::Result<void, KernelError> Product::Fail(KernelError error) {
    error.world = world_.name_;
    world_.state_ = WorldState::Interrupted;
    world_.DiscardDeferred();
    if (handler_) handler_(error);
    return Engine::Base::Err(std::move(error));
}

namespace {

// Spec E.5: an exception out of a System or hook is not caught; on the way
// out the World becomes Interrupted and the records not yet applied are dropped.
class RunGuard {
public:
    explicit RunGuard(World& world, WorldState& state, void (World::*discard)() noexcept)
        : world_(world), state_(state), discard_(discard) {}
    ~RunGuard() {
        if (armed_) {
            state_ = WorldState::Interrupted;
            (world_.*discard_)();
        }
    }
    RunGuard(const RunGuard&) = delete;
    RunGuard& operator=(const RunGuard&) = delete;
    void Disarm() noexcept { armed_ = false; }

private:
    World& world_;
    WorldState& state_;
    void (World::*discard_)() noexcept;
    bool armed_ = true;
};

std::string StepText(const std::string& schedule, std::uint32_t position, const std::string& name) {
    return "step " + std::to_string(position) + " '" + name + "' of schedule '" + schedule + "'";
}

} // namespace

Engine::Base::Result<void, KernelError> Product::Run(std::string_view schedule) {
    World& world = world_;
    // E.5: an Interrupted World runs no more; X-9.
    GYO_ASSERT(world.state_ == WorldState::Ready);
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
    // The domain tick: advanced only by the domain's clock schedule's Run, first (spec section 0, B5).
    if (record->options.clock) {
        const std::size_t d = static_cast<std::size_t>(domain);
        const std::uint64_t tick = ++world.ticks_[d];
        auto& slot = world.tickStamps_[d][tick % World::kTickStampSlots];
        slot = World::TickStampSlot{tick, world.stamp_ + 1, true};
    }

    // The Ready window (spec A.6 layer 2, E.2 step 0): a change since the last
    // comparison that did not go through the World API is the Host's or a
    // reference kept outside a System; checked before any step, so a sync point
    // at the start of the Plan cannot hide it.
    if (world.verification_) {
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
            return Fail(std::move(error));
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
    RunGuard guard(world, world.state_, &World::DiscardDeferred);
    std::uint64_t structure = world.MembershipEpoch();
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
                    error.Wrap(previousStep.empty() ? std::string("Ready window before Run of schedule '") + scheduleName + "'"
                                                    : "after " + previousStep);
                    if (world.breakOnKernelError_ && world.breakHook_) world.breakHook_(error);
                    guard.Disarm();
                    return Fail(std::move(error));
                }
            }

            std::uint32_t recSeq = 0;
            Detail::SystemContext call;
            call.world = &world;
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
            // W-3: no System call changes membership.
            GYO_ASSERT(world.MembershipEpoch() == structure);
            // (5) The comparison after the call, also when (4) was skipped.
            std::vector<std::string> changed;
            if (world.verification_) changed = world.CompareAfter(state.access);
            if (std::optional<KernelError> error = Detail::CombineCall(call, std::move(result), changed)) {
                error->Wrap("Executor: " + StepText(scheduleName, position, system.options.name));
                guard.Disarm();
                return Fail(std::move(*error));
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
                guard.Disarm();
                return Fail(std::move(error));
            }
            if (std::optional<KernelError> error = checkStructure(StepText(scheduleName, position, step.name))) {
                guard.Disarm();
                return Fail(std::move(*error));
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
                        guard.Disarm();
                        return Fail(std::move(error));
                    }
                    ++round;
                    GYO_ASSERT(world.window_.empty());
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
                        guard.Disarm();
                        return Fail(std::move(error));
                    }
                }
                if (round > 0) {
                    if (std::optional<KernelError> error = checkStructure(roundText(round))) {
                        guard.Disarm();
                        return Fail(std::move(*error));
                    }
                }
            }
            structure = world.MembershipEpoch();
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
    // Spec F-12: the hook rounds left nothing behind (and no System record is pending).
    GYO_ASSERT(world.window_.empty() && world.HookQueueOf(scheduleName).records.empty());
    world.state_ = WorldState::Ready;
    guard.Disarm();
    return {};
}

} // namespace GYO::Kernel
