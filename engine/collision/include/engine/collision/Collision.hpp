#pragma once

#include <optional>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Capsule.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/geometry/Segment.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Collision {

// Geometric primitives (Ray, Segment, Aabb, Capsule) come from Engine::Math;
// this module owns the collision algorithms and their tolerances, validation
// and contact policy.

// The character body shape specialised for collision: feet is the world-space
// bottom of the upright capsule, not its center. height includes both
// hemispheres and must be at least two radii.
struct VerticalCapsule final {
    Math::Vec3 feet{};
    float height{};
    float radius{};
};

// The normal points out of the stationary target toward the queried capsule.
// position lies on the target surface; penetrationDepth is nonzero only for
// initial overlap. A sweep fraction is measured along the supplied displacement.
struct Contact final {
    float fraction{};
    Math::Vec3 position{};
    Math::Vec3 normal{};
    float penetrationDepth{};
};

[[nodiscard]] Math::Capsule ToCapsule(const VerticalCapsule& capsule);

// The ray direction need not be normalized. Unlike Math::Intersect, results are
// world-space distances along the ray (the direction is normalized internally)
// and are limited to maximumDistance. Initial overlap returns zero.
// Invalid or non-finite inputs are a Programmer Error (GYO_ASSERT).
[[nodiscard]] std::optional<float> RaycastAabb(
    const Math::Ray& ray, float maximumDistance, const Math::Aabb& bounds);
[[nodiscard]] std::optional<float> RaycastCapsule(
    const Math::Ray& ray, float maximumDistance,
    const VerticalCapsule& capsule, float sweepRadius = 0.0f);
[[nodiscard]] std::optional<float> RaycastCapsule(
    const Math::Ray& ray, float maximumDistance,
    const Math::Capsule& capsule, float sweepRadius = 0.0f);

// Sweeps a sphere center along path; returns first contact fraction [0, 1]
// along the segment. A zero-length path is an overlap query.
[[nodiscard]] std::optional<float> SweepSphereAgainstCapsule(
    const Math::Segment& path, float sweepRadius, const VerticalCapsule& capsule);
[[nodiscard]] std::optional<float> SweepSphereAgainstCapsule(
    const Math::Segment& path, float sweepRadius, const Math::Capsule& capsule);

// Overlap queries include touching. The AABB query uses the actual rounded
// capsule shape, including at box edges and corners, not an expanded box proxy.
[[nodiscard]] std::optional<Contact> OverlapVerticalCapsuleAabb(
    const VerticalCapsule& capsule, const Math::Aabb& bounds);
[[nodiscard]] std::optional<Contact> OverlapVerticalCapsules(
    const VerticalCapsule& capsule, const VerticalCapsule& target);

// Initial penetration returns fraction zero. Initial touching only blocks when
// moving into the contact; separating and tangential motion are permitted.
// A zero displacement is an overlap query. All inputs must be finite.
[[nodiscard]] std::optional<Contact> SweepVerticalCapsuleAgainstAabb(
    const VerticalCapsule& capsule, Math::Vec3 displacement, const Math::Aabb& bounds);
[[nodiscard]] std::optional<Contact> SweepVerticalCapsuleAgainstCapsule(
    const VerticalCapsule& capsule, Math::Vec3 displacement, const VerticalCapsule& target);

} // namespace Engine::Collision
