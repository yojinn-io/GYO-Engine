#pragma once

// The camera Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.2; Design
// Invariants CM-1 to CM-7 in docs/architecture/target/v2/camera.zh-Hant.md). Carried by default (config/modules.csv).
// In every World it joins it registers the camera components and CameraTargetSizes (empty unless one is already
// there), declares the initial values (Camera2d / Camera3d -> Camera and Projection; Camera -> CameraTarget and
// ComputedCamera, CameraView) and mounts UpdateCameras (Bevy's camera_system) in the set kUpdateSet. UpdateCameras
// reads no GlobalTransform, so it has no order with the transform Module's propagation (as in Bevy). UpdateCameraViews
// (Bevy's update_frusta), in the same set, after UpdateCameras and the propagation, writes CameraView from
// ComputedCamera and the GlobalTransform: CameraOptions names transform's schedule and stage (render-pipeline
// declaration section 13).
//
// Its duties (CM-1): these and the conversions in Camera.hpp. Not: drawing, visibility, clear colours, the window.

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Camera {

// Where UpdateCameras runs (WorldDescription::options; none given: the default skeleton).
struct CameraOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

// The public set of the update (Bevy's CameraUpdateSystems), in CameraOptions::schedule.
inline constexpr std::string_view kUpdateSet = "CameraUpdate";

class CameraModule final : public Orchestration::Module {
public:
    CameraModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::Camera
