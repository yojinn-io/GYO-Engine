#pragma once

#include <optional>

namespace Engine::Collision {

struct Float3 final { float x{}, y{}, z{}; };
struct Aabb final { Float3 minimum{}, maximum{}; };

// feet is the world-space bottom of the upright capsule, not its center.
// height includes both hemispheres and must be at least two radii.
struct VerticalCapsule final {
    Float3 feet{};
    float height{};
    float radius{};
};

// The endpoints are centers of the hemispheres. Coincident endpoints form a sphere.
struct Capsule final {
    Float3 segmentStart{};
    Float3 segmentEnd{};
    float radius{};
};

// The normal points out of the stationary target toward the queried capsule.
// position lies on the target surface; penetrationDepth is nonzero only for
// initial overlap. A sweep fraction is measured along the supplied displacement.
struct Contact final {
    float fraction{};
    Float3 position{};
    Float3 normal{};
    float penetrationDepth{};
};

[[nodiscard]] Capsule ToCapsule(const VerticalCapsule& capsule);

// Ray directions need not be normalized. Distances are world-space units.
// Initial overlap returns zero. Invalid/non-finite inputs throw invalid_argument.
[[nodiscard]] std::optional<float> RaycastAabb(
    Float3 origin, Float3 direction, float maximumDistance, const Aabb& bounds);
[[nodiscard]] std::optional<float> RaycastCapsule(
    Float3 origin, Float3 direction, float maximumDistance,
    const VerticalCapsule& capsule, float sweepRadius = 0.0f);
[[nodiscard]] std::optional<float> RaycastCapsule(
    Float3 origin, Float3 direction, float maximumDistance,
    const Capsule& capsule, float sweepRadius = 0.0f);

// Sweeps a sphere center along a segment; returns first contact fraction [0, 1].
// A zero-length segment is an overlap query.
[[nodiscard]] std::optional<float> SweepSphereAgainstCapsule(
    Float3 start, Float3 end, float sweepRadius, const VerticalCapsule& capsule);
[[nodiscard]] std::optional<float> SweepSphereAgainstCapsule(
    Float3 start, Float3 end, float sweepRadius, const Capsule& capsule);

// Overlap queries include touching. The AABB query uses the actual rounded
// capsule shape, including at box edges and corners, not an expanded box proxy.
[[nodiscard]] std::optional<Contact> OverlapVerticalCapsuleAabb(
    const VerticalCapsule& capsule, const Aabb& bounds);
[[nodiscard]] std::optional<Contact> OverlapVerticalCapsules(
    const VerticalCapsule& capsule, const VerticalCapsule& target);

// Initial penetration returns fraction zero. Initial touching only blocks when
// moving into the contact; separating and tangential motion are permitted.
// A zero displacement is an overlap query. All inputs must be finite.
[[nodiscard]] std::optional<Contact> SweepVerticalCapsuleAgainstAabb(
    const VerticalCapsule& capsule, Float3 displacement, const Aabb& bounds);
[[nodiscard]] std::optional<Contact> SweepVerticalCapsuleAgainstCapsule(
    const VerticalCapsule& capsule, Float3 displacement, const VerticalCapsule& target);

} // namespace Engine::Collision
