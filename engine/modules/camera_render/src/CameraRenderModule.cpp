#include "engine/camera_render/CameraRenderModule.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "engine/base/Result.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderTargets.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"

namespace GYO::Engine::CameraRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

// Before ExtractCameras: a render target of the render World for each of the source's (render-pipeline declaration
// section 13; Bevy's prepare_assets makes the GpuImage of a target image keyed by its id, render_asset.rs:393).
constexpr std::string_view kExtractCameras = "ExtractCameras";
constexpr std::string_view kExtractHdr = "ExtractHdr";

SystemResult ExtractRenderTargets(Kernel::ExtractRes<Render::RenderTargets> source, Kernel::ExtractRes<Camera::CameraTargetSizes> sizes,
                                  Kernel::ResMut<Render::RenderTextures> textures, Kernel::ResMut<Render::TargetTextures> mapped) {
    // The main target's size the source's cameras computed with (window_camera writes the window's): the size of the
    // targets that follow it, decided here on the data side (render-pipeline declaration 14.11).
    std::optional<Render::TargetSize> main;
    if (sizes.Present()) {
        if (const Camera::TargetSize* size = sizes->Find(Camera::kMainTarget);
            size != nullptr && size->physicalSize.x > 0 && size->physicalSize.y > 0) {
            main = Render::TargetSize{static_cast<std::uint32_t>(size->physicalSize.x), static_cast<std::uint32_t>(size->physicalSize.y)};
        }
    }
    textures.Get().SetMainSize(main);
    Render::SyncTargetTextures(source.Present() ? &*source : nullptr, textures.Get(), mapped.Get());
    return {};
}

SystemResult ExtractCameras(Kernel::Extract<Kernel::Read<Camera::Camera>> cameras,
                            Kernel::Extract<Kernel::Read<Camera::ComputedCamera>> computed,
                            Kernel::Extract<Kernel::Read<Camera::CameraTarget>> targets,
                            Kernel::Extract<Kernel::Read<Camera::CameraView>> views,
                            Kernel::Extract<Kernel::Read<Camera::Camera2d>> markers2d,
                            Kernel::Extract<Kernel::Read<Camera::Camera3d>> markers3d,
                            Kernel::Extract<Kernel::Read<Render::ClearColorConfig>> clears,
                            Kernel::Extract<Kernel::Read<Render::RenderLayers>> layers,
                            Kernel::Extract<Kernel::Read<Render::Camera3dDepth>> depths,
                            Kernel::Extract<Kernel::Read<Render::CulledEntities>> culled,
                            Kernel::Extract<Kernel::Read<Render::TargetUpdate>> updates,
                            Kernel::ExtractRes<Render::ClearColor> clearColor, Kernel::Res<Render::TargetTextures> mapped,
                            Kernel::Res<Render::RenderTextures> textures, Kernel::ResMut<Render::ExtractedCameras> out,
                            Kernel::ResMut<Render::CameraPasses> passes) {
    // Each camera that draws, and whether it waits (Disabled, its target with a picture: it draws only when another
    // camera draws its target); each render target a camera names, and whether one of them draws on it this frame
    // (render-pipeline declaration 14.11: the decision is the target's).
    std::vector<std::pair<Render::ExtractedCamera, bool>> list;
    std::vector<std::pair<Render::TextureId, bool>> named;
    const auto name = [&](const Render::TextureId texture, const bool drawing) {
        const auto it = std::find_if(named.begin(), named.end(), [&](const auto& n) { return n.first == texture; });
        if (it == named.end()) {
            named.emplace_back(texture, drawing);
        } else {
            it->second = it->second || drawing;
        }
    };
    const Render::Color8 defaultClear = clearColor.Present() ? clearColor->color : Render::ClearColor{}.color;
    cameras.ForEach([&](const Kernel::EntityId id, const Camera::Camera& camera) {
        // The pass target its CameraTarget maps to; a target the source has none for (let go after the camera was
        // computed) is nothing to draw on.
        const Camera::CameraTarget* target = targets.Get(id);
        std::optional<Render::TextureId> passTarget;
        if (target != nullptr && *target != Camera::kMainTarget) {
            const Render::TextureId* made = mapped->Find(*target);
            if (made == nullptr) return;
            passTarget = *made;
        }
        // Not drawing this frame: inactive, its projection or view not valid.
        const Camera::ComputedCamera* c = computed.Get(id);
        const Camera::CameraView* v = views.Get(id);
        const bool drawable = camera.active && c != nullptr && c->valid && v != nullptr && v->valid;
        // Waiting: its TargetUpdate says not to draw this frame and its target has a picture to stand for this frame's
        // (one with none is drawn: the camera draws as one updating every frame; the main target keeps none).
        const Render::TargetUpdate* update = updates.Get(id);
        const bool waits = update != nullptr && update->mode == Render::TargetUpdate::Mode::Disabled && textures->HasPicture(passTarget);
        if (passTarget) name(*passTarget, drawable && !waits);
        if (!drawable) return;
        Render::ExtractedCamera e;
        e.entity = id;
        e.order = camera.order;
        e.target = passTarget;
        e.computed = *c;
        e.view = *v;
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
        const Render::Camera3dDepth* depth = depths.Get(id);
        e.clearDepth = depth != nullptr ? depth->clear : true;
        if (const Render::CulledEntities* list = culled.Get(id)) e.culled = list->entities;
        list.emplace_back(std::move(e), waits);
    });
    // A target none of whose cameras draws this frame stands as a frame drew it: retained (one never drawn at its size
    // is not: a pass sampling it is RP-3's error; one drawn whose picture the GPU did not get, the backend skips what
    // samples it and counts). Its waiting cameras are not extracted; on a target another camera draws, they draw.
    // Waiting goes by the picture (the backend's receipt: a camera that can draw draws one without it again);
    // retaining by what the program drew (an inactive camera cannot draw again: render-pipeline declaration 14.14).
    std::vector<Render::ExtractedCamera> drawing;
    for (auto& [e, waits] : list) {
        const auto on = std::find_if(named.begin(), named.end(), [&](const auto& n) { return e.target && n.first == *e.target; });
        if (!waits || (on != named.end() && on->second)) drawing.push_back(std::move(e));
    }
    std::vector<Render::TextureId> retained;
    for (const auto& [texture, drawn] : named) {
        if (!drawn && textures->WasDrawn(texture)) retained.push_back(texture);
    }
    out.Get().retained = std::move(retained);
    // Entities come in ascending order; a stable sort by order keeps the entity order among equal orders.
    std::stable_sort(drawing.begin(), drawing.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
    passes.Get().byCamera.assign(drawing.size(), {});
    out.Get().cameras = std::move(drawing);
    return {};
}

// After ExtractCameras: each drawing camera's curve and its Hdr targets (render-pipeline declaration section 16,
// revision 16.8; HdrTargets, RenderTargets.hpp): a Camera3d with Hdr draws on a picture of its own; every 3D camera on a
// target a Hdr camera draws on this frame tests the one depth of that target. Made by the render World itself,
// following their target's size; let go when no camera draws with them.
SystemResult ExtractHdr(Kernel::Extract<Kernel::Read<Render::Hdr>> hdr, Kernel::Extract<Kernel::Read<Render::Tonemapping>> tonemappings,
                        Kernel::ResMut<Render::ExtractedCameras> cameras, Kernel::ResMut<Render::RenderTextures> textures,
                        Kernel::ResMut<Render::HdrTargets> targets) {
    std::vector<Render::HdrCamera> withHdr;
    for (Render::ExtractedCamera& camera : cameras.Get().cameras) {
        if (const Render::Tonemapping* t = tonemappings.Get(camera.entity)) camera.tonemapping = t->method;
        if (camera.is3d && hdr.Contains(camera.entity)) withHdr.push_back(Render::HdrCamera{camera.entity, camera.target});
    }
    Render::SyncHdrTargets(withHdr, textures.Get(), targets.Get());
    for (Render::ExtractedCamera& camera : cameras.Get().cameras) {
        camera.hdr = targets.Read().PictureOf(camera.entity);
        if (camera.is3d) camera.depth = targets.Read().DepthOf(camera.target);
    }
    return {};
}

SystemResult ComposeCameraPasses(Kernel::Res<Render::ExtractedCameras> cameras, Kernel::ResMut<Render::CameraPasses> passes,
                                 Kernel::ResMut<Render::RenderCommands> commands) {
    Render::ComposePasses(*cameras, passes.Get(), commands.Get());
    return {};
}

const CameraRenderOptions& OptionsOf(const CameraRenderOptions* given) {
    static const CameraRenderOptions defaults{};
    return given != nullptr ? *given : defaults;
}

} // namespace

