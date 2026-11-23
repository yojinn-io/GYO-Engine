#pragma once

// The input Module (docs/architecture/target/v2/input.zh-Hant.md; Design Invariants IN-1 to IN-5). Carried by default
// (config/modules.csv): without a backend it is empty data (as Bevy's InputPlugin, which DefaultPlugins always add).
// In every World it joins it registers the Messages and the state of Input.hpp, declares the public set kInputSet
// and mounts UpdateInputState in it (Main/PreUpdate by default; Bevy's InputSystems in PreUpdate,
// bevy_input/src/lib.rs:106, :117).
//
// Whoever writes the input Messages (plugins/sdl_window, a test, a network injection) mounts its writer in the same
// stage with `before` kInputSet (v8-arch-gyo2-sdl-input-writer-preupdate). Without it the kernel orders the two by
// registration and the Modules come first: the state would lag a frame without any error. A System that reads the
// state runs after kInputSet (Update, or `after` it).
//
// Its duties (IN-1): register the input Messages and state; keep the state from the Messages. Not: talking to the OS
// (a backend's), mapping keys to actions (the author's or a later Module's; Bevy keeps that out of bevy_input too),
// deciding the game from raw input (IN-5).

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Input {

// Where UpdateInputState runs (WorldDescription::options; none given: the default skeleton). A World with the
// skeleton off names its own (O-4).
struct InputOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::preUpdate);
};

// The public set (Bevy's InputSystems), in InputOptions::schedule.
inline constexpr std::string_view kInputSet = "InputSystems";

class InputModule final : public Orchestration::Module {
public:
    InputModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::Input
