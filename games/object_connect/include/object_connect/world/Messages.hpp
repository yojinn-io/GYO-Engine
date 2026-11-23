#pragma once

// The game's Messages: what the menu decided in a frame (read by the Systems
// after it), and what the audio and the program's loop act on.

#include "object_connect/game/GameFlow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace object_connect::world {

// What the menu decided this frame: sent once per frame by StepMenu.
struct MenuStepped final {
    GameCommand command = GameCommand::None;
    std::optional<std::size_t> puzzleIndex;
    // The level is played this frame (the screen was Playing and stays so).
    bool simulatePuzzle = false;
};

// The player took hold of a vessel (the drag started).
struct LineHoldStarted final {};

enum class LineHoldEnd : std::uint8_t {
    Released,
    Cancelled,
};

// The drag ended: released by the player, or cancelled (pause, focus lost, leaving the level).
struct LineHoldEnded final {
    LineHoldEnd reason = LineHoldEnd::Released;
};

// The music sequence starts over (back at the main menu after a level was entered).
struct MusicRestarted final {};

// The player chose to quit the game.
struct QuitChosen final {};

} // namespace object_connect::world
