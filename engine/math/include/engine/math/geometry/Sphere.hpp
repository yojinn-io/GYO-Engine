#pragma once

#include <type_traits>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

struct Sphere final {
    Vec3 center{};
    float radius{};
};

static_assert(sizeof(Sphere) == 16 && alignof(Sphere) == 4);
static_assert(std::is_trivially_copyable_v<Sphere> && std::is_standard_layout_v<Sphere>);
static_assert(std::is_aggregate_v<Sphere>);

// Closed: points on the surface are contained.
[[nodiscard]] inline bool Contains(const Sphere& sphere, const Vec3 point) noexcept {
    const Vec3 offset = point - sphere.center;
    return Dot(offset, offset) <= sphere.radius * sphere.radius;
}

// Closed: touching spheres overlap.
[[nodiscard]] inline bool Overlaps(const Sphere& a, const Sphere& b) noexcept {
    const Vec3 offset = b.center - a.center;
    const float reach = a.radius + b.radius;
    return Dot(offset, offset) <= reach * reach;
}

[[nodiscard]] inline bool Overlaps(const Sphere& sphere, const Aabb& box) noexcept {
    return Contains(sphere, ClosestPoint(sphere.center, box));
}

[[nodiscard]] inline bool Overlaps(const Aabb& box, const Sphere& sphere) noexcept { return Overlaps(sphere, box); }

} // namespace Engine::Math
