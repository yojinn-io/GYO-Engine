#pragma once

// The glue between window and camera (docs/architecture/target/v2/window.zh-Hant.md, "window_camera"; the place the
// transform-camera declaration section 4.2 left to batch W). camera does not depend on window and window does not
// depend on camera (SDL declaration L-4); this small unit depends on both and writes the window's size into
// camera's CameraTargetSizes as the main target (CameraTarget{0}), which Bevy's camera_system reads from the Window
// directly (bevy_render/src/camera.rs:357-363).
//
// WriteWindowTargetSize runs every frame in Main/PreUpdate after the window's set (and so before camera's
// UpdateCameras in PostUpdate): it reads WindowState and writes the target size only when it differs. A new or
// replaced World gets its size in its first round, as soon as its WindowState knows the size (the backend writes the
// present size in that round; WN-3). Carried by default (config/modules.csv); a product that fills
// CameraTargetSizes{0} itself leaves this unit out.

#include <string>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::WindowCamera {

// Where WriteWindowTargetSize runs (WorldDescription::options; none given: the default skeleton). It is ordered after
// window's set, so it must be in window's schedule.
struct WindowCameraOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::preUpdate);
};

class WindowCameraModule final : public Orchestration::Module {
public:
    WindowCameraModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::WindowCamera
