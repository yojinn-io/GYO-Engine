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

// Ray directions need not be normalized. Distances are world-space units.
// Initial overlap returns zero. Invalid/non-finite inputs throw invalid_argument.
[[nodiscard]] std::optional<float> RaycastAabb(
    Float3 origin, Float3 direction, float maximumDistance, const Aabb& bounds);
[[nodiscard]] std::optional<float> RaycastCapsule(
    Float3 origin, Float3 direction, float maximumDistance,
    const VerticalCapsule& capsule, float sweepRadius = 0.0f);

// Sweeps a sphere center along a segment; returns first contact fraction [0, 1].
// A zero-length segment is an overlap query.
[[nodiscard]] std::optional<float> SweepSphereAgainstCapsule(
    Float3 start, Float3 end, float sweepRadius, const VerticalCapsule& capsule);

} // namespace Engine::Collision
