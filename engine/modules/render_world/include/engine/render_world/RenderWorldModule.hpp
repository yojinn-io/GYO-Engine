#pragma once

// render_world (docs/architecture/target/v2/render.zh-Hant.md, section 5; SDL declaration section 3.3; Design
// Invariants RD-2, RD-3): the core of a World that draws, Bevy's RenderApp (bevy_render/src/lib.rs). It joins the
// World that draws (renderer side; default=no) and:
//
// - registers render's RenderCommands, RenderTextures, RenderMeshes and RenderImages (empty in a new World) and
//   Presentation (the default in a new World; a value carried over stays);
// - inserts two stages after the frame schedule's last stage: kSubmitStage, where a backend draws and shows the
//   frame (sdl_gpu's GPUSubmit), and kCleanupStage, where ClearRenderFrame empties the passes and the asks for the next frame (Bevy's
//   RenderSystems::Render and Cleanup). A producer writes RenderCommands and asks for textures anywhere before
//   kSubmitStage, also in a schedule that runs before the frame schedule.
//
// - in a World with extractFrom (O-12), declares the extract schedule kExtractSchedule (one stage of the same name)
//   and puts it in the frame list before RenderWorldOptions::schedule (SDL declaration section 6.1 point 3, Bevy's
//   ExtractSchedule run by SubApp::extract before the render schedule): camera_render and sprite_render mount their
//   extract Systems there. A World without extractFrom gets none (a World that draws what its own Systems write, as
//   batch R2a's tests; an extract schedule there would be O-E9).
//
// Without a backend the data is made and emptied each frame and nobody draws it (SDL declaration section 9.2 point 3;
// Q4).

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::RenderWorld {

// The stages render_world inserts, in RenderWorldOptions::schedule, in this order after RenderWorldOptions::after.
inline constexpr std::string_view kSubmitStage = "Submit";
inline constexpr std::string_view kCleanupStage = "Cleanup";
// The extract schedule and its one stage (a World with extractFrom only).
inline constexpr std::string_view kExtractSchedule = "Extract";

// WorldDescription::options; none given: the default skeleton.
struct RenderWorldOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    // The stage the two stages follow (the schedule's last).
    std::string after = std::string(Orchestration::DefaultSchedules::last);
};

class RenderWorldModule final : public Orchestration::Module {
public:
    RenderWorldModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::RenderWorld
