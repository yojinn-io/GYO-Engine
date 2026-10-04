#pragma once

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

using UiActionPayload = std::variant<std::monostate, double>;

struct UiActionEvent final {
    std::string action;
    std::string sourceElement;
    UiActionPayload payload;
};

struct UiInputFrame final {
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
};

[[nodiscard]] UiResult<UiColor> DecodeSrgbHexColor(
    std::string_view value,
    std::string_view source = {},
    std::string_view jsonPointer = {});

[[nodiscard]] std::string EncodeSrgbHexColor(const UiColor& color);

} // namespace Engine::Ui
