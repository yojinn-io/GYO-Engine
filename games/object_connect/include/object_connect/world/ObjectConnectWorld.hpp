#pragma once

// Registers the game in one World: the screen state, the Resources, the
// Messages and the Systems of one frame (Main/Update, in this order):
//
//   CancelDragOnLeave  Esc or focus lost while playing drops the drag
//   StepMenu           the menu (GameFlow) on this frame's input; MenuStepped
//   ApplyMenuCommand   start, retry, next level, leave, quit
//   SimulateBoard      the level on this frame's pointer and time; solved
//   TickSolvedTimer    the delay before the Solved menu takes input
//
// The World needs the time Module (ActiveTime); the frame's delta is
// ActiveTime::delta. The Catalog starts empty: the product (or a test) puts the
// levels in before the first frame.

#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/Setup.hpp"

#include <vector>

namespace object_connect::world {

struct Catalog;
struct Session;

struct WorldOptions final {
    // Run conditions put on every System above (the product's: AfterGenerating(), so the game waits for its data).
    std::vector<GYO::Kernel::ConditionRef> conditions;
};

void BuildWorld(GYO::Engine::Orchestration::WorldSetup& setup, WorldOptions options = {});

// The level being played has a next level (the Solved menu's "next stage").
[[nodiscard]] bool HasNextPuzzle(const Catalog& catalog, const Session& session) noexcept;

} // namespace object_connect::world
