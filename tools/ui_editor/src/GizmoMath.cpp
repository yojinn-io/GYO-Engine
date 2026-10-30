#include "gyo/ui_editor/GizmoMath.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

using Engine::Math::Vec2;

// Handles sit beside the point they move, so the three never overlap.
constexpr Vec2 kAnchorMinOffset{-9.0F, -9.0F};
constexpr Vec2 kAnchorMaxOffset{9.0F, -9.0F};
constexpr Vec2 kPivotOffset{0.0F, 9.0F};
constexpr float kHitRadiusSquared = 10.0F * 10.0F;
constexpr float kCornerRadiusSquared = 8.0F * 8.0F;
constexpr float kEdgeDistance = 5.0F;

[[nodiscard]] float DistanceSquared(const Vec2 left, const Vec2 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

[[nodiscard]] float Lerp(const float from, const float to, const float t) noexcept {
    return from + (to - from) * t;
}

[[nodiscard]] float SnapTo(const float value, const float grid) noexcept {
    return std::round(value / grid) * grid;
}

// The element's rectangle in design pixels from the parent's corner.
struct DesignRect final {
    Vec2 min;
    Vec2 size;
};

[[nodiscard]] Vec2 ParentDesignSize(const GizmoFrame& frame) noexcept {
    const float scale = Engine::Math::Max(0.001F, frame.designScale);
    return {frame.parentBounds.width / scale, frame.parentBounds.height / scale};
}

// The gyo.ui v1 layout rule (UiLayout): size from the anchors and size_delta,
// the anchor reference from the pivot, and the minimum from the position.
[[nodiscard]] DesignRect Layout(const GizmoRect& rect, const Vec2 parent) noexcept {
    DesignRect result;
    result.size = {parent.x * (rect.anchorMax.x - rect.anchorMin.x) + rect.sizeDelta.x,
        parent.y * (rect.anchorMax.y - rect.anchorMin.y) + rect.sizeDelta.y};
    const Vec2 reference{parent.x * Lerp(rect.anchorMin.x, rect.anchorMax.x, rect.pivot.x),
        parent.y * Lerp(rect.anchorMin.y, rect.anchorMax.y, rect.pivot.y)};
    result.min = {reference.x + rect.position.x - rect.pivot.x * result.size.x,
        reference.y + rect.position.y - rect.pivot.y * result.size.y};
    return result;
}

// The position and size_delta that put rect's anchors and pivot around target.
[[nodiscard]] GizmoRect Solve(GizmoRect rect, const Vec2 parent, const DesignRect& target) noexcept {
    rect.sizeDelta = {target.size.x - parent.x * (rect.anchorMax.x - rect.anchorMin.x),
        target.size.y - parent.y * (rect.anchorMax.y - rect.anchorMin.y)};
    rect.position = {
        target.min.x + rect.pivot.x * target.size.x - parent.x * Lerp(rect.anchorMin.x, rect.anchorMax.x, rect.pivot.x),
        target.min.y + rect.pivot.y * target.size.y - parent.y * Lerp(rect.anchorMin.y, rect.anchorMax.y, rect.pivot.y)};
    return rect;
}

// Which edges a handle moves: -1 the minimum edge, 1 the maximum, 0 neither.
struct EdgeMask final {
    int x{};
    int y{};
};

[[nodiscard]] EdgeMask EdgesOf(const GizmoHandle handle) noexcept {
    switch (handle) {
    case GizmoHandle::Left:
        return {-1, 0};
    case GizmoHandle::Right:
        return {1, 0};
    case GizmoHandle::Top:
        return {0, -1};
    case GizmoHandle::Bottom:
        return {0, 1};
    case GizmoHandle::TopLeft:
        return {-1, -1};
    case GizmoHandle::TopRight:
        return {1, -1};
    case GizmoHandle::BottomLeft:
        return {-1, 1};
    case GizmoHandle::BottomRight:
        return {1, 1};
    default:
        return {};
    }
}

// Moves one axis's minimum or maximum edge by delta, snapped when it moved,
// and keeps it from crossing the other edge.
void MoveEdge(float& min, float& size, const int edge, const float delta, const GizmoSnap& snap) noexcept {
    if (edge == 0) {
        return;
    }
    const bool snapped = snap.enabled && snap.gridSize > 0.0F && delta != 0.0F;
    const float max = min + size;
    if (edge < 0) {
        float moved = min + delta;
        if (snapped) {
            moved = SnapTo(moved, snap.gridSize);
        }
        min = Engine::Math::Min(moved, max);
        size = max - min;
    } else {
        float moved = max + delta;
        if (snapped) {
            moved = SnapTo(moved, snap.gridSize);
        }
        size = Engine::Math::Max(moved, min) - min;
    }
}

// The anchor value after a drag of delta design pixels along one axis.
[[nodiscard]] float DragAnchor(const float value, const float delta, const float parent, const GizmoSnap& snap) noexcept {
    float pixels = value * parent + delta;
    if (snap.enabled && snap.gridSize > 0.0F && delta != 0.0F) {
        pixels = SnapTo(pixels, snap.gridSize);
    }
    return Engine::Math::Clamp(pixels / Engine::Math::Max(0.001F, parent), 0.0F, 1.0F);
}

} // namespace

