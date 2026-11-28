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

// The cameras whose TargetUpdate was Once at the end of the last frame: the render World extracted them as Once (its
// extract reads the source as the source's frame left it).
struct SeenOnce final {
    std::vector<Kernel::EntityId> cameras;
};
inline void HashValue(Kernel::Hasher&, const SeenOnce&) {}

// At the end of the frame (Last): which cameras are Once now.
SystemResult NoteOnceUpdates(Kernel::Query<Kernel::Read<Render::TargetUpdate>> updates, Kernel::ResMut<SeenOnce> seen) {
    std::vector<Kernel::EntityId> cameras;
    updates.ForEach([&](const Kernel::EntityId id, const Render::TargetUpdate& update) {
        if (update.mode == Render::TargetUpdate::Mode::Once) cameras.push_back(id);
    });
    seen.Get().cameras = std::move(cameras);
    return {};
}

// At the start of the next frame (First): those still Once drew once; they are Disabled from now on (Godot writes
// VIEWPORT_UPDATE_DISABLED back after the update, servers/rendering/renderer_viewport.cpp:982-983).
SystemResult SettleOnceUpdates(Kernel::Query<Kernel::Write<Render::TargetUpdate>> updates, Kernel::Res<SeenOnce> seen) {
    updates.ForEach([&](const Kernel::EntityId id, Kernel::Mut<Render::TargetUpdate> update) {
        if (update.Read().mode != Render::TargetUpdate::Mode::Once) return;
        if (std::find(seen->cameras.begin(), seen->cameras.end(), id) == seen->cameras.end()) return;
        update.Get().mode = Render::TargetUpdate::Mode::Disabled;
    });
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
    r.Component<Render::TargetUpdate>({.name = "TargetUpdate"});
    r.Component<Render::Hdr>({.name = "Hdr"});
    r.Component<Render::Tonemapping>({.name = "Tonemapping"});
    r.Resource<Render::ClearColor>({.name = "ClearColor"});
    r.Resource<Render::RenderTargets>({.name = "RenderTargets"});
    r.Resource<SeenOnce>({.name = "SeenOnce"});
    Kernel::SystemOptions write;
    write.name = "WriteRenderTargetSizes";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.before.push_back(std::string(Camera::kUpdateSet));
    write.mounts.push_back(std::move(mount));
    r.AddSystem<&WriteRenderTargetSizes>(std::move(write));
    Kernel::SystemOptions once;
    once.name = "SettleOnceUpdates";
    Kernel::Mount onceMount;
    onceMount.schedule = options.schedule;
    onceMount.stage = options.settleOnceStage;
    once.mounts.push_back(std::move(onceMount));
    r.AddSystem<&SettleOnceUpdates>(std::move(once));
    Kernel::SystemOptions note;
    note.name = "NoteOnceUpdates";
    Kernel::Mount noteMount;
    noteMount.schedule = options.schedule;
    noteMount.stage = options.noteOnceStage;
    note.mounts.push_back(std::move(noteMount));
    r.AddSystem<&NoteOnceUpdates>(std::move(note));
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<Render::ClearColor>() == nullptr) world.SetResource(Render::ClearColor{});
        if (world.Resource<Render::RenderTargets>() == nullptr) world.SetResource(Render::RenderTargets{});
        if (world.Resource<SeenOnce>() == nullptr) world.SetResource(SeenOnce{});
        return {};
    });
}

} // namespace GYO::Engine::WorldRender
