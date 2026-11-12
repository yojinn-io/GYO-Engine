#pragma once

#include "RetroFPS/Sim/GroundPoint.hpp"

namespace fps::pvp {
inline namespace sim {

// 長さが1を超えないXZ移動ベクトルを返す。ヨーが0なら+Z方向を向く。
// ピッチは視線方向の一部として受け取るが、XZ移動には影響させない。
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

} // namespace sim
} // namespace fps::pvp
