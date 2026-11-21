#pragma once

#include <cmath>
#include <limits>
#include <optional>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/geometry/Sphere.hpp"
#include "engine/math/geometry/Triangle.hpp"

namespace Engine::Math {

// Pure geometric ray intersections. Each returns the smallest t >= 0 at which
// the ray meets the primitive (t in units of |direction|, see Ray), or nullopt.
// An origin inside a solid primitive returns 0. There is no tolerance: callers
// that need skin widths, sweeps or contact data use the Collision module.
// direction must be non-zero. A ray with a non-finite origin or direction, or a
// computation that overflows to NaN, returns nullopt.

// A ray parallel to the plane never hits it, even when it lies in the plane.
[[nodiscard]] inline std::optional<float> Intersect(const Ray& ray, const Plane& plane) noexcept {
    if (!IsFinite(ray.origin) || !IsFinite(ray.direction)) return std::nullopt;
    const float denominator = Dot(plane.normal, ray.direction);
    if (denominator == 0.0F) return std::nullopt;
    const float t = (plane.distance - Dot(plane.normal, ray.origin)) / denominator;
    if (!(t >= 0.0F)) return std::nullopt;
    return t;
}

[[nodiscard]] inline std::optional<float> Intersect(const Ray& ray, const Sphere& sphere) noexcept {
    if (!IsFinite(ray.origin) || !IsFinite(ray.direction)) return std::nullopt;
    const Vec3 offset = ray.origin - sphere.center;
    const float radiusSquared = sphere.radius * sphere.radius;
    const float offsetSquared = Dot(offset, offset);
    if (!std::isfinite(offsetSquared)) return std::nullopt;
    if (offsetSquared <= radiusSquared) return 0.0F;
    const float a = Dot(ray.direction, ray.direction);
    const float halfB = Dot(offset, ray.direction);
    if (halfB >= 0.0F) return std::nullopt;
    // halfB^2 - a * (|offset|^2 - r^2) rewritten with Lagrange's identity: the
    // direct form cancels catastrophically when the origin is far away.
    const Vec3 perpendicular = Cross(offset, ray.direction);
    const float discriminant = a * radiusSquared - Dot(perpendicular, perpendicular);
    if (!(discriminant >= 0.0F)) return std::nullopt;
    const float t = (-halfB - std::sqrt(discriminant)) / a;
    if (!(t >= 0.0F)) return std::nullopt;
    return t;
}

// Slab test. Faces are closed: a ray grazing a face hits it.
[[nodiscard]] inline std::optional<float> Intersect(const Ray& ray, const Aabb& box) noexcept {
    if (!IsFinite(ray.origin) || !IsFinite(ray.direction)) return std::nullopt;
    const float origin[3]{ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3]{ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimum[3]{box.minimum.x, box.minimum.y, box.minimum.z};
    const float maximum[3]{box.maximum.x, box.maximum.y, box.maximum.z};
    float enter = 0.0F;
    float exit = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0.0F) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return std::nullopt;
            continue;
        }
        const float inverse = 1.0F / direction[axis];
        // Not named near/far: <windows.h> defines those as macros.
        float entry = (minimum[axis] - origin[axis]) * inverse;
        float leave = (maximum[axis] - origin[axis]) * inverse;
        if (entry > leave) {
            const float swap = entry;
            entry = leave;
            leave = swap;
        }
        enter = Max(enter, entry);
        exit = Min(exit, leave);
        if (enter > exit) return std::nullopt;
    }
    return enter;
}

// Möller-Trumbore, two-sided (Triangle defines no front face). Edges are
// closed. A ray parallel to the triangle's plane never hits it.
[[nodiscard]] inline std::optional<float> Intersect(const Ray& ray, const Triangle& triangle) noexcept {
    if (!IsFinite(ray.origin) || !IsFinite(ray.direction)) return std::nullopt;
    const Vec3 edge1 = triangle.b - triangle.a;
    const Vec3 edge2 = triangle.c - triangle.a;
    const Vec3 p = Cross(ray.direction, edge2);
    const float determinant = Dot(edge1, p);
    if (determinant == 0.0F) return std::nullopt;
    const float inverse = 1.0F / determinant;
    const Vec3 s = ray.origin - triangle.a;
    const float u = Dot(s, p) * inverse;
    if (u < 0.0F || u > 1.0F) return std::nullopt;
    const Vec3 q = Cross(s, edge1);
    const float v = Dot(ray.direction, q) * inverse;
    if (v < 0.0F || u + v > 1.0F) return std::nullopt;
    const float t = Dot(edge2, q) * inverse;
    if (!(t >= 0.0F)) return std::nullopt;
    return t;
}

} // namespace Engine::Math
