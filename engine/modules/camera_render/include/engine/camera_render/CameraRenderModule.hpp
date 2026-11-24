#pragma once

// camera_render (docs/architecture/target/v2/render.zh-Hant.md, section 7; Bevy's extract_cameras,
// bevy_render/src/camera.rs:486): in the render World (with extractFrom; render_world before it), ExtractCameras runs
// in render_world's extract schedule and rebuilds render's ExtractedCameras from the source World's cameras: every
// active camera whose ComputedCamera is valid and that has a GlobalTransform, with its order, target, viewport,
// projection, view point, 2D / 3D marker, clear (ClearColorConfig against the source's ClearColor) and RenderLayers,
// sorted by order, then by source entity.
//
// A row of its own, not in render_world (SDL declaration section 10.1, R2b): a render World that draws no camera
// (KeepTool's ImGui) does not need the source to register the cameras' types. The source World needs transform,
// camera and world_render (else R-E4).

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::CameraRender {

class CameraRenderModule final : public Orchestration::Module {
public:
    CameraRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::CameraRender
