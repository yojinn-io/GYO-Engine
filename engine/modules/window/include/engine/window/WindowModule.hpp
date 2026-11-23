#pragma once

// The window Module (docs/architecture/target/v2/window.zh-Hant.md; Design Invariants WN-1 to WN-4). Carried by
// default (config/modules.csv): without a backend it is empty data (as Bevy's WindowPlugin in DefaultPlugins). In
// every World it joins it registers the Messages, state and requests of Window.hpp, declares the public set
// kWindowSet and mounts UpdateWindowState in it (Main/PreUpdate by default).
//
// Its state comes from Messages only, as input's does (SDL declaration L17; Bevy's winit writes the Window component
// directly, bevy_winit/src/state.rs:256-257, a deviation recorded in window.zh-Hant.md). Whoever writes the window
// Messages mounts its writer in the same stage with `before` kWindowSet.
//
// Its duties (WN-1): register the window data; keep WindowState and Clipboard from the Messages; set
// WindowRequest::close on a close request when closeWhenRequested is on. Not: making or changing the window (the
// backend's), the camera's target size (window_camera), deciding when the program ends (the runner's).

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Window {

// Where UpdateWindowState runs (WorldDescription::options; none given: the default skeleton).
struct WindowOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::preUpdate);
};

// The public set, in WindowOptions::schedule.
inline constexpr std::string_view kWindowSet = "WindowSystems";

class WindowModule final : public Orchestration::Module {
public:
    WindowModule() = default;

    // Reads WindowSettings::closeWhenRequested from the product description.
    void Configure(Orchestration::ProductSetup& setup) override;
    void Build(Orchestration::WorldSetup& setup) override;

private:
    bool closeWhenRequested_ = true;
};

} // namespace GYO::Engine::Window
