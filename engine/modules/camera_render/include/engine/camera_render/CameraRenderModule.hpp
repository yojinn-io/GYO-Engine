#pragma once

// camera_render (docs/architecture/target/v2/render.zh-Hant.md, section 7; Bevy's extract_cameras,
// bevy_render/src/camera.rs:486): in the render World (with extractFrom; render_world before it), ExtractCameras runs
// in render_world's extract schedule and rebuilds render's ExtractedCameras from the source World's cameras: every
// active camera whose ComputedCamera is valid and that has a GlobalTransform, with its order, target, viewport,
// projection, view point, 2D / 3D marker, clear (ClearColorConfig against the source's ClearColor) and RenderLayers,
// sorted by order, then by source entity.
//
// The pipelines (sprite_render, mesh_render) put each camera's passes in its slot of render's CameraPasses (emptied
// by ExtractCameras); ComposeCameraPasses, in CameraRenderOptions::schedule / composeStage (Main / PostUpdate, after
// the pipelines' Update), writes them to RenderCommands camera by camera, each camera's clear first (Compose.hpp).
//
// A row of its own, not in render_world (SDL declaration section 10.1, R2b): a render World that draws no camera
// (KeepTool's ImGui) does not need the source to register the cameras' types. The source World needs transform,
// camera and world_render (else R-E4).

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::CameraRender {

// Where ComposeCameraPasses runs (WorldDescription::options; none given: the default skeleton). After the stage the
// pipelines queue in, before render_world's Submit.
// The name of ComposeCameraPasses: a pipeline that queues in the compose stage orders itself `before` it (ui_render).
inline constexpr std::string_view kComposeCameraPasses = "ComposeCameraPasses";

struct CameraRenderOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string composeStage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

class CameraRenderModule final : public Orchestration::Module {
public:
    CameraRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::CameraRender
