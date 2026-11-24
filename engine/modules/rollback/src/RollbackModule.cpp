#include "engine/rollback/RollbackModule.hpp"

#include "engine/rollback/History.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/Runtime.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace GYO::Engine::Rollback {

namespace {

std::string TypeNameIn(const Kernel::FrozenRegistry& registry, IdLookup idIn, DataKind kind) {
    const std::optional<std::uint32_t> id = idIn != nullptr ? idIn(registry) : std::nullopt;
    if (!id) return "a type not registered in this World";
    return "'" + (kind == DataKind::Component ? registry.Components()[*id].name : registry.Resources()[*id].name) + "'";
}

bool HasSchedule(const Kernel::FrozenRegistry& registry, const std::string& name) {
    for (const Kernel::ScheduleInfo& info : registry.Schedules()) {
        if (info.options.name == name) return true;
    }
    return false;
}

} // namespace

void RollbackModule::Build(Orchestration::WorldSetup& setup) {
    RollbackRegistrations& table = setup.Buffer().Create<RollbackRegistrations>(setup.Buffer());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<RollbackRegistry>({.name = "RollbackRegistry"});
    // The way in for authoritative values: one for every strategy (authority-rollback declaration 5.5).
    r.Resource<RollbackInbox>({.name = "RollbackInbox"});
    const RollbackOptions* options = setup.Options<RollbackOptions>();
    std::shared_ptr<RollbackStrategy> strategy = options != nullptr ? options->strategy : nullptr;
    if (strategy != nullptr) {
        const WorldTime::WorldTimeOptions* time = setup.Options<WorldTime::WorldTimeOptions>();
        const std::string frame = !options->frameSchedule.empty() ? options->frameSchedule
                                  : time != nullptr                ? time->frameSchedule
                                                                   : std::string(Orchestration::DefaultSchedules::frame);
        // A World that chose a strategy: the history, the strategy to call and the frame's Systems (R-4: none
        // without one).
        r.Resource<RollbackHistory>({.name = "RollbackHistory"});
        r.Resource<RollbackRuntime>({.name = "RollbackRuntime"});
        r.Resource<RollbackPending>({.name = "RollbackPending"});
        r.Message<RollbackAbandoned>({.name = "RollbackAbandoned"});
        r.Component<KeepThroughRollback>({.name = "KeepThroughRollback"});
        Kernel::SystemOptions decide;
        decide.name = "RunRollback";
        decide.mounts.push_back(Kernel::Mount{frame, options->decideStage, {}, {}, {}, {}});
        r.AddSystem<&RunRollback>(decide);
        Kernel::SystemOptions send;
        send.name = "SendRollbackNotes";
        send.mounts.push_back(Kernel::Mount{frame, options->decideStage, {}, {"RunRollback"}, {}, {}});
        r.AddSystem<&SendRollbackNotes>(send);
        Kernel::SystemOptions correct;
        correct.name = "CorrectFrame";
        correct.mounts.push_back(Kernel::Mount{frame, options->correctStage, {}, {}, {}, {}});
        r.AddSystem<&CorrectFrame>(correct);
    }
    // When the World is prepared: the resolved registrations go into it (read at run time from there, so a failed
    // replacement leaves the old World's as they were; authority-rollback declaration 5.1), an empty inbox, and,
    // with a strategy, an empty history for every registered type and the strategy to call.
    setup.Prepare([&table, strategy](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        RollbackRegistry registry = Resolve(table).registry;
        world.SetResource(RollbackInbox{});
        if (strategy != nullptr) {
            RollbackHistory history;
            for (const DataRecord& record : registry.data) history.columns.push_back(record.ops->makeHistory(record.comparison));
            for (const InputRecord& record : registry.inputs) {
                history.inputs.push_back(record.makeHistory(record.capacity != 0 ? record.capacity : strategy->DefaultInputLength()));
            }
            world.SetResource(std::move(history));
            const std::string step = registry.stepSchedules.empty() ? std::string() : registry.stepSchedules.front().name;
            world.SetResource(RollbackRuntime{strategy, step});
            world.SetResource(RollbackPending{});
        }
        world.SetResource(std::move(registry));
        return {};
    });
}

namespace {

using Ok = Base::Result<void, Kernel::KernelError>;

// The strategy and step schedule of the World (a copy: no pointer into the World leaves a closure), or nothing.
std::optional<RollbackRuntime> RuntimeOf(Exclusive& world) {
    auto runtime = world.Read<RollbackRuntime>([](const RollbackRuntime& r) { return r; });
    if (!runtime || !*runtime || (**runtime).strategy == nullptr) return std::nullopt;
    return **runtime;
}

std::uint64_t TickOf(Exclusive& world) {
    auto tick = world.Read<WorldTime::Tick>([](const WorldTime::Tick& t) { return t.value; });
    GYO_ASSERT(tick && *tick && "the rollback Module runs in a World with the time Module");
    return **tick;
}

} // namespace

