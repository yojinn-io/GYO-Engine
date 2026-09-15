#include "gyo/ui_editor/PreviewAdapter.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "ui/UiDocumentCodec.hpp"
#include "ui/UiRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace Gyo::Tools::UiEditor {
namespace {

using Json = nlohmann::json;

[[nodiscard]] float LinearToSrgb(const float linear) noexcept {
    const float clamped = std::clamp(linear, 0.0F, 1.0F);
    return clamped <= 0.0031308F
        ? clamped * 12.92F
        : 1.055F * std::pow(clamped, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] ImU32 ToImColor(const Engine::Ui::UiColor& color) noexcept {
    return ImGui::ColorConvertFloat4ToU32({
        LinearToSrgb(color.red),
        LinearToSrgb(color.green),
        LinearToSrgb(color.blue),
        std::clamp(color.alpha, 0.0F, 1.0F),
    });
}

[[nodiscard]] ImVec2 Minimum(
    const Engine::Ui::UiRect& rect,
    const PreviewAdapter::Viewport& viewport) noexcept {
    return {viewport.x + rect.x, viewport.y + rect.y};
}

[[nodiscard]] ImVec2 Maximum(
    const Engine::Ui::UiRect& rect,
    const PreviewAdapter::Viewport& viewport) noexcept {
    return {
        viewport.x + rect.x + rect.width,
        viewport.y + rect.y + rect.height,
    };
}

struct SourceLocation final {
    const Json* element{};
    const Json* parent{};
};

[[nodiscard]] bool FindSourceLocation(
    const Json& value,
    const std::string_view id,
    const Json* parent,
    SourceLocation& result) {
    if (value.is_object()) {
        const Json* nextParent = parent;
        if (value.contains("type")) {
            if (value.value("id", std::string{}) == id) {
                result = {&value, parent};
                return true;
            }
            nextParent = &value;
        }
        for (const char* collection : {"children", "template"}) {
            if (value.contains(collection) && value[collection].is_array()) {
                for (const Json& child : value[collection]) {
                    if (FindSourceLocation(child, id, nextParent, result)) {
                        return true;
                    }
                }
            }
        }
    } else if (value.is_array()) {
        for (const Json& child : value) {
            if (FindSourceLocation(child, id, parent, result)) return true;
        }
    }
    return false;
}

[[nodiscard]] float DistanceSquared(const ImVec2 left, const ImVec2 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

class ClipScope final {
public:
    ClipScope(
        ImDrawList& drawList,
        const Engine::Ui::UiRect& clip,
        const PreviewAdapter::Viewport& viewport)
        : drawList_(&drawList) {
        drawList_->PushClipRect(Minimum(clip, viewport), Maximum(clip, viewport), true);
    }

    ~ClipScope() { drawList_->PopClipRect(); }

private:
    ImDrawList* drawList_;
};

void DrawImagePlaceholder(
    ImDrawList& drawList,
    const Engine::Ui::UiImageDraw& image,
    const PreviewAdapter::Viewport& viewport,
    AssetPreviewContext* assets) {
    ClipScope clip(drawList, image.clipPixels, viewport);
    const ImVec2 minimum = Minimum(image.destinationPixels, viewport);
    const ImVec2 maximum = Maximum(image.destinationPixels, viewport);
    std::string assetError;
    SDL_Texture* texture = assets != nullptr
        ? assets->Texture(image.textureAssetId, assetError)
        : nullptr;
    if (texture != nullptr) {
        drawList.AddImage(
            ImTextureRef{static_cast<ImTextureID>(
                reinterpret_cast<std::intptr_t>(texture))},
            minimum,
            maximum,
            {image.sourceUv.x, image.sourceUv.y},
            {image.sourceUv.x + image.sourceUv.width,
             image.sourceUv.y + image.sourceUv.height},
            ToImColor(image.tint));
        return;
    }
    constexpr float tile = 12.0F;
    for (float y = minimum.y; y < maximum.y; y += tile) {
        for (float x = minimum.x; x < maximum.x; x += tile) {
            const int parity = static_cast<int>((x - minimum.x) / tile) +
                               static_cast<int>((y - minimum.y) / tile);
            drawList.AddRectFilled(
                {x, y},
                {std::min(x + tile, maximum.x), std::min(y + tile, maximum.y)},
                parity % 2 == 0
                    ? IM_COL32(68, 73, 84, 255)
                    : IM_COL32(42, 46, 55, 255));
        }
    }
    const std::string label = "UNRESOLVED  " + image.textureAssetId;
    drawList.AddText(
        {minimum.x + 6.0F, minimum.y + 6.0F},
        IM_COL32(255, 122, 122, 255),
        label.c_str());
}

void DrawText(
    ImDrawList& drawList,
    const Engine::Ui::UiTextDraw& text,
    const PreviewAdapter::Viewport& viewport,
    AssetPreviewContext* assets) {
    ClipScope clip(drawList, text.clipPixels, viewport);
    const ImVec2 minimum = Minimum(text.boundsPixels, viewport);
    const float fontSize = std::max(text.pointSizePixels, 1.0F);
    std::string assetError;
    const TextTextureView raster = assets != nullptr
        ? assets->Text(text.fontAssetId, text.utf8, fontSize, assetError)
        : TextTextureView{};
    const ImVec2 extent = raster.texture != nullptr
        ? ImVec2{raster.width, raster.height}
        : ImGui::GetFont()->CalcTextSizeA(
              fontSize,
              (std::numeric_limits<float>::max)(),
              0.0F,
              text.utf8.c_str());
    ImVec2 position = minimum;
    if (text.horizontalAlign == Engine::Ui::UiHorizontalAlign::Center) {
        position.x += (text.boundsPixels.width - extent.x) * 0.5F;
    } else if (text.horizontalAlign == Engine::Ui::UiHorizontalAlign::Right) {
        position.x += text.boundsPixels.width - extent.x;
    }
    if (text.verticalAlign == Engine::Ui::UiVerticalAlign::Center) {
        position.y += (text.boundsPixels.height - extent.y) * 0.5F;
    } else if (text.verticalAlign == Engine::Ui::UiVerticalAlign::Bottom) {
        position.y += text.boundsPixels.height - extent.y;
    }
    if (raster.texture != nullptr) {
        drawList.AddImage(
            ImTextureRef{static_cast<ImTextureID>(
                reinterpret_cast<std::intptr_t>(raster.texture))},
            position,
            {position.x + raster.width, position.y + raster.height},
            {0.0F, 0.0F},
            {1.0F, 1.0F},
            ToImColor(text.color));
    } else {
        drawList.AddText(
            ImGui::GetFont(),
            fontSize,
            position,
            ToImColor(text.color),
            text.utf8.c_str());
    }
}

} // namespace

struct PreviewAdapter::Impl final {
    std::string sourceSnapshot;
    std::string activeCanvas;
    std::shared_ptr<const Engine::Ui::UiDocument> document;
    Engine::Ui::UiBindingTable previewBindings;
    Engine::Ui::UiRuntime runtime;
    std::optional<Result::GizmoField> activeGizmo;
    std::string activeGizmoNode;
};

PreviewAdapter::PreviewAdapter() : impl_(std::make_unique<Impl>()) {}
PreviewAdapter::~PreviewAdapter() = default;
PreviewAdapter::PreviewAdapter(PreviewAdapter&&) noexcept = default;
PreviewAdapter& PreviewAdapter::operator=(PreviewAdapter&&) noexcept = default;

PreviewAdapter::Result PreviewAdapter::Draw(
    ImDrawList& drawList,
    const Json& sourceDocument,
    const std::string_view canvasId,
    const Viewport& viewport,
    const std::string_view selectedNodeId,
    AssetPreviewContext* assets,
    const bool previewMode,
    const bool inputEnabled,
    const bool snapToGrid,
    const float gridSize) const {
    Result result;
    if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
        return result;
    }

    const std::string snapshot = sourceDocument.dump();
    if (snapshot != impl_->sourceSnapshot) {
        auto parsed = Engine::Ui::UiDocumentCodec::Parse(
            snapshot, "ui-editor-preview");
        if (!parsed) {
            result.error = parsed.error().message;
            return result;
        }
        auto document = std::make_shared<const Engine::Ui::UiDocument>(
            std::move(parsed).value());
        auto initialized = impl_->runtime.Initialize(document);
        if (!initialized) {
            result.error = initialized.error().message;
            return result;
        }
        impl_->previewBindings.clear();
        for (const Engine::Ui::UiBindingDeclaration& binding : document->bindings) {
            impl_->previewBindings.emplace(binding.id, binding.preview);
        }
        impl_->document = std::move(document);
        impl_->sourceSnapshot = snapshot;
        impl_->activeCanvas.clear();
    }

    if (impl_->document == nullptr) {
        return result;
    }
    if (impl_->activeCanvas != canvasId) {
        auto activated = impl_->runtime.ActivateCanvas(canvasId);
        if (!activated) {
            result.error = activated.error().message;
            return result;
        }
        impl_->activeCanvas = canvasId;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool pointerInside =
        inputEnabled && mouse.x >= viewport.x && mouse.y >= viewport.y &&
        mouse.x < viewport.x + viewport.width &&
        mouse.y < viewport.y + viewport.height;
    Engine::Ui::UiInputFrame input;
    input.pointerAvailable = pointerInside && previewMode;
    input.pointerPixels = {mouse.x - viewport.x, mouse.y - viewport.y};
    input.pointerPrimaryPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    input.pointerPrimaryHeld = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.pointerPrimaryReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    if (previewMode && inputEnabled) {
        const ImGuiIO& io = ImGui::GetIO();
        input.focusPreviousPressed = io.KeyShift &&
            ImGui::IsKeyPressed(ImGuiKey_Tab, false);
        input.focusNextPressed = !io.KeyShift &&
            ImGui::IsKeyPressed(ImGuiKey_Tab, false);
        input.adjustPreviousPressed =
            ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false);
        input.adjustNextPressed =
            ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
        input.activatePressed =
            ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Space, false);
        input.cancelPressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    }

