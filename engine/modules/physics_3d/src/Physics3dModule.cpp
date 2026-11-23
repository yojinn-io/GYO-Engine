#include "engine/physics_3d/Physics3dModule.hpp"

#include "engine/kernel/Registry.hpp"
#include "engine/physics_3d/Collider.hpp"

namespace GYO::Engine::Physics3d {

void Physics3dModule::Build(Orchestration::WorldSetup& setup) {
    setup.Registrar().Component<Collider>({.name = "Physics3d.Collider"});
}

} // namespace GYO::Engine::Physics3d
