#pragma once

#include <type_traits>

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

struct Segment final {
    Vec3 start{};
    Vec3 end{};
};

// Double-precision segment for algorithms that work in Vec3d.
struct Segmentd final {
    Vec3d start{};
    Vec3d end{};
};

static_assert(sizeof(Segment) == 24 && alignof(Segment) == 4);
static_assert(std::is_trivially_copyable_v<Segment> && std::is_standard_layout_v<Segment>);
static_assert(std::is_aggregate_v<Segment>);
static_assert(sizeof(Segmentd) == 48 && alignof(Segmentd) == alignof(double));
static_assert(std::is_trivially_copyable_v<Segmentd> && std::is_standard_layout_v<Segmentd>);
static_assert(std::is_aggregate_v<Segmentd>);

// t = 0 is start, t = 1 is end; t is not clamped.
[[nodiscard]] inline Vec3 PointAt(const Segment& segment, const float t) noexcept {
    return segment.start + (segment.end - segment.start) * t;
}

// Closest point on the segment. A degenerate segment returns its start.
[[nodiscard]] inline Vec3 ClosestPoint(const Vec3 point, const Segment& segment) noexcept {
    const Vec3 edge = segment.end - segment.start;
    const float squared = Dot(edge, edge);
    return segment.start + edge * (squared > 0.0F
        ? Clamp(Dot(point - segment.start, edge) / squared, 0.0F, 1.0F) : 0.0F);
}

[[nodiscard]] inline Vec3d ClosestPoint(const Vec3d point, const Segmentd& segment) noexcept {
    const Vec3d edge = segment.end - segment.start;
    const double squared = Dot(edge, edge);
    return segment.start + edge * (squared > 0.0
        ? Clamp(Dot(point - segment.start, edge) / squared, 0.0, 1.0) : 0.0);
}

} // namespace Engine::Math
