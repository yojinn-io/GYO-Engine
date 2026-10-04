#pragma once

#include <cmath>
#include <cstddef>
#include <type_traits>

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

// xyzw order; the default is the identity rotation. Rotations follow the
// left-handed basis: a positive angle about +Y turns +Z toward +X.
struct Quaternion final {
    float x{};
    float y{};
    float z{};
    float w{1.0F};
};

static_assert(sizeof(Quaternion) == 16 && alignof(Quaternion) == 4);
static_assert(offsetof(Quaternion, x) == 0 && offsetof(Quaternion, w) == 12);
static_assert(std::is_trivially_copyable_v<Quaternion> && std::is_standard_layout_v<Quaternion>);
static_assert(std::is_aggregate_v<Quaternion>);

[[nodiscard]] inline float Dot(const Quaternion a, const Quaternion b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] inline float LengthSquared(const Quaternion q) noexcept {
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
}

// Requires a non-zero length; a zero quaternion yields NaN components.
[[nodiscard]] inline Quaternion Normalize(const Quaternion q) noexcept {
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return {q.x / length, q.y / length, q.z / length, q.w / length};
}

// Degenerate input (length <= 1e-12) becomes the identity rotation. A length
// that overflows to infinity yields a zero quaternion; NaN input stays NaN.
[[nodiscard]] inline Quaternion NormalizeOrIdentity(const Quaternion q) noexcept {
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length <= 1.0e-12F) return {};
    return {q.x / length, q.y / length, q.z / length, q.w / length};
}

[[nodiscard]] inline Quaternion Conjugate(const Quaternion q) noexcept { return {-q.x, -q.y, -q.z, q.w}; }

// Inverse of any non-zero quaternion; equals Conjugate for unit quaternions.
[[nodiscard]] inline Quaternion Inverse(const Quaternion q) noexcept {
    const float lengthSquared = LengthSquared(q);
    return {-q.x / lengthSquared, -q.y / lengthSquared, -q.z / lengthSquared, q.w / lengthSquared};
}

// Hamilton product, not normalized: Multiply(a, b) applies b first, then a.
[[nodiscard]] inline Quaternion Multiply(const Quaternion a, const Quaternion b) noexcept {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// axis must be unit length.
[[nodiscard]] inline Quaternion MakeQuaternionFromAxisAngle(const Vec3 axis, const float radians) noexcept {
    const float half = radians * 0.5F;
    const float sine = std::sin(half);
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(half)};
}

// Rotates v by a unit quaternion; matches MakeRotation(q) applied to v.
[[nodiscard]] inline Vec3 Rotate(const Quaternion q, const Vec3 v) noexcept {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = Cross(u, v) * 2.0F;
    return v + t * q.w + Cross(u, t);
}

// Shortest-path spherical interpolation. Inputs are normalized first
// (degenerate inputs become identity); nearly parallel inputs fall back to a
// normalized linear blend.
[[nodiscard]] inline Quaternion Slerp(Quaternion a, Quaternion b, const float t) noexcept {
    a = NormalizeOrIdentity(a);
    b = NormalizeOrIdentity(b);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    dot = Clamp(dot, -1.0F, 1.0F);
    float left = 1 - t;
    float right = t;
    if (dot < 0.9995F) {
        const float angle = std::acos(dot);
        const float divisor = std::sin(angle);
        left = std::sin((1 - t) * angle) / divisor;
        right = std::sin(t * angle) / divisor;
    }
    return NormalizeOrIdentity({a.x * left + b.x * right, a.y * left + b.y * right,
                                a.z * left + b.z * right, a.w * left + b.w * right});
}

[[nodiscard]] inline bool IsFinite(const Quaternion q) noexcept {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}

} // namespace Engine::Math