void CameraRenderModule::Build(Orchestration::WorldSetup& setup) {
    const CameraRenderOptions& options = OptionsOf(setup.Options<CameraRenderOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<Render::ExtractedCameras>({.name = "ExtractedCameras"});
    r.Resource<Render::CameraPasses>({.name = "CameraPasses"});
    r.Resource<Render::TargetTextures>({.name = "TargetTextures"});
    r.Resource<Render::HdrTargets>({.name = "HdrTargets"});
    Kernel::SystemOptions targetsExtract;
    targetsExtract.name = "ExtractRenderTargets";
    Kernel::Mount targetsMount;
    targetsMount.schedule = std::string(RenderWorld::kExtractSchedule);
    targetsMount.stage = std::string(RenderWorld::kExtractSchedule);
    targetsMount.before.push_back(std::string(kExtractCameras));
    targetsExtract.mounts.push_back(std::move(targetsMount));
    r.AddSystem<&ExtractRenderTargets>(std::move(targetsExtract));
    Kernel::SystemOptions extract;
    extract.name = std::string(kExtractCameras);
    Kernel::Mount mount;
    mount.schedule = std::string(RenderWorld::kExtractSchedule);
    mount.stage = std::string(RenderWorld::kExtractSchedule);
    extract.mounts.push_back(std::move(mount));
    r.AddSystem<&ExtractCameras>(std::move(extract));
    Kernel::SystemOptions hdrExtract;
    hdrExtract.name = std::string(kExtractHdr);
    Kernel::Mount hdrMount;
    hdrMount.schedule = std::string(RenderWorld::kExtractSchedule);
    hdrMount.stage = std::string(RenderWorld::kExtractSchedule);
    hdrMount.after.push_back(std::string(kExtractCameras));
    hdrExtract.mounts.push_back(std::move(hdrMount));
    r.AddSystem<&ExtractHdr>(std::move(hdrExtract));
    Kernel::SystemOptions compose;
    compose.name = std::string(kComposeCameraPasses);
    Kernel::Mount composeMount;
    composeMount.schedule = options.schedule;
    composeMount.stage = options.composeStage;
    compose.mounts.push_back(std::move(composeMount));
    r.AddSystem<&ComposeCameraPasses>(std::move(compose));
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<Render::ExtractedCameras>() == nullptr) world.SetResource(Render::ExtractedCameras{});
        if (world.Resource<Render::CameraPasses>() == nullptr) world.SetResource(Render::CameraPasses{});
        if (world.Resource<Render::TargetTextures>() == nullptr) world.SetResource(Render::TargetTextures{});
        if (world.Resource<Render::HdrTargets>() == nullptr) world.SetResource(Render::HdrTargets{});
        return {};
    });
}

} // namespace GYO::Engine::CameraRender
