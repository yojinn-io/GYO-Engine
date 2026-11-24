#pragma once

// world_ui (docs/architecture/target/v2/ui.zh-Hant.md, section 4.6): the World end of ui, in the World the UI lives
// in (source side; default=no). It registers ui's UiRoot, UiBindings, UiRootState (UiRoot brings it), the Message
// UiAction and the Resources UiInput and UiText, declares the public set kUiSet and mounts, in it:
//
// - MapUiInput (UiOptions::inputSchedule / inputStage, Main / PreUpdate, after input's kInputSet): input's Messages and
//   state -> UiInput, the default key mapping (Tab / Shift+Tab, arrows, Enter, Escape; while a field is edited the
//   edit keys, Ctrl or Command + A / V). Not mounted with UiOptions::defaultInputMapping off: the author writes
//   UiInput (AGENTS §1.10: a convenient default that does not bind the author).
// - UpdateUi (same stage, after MapUiInput): each root's runtime, its input in its camera's viewport, its actions as
//   UiAction (Bevy's UiSystems::Focus runs in PreUpdate too, so an author's Update reads this frame's actions).
// - ComposeUi (UiOptions::composeSchedule / composeStage, Main / PostUpdate, after camera's kUpdateSet): fonts into
//   UiText, each root's draw list (glyphs drawn into the atlas, as Bevy's text_system does in the main world), the
//   pictures requested, input's TextInputRequest while a field is edited.
//
// UiInput's pointer is in logical pixels of the window (as input's CursorPosition); UpdateUi turns it into each root's
// viewport pixels. A failure of a root goes into its UiRootState; no System returns an error for it (E2: that root,
// not the World, O-11).
//
// The World needs input, window, camera and world_asset (all carried by default).

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::WorldUi {

// WorldDescription::options; none given: the default skeleton.
struct UiOptions final {
    std::string inputSchedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string inputStage = std::string(Orchestration::DefaultSchedules::preUpdate);
    std::string composeSchedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string composeStage = std::string(Orchestration::DefaultSchedules::postUpdate);
    // MapUiInput writes UiInput from input's Messages; off: the author writes it.
    bool defaultInputMapping = true;
};

// The public set (Bevy's UiSystems): in both schedules.
inline constexpr std::string_view kUiSet = "UiSystems";

class WorldUiModule final : public Orchestration::Module {
public:
    WorldUiModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::WorldUi
