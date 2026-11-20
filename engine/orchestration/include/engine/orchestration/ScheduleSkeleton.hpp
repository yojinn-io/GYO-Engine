#pragma once

#include <string_view>

// The default schedule skeleton (time-events declaration
// docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 3.6;
// v8-arch-gyo2-orchestration-gives-schedule-skeleton; O-4). Before any unit's
// Build, the orchestration layer declares in every World one frame schedule and
// one fixed-step schedule, and the World's frame list runs the frame schedule.
// The orchestration layer gives the names and the order of the stages and runs
// the frame schedule through the kernel; it knows no time and nothing a unit
// mounts into them (a unit mounts its Systems while it registers; O-3).
// Genealogy: Bevy's bevy_app gives Main (First, PreUpdate, RunFixedMainLoop,
// Update, PostUpdate, Last) and FixedMain; flecs' pipeline gives its phases.

namespace GYO::Engine::Orchestration {

// WorldDescription::skeleton.
enum class ScheduleSkeleton : unsigned char {
    // DefaultSchedules::frame with its six stages and DefaultSchedules::fixed
    // with its five; the frame list starts with the frame schedule.
    Default,
    // Nothing is declared: the author declares its own schedules.
    Off,
};

// The names of the default skeleton. A stage name belongs to its schedule (the
// fixed-step schedule's stages carry no prefix).
struct DefaultSchedules final {
    static constexpr std::string_view frame = "Main";
    static constexpr std::string_view fixed = "FixedUpdate";
    static constexpr std::string_view first = "First";
    static constexpr std::string_view preUpdate = "PreUpdate";
    // Main only: where the fixed step is driven from (a unit mounts the driver here).
    static constexpr std::string_view fixedMainLoop = "FixedMainLoop";
    static constexpr std::string_view update = "Update";
    static constexpr std::string_view postUpdate = "PostUpdate";
    static constexpr std::string_view last = "Last";
};

} // namespace GYO::Engine::Orchestration
