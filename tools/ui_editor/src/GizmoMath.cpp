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

[[nodiscard]] Vec2 Offset(const GizmoField field) noexcept {
    switch (field) {
    case GizmoField::AnchorMin:
        return kAnchorMinOffset;
    case GizmoField::AnchorMax:
        return kAnchorMaxOffset;
    case GizmoField::Pivot:
        return kPivotOffset;
    }
    return kPivotOffset;
}

[[nodiscard]] float DistanceSquared(const Vec2 left, const Vec2 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

} // namespace

GizmoPoints ComputeGizmoPoints(const GizmoFrame& frame) noexcept {
    const Engine::Math::Rect& parent = frame.parentBounds;
    const Engine::Math::Rect& element = frame.elementBounds;
    GizmoPoints points;
    points.anchorMinLogical = {
        parent.x + frame.anchorMin.x * parent.width,
        parent.y + frame.anchorMin.y * parent.height,
    };
    points.anchorMaxLogical = {
        parent.x + frame.anchorMax.x * parent.width,
        parent.y + frame.anchorMax.y * parent.height,
    };
    points.pivotLogical = {
        element.x + frame.pivot.x * element.width,
        element.y + frame.pivot.y * element.height,
    };
    points.anchorMinHandle = points.anchorMinLogical + kAnchorMinOffset;
    points.anchorMaxHandle = points.anchorMaxLogical + kAnchorMaxOffset;
    points.pivotHandle = points.pivotLogical + kPivotOffset;
    return points;
}

std::optional<GizmoField> HitGizmoHandle(const GizmoPoints& points, const Vec2 mouse) noexcept {
    std::optional<GizmoField> hit;
    float best = kHitRadiusSquared;
    for (const auto& [field, handle] : {
             std::pair{GizmoField::AnchorMin, points.anchorMinHandle},
             std::pair{GizmoField::AnchorMax, points.anchorMaxHandle},
             std::pair{GizmoField::Pivot, points.pivotHandle}}) {
        const float distance = DistanceSquared(mouse, handle);
        if (distance <= best) {
            best = distance;
            hit = field;
        }
    }
    return hit;
}

Vec2 DragGizmoHandle(
    const GizmoField field, const GizmoFrame& frame, const Vec2 mouse, const GizmoSnap& snap) noexcept {
    const bool pivotField = field == GizmoField::Pivot;
    const Engine::Math::Rect& basis = pivotField ? frame.elementBounds : frame.parentBounds;
    const float width = Engine::Math::Max(0.001F, basis.width);
    const float height = Engine::Math::Max(0.001F, basis.height);
    const Vec2 offset = Offset(field);
    float x = (mouse.x - offset.x - basis.x) / width;
    float y = (mouse.y - offset.y - basis.y) / height;
    if (snap.enabled && snap.gridSize > 0.0F) {
        const float snapPixels = snap.gridSize * snap.designScale;
        x = std::round(x * width / snapPixels) * snapPixels / width;
        y = std::round(y * height / snapPixels) * snapPixels / height;
    }
    x = Engine::Math::Clamp(x, 0.0F, 1.0F);
    y = Engine::Math::Clamp(y, 0.0F, 1.0F);
    if (field == GizmoField::AnchorMin) {
        x = Engine::Math::Min(x, frame.anchorMax.x);
        y = Engine::Math::Min(y, frame.anchorMax.y);
    } else if (field == GizmoField::AnchorMax) {
        x = Engine::Math::Max(x, frame.anchorMin.x);
        y = Engine::Math::Max(y, frame.anchorMin.y);
    }
    return {x, y};
}

} // namespace Gyo::Tools::UiEditor
