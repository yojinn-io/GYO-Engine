#pragma once

#include <algorithm>

namespace Engine::Math {

// Function bodies are the single GYO implementation of each operation. Any
// change to an expression's shape can change float results (compilers may fuse
// a multiply and add within one expression), so treat edits as behaviour
// changes. Bit-identical results are only guaranteed between computations made
// in the same contraction mode and evaluated at run time: an optimizing clang
// may constant-fold a contractible expression with fused rounding when its
// inputs are compile-time constants, even on targets without FMA.

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
