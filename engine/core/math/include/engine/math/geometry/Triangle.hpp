#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "engine/math/geometry/Segment.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// Vertices in order a, b, c. The only orientation fact defined here is how the
// normal follows that order: Normal = Cross(b - a, c - a). Which side counts as
// a front face (for culling) is render pipeline state, not a Math concept.
struct Triangle final {
    Vec3 a{};
    Vec3 b{};
    Vec3 c{};
};

static_assert(sizeof(Triangle) == 36 && alignof(Triangle) == 4);
static_assert(std::is_trivially_copyable_v<Triangle> && std::is_standard_layout_v<Triangle>);
static_assert(std::is_aggregate_v<Triangle>);

// Unnormalized; its length is twice the area.
[[nodiscard]] inline Vec3 Normal(const Triangle& triangle) noexcept {
    return Cross(triangle.b - triangle.a, triangle.c - triangle.a);
}

// Requires a non-degenerate triangle; a degenerate one yields NaN components.
[[nodiscard]] inline Vec3 UnitNormal(const Triangle& triangle) noexcept { return Normalize(Normal(triangle)); }

[[nodiscard]] inline float Area(const Triangle& triangle) noexcept { return Length(Normal(triangle)) * 0.5F; }

[[nodiscard]] inline Vec3 Centroid(const Triangle& triangle) noexcept {
    return (triangle.a + triangle.b + triangle.c) / 3.0F;
}

// Closest point on the triangle. Region classification follows Ericson,
// Real-Time Collision Detection 5.1.5; the interior case projects onto the
// plane along the normal, whose error grows as eps / sin(angle) rather than
// eps / sin(angle)^2.
//
// Conditioning guard (not a caller tolerance): when |ab x ac|^2 <= FLT_EPSILON
// * |ab|^2 * |ac|^2, the triangle is a segment at float precision and region
// tests are unreliable, so the closest of the three edge points is returned.
// Its error is at most about sqrt(FLT_EPSILON) times the edge length.
// Coordinates must stay below about 1e9 so the fourth powers do not overflow.
[[nodiscard]] inline Vec3 ClosestPoint(const Vec3 point, const Triangle& triangle) noexcept {
    const Vec3 ab = triangle.b - triangle.a;
    const Vec3 ac = triangle.c - triangle.a;
    const Vec3 normal = Cross(ab, ac);
    const float normalSquared = Dot(normal, normal);
    if (normalSquared <= std::numeric_limits<float>::epsilon() * Dot(ab, ab) * Dot(ac, ac)) {
        const Vec3 onAb = ClosestPoint(point, Segment{triangle.a, triangle.b});
        const Vec3 onBc = ClosestPoint(point, Segment{triangle.b, triangle.c});
        const Vec3 onAc = ClosestPoint(point, Segment{triangle.a, triangle.c});
        const float toAb = LengthSquared(point - onAb);
        const float toBc = LengthSquared(point - onBc);
        const float toAc = LengthSquared(point - onAc);
        if (toAb <= toBc && toAb <= toAc) return onAb;
        return toBc <= toAc ? onBc : onAc;
    }

    const Vec3 ap = point - triangle.a;
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) return triangle.a;

    const Vec3 bp = point - triangle.b;
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) return triangle.b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F && d1 - d3 > 0.0F) {
        return triangle.a + ab * (d1 / (d1 - d3));
    }

    const Vec3 cp = point - triangle.c;
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) return triangle.c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F && d2 - d6 > 0.0F) {
        return triangle.a + ac * (d2 / (d2 - d6));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F && (d4 - d3) + (d5 - d6) > 0.0F) {
        return triangle.b + (triangle.c - triangle.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }

    return point - normal * (Dot(normal, ap) / normalSquared);
}

} // namespace Engine::Math
