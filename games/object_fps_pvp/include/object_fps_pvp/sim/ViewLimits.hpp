#pragma once

#include "engine/math/scalar/Angle.hpp"

namespace object_fps_pvp::sim {

// The view never pitches further up or down than this, so it never looks
// straight up or down (where the yaw would lose its meaning).
inline constexpr float MaximumPitchRadians = GYO::Engine::Math::DegreesToRadians(89.0F);

} // namespace object_fps_pvp::sim
