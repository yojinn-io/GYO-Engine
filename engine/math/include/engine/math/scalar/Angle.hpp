#pragma once

#include <cmath>

#include "engine/math/scalar/Constants.hpp"

namespace Engine::Math {

// A single correctly rounded multiplication, so constant evaluation and run
// time give the same bits; constexpr lets products declare angle constants.
[[nodiscard]] constexpr float DegreesToRadians(const float degrees) noexcept {
    return degrees * (Pi / 180.0F);
}

[[nodiscard]] constexpr float RadiansToDegrees(const float radians) noexcept {
    return radians * (180.0F / Pi);
}

// Wraps into [-Pi, Pi] using the IEEE remainder. Infinite or NaN input yields NaN.
[[nodiscard]] inline float WrapRadians(const float radians) noexcept {
    return std::remainder(radians, TwoPi);
}

// Interpolates along the shorter arc from a to b; the result is wrapped.
[[nodiscard]] inline float LerpRadiansShortest(const float a, const float b, const float t) noexcept {
    return std::remainder(a + std::remainder(b - a, TwoPi) * t, TwoPi);
}

} // namespace Engine::Math
