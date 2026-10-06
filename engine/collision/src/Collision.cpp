#include "engine/collision/Collision.hpp"

#include "engine/base/Assert.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Engine::Collision {
namespace {

constexpr float kEpsilon = 0.000001f;

void ValidateQuery(
    const Math::Vec3 origin,
    const Math::Vec3 direction,
    const float maximumDistance,
    const float sweepRadius) {
    GYO_ASSERT(IsValid(Math::Ray{origin, direction}));
    GYO_ASSERT(std::isfinite(maximumDistance) && !(maximumDistance < 0.0f));
    GYO_ASSERT(std::isfinite(sweepRadius) && !(sweepRadius < 0.0f));
    // A zero direction has no ray.
    const float directionLength = Length(direction);
    GYO_ASSERT(std::isfinite(directionLength) && !(directionLength <= kEpsilon));
}

void ValidateCapsule(const VerticalCapsule& capsule) {
    GYO_ASSERT(IsValid(capsule));
}

[[nodiscard]] std::optional<float> RaySphere(
    const Math::Vec3 origin,
    const Math::Vec3 direction,
    const float maximumDistance,
    const Math::Vec3 center,
    const float radius) noexcept {
    const Math::Vec3 offset = origin - center;
    const float halfB = Dot(offset, direction);
    const float c = LengthSquared(offset) - radius * radius;
    const float discriminant = halfB * halfB - c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    const float root = std::sqrt(Math::Max(0.0f, discriminant));
    float distance = -halfB - root;
    if (distance < 0.0f) {
        distance = -halfB + root;
    }
    if (distance < 0.0f || distance > maximumDistance) {
        return std::nullopt;
    }
    return distance;
}

[[nodiscard]] std::optional<float> RayAabb(
    const Math::Vec3 origin,
    const Math::Vec3 direction,
    const float maximumDistance,
    const Math::Vec3 minimum,
    const Math::Vec3 maximum) noexcept {
    float entry = 0.0f;
    float exit = maximumDistance;
    const std::array<float, 3> origins{origin.x, origin.y, origin.z};
    const std::array<float, 3> directions{direction.x, direction.y, direction.z};
    const std::array<float, 3> minima{minimum.x, minimum.y, minimum.z};
    const std::array<float, 3> maxima{maximum.x, maximum.y, maximum.z};

    for (std::size_t axis = 0; axis < origins.size(); ++axis) {
        if (std::fabs(directions[axis]) <= kEpsilon) {
            if (origins[axis] < minima[axis] || origins[axis] > maxima[axis]) {
                return std::nullopt;
            }
            continue;
        }

        float first = (minima[axis] - origins[axis]) / directions[axis];
        float second = (maxima[axis] - origins[axis]) / directions[axis];
        if (first > second) {
            std::swap(first, second);
        }
        entry = Math::Max(entry, first);
        exit = Math::Min(exit, second);
        if (entry > exit) {
            return std::nullopt;
        }
    }

    if (exit < 0.0f || entry > maximumDistance) {
        return std::nullopt;
    }
    return Math::Max(0.0f, entry);
}

[[nodiscard]] std::optional<float> RayCapsuleUnchecked(
    const Math::Vec3 origin,
    const Math::Vec3 direction,
    const float maximumDistance,
    const VerticalCapsule& capsule,
    const float sweepRadius) noexcept {
    const float radius = capsule.radius + sweepRadius;
    const float segmentBottom = capsule.feet.y + capsule.radius;
    const float segmentTop = capsule.feet.y + capsule.height - capsule.radius;
    const float closestHeight = Math::Clamp(origin.y, segmentBottom, segmentTop);
    const float overlapX = origin.x - capsule.feet.x;
    const float overlapY = origin.y - closestHeight;
    const float overlapZ = origin.z - capsule.feet.z;
    if (overlapX * overlapX + overlapY * overlapY + overlapZ * overlapZ <=
        radius * radius) {
        return 0.0f;
    }
    float closest = (std::numeric_limits<float>::max)();

    const float offsetX = origin.x - capsule.feet.x;
    const float offsetZ = origin.z - capsule.feet.z;
    const float a = direction.x * direction.x + direction.z * direction.z;
    if (a > kEpsilon) {
        const float halfB = offsetX * direction.x + offsetZ * direction.z;
        const float c = offsetX * offsetX + offsetZ * offsetZ - radius * radius;
        const float discriminant = halfB * halfB - a * c;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(Math::Max(0.0f, discriminant));
            const std::array<float, 2> roots{
                (-halfB - root) / a,
                (-halfB + root) / a,
            };
            for (const float distance : roots) {
                if (distance < 0.0f || distance > maximumDistance) {
                    continue;
                }
                const float height = origin.y + direction.y * distance;
                if (height >= segmentBottom && height <= segmentTop) {
                    closest = Math::Min(closest, distance);
                }
            }
        }
    }

    const std::array<Math::Vec3, 2> ends{{
        {capsule.feet.x, segmentBottom, capsule.feet.z},
        {capsule.feet.x, segmentTop, capsule.feet.z},
    }};
    for (const Math::Vec3 end : ends) {
        const std::optional<float> hit =
            RaySphere(origin, direction, maximumDistance, end, radius);
        if (hit.has_value()) {
            closest = Math::Min(closest, *hit);
        }
    }

    if (closest == (std::numeric_limits<float>::max)()) {
        return std::nullopt;
    }
    return closest;
}

} // namespace

std::optional<float> RaycastAabb(
    const Math::Ray& ray, const float maximumDistance, const Math::Aabb& bounds) {
    const Math::Vec3 origin = ray.origin;
    const Math::Vec3 direction = ray.direction;
    ValidateQuery(origin, direction, maximumDistance, 0.0f);
    GYO_ASSERT(IsValid(bounds));
    return RayAabb(origin, Normalize(direction), maximumDistance, bounds.minimum, bounds.maximum);
}

std::optional<float> RaycastCapsule(
    const Math::Ray& ray, const float maximumDistance,
    const VerticalCapsule& capsule, const float sweepRadius) {
    const Math::Vec3 origin = ray.origin;
    const Math::Vec3 direction = ray.direction;
    ValidateQuery(origin, direction, maximumDistance, sweepRadius);
    ValidateCapsule(capsule);
    return RayCapsuleUnchecked(origin, Normalize(direction), maximumDistance, capsule, sweepRadius);
}

std::optional<float> SweepSphereAgainstCapsule(
    const Math::Segment& path, const float sweepRadius, const VerticalCapsule& capsule) {
    const Math::Vec3 start = path.start;
    const Math::Vec3 end = path.end;
    GYO_ASSERT(IsFinite(start) && IsFinite(end) && std::isfinite(sweepRadius) && !(sweepRadius < 0.0f));
    ValidateCapsule(capsule);
    const Math::Vec3 delta = end - start;
    const float length = Length(delta);
    GYO_ASSERT(std::isfinite(length));
    if (length <= kEpsilon) {
        return RayCapsuleUnchecked(start, {0.0f, 1.0f, 0.0f}, 0.0f, capsule, sweepRadius);
    }
    const auto distance = RaycastCapsule(Math::Ray{start, delta}, length, capsule, sweepRadius);
    return distance ? std::optional<float>{*distance / length} : std::nullopt;
}

} // namespace Engine::Collision
