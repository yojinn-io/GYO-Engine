#pragma once

// camera_render (docs/architecture/target/v2/render.zh-Hant.md, section 7; Bevy's extract_cameras,
// bevy_render/src/camera.rs:486): in the render World (with extractFrom; render_world before it), ExtractCameras runs
// in render_world's extract schedule and rebuilds render's ExtractedCameras from the source World's cameras: every
// active camera whose ComputedCamera and CameraView are valid, with its order, target, viewport,
// projection, view point, 2D / 3D marker, clear (ClearColorConfig against the source's ClearColor) and RenderLayers,
// sorted by order, then by source entity.
//
// Render targets (render-pipeline declaration section 13): ExtractRenderTargets, before it, keeps render's
// TargetTextures in step with the source's RenderTargets (a render target of RenderTextures for each; let go when the
// source lets its target go; every one of them when the source's table is another, a replaced source World). A
// camera's target is the pass target its CameraTarget maps to (the main target stays the main target); a camera whose
// CameraTarget has no render target (let go after the camera was computed) is not extracted: nothing to draw on.
//
// High dynamic range (render-pipeline declaration section 16): ExtractHdr, after it, gives each extracted camera its
// Tonemapping's curve, each 3D camera with Hdr a picture of its own and every 3D camera on a target such a camera draws
// on the one depth of that target (render's HdrTargets, made by the render World itself); ComposeCameraPasses puts that
// camera's tonemapping pass after its PostProcess phase.
//
// The pipelines (sprite_render, mesh_render, ui_render) put each camera's passes in a phase of its slot of render's
// CameraPasses (its slots made by ExtractCameras), and the passes of views that are no camera's in its beforeCameras;
// ComposeCameraPasses, in CameraRenderOptions::schedule / composeStage (Main / PostUpdate, after the pipelines'
// Update), writes them to RenderCommands: the views that are no camera's, then camera by camera, each camera's clear
// first, then its phases in order (Compose.hpp).
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
