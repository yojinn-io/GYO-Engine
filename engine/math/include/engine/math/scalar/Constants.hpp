#pragma once

#include <numbers>

namespace Engine::Math {

// Angles are radians throughout GYO.
inline constexpr float Pi = std::numbers::pi_v<float>;
// Computed in float so it matches the literal 2 * pi_v<float> used by callers.
inline constexpr float TwoPi = 2.0F * Pi;
inline constexpr float HalfPi = 0.5F * Pi;

} // namespace Engine::Math