    const Engine::Ui::UiViewport runtimeViewport{viewport.width, viewport.height};
    auto events = impl_->runtime.Update(
        input, impl_->previewBindings, runtimeViewport);
    if (!events) {
        result.error = events.error().message;
        return result;
    }
    for (const Engine::Ui::UiActionEvent& event : events.value()) {
        Result::Action action;
        action.id = event.action;
        action.sourceElement = event.sourceElement;
        if (const double* number = std::get_if<double>(&event.payload)) {
            action.numberPayload = *number;
            SourceLocation source;
            if (FindSourceLocation(
                    sourceDocument["canvases"],
                    event.sourceElement,
                    nullptr,
                    source) &&
                source.element != nullptr &&
                source.element->contains("binding") &&
                (*source.element)["binding"].is_string()) {
                impl_->previewBindings[(*source.element)["binding"].get<std::string>()] =
                    *number;
            }
        }
        result.actions.push_back(std::move(action));
    }

    auto evaluated = impl_->runtime.EvaluateLayout(
        impl_->previewBindings, runtimeViewport);
    if (!evaluated) {
        result.error = evaluated.error().message;
        return result;
    }
    if (pointerInside) {
        const Engine::Ui::UiEvaluatedElement* hit =
            Engine::Ui::HitTestUiLayout(
                evaluated.value(), input.pointerPixels, false);
        if (hit != nullptr) {
            result.hoveredNodeId = hit->id;
            if (input.pointerPrimaryPressed) {
                result.clickedNodeId = hit->id;
            }
        }
    }

