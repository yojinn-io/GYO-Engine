#pragma once

#include <type_traits>

#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// Points p with Dot(normal, p) == distance. normal must be unit length; the
// Make* constructors normalize. The positive side is the one normal points to.
struct Plane final {
    Vec3 normal{0.0F, 1.0F, 0.0F};
    float distance{};
};

static_assert(sizeof(Plane) == 16 && alignof(Plane) == 4);
static_assert(std::is_trivially_copyable_v<Plane> && std::is_standard_layout_v<Plane>);
static_assert(std::is_aggregate_v<Plane>);

// normal need not be unit length but must be non-zero.
[[nodiscard]] inline Plane MakePlane(const Vec3 point, const Vec3 normal) noexcept {
    const Vec3 unit = Normalize(normal);
    return {unit, Dot(unit, point)};
}

// Normal follows the triangle convention: Cross(b - a, c - a). Collinear
// points have no plane and yield NaN.
[[nodiscard]] inline Plane MakePlane(const Vec3 a, const Vec3 b, const Vec3 c) noexcept {
    return MakePlane(a, Cross(b - a, c - a));
}

// Positive on the side normal points to.
[[nodiscard]] inline float SignedDistance(const Plane& plane, const Vec3 point) noexcept {
    return Dot(plane.normal, point) - plane.distance;
}

[[nodiscard]] inline Vec3 ClosestPoint(const Vec3 point, const Plane& plane) noexcept {
    return point - plane.normal * SignedDistance(plane, point);
}

} // namespace Engine::Math
