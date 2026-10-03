#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// Double precision for algorithms that must keep accuracy over short sweeps
// and long rays. Only the operations those algorithms need are provided.
struct Vec3d final {
    double x{};
    double y{};
    double z{};
};

static_assert(sizeof(Vec3d) == 24 && alignof(Vec3d) == alignof(double));
static_assert(std::is_trivially_copyable_v<Vec3d> && std::is_standard_layout_v<Vec3d>);
static_assert(std::is_aggregate_v<Vec3d>);

[[nodiscard]] inline Vec3d ToVec3d(const Vec3 v) noexcept { return {v.x, v.y, v.z}; }
[[nodiscard]] inline Vec3 ToVec3(const Vec3d v) noexcept {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

[[nodiscard]] inline Vec3d operator+(const Vec3d a, const Vec3d b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
[[nodiscard]] inline Vec3d operator-(const Vec3d a, const Vec3d b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
[[nodiscard]] inline Vec3d operator-(const Vec3d a) noexcept { return {-a.x, -a.y, -a.z}; }
[[nodiscard]] inline Vec3d operator*(const Vec3d a, const double s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
[[nodiscard]] inline Vec3d operator*(const double s, const Vec3d a) noexcept { return {s * a.x, s * a.y, s * a.z}; }
[[nodiscard]] inline Vec3d operator/(const Vec3d a, const double s) noexcept { return {a.x / s, a.y / s, a.z / s}; }

[[nodiscard]] inline double Dot(const Vec3d a, const Vec3d b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[nodiscard]] inline Vec3d Cross(const Vec3d a, const Vec3d b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] inline double LengthSquared(const Vec3d a) noexcept { return a.x * a.x + a.y * a.y + a.z * a.z; }
[[nodiscard]] inline double Length(const Vec3d a) noexcept { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

[[nodiscard]] inline Vec3d Clamp(const Vec3d v, const Vec3d lo, const Vec3d hi) noexcept {
    return {std::clamp(v.x, lo.x, hi.x), std::clamp(v.y, lo.y, hi.y), std::clamp(v.z, lo.z, hi.z)};
}

[[nodiscard]] inline bool IsFinite(const Vec3d a) noexcept {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}

} // namespace Engine::Math
