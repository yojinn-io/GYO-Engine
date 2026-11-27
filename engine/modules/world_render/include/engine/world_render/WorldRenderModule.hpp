#pragma once

// world_render (docs/architecture/target/v2/render.zh-Hant.md, section 7): the source World end of render (the
// world_<x> naming of world_asset and world_time; Bevy's SpritePlugin and the camera's render settings). In the World
// it joins (a simulation or document World; default=no) it registers render's drawables, Sprite, Shape2d, Mesh3d,
// SkinPose and RenderLayers, the cameras' ClearColorConfig and the ClearColor Resource (opaque black unless one is already there),
// so the render World's camera_render and sprite_render can extract them (a type they read that the source did not
// register is the kernel's R-E4). The author writes the drawables, transform places them, camera computes the cameras.
//
// Render targets (render-pipeline declaration section 13): it registers render's RenderTargets (empty unless one is
// already there) and runs one System, WriteRenderTargetSizes (WorldRenderOptions; before camera's CameraUpdate): each
// live target's size goes into camera's CameraTargetSizes under the target's id, a target that follows the main
// target with target 0's size (window_camera writes it in PreUpdate); a size is written only when it differs, and a
// target let go has its size removed. So a camera on a target computes as a camera on the window does (CM-5: the
// sizes are a copy, used for the projections alone). Bevy's camera_system reads an image target's size from
// Assets<Image> itself (bevy_render/src/camera.rs:292); camera cannot read render's types (CM-2), so the copy is
// written here, as window_camera writes the window's.

#include <string>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::WorldRender {

// Where WriteRenderTargetSizes runs (WorldDescription::options; none given: the default skeleton): camera's schedule
// (it is ordered before camera's CameraUpdate set) and a stage at or before camera's.
struct WorldRenderOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

class WorldRenderModule final : public Orchestration::Module {
public:
    WorldRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::WorldRender
