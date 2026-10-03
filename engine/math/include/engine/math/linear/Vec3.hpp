#pragma once

#include <cmath>
#include <type_traits>

#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

// Left-handed: +X right, +Y up, +Z forward. Model and world space use metres.
struct Vec3 final {
    float x{};
    float y{};
    float z{};
};

// 12 bytes with no padding keeps GPU vertex layouts that embed Vec3 stable.
static_assert(sizeof(Vec3) == 12 && alignof(Vec3) == 4);
static_assert(std::is_trivially_copyable_v<Vec3> && std::is_standard_layout_v<Vec3>);
static_assert(std::is_aggregate_v<Vec3>);

[[nodiscard]] inline Vec3 operator+(const Vec3 a, const Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
[[nodiscard]] inline Vec3 operator-(const Vec3 a, const Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
[[nodiscard]] inline Vec3 operator-(const Vec3 a) noexcept { return {-a.x, -a.y, -a.z}; }
[[nodiscard]] inline Vec3 operator*(const Vec3 a, const float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
[[nodiscard]] inline Vec3 operator*(const float s, const Vec3 a) noexcept { return {s * a.x, s * a.y, s * a.z}; }
[[nodiscard]] inline Vec3 operator/(const Vec3 a, const float s) noexcept { return {a.x / s, a.y / s, a.z / s}; }

[[nodiscard]] inline float Dot(const Vec3 a, const Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

[[nodiscard]] inline Vec3 Cross(const Vec3 a, const Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] inline float LengthSquared(const Vec3 a) noexcept { return a.x * a.x + a.y * a.y + a.z * a.z; }
// sqrt of the sum of squares, not hypot: overflows above ~1.8e19 per component.
[[nodiscard]] inline float Length(const Vec3 a) noexcept { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
[[nodiscard]] inline float Distance(const Vec3 a, const Vec3 b) noexcept { return Length(b - a); }

// Requires a non-zero length; a zero vector yields NaN components.
[[nodiscard]] inline Vec3 Normalize(const Vec3 a) noexcept {
    const float length = Length(a);
    return {a.x / length, a.y / length, a.z / length};
}

[[nodiscard]] inline Vec3 NormalizeOrZero(const Vec3 a) noexcept {
    const float length = Length(a);
    return length > 0.0F ? Vec3{a.x / length, a.y / length, a.z / length} : Vec3{};
}

[[nodiscard]] inline Vec3 Lerp(const Vec3 a, const Vec3 b, const float t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

[[nodiscard]] inline Vec3 Min(const Vec3 a, const Vec3 b) noexcept {
    return {Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z)};
}
[[nodiscard]] inline Vec3 Max(const Vec3 a, const Vec3 b) noexcept {
    return {Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z)};
}
[[nodiscard]] inline Vec3 Clamp(const Vec3 v, const Vec3 lo, const Vec3 hi) noexcept {
    return {Clamp(v.x, lo.x, hi.x), Clamp(v.y, lo.y, hi.y), Clamp(v.z, lo.z, hi.z)};
}

[[nodiscard]] inline bool IsFinite(const Vec3 a) noexcept {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

} // namespace Engine::Math
