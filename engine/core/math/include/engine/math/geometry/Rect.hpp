#pragma once

#include <cmath>
#include <type_traits>

#include "engine/math/linear/Vec2.hpp"
#include "engine/math/scalar/Scalar.hpp"

namespace Engine::Math {

// Axis-aligned 2D rectangle: origin (x, y) and size. The queries below assume a
// non-negative size; callers that can produce negative sizes (for example UI
// layout) handle them before querying. The axis direction (for example y-down
// pixels) belongs to the caller's space.
struct Rect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

static_assert(sizeof(Rect) == 16 && alignof(Rect) == 4);
static_assert(std::is_trivially_copyable_v<Rect> && std::is_standard_layout_v<Rect>);
static_assert(std::is_aggregate_v<Rect>);

// Overlapping region; zero width and/or height when the rectangles do not overlap.
[[nodiscard]] inline Rect Intersection(const Rect a, const Rect b) noexcept {
    const float x = Max(a.x, b.x);
    const float y = Max(a.y, b.y);
    const float rightEdge = Min(a.x + a.width, b.x + b.width);
    const float bottomEdge = Min(a.y + a.height, b.y + b.height);
    return {x, y, Max(0.0F, rightEdge - x), Max(0.0F, bottomEdge - y)};
}

// Closed on all edges.
[[nodiscard]] inline bool Contains(const Rect rect, const Vec2 point) noexcept {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

// Closed: rectangles that only touch overlap.
[[nodiscard]] inline bool Overlaps(const Rect a, const Rect b) noexcept {
    return a.x <= b.x + b.width && b.x <= a.x + a.width &&
           a.y <= b.y + b.height && b.y <= a.y + a.height;
}

[[nodiscard]] inline bool IsFinite(const Rect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.width) && std::isfinite(rect.height);
}

} // namespace Engine::Math
