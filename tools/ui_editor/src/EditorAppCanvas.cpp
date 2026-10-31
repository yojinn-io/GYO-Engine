#include "gyo/ui_editor/EditorApp.hpp"

#include "EditorDocumentEdits.hpp"

#include "engine/math/scalar/Scalar.hpp"
#include "engine/os/Files.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// EditorApp: the Canvas panel (preview, grid and gizmo edits).

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;
using Detail::FindSelectedNode;

// The undo label for a canvas gizmo drag.
const char* GizmoLabel(const GizmoHandle handle) {
    switch (handle) {
    case GizmoHandle::AnchorMin:
        return "anchor_min";
    case GizmoHandle::AnchorMax:
        return "anchor_max";
    case GizmoHandle::Pivot:
        return "pivot";
    case GizmoHandle::Move:
        return "move";
    default:
        return "resize";
    }
}

} // namespace

void EditorApp::DrawCanvas() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoScrollbar |
                                       // Arrow keys nudge the selected element
                                       // here instead of moving keyboard focus.
                                       ImGuiWindowFlags_NoNavInputs;
    if (!ImGui::Begin("Canvas", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Grid", &showGrid_);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snapToGrid_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0F);
    ImGui::SliderFloat("Zoom", &canvasZoom_, 0.25F, 4.0F, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        canvasZoom_ = 1.0F;
        canvasPanX_ = 0.0F;
        canvasPanY_ = 0.0F;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton(
        "canvas-input",
        available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasFocused = ImGui::IsItemFocused();
    if (canvasHovered) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.MouseWheel != 0.0F) {
            canvasZoom_ = Engine::Math::Clamp(
                canvasZoom_ * (io.MouseWheel > 0.0F ? 1.1F : 1.0F / 1.1F),
                0.25F,
                4.0F);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            canvasPanX_ += io.MouseDelta.x;
            canvasPanY_ += io.MouseDelta.y;
        }
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        origin,
        {origin.x + available.x, origin.y + available.y},
        IM_COL32(13, 15, 19, 255));
    if (showGrid_) {
        const float step = Engine::Math::Max(gridSize_ * canvasZoom_, 8.0F);
        const float xOffset = std::fmod(canvasPanX_, step);
        const float yOffset = std::fmod(canvasPanY_, step);
        for (float x = origin.x + xOffset; x < origin.x + available.x; x += step) {
            drawList->AddLine(
                {x, origin.y},
                {x, origin.y + available.y},
                IM_COL32(36, 40, 49, 255));
        }
        for (float y = origin.y + yOffset; y < origin.y + available.y; y += step) {
            drawList->AddLine(
                {origin.x, y},
                {origin.x + available.x, y},
                IM_COL32(36, 40, 49, 255));
        }
    }
    const float scaledWidth = available.x * canvasZoom_;
    const float scaledHeight = available.y * canvasZoom_;
    const ImVec2 previewOrigin{
        origin.x + canvasPanX_ + (available.x - scaledWidth) * 0.5F,
        origin.y + canvasPanY_ + (available.y - scaledHeight) * 0.5F,
    };
    const PreviewAdapter::FrameOutput result = preview_.Draw(
        *drawList,
        session_.Document(),
        selectedCanvas_,
        {previewOrigin.x, previewOrigin.y, scaledWidth, scaledHeight},
        selectedNode_,
        previewAssets_.IsMounted() ? &previewAssets_ : nullptr,
        previewMode_,
        canvasHovered || canvasFocused,
        snapToGrid_,
        gridSize_);
    if (result.error) {
        statusMessage_ = "Preview: " + Engine::Base::Describe(*result.error);
    }
    if (!result.clickedNodeId.empty() && !result.gizmoEdit.has_value()) {
        selectedNode_ = result.clickedNodeId;
    }
    if (result.gizmoEdit.has_value()) {
        const PreviewAdapter::FrameOutput::GizmoEdit& edit = *result.gizmoEdit;
        if (edit.began) {
            session_.BeginEdit(std::string{"Drag "} + GizmoLabel(edit.handle) + " gizmo");
            gizmoTransactionActive_ = true;
        }
        if (Json* edited = FindSelectedNode(
                session_.EditDocument(), selectedNode_);
            edited != nullptr) {
            Json& rect = (*edited)["rect"];
            rect["anchor_min"] = {edit.rect.anchorMin.x, edit.rect.anchorMin.y};
            rect["anchor_max"] = {edit.rect.anchorMax.x, edit.rect.anchorMax.y};
            rect["pivot"] = {edit.rect.pivot.x, edit.rect.pivot.y};
            rect["position"] = {edit.rect.position.x, edit.rect.position.y};
            rect["size_delta"] = {edit.rect.sizeDelta.x, edit.rect.sizeDelta.y};
            session_.UpdateEdit();
            RefreshDiagnostics();
        }
        if (edit.finished) {
            session_.EndEdit();
            gizmoTransactionActive_ = false;
        }
    } else if (gizmoTransactionActive_ &&
               !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        session_.EndEdit();
        gizmoTransactionActive_ = false;
    }
    for (const PreviewAdapter::FrameOutput::Action& action : result.actions) {
        std::string entry = action.id + "  source:" + action.sourceElement;
        entry += action.stringPayload.has_value() ? "  payload:\"" + *action.stringPayload + "\""
            : action.numberPayload.has_value()   ? "  payload:" + std::to_string(*action.numberPayload)
                                                 : "  payload:none";
        actionLog_.push_back(std::move(entry));
    }
    NudgeSelectedElement(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    ImGui::End();
}

} // namespace Gyo::Tools::UiEditor
