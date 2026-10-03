#pragma once

#include <type_traits>

#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// direction need not be normalized. Ray parameters t are measured in units of
// |direction|; the distance travelled is t * Length(direction).
struct Ray final {
    Vec3 origin{};
    Vec3 direction{};
};

static_assert(sizeof(Ray) == 24 && alignof(Ray) == 4);
static_assert(std::is_trivially_copyable_v<Ray> && std::is_standard_layout_v<Ray>);
static_assert(std::is_aggregate_v<Ray>);

[[nodiscard]] inline Vec3 PointAt(const Ray& ray, const float t) noexcept { return ray.origin + ray.direction * t; }

} // namespace Engine::Math