    auto composed = impl_->runtime.Compose(
        impl_->previewBindings, runtimeViewport);
    if (!composed) {
        result.error = composed.error().message;
        return result;
    }
    for (const Engine::Ui::UiDrawCommand& command : composed.value().commands) {
        std::visit(
            [&](const auto& typed) {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, Engine::Ui::UiQuadDraw>) {
                    ClipScope clip(drawList, typed.clipPixels, viewport);
                    drawList.AddRectFilled(
                        Minimum(typed.destinationPixels, viewport),
                        Maximum(typed.destinationPixels, viewport),
                        ToImColor(typed.color));
                } else if constexpr (
                    std::is_same_v<T, Engine::Ui::UiImageDraw>) {
                    DrawImagePlaceholder(drawList, typed, viewport, assets);
                } else {
                    DrawText(drawList, typed, viewport, assets);
                }
            },
            command);
    }

    const auto selected = std::ranges::find_if(
        evaluated.value(),
        [selectedNodeId](const Engine::Ui::UiEvaluatedElement& element) {
            return element.id == selectedNodeId;
        });
    SourceLocation source;
    if (selected != evaluated.value().end() &&
        FindSourceLocation(
            sourceDocument["canvases"],
            selectedNodeId,
            nullptr,
            source) &&
        source.element != nullptr && source.element->contains("rect")) {
        const Json& rect = (*source.element)["rect"];
        const ImVec2 minimum = Minimum(selected->boundsPixels, viewport);
        const ImVec2 maximum = Maximum(selected->boundsPixels, viewport);
        drawList.PushClipRect(
            {viewport.x, viewport.y},
            {viewport.x + viewport.width, viewport.y + viewport.height},
            true);
        drawList.AddRect(
            minimum, maximum, IM_COL32(255, 192, 64, 255), 0.0F, 0, 2.0F);

        Engine::Ui::UiRect parentBounds;
        if (source.parent != nullptr) {
            const std::string parentId = source.parent->value(
                "id", std::string{});
            const auto parent = std::ranges::find_if(
                evaluated.value(),
                [&parentId](const Engine::Ui::UiEvaluatedElement& element) {
                    return element.id == parentId;
                });
            if (parent != evaluated.value().end()) {
                parentBounds = parent->boundsPixels;
            }
        }
        const Json& design = sourceDocument["design_canvas"]["size"];
        const float designWidth = design[0].get<float>();
        const float designHeight = design[1].get<float>();
        const float designScale = std::min(
            viewport.width / designWidth,
            viewport.height / designHeight);
        if (source.parent == nullptr) {
            parentBounds.width = designWidth * designScale;
            parentBounds.height = designHeight * designScale;
            parentBounds.x = (viewport.width - parentBounds.width) * 0.5F;
            parentBounds.y = (viewport.height - parentBounds.height) * 0.5F;
        }

        if (parentBounds.width > 0.0F && parentBounds.height > 0.0F &&
            rect.contains("anchor_min") && rect.contains("anchor_max") &&
            rect.contains("pivot")) {
            const float anchorMinX = rect["anchor_min"][0].get<float>();
            const float anchorMinY = rect["anchor_min"][1].get<float>();
            const float anchorMaxX = rect["anchor_max"][0].get<float>();
            const float anchorMaxY = rect["anchor_max"][1].get<float>();
            const float pivotX = rect["pivot"][0].get<float>();
            const float pivotY = rect["pivot"][1].get<float>();
            const ImVec2 parentMinimum{
                viewport.x + parentBounds.x,
                viewport.y + parentBounds.y,
            };
            const ImVec2 anchorMinLogical{
                parentMinimum.x + anchorMinX * parentBounds.width,
                parentMinimum.y + anchorMinY * parentBounds.height,
            };
            const ImVec2 anchorMaxLogical{
                parentMinimum.x + anchorMaxX * parentBounds.width,
                parentMinimum.y + anchorMaxY * parentBounds.height,
            };
            const ImVec2 pivotLogical{
                minimum.x + pivotX * selected->boundsPixels.width,
                minimum.y + pivotY * selected->boundsPixels.height,
            };
            const ImVec2 anchorMinOffset{-9.0F, -9.0F};
            const ImVec2 anchorMaxOffset{9.0F, -9.0F};
            const ImVec2 pivotOffset{0.0F, 9.0F};
            const ImVec2 anchorMinHandle{
                anchorMinLogical.x + anchorMinOffset.x,
                anchorMinLogical.y + anchorMinOffset.y,
            };
            const ImVec2 anchorMaxHandle{
                anchorMaxLogical.x + anchorMaxOffset.x,
                anchorMaxLogical.y + anchorMaxOffset.y,
            };
            const ImVec2 pivotHandle{
                pivotLogical.x + pivotOffset.x,
                pivotLogical.y + pivotOffset.y,
            };
            const ImVec2 anchorCenter{
                (anchorMinLogical.x + anchorMaxLogical.x) * 0.5F,
                (anchorMinLogical.y + anchorMaxLogical.y) * 0.5F,
            };
            drawList.AddLine(
                anchorMinLogical,
                anchorMaxLogical,
                IM_COL32(64, 214, 255, 220),
                1.5F);
            drawList.AddLine(
                anchorCenter,
                pivotLogical,
                IM_COL32(255, 192, 64, 180),
                1.0F);
            for (const auto& [logical, handle] : {
                     std::pair{anchorMinLogical, anchorMinHandle},
                     std::pair{anchorMaxLogical, anchorMaxHandle},
                     std::pair{pivotLogical, pivotHandle}}) {
                drawList.AddLine(logical, handle, IM_COL32(210, 220, 235, 180));
            }
            drawList.AddRectFilled(
                {anchorMinHandle.x - 5.0F, anchorMinHandle.y - 5.0F},
                {anchorMinHandle.x + 5.0F, anchorMinHandle.y + 5.0F},
                IM_COL32(64, 214, 255, 255));
            drawList.AddRectFilled(
                {anchorMaxHandle.x - 5.0F, anchorMaxHandle.y - 5.0F},
                {anchorMaxHandle.x + 5.0F, anchorMaxHandle.y + 5.0F},
                IM_COL32(112, 236, 255, 255));
            drawList.AddCircleFilled(
                pivotHandle, 5.5F, IM_COL32(255, 192, 64, 255));

            if (impl_->activeGizmoNode != selectedNodeId &&
                !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                impl_->activeGizmo.reset();
                impl_->activeGizmoNode.clear();
            }
            bool began = false;
            if (!previewMode && inputEnabled && !impl_->activeGizmo.has_value() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                constexpr float hitRadiusSquared = 10.0F * 10.0F;
                float best = hitRadiusSquared;
                for (const auto& [field, handle] : {
                         std::pair{Result::GizmoField::AnchorMin, anchorMinHandle},
                         std::pair{Result::GizmoField::AnchorMax, anchorMaxHandle},
                         std::pair{Result::GizmoField::Pivot, pivotHandle}}) {
                    const float distance = DistanceSquared(mouse, handle);
                    if (distance <= best) {
                        best = distance;
                        impl_->activeGizmo = field;
                        impl_->activeGizmoNode = std::string{selectedNodeId};
                        began = true;
                    }
                }
            }
            if (impl_->activeGizmo.has_value() &&
                impl_->activeGizmoNode == selectedNodeId) {
                const Result::GizmoField field = *impl_->activeGizmo;
                const ImVec2 offset = field == Result::GizmoField::AnchorMin
                    ? anchorMinOffset
                    : field == Result::GizmoField::AnchorMax
                        ? anchorMaxOffset
                        : pivotOffset;
                const bool pivotField = field == Result::GizmoField::Pivot;
                const ImVec2 basis = pivotField ? minimum : parentMinimum;
                const float width = std::max(0.001F, pivotField
                    ? selected->boundsPixels.width
                    : parentBounds.width);
                const float height = std::max(0.001F, pivotField
                    ? selected->boundsPixels.height
                    : parentBounds.height);
                float x = (mouse.x - offset.x - basis.x) / width;
                float y = (mouse.y - offset.y - basis.y) / height;
                if (snapToGrid && gridSize > 0.0F) {
                    const float snapPixels = gridSize * designScale;
                    x = std::round(x * width / snapPixels) * snapPixels / width;
                    y = std::round(y * height / snapPixels) * snapPixels / height;
                }
                x = std::clamp(x, 0.0F, 1.0F);
                y = std::clamp(y, 0.0F, 1.0F);
                if (field == Result::GizmoField::AnchorMin) {
                    x = std::min(x, anchorMaxX);
                    y = std::min(y, anchorMaxY);
                } else if (field == Result::GizmoField::AnchorMax) {
                    x = std::max(x, anchorMinX);
                    y = std::max(y, anchorMinY);
                }
                const bool finished =
                    ImGui::IsMouseReleased(ImGuiMouseButton_Left);
                result.gizmoEdit = Result::GizmoEdit{
                    field,
                    x,
                    y,
                    began,
                    finished,
                };
                if (finished) {
                    impl_->activeGizmo.reset();
                    impl_->activeGizmoNode.clear();
                }
            }
        }
        drawList.PopClipRect();
    }

    if (!previewMode) {
        result.actions.clear();
    }
    return result;
}

} // namespace Gyo::Tools::UiEditor