GizmoPoints ComputeGizmoPoints(const GizmoFrame& frame) noexcept {
    const Engine::Math::Rect& parent = frame.parentBounds;
    const Engine::Math::Rect& element = frame.elementBounds;
    GizmoPoints points;
    points.anchorMinLogical = {parent.x + frame.rect.anchorMin.x * parent.width,
        parent.y + frame.rect.anchorMin.y * parent.height};
    points.anchorMaxLogical = {parent.x + frame.rect.anchorMax.x * parent.width,
        parent.y + frame.rect.anchorMax.y * parent.height};
    points.pivotLogical = {element.x + frame.rect.pivot.x * element.width,
        element.y + frame.rect.pivot.y * element.height};
    points.anchorMinHandle = points.anchorMinLogical + kAnchorMinOffset;
    points.anchorMaxHandle = points.anchorMaxLogical + kAnchorMaxOffset;
    points.pivotHandle = points.pivotLogical + kPivotOffset;
    return points;
}

std::optional<GizmoHandle> HitGizmoHandle(const GizmoFrame& frame, const Vec2 mouse, const bool bodyOnTop) noexcept {
    const GizmoPoints points = ComputeGizmoPoints(frame);
    const Engine::Math::Rect& element = frame.elementBounds;
    const float left = element.x;
    const float top = element.y;
    const float right = element.x + element.width;
    const float bottom = element.y + element.height;

    std::optional<GizmoHandle> hit;
    float best = kHitRadiusSquared;
    const auto consider = [&](const GizmoHandle handle, const Vec2 position, const float radiusSquared) {
        const float distance = DistanceSquared(mouse, position);
        if (distance <= radiusSquared && distance <= best) {
            best = distance;
            hit = handle;
        }
    };
    consider(GizmoHandle::AnchorMin, points.anchorMinHandle, kHitRadiusSquared);
    consider(GizmoHandle::AnchorMax, points.anchorMaxHandle, kHitRadiusSquared);
    consider(GizmoHandle::Pivot, points.pivotHandle, kHitRadiusSquared);
    consider(GizmoHandle::TopLeft, {left, top}, kCornerRadiusSquared);
    consider(GizmoHandle::TopRight, {right, top}, kCornerRadiusSquared);
    consider(GizmoHandle::BottomLeft, {left, bottom}, kCornerRadiusSquared);
    consider(GizmoHandle::BottomRight, {right, bottom}, kCornerRadiusSquared);
    if (hit.has_value()) {
        return hit;
    }

    const bool withinX = mouse.x >= left && mouse.x <= right;
    const bool withinY = mouse.y >= top && mouse.y <= bottom;
    if (withinX && std::abs(mouse.y - top) <= kEdgeDistance) {
        return GizmoHandle::Top;
    }
    if (withinX && std::abs(mouse.y - bottom) <= kEdgeDistance) {
        return GizmoHandle::Bottom;
    }
    if (withinY && std::abs(mouse.x - left) <= kEdgeDistance) {
        return GizmoHandle::Left;
    }
    if (withinY && std::abs(mouse.x - right) <= kEdgeDistance) {
        return GizmoHandle::Right;
    }
    if (bodyOnTop && withinX && withinY) {
        return GizmoHandle::Move;
    }
    return std::nullopt;
}

