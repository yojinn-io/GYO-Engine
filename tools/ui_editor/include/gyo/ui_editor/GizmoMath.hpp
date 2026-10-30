#pragma once

#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Vec2.hpp"

#include <optional>

namespace Gyo::Tools::UiEditor {

// The canvas gizmo's handles and drag rule, without ImGui: every input is an
// explicit value, so the rule can be tested frame by frame.

enum class GizmoHandle {
    AnchorMin,
    AnchorMax,
    Pivot,
    Move, // the element's body
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

// An element's rect as gyo.ui stores it. Anchors and pivot are normalized;
// position and size_delta are design pixels (y down).
struct GizmoRect final {
    Engine::Math::Vec2 anchorMin;
    Engine::Math::Vec2 anchorMax;
    Engine::Math::Vec2 pivot;
    Engine::Math::Vec2 position;
    Engine::Math::Vec2 sizeDelta;
};

// One selected element as the preview draws it. Bounds are screen pixels
// (y down); designScale is screen pixels per design pixel.
struct GizmoFrame final {
    Engine::Math::Rect parentBounds;
    Engine::Math::Rect elementBounds;
    GizmoRect rect;
    float designScale{1.0F};
};

// Where each value sits (logical) and where its handle is drawn and grabbed.
struct GizmoPoints final {
    Engine::Math::Vec2 anchorMinLogical;
    Engine::Math::Vec2 anchorMaxLogical;
    Engine::Math::Vec2 pivotLogical;
    Engine::Math::Vec2 anchorMinHandle;
    Engine::Math::Vec2 anchorMaxHandle;
    Engine::Math::Vec2 pivotHandle;
};

struct GizmoSnap final {
    bool enabled{};
    float gridSize{}; // design pixels
};

// Held keys that change an edge or corner drag (Unity's rect tool).
struct GizmoModifiers final {
    bool fromCenter{}; // Alt: the opposite side moves the other way, around the centre
    bool keepAspect{}; // Shift on a corner: both axes scale by the same factor
};

[[nodiscard]] GizmoPoints ComputeGizmoPoints(const GizmoFrame& frame) noexcept;

// The handle under mouse. Point handles (anchors, pivot, corners) come first
// and the nearest wins; a tie goes to the later of anchor min, anchor max,
// pivot, corners. Then the edges, then the body, which is grabbed only when
// the selected element is the topmost element under the mouse (bodyOnTop),
// so a click on a child still selects the child.
[[nodiscard]] std::optional<GizmoHandle> HitGizmoHandle(
    const GizmoFrame& frame, Engine::Math::Vec2 mouse, bool bodyOnTop) noexcept;

// The rect after dragging handle from grab to mouse, starting from start.
// - Move and the edges and corners change position and size_delta only; the
//   anchors stay. An edge does not cross the opposite edge.
// - The anchor and pivot handles keep the element's rectangle where it is by
//   recomputing position and size_delta. Anchors stay inside [0, 1]; an
//   anchor pushed past the other moves the other with it, so equal anchors
//   move together toward each other's side and split the other way.
// - Snap rounds only the coordinates that the drag moved: element edges to
//   the grid in design pixels from the parent's corner, anchors likewise,
//   and the pivot to the grid in the element's own pixels. On an edge or
//   corner, snap applies to the dragged side before the modifiers.
[[nodiscard]] GizmoRect DragGizmo(GizmoHandle handle, const GizmoFrame& start, Engine::Math::Vec2 grab,
    Engine::Math::Vec2 mouse, const GizmoSnap& snap, const GizmoModifiers& modifiers = {}) noexcept;

} // namespace Gyo::Tools::UiEditor
