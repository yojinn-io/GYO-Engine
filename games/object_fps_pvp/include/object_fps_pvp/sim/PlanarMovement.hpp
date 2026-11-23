#pragma once

#include "object_fps_pvp/sim/GroundPoint.hpp"

namespace object_fps_pvp::sim {

// The XZ direction of a relative movement input: never longer than 1, +Z when
// the yaw is 0 (yaw turns +Z toward +X). The pitch is part of the view but
// never changes the planar movement.
[[nodiscard]] GroundPoint ComputePlanarInput(
    float forwardAxis,
    float rightAxis,
    float yawRadians,
    float pitchRadians = 0.0f) noexcept;

// Pure gameplay rule: normalized relative input becomes a world-space step.
// Callers validate finite axes, yaw, non-negative speed and elapsed time.
[[nodiscard]] GroundPoint ComputePlanarDisplacement(
    float forwardAxis, float rightAxis, float yawRadians,
    float movementSpeed, float deltaSeconds) noexcept;

} // namespace object_fps_pvp::sim
