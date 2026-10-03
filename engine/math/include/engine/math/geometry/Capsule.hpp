#pragma once

#include <type_traits>

#include "engine/math/geometry/Segment.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// The set of points within radius of the segment. The endpoints are the
// centres of the two hemispheres; coincident endpoints form a sphere.
struct Capsule final {
    Vec3 segmentStart{};
    Vec3 segmentEnd{};
    float radius{};
};

static_assert(sizeof(Capsule) == 28 && alignof(Capsule) == 4);
static_assert(std::is_trivially_copyable_v<Capsule> && std::is_standard_layout_v<Capsule>);
static_assert(std::is_aggregate_v<Capsule>);

[[nodiscard]] inline Segment Axis(const Capsule& capsule) noexcept {
    return {capsule.segmentStart, capsule.segmentEnd};
}

// Closed: points on the surface are contained.
[[nodiscard]] inline bool Contains(const Capsule& capsule, const Vec3 point) noexcept {
    const Vec3 offset = point - ClosestPoint(point, Axis(capsule));
    return Dot(offset, offset) <= capsule.radius * capsule.radius;
}

} // namespace Engine::Math
