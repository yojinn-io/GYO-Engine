#pragma once

#include <cmath>

#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

// sRGB transfer functions (IEC 61966-2-1) on one channel. Inputs are clamped
// to [0, 1] (NaN passes through as NaN); alpha is never encoded.
[[nodiscard]] inline float DecodeSrgb(const float encoded) noexcept {
    const float value = Clamp(encoded, 0.0F, 1.0F);
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] inline float EncodeSrgb(const float linear) noexcept {
    const float value = Clamp(linear, 0.0F, 1.0F);
    return value <= 0.0031308F
        ? value * 12.92F
        : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

} // namespace Engine::Math
