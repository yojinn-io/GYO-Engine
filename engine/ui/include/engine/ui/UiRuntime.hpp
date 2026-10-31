#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/ui/UiDocument.hpp"

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

// The product's character rule for a text field (Design Invariant 5): given
// the field's id and committed text (typed or pasted), it returns the text to
// keep. Ui adds no rule of its own beyond single-line text and max_length.
using UiTextFilter = std::function<std::string(std::string_view element, std::string_view text)>;

// Text editing on UTF-8, by code point: the pieces the text field uses, kept
// apart so they can be tested on their own.
[[nodiscard]] std::size_t UiCodePointCount(std::string_view utf8) noexcept;
// The byte offset of the code point before (after) offset; offset itself at the start (end).
[[nodiscard]] std::size_t UiPreviousCodePoint(std::string_view utf8, std::size_t offset) noexcept;
[[nodiscard]] std::size_t UiNextCodePoint(std::string_view utf8, std::size_t offset) noexcept;
// Committed text as one line: whole code points only, without control characters.
[[nodiscard]] std::string UiSingleLineText(std::string_view utf8);

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

    // Without a filter every committed character is kept.
    void SetTextFilter(UiTextFilter filter);

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
