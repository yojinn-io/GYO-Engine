#include "engine/entity_ids/EntityIdsModule.hpp"

#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"

namespace GYO::Engine::EntityIds {

void EntityIdsModule::Build(Orchestration::WorldSetup& setup) {
    Kernel::Registrar& registrar = setup.Registrar();
    registrar.Resource<RangeAllotter>({.name = "RangeAllotter"});
    registrar.Allotter<RangeAllotter>();
    const EntityIdsOptions* given = setup.Options<EntityIdsOptions>();
    const EntityIdsOptions options = given != nullptr ? *given : EntityIdsOptions{};
    setup.Prepare([options](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        // A value already there (carried over) stays (O-7); a later prepare function may still replace it.
        if (world.Resource<RangeAllotter>() == nullptr) world.SetResource(RangeAllotter::Of(options.first, options.last));
        return {};
    });
}

} // namespace GYO::Engine::EntityIds
