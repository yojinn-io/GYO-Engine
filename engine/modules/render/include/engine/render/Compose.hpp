#pragma once

// Composing a frame from its views (docs/architecture/target/v2/render.zh-Hant.md, section 8.2; SDL declaration section
// 6.4 "合成"; render-pipeline declaration section 14, phases): each pipeline puts the passes of a camera in a phase of
// that camera's slot of CameraPasses (sprite_render's 2D in Transparent, mesh_render's 3D in Opaque, ui_render's UI in
// Overlay), and the passes of views that are no camera's (a point or spot light's shadow map) in beforeCameras;
// camera_render then writes them to RenderCommands: those first, then camera by camera in the cameras' order, each
// camera's clear first, then its phases in order. So a HUD's 2D camera (order 1) comes after the world's 3D camera
// (order 0), and a camera's post-processing after its scene, whichever pipeline ran first.
//
// Bevy runs each camera's schedule (CameraRenderGraph) with its system sets Prepass, MainPass, EarlyPostProcess,
// PostProcess chained (bevy_core_pipeline/src/schedule.rs:50,69) and the views that are no camera's
// (RootNonCameraView: the point and spot lights' shadows, bevy_pbr/src/render/light.rs:2235,2328; a directional light's
// cascades follow the camera) before the cameras (camera_driver, :136-152); the cameras by order (sort_cameras,
// bevy_render/src/camera.rs:836). Here a pass is data a backend records later (RD-1), so a phase is a list of passes,
// not a system set: a pipeline adds a pass to a phase with a System, and the order inside a phase is the order of
// those Systems (before / after).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"

namespace GYO::Engine::Render {

// The phases of a camera, in the order they are drawn (Bevy's Core3dSystems and Core2dSystems with the main pass in two
// and the UI's place after post-processing, bevy_ui_render/src/lib.rs:302). The values are the order and the index of
// a camera's list (from 0, unlike GYO's other enums, so every value, CameraPhase{} too, is a phase).
enum class CameraPhase : std::uint8_t {
    // Before the main pass: depth-only passes that later passes test against or sample (Bevy's Prepass).
    Prepass = 0,
    // The opaque scene (Bevy's main_opaque_pass_3d): mesh_render's Pass3D, whose blended draws follow its opaque ones.
    Opaque,
    // What is blended over the scene (Bevy's main_transparent_pass_3d and Transparent2d): sprite_render's Pass2D.
    Transparent,
    // Full-screen passes over the camera's picture (Bevy's EarlyPostProcess and PostProcess: tonemapping, bloom).
    PostProcess,
    // Over everything of the camera, after post-processing (Bevy's ui_pass): ui_render's Pass2D.
    Overlay,
};
inline constexpr std::size_t kCameraPhaseCount = static_cast<std::size_t>(CameraPhase::Overlay) + 1;

// One camera's passes, phase by phase.
class CameraPhases final {
public:
    [[nodiscard]] std::vector<RenderPass>& operator[](const CameraPhase phase) noexcept { return lists_[static_cast<std::size_t>(phase)]; }
    [[nodiscard]] const std::vector<RenderPass>& operator[](const CameraPhase phase) const noexcept {
        return lists_[static_cast<std::size_t>(phase)];
    }
    // The lists in the order they are drawn.
    [[nodiscard]] std::span<std::vector<RenderPass>> InOrder() noexcept { return lists_; }

private:
    std::array<std::vector<RenderPass>, kCameraPhaseCount> lists_;
};

// render's Resource in the render World (camera_render): one slot per extracted camera (ExtractedCameras' order, made
// when the cameras are extracted) and the passes of views that are no camera's; ComposePasses empties them.
struct CameraPasses final {
    // Passes before every camera: the views that are no camera's (Bevy's RootNonCameraView: a point or spot light's
    // shadow map).
    std::vector<RenderPass> beforeCameras;
    std::vector<CameraPhases> byCamera;
};

inline void HashValue(Kernel::Hasher&, const CameraPasses&) {}

// Appends the frame's passes to out (and the cameras' retained targets to out.retained): beforeCameras, then camera by
// camera in order. A camera's clear goes first (as
// Bevy: a window is cleared once a frame, each camera's picture put within its viewport): the first camera on a
// target (no camera before it is on that target and no pass in out draws colour on it) clears the whole target with
// its colour (a Pass2D with clear only: the letterbox bars); a later camera with a colour clears its own viewport (a
// Pass2D whose one draw is an opaque rectangle over it); a camera without one clears nothing. Then the camera's phases
// in CameraPhase's order, each phase's passes in their order. Empties every list.
void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out);

} // namespace GYO::Engine::Render
