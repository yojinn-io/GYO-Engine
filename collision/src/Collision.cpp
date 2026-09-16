#include "engine/collision/Collision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Engine::Collision {
namespace {

constexpr float kEpsilon = 0.000001f;

[[nodiscard]] bool IsFinite(const Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float Length(const Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void ValidateQuery(
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const float sweepRadius) {
    if (!IsFinite(origin) || !IsFinite(direction)) {
        throw std::invalid_argument("collision query vectors must be finite");
    }
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0f) {
        throw std::invalid_argument("collision query distance must be finite and non-negative");
    }
    if (!std::isfinite(sweepRadius) || sweepRadius < 0.0f) {
        throw std::invalid_argument("collision query sweep radius must be finite and non-negative");
    }
    const float directionLength = Length(direction);
    if (!std::isfinite(directionLength) || directionLength <= kEpsilon) {
        throw std::invalid_argument("collision query direction must be non-zero");
    }
}

void ValidateCapsule(const VerticalCapsule& capsule) {
    if (!std::isfinite(capsule.feet.x) || !std::isfinite(capsule.feet.z) ||
        !std::isfinite(capsule.feet.y) || !std::isfinite(capsule.height) || !std::isfinite(capsule.radius) ||
        capsule.radius <= 0.0f || capsule.height < capsule.radius * 2.0f) {
        throw std::invalid_argument(
            "collision capsule must be finite, positive, and at least two radii high");
    }
}

[[nodiscard]] Float3 Normalize(const Float3 value) noexcept {
    const float length = Length(value);
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] std::optional<float> RaySphere(
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const Float3 center,
    const float radius) noexcept {
    const Float3 offset{origin.x - center.x, origin.y - center.y, origin.z - center.z};
    const float halfB = offset.x * direction.x + offset.y * direction.y +
                        offset.z * direction.z;
    const float c = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z -
                    radius * radius;
    const float discriminant = halfB * halfB - c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    const float root = std::sqrt((std::max)(0.0f, discriminant));
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
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const Float3 minimum,
    const Float3 maximum) noexcept {
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
        entry = (std::max)(entry, first);
        exit = (std::min)(exit, second);
        if (entry > exit) {
            return std::nullopt;
        }
    }

    if (exit < 0.0f || entry > maximumDistance) {
        return std::nullopt;
    }
    return (std::max)(0.0f, entry);
}

[[nodiscard]] std::optional<float> RayCapsuleUnchecked(
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const VerticalCapsule& capsule,
    const float sweepRadius) noexcept {
    const float radius = capsule.radius + sweepRadius;
    const float segmentBottom = capsule.feet.y + capsule.radius;
    const float segmentTop = capsule.feet.y + capsule.height - capsule.radius;
    const float closestHeight = std::clamp(origin.y, segmentBottom, segmentTop);
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
            const float root = std::sqrt((std::max)(0.0f, discriminant));
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
                    closest = (std::min)(closest, distance);
                }
            }
        }
    }

    const std::array<Float3, 2> ends{{
        {capsule.feet.x, segmentBottom, capsule.feet.z},
        {capsule.feet.x, segmentTop, capsule.feet.z},
    }};
    for (const Float3 end : ends) {
        const std::optional<float> hit =
            RaySphere(origin, direction, maximumDistance, end, radius);
        if (hit.has_value()) {
            closest = (std::min)(closest, *hit);
        }
    }

    if (closest == (std::numeric_limits<float>::max)()) {
        return std::nullopt;
    }
    return closest;
}

} // namespace

std::optional<float> RaycastAabb(
    const Float3 origin, const Float3 direction, const float maximumDistance,
    const Aabb& bounds) {
    ValidateQuery(origin, direction, maximumDistance, 0.0f);
    if (!IsFinite(bounds.minimum) || !IsFinite(bounds.maximum) ||
        bounds.minimum.x > bounds.maximum.x || bounds.minimum.y > bounds.maximum.y ||
        bounds.minimum.z > bounds.maximum.z) {
        throw std::invalid_argument("collision AABB must be finite with ordered bounds");
    }
    return RayAabb(origin, Normalize(direction), maximumDistance, bounds.minimum, bounds.maximum);
}

std::optional<float> RaycastCapsule(
    const Float3 origin, const Float3 direction, const float maximumDistance,
    const VerticalCapsule& capsule, const float sweepRadius) {
    ValidateQuery(origin, direction, maximumDistance, sweepRadius);
    ValidateCapsule(capsule);
    return RayCapsuleUnchecked(origin, Normalize(direction), maximumDistance, capsule, sweepRadius);
}

std::optional<float> SweepSphereAgainstCapsule(
    const Float3 start, const Float3 end, const float sweepRadius,
    const VerticalCapsule& capsule) {
    if (!IsFinite(start) || !IsFinite(end) || !std::isfinite(sweepRadius) || sweepRadius < 0.0f) {
        throw std::invalid_argument("collision sweep endpoints/radius must be finite with non-negative radius");
    }
    ValidateCapsule(capsule);
    const Float3 delta{end.x - start.x, end.y - start.y, end.z - start.z};
    const float length = Length(delta);
    if (!std::isfinite(length)) {
        throw std::invalid_argument("collision sweep length must be finite");
    }
    if (length <= kEpsilon) {
        return RayCapsuleUnchecked(start, {0.0f, 1.0f, 0.0f}, 0.0f, capsule, sweepRadius);
    }
    const auto distance = RaycastCapsule(start, delta, length, capsule, sweepRadius);
    return distance ? std::optional<float>{*distance / length} : std::nullopt;
}

} // namespace Engine::Collision
