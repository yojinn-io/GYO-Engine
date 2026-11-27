#include "engine/world_render/WorldRenderModule.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/RenderTargets.hpp"

namespace GYO::Engine::WorldRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

// The ids this System wrote a size for, so a target let go has its size removed.
struct WrittenTargets {
    std::vector<std::uint32_t> ids;
};

// Each live target's size into CameraTargetSizes, in id order (a followed target's before its follower's), through
// camera's WriteTargetSize (written only when it differs); the sizes of targets let go are removed.
SystemResult WriteRenderTargetSizes(Kernel::Res<Render::RenderTargets> targets, Kernel::ResMut<Camera::CameraTargetSizes> sizes,
                                    Kernel::Local<WrittenTargets> written) {
    std::vector<std::uint32_t> now;
    now.reserve(targets->Live().size());
    for (const auto& [id, desc] : targets->Live()) {
        now.push_back(id);
        if (const std::optional<Camera::TargetSize> size = Render::TargetSizeOf(desc, sizes.Read())) {
            Camera::WriteTargetSize(sizes, Camera::CameraTarget{id}, *size);
        }
    }
    // Both lists are in id order.
    for (const std::uint32_t id : written->ids) {
        if (!std::binary_search(now.begin(), now.end(), id)) Camera::RemoveTargetSize(sizes, Camera::CameraTarget{id});
    }
    written->ids = std::move(now);
    return {};
}

const WorldRenderOptions& OptionsOf(const WorldRenderOptions* given) {
    static const WorldRenderOptions defaults{};
    return given != nullptr ? *given : defaults;
}

} // namespace

void WorldRenderModule::Build(Orchestration::WorldSetup& setup) {
    const WorldRenderOptions& options = OptionsOf(setup.Options<WorldRenderOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Render::Sprite>({.name = "Sprite"});
    r.Component<Render::Shape2d>({.name = "Shape2d"});
    r.Component<Render::Mesh3d>({.name = "Mesh3d"});
    r.Component<Render::SkinPose>({.name = "SkinPose"});
    r.Component<Render::Camera3dDepth>({.name = "Camera3dDepth"});
    r.Component<Render::RenderLayers>({.name = "RenderLayers"});
    r.Component<Render::Aabb>({.name = "Aabb"});
    r.Component<Render::NoFrustumCulling>({.name = "NoFrustumCulling"});
    r.Component<Render::VisibilityRange>({.name = "VisibilityRange"});
    r.Component<Render::CulledEntities>({.name = "CulledEntities"});
    r.Component<Render::ClearColorConfig>({.name = "ClearColorConfig"});
    r.Resource<Render::ClearColor>({.name = "ClearColor"});
    r.Resource<Render::RenderTargets>({.name = "RenderTargets"});
    Kernel::SystemOptions write;
    write.name = "WriteRenderTargetSizes";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.before.push_back(std::string(Camera::kUpdateSet));
    write.mounts.push_back(std::move(mount));
    r.AddSystem<&WriteRenderTargetSizes>(std::move(write));
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<Render::ClearColor>() == nullptr) world.SetResource(Render::ClearColor{});
        if (world.Resource<Render::RenderTargets>() == nullptr) world.SetResource(Render::RenderTargets{});
        return {};
    });
}

} // namespace GYO::Engine::WorldRender
