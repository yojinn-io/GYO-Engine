#pragma once

#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Vec2.hpp"

#include <optional>

namespace Gyo::Tools::UiEditor {

// The canvas gizmo's handles and drag rule, without ImGui: every input is an
// explicit value, so the rule can be tested frame by frame.

enum class GizmoField {
    AnchorMin,
    AnchorMax,
    Pivot,
};

// One selected element as the preview draws it. Rectangles are screen pixels
// (y down); anchors and pivot are the element's normalized rect values.
struct GizmoFrame final {
    Engine::Math::Rect parentBounds;
    Engine::Math::Rect elementBounds;
    Engine::Math::Vec2 anchorMin;
    Engine::Math::Vec2 anchorMax;
    Engine::Math::Vec2 pivot;
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
    float gridSize{};    // design pixels
    float designScale{}; // screen pixels per design pixel
};

[[nodiscard]] GizmoPoints ComputeGizmoPoints(const GizmoFrame& frame) noexcept;

// The handle within the grab radius of mouse; the nearest wins, and a tie goes
// to the later of anchor min, anchor max, pivot.
[[nodiscard]] std::optional<GizmoField> HitGizmoHandle(
    const GizmoPoints& points, Engine::Math::Vec2 mouse) noexcept;

// The field's new normalized value with the handle under mouse: snapped, held
// inside [0, 1], and anchor min kept at or below anchor max (and the reverse).
[[nodiscard]] Engine::Math::Vec2 DragGizmoHandle(
    GizmoField field, const GizmoFrame& frame, Engine::Math::Vec2 mouse, const GizmoSnap& snap) noexcept;

} // namespace Gyo::Tools::UiEditor
