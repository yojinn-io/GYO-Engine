#pragma once

#include <cstdint>

namespace object_fps_pvp::sim {

// Whether a player is alive in the arena or waits for its respawn.
enum class LifeState : std::uint8_t { Alive = 0, Dead = 1 };

} // namespace object_fps_pvp::sim
