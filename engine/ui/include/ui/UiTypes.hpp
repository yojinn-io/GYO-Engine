#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "engine/base/Result.hpp"

namespace Engine::Ui {

enum class UiErrorCode : std::uint8_t {
    None = 0,
    InvalidJson,
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

struct UiError final {
    UiErrorCode code{UiErrorCode::None};
    std::string message;
    std::string source;
    std::string jsonPointer;
};

template <class T>
using UiResult = Base::Result<T, UiError>;

struct UiFloat2 final {
    float x{};
    float y{};
};

struct UiRect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

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
    UiFloat2 pointerPixels{};
    bool pointerPrimaryPressed{};
    bool pointerPrimaryHeld{};
    bool pointerPrimaryReleased{};
};

struct UiQuadDraw final {
    UiRect destinationPixels{};
    UiColor color{};
    UiRect clipPixels{
        0.0F,
        0.0F,
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
};

struct UiTextDraw final {
    UiRect boundsPixels{};
    std::string utf8;
    std::string fontAssetId;
    float pointSizePixels{};
    UiColor color{};
    UiHorizontalAlign horizontalAlign{UiHorizontalAlign::Left};
    UiVerticalAlign verticalAlign{UiVerticalAlign::Top};
    UiRect clipPixels{
        0.0F,
        0.0F,
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
};

struct UiImageDraw final {
    UiRect destinationPixels{};
    UiRect sourceUv{0.0F, 0.0F, 1.0F, 1.0F};
    std::string textureAssetId;
    UiColor tint{};
    UiRect clipPixels{
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
