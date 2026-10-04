#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ui/UiDocument.hpp"

namespace Engine::Ui {

struct UiEvaluatedElement final {
    std::string id;
    UiElementType type{UiElementType::Container};
    // Pixel bounds may have a negative size when a negative size_delta exceeds
    // the anchored span; hit testing treats such bounds as empty.
    Math::Rect boundsPixels{};
    Math::Rect clipPixels{};
    std::size_t drawOrder{};
    bool interactive{};
    std::optional<std::size_t> listItemIndex;
};

// Layout rules shared by the runtime, the renderer and tools that preview UI.

// The design canvas fitted into a viewport: one uniform scale, centred
// (letterboxed or pillarboxed).
struct UiCanvasFit final {
    float scale{1.0F};
    Math::Vec2 offset{};
};
[[nodiscard]] UiCanvasFit FitDesignCanvas(Math::Vec2 designSize, UiViewport viewport) noexcept;

// UI hit testing is half-open (right and bottom edges excluded, empty rects
// never hit), unlike the closed Math::Contains.
[[nodiscard]] bool ContainsUiPoint(Math::Rect rect, Math::Vec2 point) noexcept;

// The top-left corner of a text block of the given extent aligned inside bounds.
[[nodiscard]] Math::Vec2 AlignUiText(
    Math::Rect bounds, Math::Vec2 extent,
    UiHorizontalAlign horizontal, UiVerticalAlign vertical) noexcept;

// Returns the topmost evaluated element containing point. The caller owns the
// span and therefore the returned pointer's lifetime.
[[nodiscard]] const UiEvaluatedElement* HitTestUiLayout(
    std::span<const UiEvaluatedElement> layout,
    Math::Vec2 pointPixels,
    bool interactiveOnly = false) noexcept;

class UiRuntime final {
public:
    UiRuntime();
    ~UiRuntime();
    UiRuntime(UiRuntime&&) noexcept;
    UiRuntime& operator=(UiRuntime&&) noexcept;
    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;

    [[nodiscard]] UiResult<void> Initialize(
        std::shared_ptr<const UiDocument> document);
    void Reset() noexcept;

    [[nodiscard]] UiResult<void> ActivateCanvas(std::string_view canvasId);

    [[nodiscard]] UiResult<std::vector<UiActionEvent>> Update(
        const UiInputFrame& input,
        const UiBindingTable& bindings,
        UiViewport viewport);

    [[nodiscard]] UiResult<UiDrawList> Compose(
        const UiBindingTable& bindings,
        UiViewport viewport) const;

    [[nodiscard]] UiResult<UiDrawList> ComposePreview(
        UiViewport viewport) const;

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluateLayout(
        const UiBindingTable& bindings,
        UiViewport viewport) const;

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluatePreviewLayout(
        UiViewport viewport) const;

    [[nodiscard]] const UiInteractionState& InteractionState() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Ui