Ok RecordStep(Exclusive world) {
    const std::optional<RollbackRuntime> runtime = RuntimeOf(world);
    if (!runtime) return {};
    RecordContext context(world, TickOf(world));
    return runtime->strategy->Record(context);
}

Ok RunRollback(Exclusive world) {
    const std::optional<RollbackRuntime> runtime = RuntimeOf(world);
    if (!runtime) return {};
    const std::uint64_t tick = TickOf(world);
    DecideContext decide(world, tick);
    if (auto decided = runtime->strategy->Decide(decide); !decided) return decided;
    if (!decide.Abandoned().empty()) {
        auto noted = world.Modify<RollbackPending>([&decide, tick](RollbackPending& pending) {
            for (const auto& [requested, depth] : decide.Abandoned()) pending.abandoned.push_back({requested, tick, depth});
        });
        if (!noted) return Base::Err(std::move(noted).error());
    }
    if (!decide.Target()) return {};
    RestoreContext restore(world, *decide.Target(), tick);
    if (auto restored = runtime->strategy->Restore(restore); !restored) return restored;
    ResimulateContext resimulate(world, *decide.Target(), tick, runtime->stepSchedule);
    return runtime->strategy->Resimulate(resimulate);
}

Ok CorrectFrame(Exclusive world) {
    const std::optional<RollbackRuntime> runtime = RuntimeOf(world);
    if (!runtime) return {};
    CorrectContext context(world);
    return runtime->strategy->Correct(context);
}

Ok SendRollbackNotes(Kernel::ResMut<RollbackPending> pending, Kernel::MessageWriter<RollbackAbandoned> notes) {
    for (const RollbackAbandoned& note : pending->abandoned) notes.Send(note);
    pending->abandoned.clear();
    return {};
}

void RollbackModule::Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) {
    const RollbackRegistrations* table = context.Buffer().Find<RollbackRegistrations>();
    GYO_ASSERT(table != nullptr);
    Resolved resolved = Resolve(*table);
    for (const RegistrationError& error : resolved.errors) {
        context.Report(error.code, error.message + (error.idIn != nullptr ? " (" + TypeNameIn(registry, error.idIn, error.kind) + ")" : ""));
    }
    const RollbackRegistry& registrations = resolved.registry;
    for (const DataRecord& record : registrations.data) {
        if (!record.ops->idIn(registry)) {
            context.Report("RB-E2", std::string("rollback.") + (record.kind == DataKind::Component ? "Component" : "Resource") +
                                        ": the type is not registered in this World (register it with the Registrar first)");
        }
    }
    for (const DerivedRecord& record : registrations.derived) {
        if (!record.idIn(registry)) {
            context.Report("RB-E2", "rollback.Derived: the component is not registered in this World");
        }
    }
    for (const InputRecord& record : registrations.inputs) {
        if (!record.idIn(registry)) {
            context.Report("RB-E3", "rollback.Input: the type is not registered in this World (one entry registers it: "
                                    "the Registrar, or the authority Module's input registration)");
        }
    }
    for (const StepScheduleRecord& record : registrations.stepSchedules) {
        if (!HasSchedule(registry, record.name)) {
            context.Report("RB-E4", "rollback.StepSchedule '" + record.name + "': no schedule of that name in this World");
        }
    }
    for (const NestedScheduleRecord& record : registrations.nestedSchedules) {
        if (!HasSchedule(registry, record.name)) {
            context.Report("RB-E4", "rollback.NestedSchedule '" + record.name + "': no schedule of that name in this World");
        }
        if (registrations.StepSchedule(record.stepSchedule) == nullptr) {
            context.Report("RB-E5", "rollback.NestedSchedule '" + record.name + "': '" + record.stepSchedule +
                                        "' is not a registered step schedule");
        }
    }
    const RollbackOptions* options = context.Options<RollbackOptions>();
    if (options != nullptr && options->strategy) {
        CheckContext check(registry, registrations, context);
        options->strategy->Check(check);
    }
}

void PeerRollbackStrategy::Check(CheckContext& context) {
    context.Report("RB-E6", "PeerRollbackStrategy is not implemented yet; a strategy of your own can implement "
                            "RollbackStrategy and be chosen in RollbackOptions");
}

// Never run: Check refuses every World that chooses this strategy.
Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Record(RecordContext&) { return {}; }
Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Decide(DecideContext&) { return {}; }
Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Restore(RestoreContext&) { return {}; }
Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Resimulate(ResimulateContext&) { return {}; }
Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Correct(CorrectContext&) { return {}; }

} // namespace GYO::Engine::Rollback
