#include "engine/rollback/RollbackModule.hpp"

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
    setup.Registrar().Resource<RollbackRegistry>({.name = "RollbackRegistry"});
    // The resolved registrations go into the World when it is prepared: read at run time from there, so a failed
    // replacement leaves the old World's as they were (authority-rollback declaration 5.1).
    setup.Prepare([&table](Kernel::World& world) -> ::Engine::Base::Result<void, Orchestration::OrchestrationError> {
        world.SetResource(Resolve(table).registry);
        return {};
    });
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
::Engine::Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Record(RecordContext&) { return {}; }
::Engine::Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Decide(DecideContext&) { return {}; }
::Engine::Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Restore(RestoreContext&) { return {}; }
::Engine::Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Resimulate(ResimulateContext&) { return {}; }
::Engine::Base::Result<void, Kernel::KernelError> PeerRollbackStrategy::Correct(CorrectContext&) { return {}; }

} // namespace GYO::Engine::Rollback
