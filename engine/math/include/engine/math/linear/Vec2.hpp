#pragma once

#include <cmath>
#include <type_traits>

#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

// GYO uses one vector type per dimension for points, directions and sizes;
// TransformPoint/TransformVector express the point/direction distinction.
struct Vec2 final {
    float x{};
    float y{};
};

static_assert(sizeof(Vec2) == 8 && alignof(Vec2) == 4);
static_assert(std::is_trivially_copyable_v<Vec2> && std::is_standard_layout_v<Vec2>);
static_assert(std::is_aggregate_v<Vec2>);

[[nodiscard]] inline Vec2 operator+(const Vec2 a, const Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] inline Vec2 operator-(const Vec2 a, const Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] inline Vec2 operator-(const Vec2 a) noexcept { return {-a.x, -a.y}; }
[[nodiscard]] inline Vec2 operator*(const Vec2 a, const float s) noexcept { return {a.x * s, a.y * s}; }
[[nodiscard]] inline Vec2 operator*(const float s, const Vec2 a) noexcept { return {s * a.x, s * a.y}; }
[[nodiscard]] inline Vec2 operator/(const Vec2 a, const float s) noexcept { return {a.x / s, a.y / s}; }

[[nodiscard]] inline float Dot(const Vec2 a, const Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }
[[nodiscard]] inline float LengthSquared(const Vec2 a) noexcept { return a.x * a.x + a.y * a.y; }
[[nodiscard]] inline float Length(const Vec2 a) noexcept { return std::sqrt(a.x * a.x + a.y * a.y); }
[[nodiscard]] inline float Distance(const Vec2 a, const Vec2 b) noexcept { return Length(b - a); }

// Requires a non-zero length; a zero vector yields NaN components.
[[nodiscard]] inline Vec2 Normalize(const Vec2 a) noexcept {
    const float length = Length(a);
    return {a.x / length, a.y / length};
}

[[nodiscard]] inline Vec2 NormalizeOrZero(const Vec2 a) noexcept {
    const float length = Length(a);
    return length > 0.0F ? Vec2{a.x / length, a.y / length} : Vec2{};
}

[[nodiscard]] inline Vec2 Lerp(const Vec2 a, const Vec2 b, const float t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

[[nodiscard]] inline Vec2 Min(const Vec2 a, const Vec2 b) noexcept { return {Min(a.x, b.x), Min(a.y, b.y)}; }
[[nodiscard]] inline Vec2 Max(const Vec2 a, const Vec2 b) noexcept { return {Max(a.x, b.x), Max(a.y, b.y)}; }
[[nodiscard]] inline Vec2 Clamp(const Vec2 v, const Vec2 lo, const Vec2 hi) noexcept {
    return {Clamp(v.x, lo.x, hi.x), Clamp(v.y, lo.y, hi.y)};
}

[[nodiscard]] inline bool IsFinite(const Vec2 a) noexcept { return std::isfinite(a.x) && std::isfinite(a.y); }

} // namespace Engine::Math
