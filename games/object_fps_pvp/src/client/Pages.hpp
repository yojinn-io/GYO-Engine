#pragma once

// The pages inside one scene (game-flow declaration 3.7): a state of the Engine's state_machine Module per scene that
// has more than one page (Bevy's States). Each value has its enter and exit schedule: entering a page puts its UI root,
// leaving takes it away; the Systems of a page run on InState<page>; a page asks for another with a request. Changing
// scene is not a page: it replaces the World (Scenes.hpp).

#include "engine/state_machine/Conditions.hpp"
#include "engine/state_machine/StateMachine.hpp"
#include "engine/state_machine/Traits.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace object_fps_pvp::client {

// The main menu's pages: the menu, the settings.
enum class MenuPage : std::uint8_t { Main, Settings };
// The game scene's overlay: the loading screen until ready and started, then nothing (the game), or the pause while the
// pointer is the player's.
enum class GamePage : std::uint8_t { Loading, Playing, Paused };

} // namespace object_fps_pvp::client

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<object_fps_pvp::client::MenuPage> {
    static constexpr std::string_view name = "MenuPage";
    static constexpr std::array values{ValueName<object_fps_pvp::client::MenuPage>{object_fps_pvp::client::MenuPage::Main, "Main"},
                                       ValueName<object_fps_pvp::client::MenuPage>{object_fps_pvp::client::MenuPage::Settings, "Settings"}};
    static constexpr object_fps_pvp::client::MenuPage initial = object_fps_pvp::client::MenuPage::Main;
};

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<object_fps_pvp::client::GamePage> {
    static constexpr std::string_view name = "GamePage";
    static constexpr std::array values{ValueName<object_fps_pvp::client::GamePage>{object_fps_pvp::client::GamePage::Loading, "Loading"},
                                       ValueName<object_fps_pvp::client::GamePage>{object_fps_pvp::client::GamePage::Playing, "Playing"},
                                       ValueName<object_fps_pvp::client::GamePage>{object_fps_pvp::client::GamePage::Paused, "Paused"}};
    static constexpr object_fps_pvp::client::GamePage initial = object_fps_pvp::client::GamePage::Loading;
};

namespace object_fps_pvp::client {

using MenuPageState = GYO::Engine::StateMachine::StateMachine<MenuPage>;
using MenuPageRequest = GYO::Engine::StateMachine::StateMachineRequest<MenuPage>;
using GamePageState = GYO::Engine::StateMachine::StateMachine<GamePage>;
using GamePageRequest = GYO::Engine::StateMachine::StateMachineRequest<GamePage>;

} // namespace object_fps_pvp::client
