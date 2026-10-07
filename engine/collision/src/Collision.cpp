#include "engine/collision/Collision.hpp"

#include "engine/base/Assert.hpp"
#include "engine/math/linear/Vec3d.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Engine::Collision {

std::optional<float> RaycastAabb(
    const Math::Ray& ray, const float maximumDistance, const Math::Aabb& bounds) {
    GYO_ASSERT(IsValid(ray) && IsValid(bounds));
    GYO_ASSERT(std::isfinite(maximumDistance) && !(maximumDistance < 0.0f));
    // A slab test in double on the direction normalized in double: only an
    // exactly zero component is parallel to its slab, and the result is
    // rounded to float once.
    const Math::Vec3d direction = Math::ToVec3d(ray.direction);
    const Math::Vec3d unit = direction * (1.0 / Length(direction));
    const std::array<double, 3> origins{ray.origin.x, ray.origin.y, ray.origin.z};
    const std::array<double, 3> directions{unit.x, unit.y, unit.z};
    const std::array<double, 3> minima{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const std::array<double, 3> maxima{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};

    double entry = 0.0;
    double exit = std::numeric_limits<double>::infinity();
    for (std::size_t axis = 0; axis < origins.size(); ++axis) {
        if (directions[axis] == 0.0) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) {
                return std::nullopt;
            }
            continue;
        }
        double first = (minima[axis] - origins[axis]) / directions[axis];
        double second = (maxima[axis] - origins[axis]) / directions[axis];
        if (first > second) {
            std::swap(first, second);
        }
        if (first > entry) {
            entry = first;
        }
        if (second < exit) {
            exit = second;
        }
        if (entry > exit) {
            return std::nullopt;
        }
    }
    // entry starts at +0, so an initial overlap returns +0.
    if (exit < 0.0 || entry > maximumDistance) {
        return std::nullopt;
    }
    return static_cast<float>(entry);
}

// The VerticalCapsule overloads are the Math::Capsule queries on ToCapsule.
std::optional<float> RaycastCapsule(
    const Math::Ray& ray, const float maximumDistance,
    const VerticalCapsule& capsule, const float sweepRadius) {
    return RaycastCapsule(ray, maximumDistance, ToCapsule(capsule), sweepRadius);
}

std::optional<float> SweepSphereAgainstCapsule(
    const Math::Segment& path, const float sweepRadius, const VerticalCapsule& capsule) {
    return SweepSphereAgainstCapsule(path, sweepRadius, ToCapsule(capsule));
}

} // namespace Engine::Collision
