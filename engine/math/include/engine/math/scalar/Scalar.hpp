#pragma once

#include <algorithm>

namespace Engine::Math {

// Function bodies are the single GYO implementation of each operation. Any
// change to an expression's shape (operand order, association) can change
// float results, so treat edits as behaviour changes. GYO builds with
// floating-point contraction off (build/cmake/GyoBuild.cmake); code compiled
// with contraction on may fuse a multiply and add, at run time or when an
// optimizer constant-folds, and then differs from GYO builds.

[[nodiscard]] inline float Min(const float a, const float b) noexcept { return (std::min)(a, b); }
[[nodiscard]] inline float Max(const float a, const float b) noexcept { return (std::max)(a, b); }

// Requires lo <= hi. NaN input is returned unchanged.
[[nodiscard]] inline float Clamp(const float value, const float lo, const float hi) noexcept {
    return std::clamp(value, lo, hi);
}

// Unclamped: t outside [0, 1] extrapolates. Not std::lerp, whose rounding differs.
[[nodiscard]] inline float Lerp(const float a, const float b, const float t) noexcept {
    return a + (b - a) * t;
}

} // namespace Engine::Math
