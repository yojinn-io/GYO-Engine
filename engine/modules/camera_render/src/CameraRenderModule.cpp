#include "engine/camera_render/CameraRenderModule.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "engine/base/Result.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/transform/Transform.hpp"

namespace GYO::Engine::CameraRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

SystemResult ExtractCameras(Kernel::Extract<Kernel::Read<Camera::Camera>> cameras,
                            Kernel::Extract<Kernel::Read<Camera::ComputedCamera>> computed,
                            Kernel::Extract<Kernel::Read<Camera::CameraTarget>> targets,
                            Kernel::Extract<Kernel::Read<Transform::GlobalTransform>> globals,
                            Kernel::Extract<Kernel::Read<Camera::Camera2d>> markers2d,
                            Kernel::Extract<Kernel::Read<Camera::Camera3d>> markers3d,
                            Kernel::Extract<Kernel::Read<Render::ClearColorConfig>> clears,
                            Kernel::Extract<Kernel::Read<Render::RenderLayers>> layers,
                            Kernel::ExtractRes<Render::ClearColor> clearColor, Kernel::ResMut<Render::ExtractedCameras> out) {
    std::vector<Render::ExtractedCamera> list;
    const Render::Color8 defaultClear = clearColor.Present() ? clearColor->color : Render::ClearColor{}.color;
    cameras.ForEach([&](const Kernel::EntityId id, const Camera::Camera& camera) {
        if (!camera.active) return;
        const Camera::ComputedCamera* c = computed.Get(id);
        const Transform::GlobalTransform* g = globals.Get(id);
        if (c == nullptr || !c->valid || g == nullptr) return;
        Render::ExtractedCamera e;
        e.entity = id;
        e.order = camera.order;
        const Camera::CameraTarget* target = targets.Get(id);
        e.target = target != nullptr ? target->id : 0;
        e.computed = *c;
        e.view = g->matrix;
        e.is2d = markers2d.Contains(id);
        e.is3d = markers3d.Contains(id);
        const Render::ClearColorConfig* clear = clears.Get(id);
        const Render::ClearColorConfig config = clear != nullptr ? *clear : Render::ClearColorConfig{};
        switch (config.mode) {
        case Render::ClearColorConfig::Mode::Default: e.clear = defaultClear; break;
        case Render::ClearColorConfig::Mode::Custom: e.clear = config.color; break;
        case Render::ClearColorConfig::Mode::None: e.clear = std::nullopt; break;
        }
        const Render::RenderLayers* l = layers.Get(id);
        e.layers = l != nullptr ? l->mask : Render::RenderLayers{}.mask;
        list.push_back(std::move(e));
    });
    // Entities come in ascending order; a stable sort by order keeps the entity order among equal orders.
    std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
    out.Get().cameras = std::move(list);
    return {};
}

} // namespace

void CameraRenderModule::Build(Orchestration::WorldSetup& setup) {
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<Render::ExtractedCameras>({.name = "ExtractedCameras"});
    Kernel::SystemOptions extract;
    extract.name = "ExtractCameras";
    Kernel::Mount mount;
    mount.schedule = std::string(RenderWorld::kExtractSchedule);
    mount.stage = std::string(RenderWorld::kExtractSchedule);
    extract.mounts.push_back(std::move(mount));
    r.AddSystem<&ExtractCameras>(std::move(extract));
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<Render::ExtractedCameras>() == nullptr) world.SetResource(Render::ExtractedCameras{});
        return {};
    });
}

} // namespace GYO::Engine::CameraRender
