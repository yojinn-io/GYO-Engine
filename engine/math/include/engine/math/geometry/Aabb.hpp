#pragma once

#include <type_traits>

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"

namespace Engine::Math {

// Axis-aligned box; valid when minimum <= maximum on every axis.
struct Aabb final {
    Vec3 minimum{};
    Vec3 maximum{};
};

// Double-precision box for algorithms that work in Vec3d.
struct Aabbd final {
    Vec3d minimum{};
    Vec3d maximum{};
};

static_assert(sizeof(Aabb) == 24 && alignof(Aabb) == 4);
static_assert(std::is_trivially_copyable_v<Aabb> && std::is_standard_layout_v<Aabb>);
static_assert(std::is_aggregate_v<Aabb>);
static_assert(sizeof(Aabbd) == 48 && alignof(Aabbd) == alignof(double));
static_assert(std::is_trivially_copyable_v<Aabbd> && std::is_standard_layout_v<Aabbd>);
static_assert(std::is_aggregate_v<Aabbd>);

[[nodiscard]] inline Aabbd ToAabbd(const Aabb& box) noexcept { return {ToVec3d(box.minimum), ToVec3d(box.maximum)}; }

[[nodiscard]] inline Vec3 Center(const Aabb& box) noexcept { return (box.minimum + box.maximum) * 0.5F; }
// Half the size on each axis.
[[nodiscard]] inline Vec3 Extents(const Aabb& box) noexcept { return (box.maximum - box.minimum) * 0.5F; }

// Closed: points on the faces are contained.
[[nodiscard]] inline bool Contains(const Aabb& box, const Vec3 point) noexcept {
    return point.x >= box.minimum.x && point.x <= box.maximum.x &&
           point.y >= box.minimum.y && point.y <= box.maximum.y &&
           point.z >= box.minimum.z && point.z <= box.maximum.z;
}

[[nodiscard]] inline Aabb Merge(const Aabb& a, const Aabb& b) noexcept {
    return {Min(a.minimum, b.minimum), Max(a.maximum, b.maximum)};
}

[[nodiscard]] inline Aabb Expand(const Aabb& box, const Vec3 point) noexcept {
    return {Min(box.minimum, point), Max(box.maximum, point)};
}

// Closest point in the box (the point itself when inside). The box must be
// ordered (asserts through Clamp).
[[nodiscard]] inline Vec3 ClosestPoint(const Vec3 point, const Aabb& box) {
    return Clamp(point, box.minimum, box.maximum);
}

[[nodiscard]] inline Vec3d ClosestPoint(const Vec3d point, const Aabbd& box) {
    return Clamp(point, box.minimum, box.maximum);
}

// Closed: boxes that only touch overlap.
[[nodiscard]] inline bool Overlaps(const Aabb& a, const Aabb& b) noexcept {
    return a.minimum.x <= b.maximum.x && b.minimum.x <= a.maximum.x &&
           a.minimum.y <= b.maximum.y && b.minimum.y <= a.maximum.y &&
           a.minimum.z <= b.maximum.z && b.minimum.z <= a.maximum.z;
}

[[nodiscard]] inline bool IsFinite(const Aabb& box) noexcept { return IsFinite(box.minimum) && IsFinite(box.maximum); }

} // namespace Engine::Math
