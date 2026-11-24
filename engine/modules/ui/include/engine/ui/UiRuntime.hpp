#pragma once

// A gyo.ui document at run time (docs/architecture/target/v2/ui.zh-Hant.md, section 4.5; Design Invariants UI-3 to
// UI-5): the active canvas, the focus, the slider being dragged and the text field being edited; layout, interaction
// and the draw list. Ported from master's engine/ui (UiRuntime); text is measured and placed with the text Module
// through UiText (layout "measures text synchronously", SDL declaration section 3.3).
//
// Pixels are those of the area the UI is laid out in (UiViewport), origin at its top-left corner, y down; the design
// canvas is fitted into it (one uniform scale, centred). world_ui lays a document out in its camera's viewport.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/ui/UiDocument.hpp"
#include "engine/ui/UiText.hpp"

namespace GYO::Engine::Ui {

struct UiEvaluatedElement final {
    std::string id;
    UiElementType type{UiElementType::Container};
    // Pixel bounds may have a negative size when a negative size_delta exceeds the anchored span; hit testing treats
    // such bounds as empty.
    Math::Rect boundsPixels{};
    Math::Rect clipPixels{};
    std::size_t drawOrder{};
    bool interactive{};
    std::optional<std::size_t> listItemIndex;
};

// The design canvas fitted into a viewport: one uniform scale, centred (letterboxed or pillarboxed).
struct UiCanvasFit final {
    float scale{1.0F};
    Math::Vec2 offset{};
};
[[nodiscard]] UiCanvasFit FitDesignCanvas(Math::Vec2 designSize, UiViewport viewport) noexcept;

// UI hit testing is half-open (right and bottom edges excluded, empty rects never hit), unlike the closed
// Math::Contains.
[[nodiscard]] bool ContainsUiPoint(Math::Rect rect, Math::Vec2 point) noexcept;

// The top-left corner of a text block of the given extent aligned inside bounds.
[[nodiscard]] Math::Vec2 AlignUiText(Math::Rect bounds, Math::Vec2 extent, UiHorizontalAlign horizontal,
                                     UiVerticalAlign vertical) noexcept;

// Returns the topmost evaluated element containing point. The caller owns the span and therefore the returned
// pointer's lifetime.
[[nodiscard]] const UiEvaluatedElement* HitTestUiLayout(std::span<const UiEvaluatedElement> layout,
                                                        Math::Vec2 pointPixels, bool interactiveOnly = false) noexcept;

// The product's character rule for a text field: given the field's id and committed text (typed or pasted), it
// returns the text to keep. ui adds no rule of its own beyond single-line text and max_length.
using UiTextFilter = std::function<std::string(std::string_view element, std::string_view text)>;

// Text editing on UTF-8, by code point: the pieces the text field uses, kept apart so they can be tested on their own.
[[nodiscard]] std::size_t UiCodePointCount(std::string_view utf8) noexcept;
// The byte offset of the code point before (after) offset; offset itself at the start (end).
[[nodiscard]] std::size_t UiPreviousCodePoint(std::string_view utf8, std::size_t offset) noexcept;
[[nodiscard]] std::size_t UiNextCodePoint(std::string_view utf8, std::size_t offset) noexcept;
// Committed text as one line: whole code points only, without control characters.
[[nodiscard]] std::string UiSingleLineText(std::string_view utf8);
// The byte offset of the UTF-16 code unit index units in valid UTF-8 (a code point outside the Basic Multilingual
// Plane counts two), clamped to the end.
[[nodiscard]] std::size_t UiUtf16Offset(std::string_view utf8, std::size_t units) noexcept;

// The text field being edited this frame, for the input method's candidate window: the field and the caret.
struct UiEditArea final {
    Math::Rect field{};
    // The caret's x from the field's left edge, in pixels.
    float caretX = 0.0F;
};

// What Compose produces.
struct UiFrame final {
    UiDrawList drawList;
    // A text field is being edited.
    std::optional<UiEditArea> editArea;
};

class UiRuntime final {
public:
    UiRuntime();
    ~UiRuntime();
    UiRuntime(UiRuntime&&) noexcept;
    UiRuntime& operator=(UiRuntime&&) noexcept;
    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;

    // Validates the document first (UI-3).
    [[nodiscard]] UiResult<void> Initialize(std::shared_ptr<const UiDocument> document);
    void Reset() noexcept;
    [[nodiscard]] const std::shared_ptr<const UiDocument>& Document() const noexcept;

    [[nodiscard]] UiResult<void> ActivateCanvas(std::string_view canvasId);

    // Without a filter every committed character is kept.
    void SetTextFilter(UiTextFilter filter);

    // One frame of input. text places the caret where a text field is pressed (a field whose font is not in text
    // puts it at the end).
    [[nodiscard]] UiResult<std::vector<UiActionEvent>> Update(const UiInput& input, const UiBindingTable& bindings,
                                                             UiViewport viewport, UiText& text);

    // The draw list; text is laid out and its glyphs drawn into text's atlas. A text whose font is not in text is
    // left out.
    [[nodiscard]] UiResult<UiFrame> Compose(const UiBindingTable& bindings, UiViewport viewport, UiText& text) const;
    // The same with each binding's preview value (an editor's preview).
    [[nodiscard]] UiResult<UiFrame> ComposePreview(UiViewport viewport, UiText& text) const;

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluateLayout(const UiBindingTable& bindings,
                                                                          UiViewport viewport) const;
    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluatePreviewLayout(UiViewport viewport) const;

    [[nodiscard]] const UiInteractionState& InteractionState() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::Ui
