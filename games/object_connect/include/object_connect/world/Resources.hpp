#pragma once

// The game's Resources in its World. The input and the screen drawing are not
// here yet: FrameInput is written by whoever has the window (the input and UI
// Systems that come with the window and render batches; a test writes it
// directly), and the renderer will read Session and RunResults.

#include "object_connect/data/PuzzleData.hpp"
#include "object_connect/game/FinalResults.hpp"
#include "object_connect/game/GameFlow.hpp"
#include "object_connect/game/MusicSequencePolicy.hpp"
#include "object_connect/puzzle/PuzzleBoard.hpp"

#include <cstddef>
#include <optional>

namespace object_connect::world {

// The levels, in level-select order.
struct Catalog final {
    PuzzleCatalog puzzles;
};

// The playfield in game coordinates: the original's fixed 1280 x 720 back
// buffer. Boards are built inside it (wrapEdges levels wrap at its edges).
struct Playfield final {
    AxisAlignedBox bounds{{0.0f, 0.0f}, {1280.0f, 720.0f}};
};

// One frame of the player's intent, already in game terms: the menu keys and
// the item under the pointer (the UI hit test and the page arrows decide
// hoveredItem and drop the keys a page turn used, as the original Game did
// before GameFlow), and the pointer in playfield coordinates. Overwritten every
// frame by its writer; read, never changed, by the game's Systems.
struct FrameInput final {
    GameFlowInput menu{};
    BoardPointerInput pointer{};
};

// The item the menu cursor is on. Reset to 0 whenever the screen changes.
struct MenuSelection final {
    std::size_t item = 0;
};

// The level being played, if any.
struct Session final {
    std::optional<PuzzleBoard> board;
    std::optional<std::size_t> puzzleIndex;
    // Time on the Solved screen; its menu takes input from kSolvedMenuDelaySeconds on.
    float solvedElapsedSeconds = 0.0f;
    // The current level's result was put in RunResults (a retry takes it out again).
    bool resultRecorded = false;
    MusicSequencePolicy music;
};

// The levels cleared in this run, and the summary the FinalResults screen shows.
struct RunResults final {
    RunCompletionTracker tracker;
    FinalResultsSummary summary;
};

inline constexpr float kSolvedMenuDelaySeconds = 0.6f;

} // namespace object_connect::world
