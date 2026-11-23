#pragma once

// The game's screen as a state of the Engine's state machine Module: the
// current value is StateMachine<GameScreen>, a change is a request applied by
// the Module's apply System (right after Main/PreUpdate), so a screen asked for
// in one frame is the current one from the next frame on.

#include "object_connect/game/GameFlow.hpp"

#include "engine/state_machine/StateMachine.hpp"
#include "engine/state_machine/Traits.hpp"

#include <array>
#include <optional>
#include <string_view>

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<object_connect::GameScreen> {
    static constexpr std::string_view name = "GameScreen";
    static constexpr std::array values{
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::MainMenu, "MainMenu"},
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::LevelSelect, "LevelSelect"},
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::Playing, "Playing"},
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::Paused, "Paused"},
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::Solved, "Solved"},
        ValueName<object_connect::GameScreen>{object_connect::GameScreen::FinalResults, "FinalResults"},
    };
    static constexpr object_connect::GameScreen initial = object_connect::GameScreen::MainMenu;
};

namespace object_connect::world {

using ScreenState = GYO::Engine::StateMachine::StateMachine<GameScreen>;
using ScreenRequest = GYO::Engine::StateMachine::StateMachineRequest<GameScreen>;

// The screen the frame ends on: a request made earlier in this frame, or the
// current value. MainMenu before the first apply (the state's initial value).
[[nodiscard]] inline GameScreen EffectiveScreen(const ScreenState& state, const ScreenRequest& request) noexcept {
    if (const std::optional<GameScreen> pending = request.Pending()) {
        return *pending;
    }
    return state.Current().value_or(GameScreen::MainMenu);
}

} // namespace object_connect::world
