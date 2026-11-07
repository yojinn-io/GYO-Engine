#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// The volume a clip matrix keeps: six planes whose normals point inward, in
// the order left, right, bottom, top, near, far. A point is inside when its
// SignedDistance to every plane is >= 0.
struct Frustum final {
    std::array<Plane, 6> planes{};
};

static_assert(sizeof(Frustum) == 96 && alignof(Frustum) == 4);
static_assert(std::is_trivially_copyable_v<Frustum> && std::is_standard_layout_v<Frustum>);
static_assert(std::is_aggregate_v<Frustum>);

namespace FrustumDetail {

// Clip-space inequality row . (p, 1) >= 0 as a plane: normal (a, b, c), distance -d, normalized.
[[nodiscard]] inline Plane PlaneFromRow(const float a, const float b, const float c, const float d) noexcept {
    const Vec3 normal{a, b, c};
    const float length = Length(normal);
    return {normal / length, -d / length};
}

} // namespace FrustumDetail

// Planes of a clip matrix (for example Multiply(projection, view)) under the
// math.md clip convention: -w <= x, y <= w and 0 <= z <= w. The matrix must
// describe a non-degenerate volume; a plane with a zero normal yields NaN.
[[nodiscard]] inline Frustum MakeFrustum(const Matrix4& clip) noexcept {
    const auto& m = clip.values;
    // Row i of a column-major matrix is (m[i], m[4 + i], m[8 + i], m[12 + i]).
    // Each plane is the inequality w + sign * row(j) >= 0.
    const auto bound = [&m](const std::size_t j, const float sign) noexcept {
        return FrustumDetail::PlaneFromRow(m[3] + sign * m[j], m[7] + sign * m[4 + j], m[11] + sign * m[8 + j],
                                           m[15] + sign * m[12 + j]);
    };
    Frustum result;
    result.planes[0] = bound(0, 1.0F);                                        // left:   w + x >= 0
    result.planes[1] = bound(0, -1.0F);                                       // right:  w - x >= 0
    result.planes[2] = bound(1, 1.0F);                                        // bottom: w + y >= 0
    result.planes[3] = bound(1, -1.0F);                                       // top:    w - y >= 0
    result.planes[4] = FrustumDetail::PlaneFromRow(m[2], m[6], m[10], m[14]); // near:   z >= 0
    result.planes[5] = bound(2, -1.0F);                                       // far:    w - z >= 0
    return result;
}

// Conservative overlap for culling: false only when the box lies entirely on
// the outside of one plane. A box outside the frustum near an edge or corner
// may still return true. Touching counts as overlap; there is no tolerance.
[[nodiscard]] inline bool Intersects(const Frustum& frustum, const Aabb& box) noexcept {
    for (const Plane& plane : frustum.planes) {
        // The box corner furthest along the normal (the "positive vertex").
        const Vec3 corner{plane.normal.x >= 0.0F ? box.maximum.x : box.minimum.x,
                          plane.normal.y >= 0.0F ? box.maximum.y : box.minimum.y,
                          plane.normal.z >= 0.0F ? box.maximum.z : box.minimum.z};
        if (SignedDistance(plane, corner) < 0.0F) return false;
    }
    return true;
}

} // namespace Engine::Math
