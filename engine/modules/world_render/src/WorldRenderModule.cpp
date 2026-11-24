#include "engine/world_render/WorldRenderModule.hpp"

#include "engine/base/Result.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Drawables.hpp"

namespace GYO::Engine::WorldRender {

void WorldRenderModule::Build(Orchestration::WorldSetup& setup) {
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Render::Sprite>({.name = "Sprite"});
    r.Component<Render::Shape2d>({.name = "Shape2d"});
    r.Component<Render::Mesh3d>({.name = "Mesh3d"});
    r.Component<Render::SkinPose>({.name = "SkinPose"});
    r.Component<Render::RenderLayers>({.name = "RenderLayers"});
    r.Component<Render::ClearColorConfig>({.name = "ClearColorConfig"});
    r.Resource<Render::ClearColor>({.name = "ClearColor"});
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<Render::ClearColor>() == nullptr) world.SetResource(Render::ClearColor{});
        return {};
    });
}

} // namespace GYO::Engine::WorldRender
