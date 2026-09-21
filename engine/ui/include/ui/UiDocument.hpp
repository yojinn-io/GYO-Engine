#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ui/UiTypes.hpp"

namespace Engine::Ui {

inline constexpr std::string_view kUiSchemaName = "gyo.ui";
inline constexpr std::uint32_t kUiSchemaVersion = 1;

enum class UiScaleMode : std::uint8_t {
    Fit,
};

struct UiDesignCanvas final {
    UiFloat2 size{1280.0F, 720.0F};
    UiScaleMode scaleMode{UiScaleMode::Fit};
};

enum class UiActionPayloadType : std::uint8_t {
    None,
    Number,
};

struct UiActionDeclaration final {
    std::string id;
    UiActionPayloadType payload{UiActionPayloadType::None};
};

enum class UiScalarType : std::uint8_t {
    String,
    Integer,
    Number,
    Boolean,
    Enum,
};

enum class UiBindingType : std::uint8_t {
    String,
    Integer,
    Number,
    Boolean,
    Enum,
    List,
};

struct UiBindingDeclaration final {
    std::string id;
    UiBindingType type{UiBindingType::String};
    std::vector<std::string> enumValues;
    std::map<std::string, UiScalarType, std::less<>> itemFields;
    UiBindingValue preview{std::string{}};
};

struct UiRectTransform final {
    UiFloat2 anchorMin{};
    UiFloat2 anchorMax{};
    UiFloat2 pivot{};
    UiFloat2 position{};
    UiFloat2 sizeDelta{};
};

enum class UiValueReferenceKind : std::uint8_t {
    Binding,
    ItemField,
};

struct UiValueReference final {
    UiValueReferenceKind kind{UiValueReferenceKind::Binding};
    std::string id;
};

struct UiTextPlaceholder final {
    UiValueReference value;
    UiNumberFormat format{};
};

enum class UiTextSourceKind : std::uint8_t {
    Literal,
    Value,
    Compose,
    Select,
};

struct UiTextSource final {
    UiTextSourceKind kind{UiTextSourceKind::Literal};
    std::string literal;
    UiValueReference value;
    UiNumberFormat format{};
    std::string composeFormat;
    std::map<std::string, UiTextPlaceholder, std::less<>> placeholders;
    std::string selectionBinding;
    std::map<std::string, std::string, std::less<>> cases;
};

enum class UiColorSourceKind : std::uint8_t {
    Color,
    Select,
};

struct UiColorSource final {
    UiColorSourceKind kind{UiColorSourceKind::Color};
    std::string color;
    std::string selectionBinding;
    std::map<std::string, std::string, std::less<>> cases;
};

struct UiTextStyle final {
    std::string font;
    float pointSize{16.0F};
    UiColorSource color;
    UiHorizontalAlign horizontalAlign{UiHorizontalAlign::Left};
    UiVerticalAlign verticalAlign{UiVerticalAlign::Top};
};

struct UiButtonColors final {
    std::string normal;
    std::string focused;
    std::string pressed;
};

enum class UiElementType : std::uint8_t {
    Container,
    Panel,
    Image,
    Text,
    Button,
    HorizontalSlider,
    FixedStepList,
};

// Closed v1 element representation. Fields not used by an element kind remain
// at their defaults and are never serialized.
struct UiElement final {
    UiElementType type{UiElementType::Container};
    std::string id;
    UiRectTransform rect;
    std::vector<UiElement> children;

    // panel
    UiColorSource color;

    // image
    std::string textureAsset;
    UiRect sourceUv{0.0F, 0.0F, 1.0F, 1.0F};

    // text / button
    UiTextSource text;
    UiTextStyle textStyle;

    // button / horizontal_slider
    std::string action;
    UiButtonColors background;

    // horizontal_slider
    std::string binding;
    double minimum{};
    double maximum{1.0};
    double step{0.1};
    UiNumberFormat valueFormat{};
    std::string trackColor;
    std::string fillColor;
    std::string thumbColor;

    // fixed_step_list
    std::size_t maxItems{};
    UiFloat2 itemStep{};
    std::vector<UiElement> itemTemplate;
};

struct UiCanvas final {
    std::string id;
    std::string backdropColor;
    std::optional<std::string> defaultFocus;
    std::optional<std::string> cancelAction;
    std::vector<std::string> focusOrder;
    std::vector<UiElement> children;
};

struct UiDocument final {
    UiDesignCanvas designCanvas;
    std::map<std::string, std::string, std::less<>> fonts;
    std::string defaultFont;
    std::map<std::string, UiColor, std::less<>> colors;
    std::vector<UiActionDeclaration> actions;
    std::vector<UiBindingDeclaration> bindings;
    std::vector<UiCanvas> canvases;
};

} // namespace Engine::Ui
