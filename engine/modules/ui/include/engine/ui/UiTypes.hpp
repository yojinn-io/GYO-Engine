#pragma once

// The ui Module's common types (docs/architecture/target/v2/ui.zh-Hant.md, section 4): the failure type, colours,
// binding values, actions, one frame of neutral input and the draw list. Ported from master's engine/ui
// (UiTypes.hpp); the draw list is one step further than master's: text is laid out and its glyphs placed in the
// atlas here, so a renderer only copies rectangles (Bevy rasterizes in the main world too, bevy_ui text_system).

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Vec2.hpp"

namespace GYO::Engine::Ui {

// Zero is not a valid code; the numeric values are not a data contract.
enum class UiFailureCode : std::uint8_t {
    InvalidJson = 1,
    UnsupportedSchema,
    UnsupportedVersion,
    ValidationFailed,
    MissingReference,
    BindingTypeMismatch,
    MissingBinding,
    RuntimeState,
};

[[nodiscard]] constexpr const char* ToString(const UiFailureCode code) noexcept {
    switch (code) {
    case UiFailureCode::InvalidJson: return "InvalidJson";
    case UiFailureCode::UnsupportedSchema: return "UnsupportedSchema";
    case UiFailureCode::UnsupportedVersion: return "UnsupportedVersion";
    case UiFailureCode::ValidationFailed: return "ValidationFailed";
    case UiFailureCode::MissingReference: return "MissingReference";
    case UiFailureCode::BindingTypeMismatch: return "BindingTypeMismatch";
    case UiFailureCode::MissingBinding: return "MissingBinding";
    case UiFailureCode::RuntimeState: return "RuntimeState";
    }
    return "Unknown";
}

// E2 (N25): this document, this update or this composition was not done; everything else is fine. Besides the
// common code, message and detail it locates document errors by source and JSON pointer.
struct UiFailure final {
    UiFailure(const UiFailureCode failureCode, std::string failureMessage, std::string failureSource = {},
              std::string failureJsonPointer = {}, std::string failureDetail = {})
        : code(failureCode),
          message(std::move(failureMessage)),
          detail(std::move(failureDetail)),
          source(std::move(failureSource)),
          jsonPointer(std::move(failureJsonPointer)) {}

    UiFailureCode code;
    std::string message;
    std::string detail;
    std::string source;
    std::string jsonPointer;
};
static_assert(Base::CodedError<UiFailure>);

template <class T>
using UiResult = Base::Result<T, UiFailure>;

// RGB channels are linear-light, alpha is linear coverage (decoded from the document's sRGB hex). A renderer that
// stores sRGB bytes encodes them (ui_render).
struct UiColor final {
    float red{1.0F};
    float green{1.0F};
    float blue{1.0F};
    float alpha{1.0F};

    friend bool operator==(const UiColor&, const UiColor&) = default;
};

// The area the UI is laid out in, in pixels.
struct UiViewport final {
    float width{};
    float height{};
};

enum class UiHorizontalAlign : std::uint8_t {
    Left,
    Center,
    Right,
};

enum class UiVerticalAlign : std::uint8_t {
    Top,
    Center,
    Bottom,
};

struct UiNumberFormat final {
    std::uint8_t decimals{};
    bool showPlus{};

    friend bool operator==(const UiNumberFormat&, const UiNumberFormat&) = default;
};

using UiScalarValue = std::variant<std::string, std::int64_t, double, bool>;

struct UiListItem final {
    std::map<std::string, UiScalarValue, std::less<>> fields;

    friend bool operator==(const UiListItem&, const UiListItem&) = default;
};

struct UiList final {
    std::vector<UiListItem> items;

    friend bool operator==(const UiList&, const UiList&) = default;
};

using UiBindingValue = std::variant<std::string, std::int64_t, double, bool, UiList>;
using UiBindingTable = std::unordered_map<std::string, UiBindingValue>;

using UiActionPayload = std::variant<std::monostate, double, std::string>;

struct UiActionEvent final {
    std::string action;
    std::string sourceElement;
    UiActionPayload payload;

    friend bool operator==(const UiActionEvent&, const UiActionEvent&) = default;
};

// One edit of the text field being edited. Which key means which edit is the input mapping's (world_ui's default
// MapUiInput, or the author's); what each edit does is ui's.
enum class UiTextInputKind : std::uint8_t {
    Insert,      // committed text, typed or pasted (text)
    Composition, // an input method's current composition (text, cursor); shown only
    Backspace,
    Delete,
    MoveLeft,
    MoveRight,
    MoveHome,
    MoveEnd,
    SelectAll,
};

struct UiTextInput final {
    UiTextInputKind kind{UiTextInputKind::Insert};
    std::string text;
    // Composition: the caret inside text in UTF-16 code units (the unit SDL reports on Windows and macOS; equal to
    // code points inside the Basic Multilingual Plane); -1 at its end.
    std::int32_t cursor{-1};

    friend bool operator==(const UiTextInput&, const UiTextInput&) = default;
};

// One frame of neutral input (master's UiInputFrame). Pixels are those of the area the UI is laid out in.
struct UiInput final {
    // While a text field is being edited, activate submits it and cancel discards the edit.
    std::vector<UiTextInput> text;
    bool focusPreviousPressed{};
    bool focusNextPressed{};
    bool adjustPreviousPressed{};
    bool adjustNextPressed{};
    bool activatePressed{};
    bool cancelPressed{};
    bool pointerAvailable{};
    Math::Vec2 pointerPixels{};
    bool pointerPrimaryPressed{};
    bool pointerPrimaryHeld{};
    bool pointerPrimaryReleased{};
};

// No clip: everything.
inline constexpr Math::Rect kNoClip{
    0.0F,
    0.0F,
    (std::numeric_limits<float>::max)(),
    (std::numeric_limits<float>::max)(),
};

// ---- The draw list: what Compose produces, in drawing order, in pixels of the viewport (y down).

// A filled rectangle (panels, buttons, the caret, a selection, a composition's underline).
struct UiQuadDraw final {
    Math::Rect rect{};
    UiColor color{};
    Math::Rect clip{kNoClip};
};

// An image element: a region of a picture named by its asset id.
struct UiImageDraw final {
    Math::Rect rect{};
    // 0..1 of the picture, origin at its top-left corner.
    Math::Rect sourceUv{0.0F, 0.0F, 1.0F, 1.0F};
    std::string textureAsset;
    UiColor tint{};
    Math::Rect clip{kNoClip};
};

// One glyph: a rectangle of a page of UiText's atlas (whole pixels), drawn at rect (whole pixels, the glyph's
// own size) in color.
struct UiGlyphDraw final {
    Math::Rect rect{};
    std::uint32_t page = 0;
    std::uint32_t atlasX = 0;
    std::uint32_t atlasY = 0;
    UiColor color{};
    Math::Rect clip{kNoClip};
};

using UiDrawCommand = std::variant<UiQuadDraw, UiImageDraw, UiGlyphDraw>;

struct UiDrawList final {
    std::vector<UiDrawCommand> commands;
};

struct UiInteractionState final {
    std::string activeCanvas;
    std::string focusedElement;
    std::string capturedElement;
    // The text field being edited; text input is wanted while it is not empty.
    std::string editingElement;
};

[[nodiscard]] UiResult<UiColor> DecodeSrgbHexColor(std::string_view value, std::string_view source = {},
                                                   std::string_view jsonPointer = {});

[[nodiscard]] std::string EncodeSrgbHexColor(const UiColor& color);

} // namespace GYO::Engine::Ui