GizmoRect DragGizmo(const GizmoHandle handle, const GizmoFrame& start, const Vec2 grab, const Vec2 mouse,
    const GizmoSnap& snap) noexcept {
    const float scale = Engine::Math::Max(0.001F, start.designScale);
    const Vec2 delta{(mouse.x - grab.x) / scale, (mouse.y - grab.y) / scale};
    const Vec2 parent = ParentDesignSize(start);
    const DesignRect original = Layout(start.rect, parent);
    GizmoRect rect = start.rect;

    switch (handle) {
    case GizmoHandle::Move: {
        // Only position changes, so size_delta keeps its exact value.
        Vec2 min = original.min + delta;
        if (snap.enabled && snap.gridSize > 0.0F) {
            min.x = delta.x != 0.0F ? SnapTo(min.x, snap.gridSize) : min.x;
            min.y = delta.y != 0.0F ? SnapTo(min.y, snap.gridSize) : min.y;
        }
        rect.position = rect.position + (min - original.min);
        return rect;
    }
    case GizmoHandle::AnchorMin:
    case GizmoHandle::AnchorMax: {
        const bool minimum = handle == GizmoHandle::AnchorMin;
        Vec2& moved = minimum ? rect.anchorMin : rect.anchorMax;
        Vec2& other = minimum ? rect.anchorMax : rect.anchorMin;
        moved = {DragAnchor(moved.x, delta.x, parent.x, snap), DragAnchor(moved.y, delta.y, parent.y, snap)};
        // The dragged anchor pushes the other one instead of stopping at it.
        other = minimum ? Vec2{Engine::Math::Max(other.x, moved.x), Engine::Math::Max(other.y, moved.y)}
                        : Vec2{Engine::Math::Min(other.x, moved.x), Engine::Math::Min(other.y, moved.y)};
        return Solve(rect, parent, original);
    }
    case GizmoHandle::Pivot: {
        const Vec2 size{Engine::Math::Max(0.001F, original.size.x), Engine::Math::Max(0.001F, original.size.y)};
        float x = rect.pivot.x * size.x + delta.x;
        float y = rect.pivot.y * size.y + delta.y;
        if (snap.enabled && snap.gridSize > 0.0F) {
            x = delta.x != 0.0F ? SnapTo(x, snap.gridSize) : x;
            y = delta.y != 0.0F ? SnapTo(y, snap.gridSize) : y;
        }
        rect.pivot = {Engine::Math::Clamp(x / size.x, 0.0F, 1.0F), Engine::Math::Clamp(y / size.y, 0.0F, 1.0F)};
        return Solve(rect, parent, original);
    }
    default: {
        const EdgeMask edges = EdgesOf(handle);
        DesignRect target = original;
        MoveEdge(target.min.x, target.size.x, edges.x, delta.x, snap);
        MoveEdge(target.min.y, target.size.y, edges.y, delta.y, snap);
        // Same as Solve with the anchors unchanged, written as a change so the
        // untouched axis keeps its exact values.
        const Vec2 minChange = target.min - original.min;
        const Vec2 sizeChange = target.size - original.size;
        rect.sizeDelta = rect.sizeDelta + sizeChange;
        rect.position = {rect.position.x + minChange.x + rect.pivot.x * sizeChange.x,
            rect.position.y + minChange.y + rect.pivot.y * sizeChange.y};
        return rect;
    }
    }
}

} // namespace Gyo::Tools::UiEditor
