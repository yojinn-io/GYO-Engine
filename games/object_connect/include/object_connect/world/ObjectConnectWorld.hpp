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

#include "engine/orchestration/Setup.hpp"

namespace object_connect::world {

void BuildWorld(GYO::Engine::Orchestration::WorldSetup& setup);

} // namespace object_connect::world
