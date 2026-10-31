#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
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

namespace Engine::Ui {

// Zero is not a valid code; the numeric values are not a data contract.
enum class UiErrorCode : std::uint8_t {
    InvalidJson = 1,
    UnsupportedSchema,
    UnsupportedVersion,
    ValidationFailed,
    MissingReference,
    BindingTypeMismatch,
    MissingBinding,
    RuntimeState,
    ResourceFailure,
    RenderSubmissionFailed,
};

[[nodiscard]] constexpr const char* ToString(const UiErrorCode code) noexcept {
    switch (code) {
    case UiErrorCode::InvalidJson: return "InvalidJson";
    case UiErrorCode::UnsupportedSchema: return "UnsupportedSchema";
    case UiErrorCode::UnsupportedVersion: return "UnsupportedVersion";
    case UiErrorCode::ValidationFailed: return "ValidationFailed";
    case UiErrorCode::MissingReference: return "MissingReference";
    case UiErrorCode::BindingTypeMismatch: return "BindingTypeMismatch";
    case UiErrorCode::MissingBinding: return "MissingBinding";
    case UiErrorCode::RuntimeState: return "RuntimeState";
    case UiErrorCode::ResourceFailure: return "ResourceFailure";
    case UiErrorCode::RenderSubmissionFailed: return "RenderSubmissionFailed";
    }
    return "Unknown";
}

// UI's error type. Besides the common code, message and detail it locates
// document errors by source and JSON pointer. There is no default
// constructor: a UiError always describes a failure.
struct UiError final {
    UiError(const UiErrorCode errorCode, std::string errorMessage, std::string errorSource = {},
            std::string errorJsonPointer = {}, std::string errorDetail = {})
        : code(errorCode),
          message(std::move(errorMessage)),
          source(std::move(errorSource)),
          jsonPointer(std::move(errorJsonPointer)),
          detail(std::move(errorDetail)) {}

    UiErrorCode code;
    std::string message;
    std::string source;
    std::string jsonPointer;
    std::string detail;
};
static_assert(Base::CodedError<UiError>);

template <class T>
using UiResult = Base::Result<T, UiError>;

// RGB channels are linear-light. Alpha is linear coverage.
// Deliberately separate from Render::Color, though alike: gyo_ui does not
// depend on render (gyo_ui_renderer bridges the two), and Math gains no color
// type without a consumer of its own. For the same reason UiValidation keeps
// its own finiteness check.
struct UiColor final {
    float red{1.0F};
    float green{1.0F};
    float blue{1.0F};
    float alpha{1.0F};
};

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
};

using UiScalarValue = std::variant<std::string, std::int64_t, double, bool>;

struct UiListItem final {
    std::map<std::string, UiScalarValue, std::less<>> fields;
};

struct UiList final {
    std::vector<UiListItem> items;
};

using UiBindingValue =
    std::variant<std::string, std::int64_t, double, bool, UiList>;
using UiBindingTable = std::unordered_map<std::string, UiBindingValue>;

using UiActionPayload = std::variant<std::monostate, double, std::string>;

struct UiActionEvent final {
    std::string action;
    std::string sourceElement;
    UiActionPayload payload;
};

// Text editing input for the text field being edited, converted from the
// platform's text events by the product (Design Invariant 7): which key means
// what is the product's mapping, what each kind does to the edit is Ui's.
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
    // Composition: the cursor inside text in code points; -1 at its end.
    std::int32_t cursor{-1};
};

struct UiInputFrame final {
    // While a text field is being edited, activate submits it and cancel
    // discards the edit.
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

struct UiQuadDraw final {
    Math::Rect destinationPixels{};
    UiColor color{};
    Math::Rect clipPixels{
        0.0F,
        0.0F,
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
};

// The editing marks of a text field, drawn by the renderer over its text.
// Offsets are byte offsets into UiTextDraw::utf8 at code point boundaries.
struct UiTextCaret final {
    std::size_t cursor{};
    std::size_t compositionBegin{};
    std::size_t compositionEnd{};   // equal to compositionBegin: no composition
    std::size_t selectionBegin{};
    std::size_t selectionEnd{};     // equal to selectionBegin: no selection
    UiColor color{};
};

struct UiTextDraw final {
    Math::Rect boundsPixels{};
    std::string utf8;
    std::string fontAssetId;
    float pointSizePixels{};
    UiColor color{};
    UiHorizontalAlign horizontalAlign{UiHorizontalAlign::Left};
    UiVerticalAlign verticalAlign{UiVerticalAlign::Top};
    Math::Rect clipPixels{
        0.0F,
        0.0F,
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
    std::optional<UiTextCaret> caret; // text_field being edited
};

struct UiImageDraw final {
    Math::Rect destinationPixels{};
    Math::Rect sourceUv{0.0F, 0.0F, 1.0F, 1.0F};
    std::string textureAssetId;
    UiColor tint{};
    Math::Rect clipPixels{
        0.0F,
        0.0F,
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
};

using UiDrawCommand = std::variant<UiQuadDraw, UiImageDraw, UiTextDraw>;

struct UiDrawList final {
    std::vector<UiDrawCommand> commands;
};

struct UiInteractionState final {
    std::string activeCanvas;
    std::string focusedElement;
    std::string capturedElement;
    // The text field being edited; the product turns the platform's text
    // input on while it is not empty.
    std::string editingElement;
};

[[nodiscard]] UiResult<UiColor> DecodeSrgbHexColor(
    std::string_view value,
    std::string_view source = {},
    std::string_view jsonPointer = {});

[[nodiscard]] std::string EncodeSrgbHexColor(const UiColor& color);

} // namespace Engine::Ui
