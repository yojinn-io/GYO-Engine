#pragma once

#include <cmath>
#include <type_traits>

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

struct Vec4 final {
    float x{};
    float y{};
    float z{};
    float w{};
};

static_assert(sizeof(Vec4) == 16 && alignof(Vec4) == 4);
static_assert(std::is_trivially_copyable_v<Vec4> && std::is_standard_layout_v<Vec4>);
static_assert(std::is_aggregate_v<Vec4>);

[[nodiscard]] inline Vec4 operator+(const Vec4 a, const Vec4 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
[[nodiscard]] inline Vec4 operator-(const Vec4 a, const Vec4 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
[[nodiscard]] inline Vec4 operator-(const Vec4 a) noexcept { return {-a.x, -a.y, -a.z, -a.w}; }
[[nodiscard]] inline Vec4 operator*(const Vec4 a, const float s) noexcept { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
[[nodiscard]] inline Vec4 operator*(const float s, const Vec4 a) noexcept { return {s * a.x, s * a.y, s * a.z, s * a.w}; }
[[nodiscard]] inline Vec4 operator/(const Vec4 a, const float s) noexcept { return {a.x / s, a.y / s, a.z / s, a.w / s}; }

[[nodiscard]] inline float Dot(const Vec4 a, const Vec4 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
[[nodiscard]] inline float LengthSquared(const Vec4 a) noexcept { return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w; }
[[nodiscard]] inline float Length(const Vec4 a) noexcept {
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}
[[nodiscard]] inline float Distance(const Vec4 a, const Vec4 b) noexcept { return Length(b - a); }

// Requires a non-zero length; a zero vector yields NaN components.
[[nodiscard]] inline Vec4 Normalize(const Vec4 a) noexcept {
    const float length = Length(a);
    return {a.x / length, a.y / length, a.z / length, a.w / length};
}

[[nodiscard]] inline Vec4 NormalizeOrZero(const Vec4 a) noexcept {
    const float length = Length(a);
    return length > 0.0F ? Vec4{a.x / length, a.y / length, a.z / length, a.w / length} : Vec4{};
}

[[nodiscard]] inline Vec4 Lerp(const Vec4 a, const Vec4 b, const float t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

[[nodiscard]] inline Vec4 Min(const Vec4 a, const Vec4 b) noexcept {
    return {Min(a.x, b.x), Min(a.y, b.y), Min(a.z, b.z), Min(a.w, b.w)};
}
[[nodiscard]] inline Vec4 Max(const Vec4 a, const Vec4 b) noexcept {
    return {Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z), Max(a.w, b.w)};
}
[[nodiscard]] inline Vec4 Clamp(const Vec4 v, const Vec4 lo, const Vec4 hi) noexcept {
    return {Clamp(v.x, lo.x, hi.x), Clamp(v.y, lo.y, hi.y), Clamp(v.z, lo.z, hi.z), Clamp(v.w, lo.w, hi.w)};
}

[[nodiscard]] inline bool IsFinite(const Vec4 a) noexcept {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) && std::isfinite(a.w);
}

// Homogeneous helpers: a point carries w = 1, a direction w = 0.
[[nodiscard]] inline Vec4 MakePoint4(const Vec3 p) noexcept { return {p.x, p.y, p.z, 1.0F}; }
[[nodiscard]] inline Vec4 MakeDirection4(const Vec3 d) noexcept { return {d.x, d.y, d.z, 0.0F}; }
[[nodiscard]] inline Vec3 XYZ(const Vec4 a) noexcept { return {a.x, a.y, a.z}; }

} // namespace Engine::Math
