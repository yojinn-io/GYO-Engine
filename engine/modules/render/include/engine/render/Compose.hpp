#pragma once

// Composing a frame from its cameras (docs/architecture/target/v2/render.zh-Hant.md, section 8.2; SDL declaration
// section 6.4 "合成"): each pipeline (sprite_render's 2D, mesh_render's 3D) puts the passes of a camera in that
// camera's slot of CameraPasses; camera_render then writes them to RenderCommands camera by camera, in the cameras'
// order, each camera's clear first. So a HUD's 2D camera (order 1) comes after the world's 3D camera (order 0)
// whichever pipeline ran first. Bevy's cameras are ordered the same way (sort_cameras, bevy_render/src/camera.rs:836)
// and each runs its own phases.

#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"

namespace GYO::Engine::Render {

// render's Resource in the render World (camera_render): one slot per extracted camera (ExtractedCameras' order),
// emptied when the cameras are extracted.
struct CameraPasses final {
    std::vector<std::vector<RenderPass>> byCamera;
};

inline void HashValue(Kernel::Hasher&, const CameraPasses&) {}

// Appends the frame's passes to out, camera by camera in order. A camera's clear goes first (as Bevy: a window is
// cleared once a frame, each camera's picture put within its viewport): the first camera on a target clears the
// whole target with its colour (a Pass2D with clear only: the letterbox bars); a later camera with a colour clears
// its own viewport (a Pass2D whose one draw is an opaque rectangle over it); a camera without one clears nothing.
// Then the camera's passes. Empties each slot.
void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out);

} // namespace GYO::Engine::Render
