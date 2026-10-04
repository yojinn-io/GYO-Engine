#pragma once

#include <type_traits>

namespace Engine::Math {

// Function bodies are the single GYO implementation of each operation. Any
// change to an expression's shape (operand order, association) can change
// float results, so treat edits as behaviour changes. GYO builds with
// floating-point contraction off (build/cmake/GyoBuild.cmake); code compiled
// with contraction on may fuse a multiply and add, at run time or when an
// optimizer constant-folds, and then differs from GYO builds.

// The scalar types Min, Max and Clamp accept: integers and floating point,
// not bool.
template <class T>
concept Arithmetic = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

// GYO's min, max and clamp for every arithmetic type, with exactly the
// std::min, std::max and std::clamp results (the comparisons below are the
// standard's definitions, so ties and NaN behave the same) but without
// depending on <algorithm> or platform min/max macros. All arguments must
// have the same type; a mixed call does not compile rather than converting
// silently, so cast explicitly where the types differ.
template <Arithmetic T>
[[nodiscard]] constexpr T Min(const T a, const T b) noexcept {
    return b < a ? b : a;
}

template <Arithmetic T>
[[nodiscard]] constexpr T Max(const T a, const T b) noexcept {
    return a < b ? b : a;
}

// Requires lo <= hi. A NaN value is returned unchanged.
template <Arithmetic T>
[[nodiscard]] constexpr T Clamp(const T value, const T lo, const T hi) noexcept {
    return value < lo ? lo : hi < value ? hi : value;
}

// Unclamped: t outside [0, 1] extrapolates. Not std::lerp, whose rounding differs.
[[nodiscard]] inline float Lerp(const float a, const float b, const float t) noexcept {
    return a + (b - a) * t;
}

} // namespace Engine::Math
