#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace Engine::Ui {
namespace {

using Json = nlohmann::ordered_json;

struct ParseFailure final : std::exception {
    UiError error;

    explicit ParseFailure(UiError value) : error(std::move(value)) {}
};

[[noreturn]] void Fail(
    UiErrorCode code,
    std::string message,
    std::string_view source,
    std::string path) {
    throw ParseFailure{{
        code,
        std::move(message),
        std::string(source),
        std::move(path),
    }};
}

[[nodiscard]] bool IsValidUtf8(std::string_view value) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char first = bytes[index++];
        if (first <= 0x7FU) {
            continue;
        }
        std::size_t trailing{};
        std::uint32_t codePoint{};
        if (first >= 0xC2U && first <= 0xDFU) {
            trailing = 1;
            codePoint = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            trailing = 2;
            codePoint = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            trailing = 3;
            codePoint = first & 0x07U;
        } else {
            return false;
        }
        if (index + trailing > value.size()) {
            return false;
        }
        for (std::size_t offset = 0; offset < trailing; ++offset) {
            const unsigned char next = bytes[index++];
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (next & 0x3FU);
        }
        if ((trailing == 2 && codePoint < 0x800U) ||
            (trailing == 3 && codePoint < 0x10000U) ||
            codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
    }
    return true;
}

void RequireObject(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_object()) {
        Fail(UiErrorCode::ValidationFailed, "expected an object", source, path);
    }
}

void RejectUnknownKeys(
    const Json& value,
    std::initializer_list<std::string_view> allowed,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    for (const auto& [key, ignored] : value.items()) {
        static_cast<void>(ignored);
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            Fail(
                UiErrorCode::ValidationFailed,
                "unknown property '" + key + "'",
                source,
                path + "/" + key);
        }
    }
}

[[nodiscard]] const Json& Required(
    const Json& object,
    std::string_view key,
    std::string_view source,
    std::string_view path) {
    const auto found = object.find(std::string(key));
    if (found == object.end()) {
        Fail(
            UiErrorCode::ValidationFailed,
            "missing required property '" + std::string(key) + "'",
            source,
            std::string(path) + "/" + std::string(key));
    }
    return *found;
}

[[nodiscard]] std::string ReadString(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_string()) {
        Fail(UiErrorCode::ValidationFailed, "expected a string", source, path);
    }
    return value.get<std::string>();
}

[[nodiscard]] double ReadNumber(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_number()) {
        Fail(UiErrorCode::ValidationFailed, "expected a number", source, path);
    }
    const double number = value.get<double>();
    if (!std::isfinite(number)) {
        Fail(UiErrorCode::ValidationFailed, "number must be finite", source, path);
    }
    return number;
}

[[nodiscard]] std::int64_t ReadInteger(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        Fail(UiErrorCode::ValidationFailed, "expected an integer", source, path);
    }
    try {
        return value.get<std::int64_t>();
    } catch (const std::exception&) {
        Fail(UiErrorCode::ValidationFailed, "integer is out of range", source, path);
    }
}

[[nodiscard]] bool ReadBool(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_boolean()) {
        Fail(UiErrorCode::ValidationFailed, "expected a boolean", source, path);
    }
    return value.get<bool>();
}

[[nodiscard]] UiFloat2 ParseFloat2(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_array() || value.size() != 2) {
        Fail(
            UiErrorCode::ValidationFailed,
            "expected a two-number array",
            source,
            path);
    }
    return {
        static_cast<float>(ReadNumber(value[0], source, path + "/0")),
        static_cast<float>(ReadNumber(value[1], source, path + "/1")),
    };
}

[[nodiscard]] UiRectTransform ParseRectTransform(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RejectUnknownKeys(
        value,
        {"anchor_min", "anchor_max", "pivot", "position", "size_delta"},
        source,
        path);
    return {
        ParseFloat2(Required(value, "anchor_min", source, path), source, path + "/anchor_min"),
        ParseFloat2(Required(value, "anchor_max", source, path), source, path + "/anchor_max"),
        ParseFloat2(Required(value, "pivot", source, path), source, path + "/pivot"),
        ParseFloat2(Required(value, "position", source, path), source, path + "/position"),
        ParseFloat2(Required(value, "size_delta", source, path), source, path + "/size_delta"),
    };
}

[[nodiscard]] UiRect ParseRect4(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_array() || value.size() != 4) {
        Fail(
            UiErrorCode::ValidationFailed,
            "expected a four-number array",
            source,
            path);
    }
    return {
        static_cast<float>(ReadNumber(value[0], source, path + "/0")),
        static_cast<float>(ReadNumber(value[1], source, path + "/1")),
        static_cast<float>(ReadNumber(value[2], source, path + "/2")),
        static_cast<float>(ReadNumber(value[3], source, path + "/3")),
    };
}

[[nodiscard]] UiNumberFormat ParseNumberFormat(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RejectUnknownKeys(value, {"decimals", "show_plus"}, source, path);
    const auto decimals = ReadInteger(
        Required(value, "decimals", source, path),
        source,
        path + "/decimals");
    if (decimals < 0 || decimals > 9) {
        Fail(
            UiErrorCode::ValidationFailed,
            "decimals must be between 0 and 9",
            source,
            path + "/decimals");
    }
    return {
        static_cast<std::uint8_t>(decimals),
        ReadBool(
            Required(value, "show_plus", source, path),
            source,
            path + "/show_plus"),
    };
}

[[nodiscard]] UiValueReference ParseValueReference(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    const bool hasBinding = value.contains("binding");
    const bool hasItemField = value.contains("item_field");
    if (hasBinding == hasItemField) {
        Fail(
            UiErrorCode::ValidationFailed,
            "value source must contain exactly one of binding or item_field",
            source,
            path);
    }
    return {
        hasBinding ? UiValueReferenceKind::Binding : UiValueReferenceKind::ItemField,
        ReadString(
            value[hasBinding ? "binding" : "item_field"],
            source,
            path + (hasBinding ? "/binding" : "/item_field")),
    };
}

[[nodiscard]] UiTextPlaceholder ParseTextPlaceholder(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RejectUnknownKeys(value, {"binding", "item_field", "format"}, source, path);
    UiTextPlaceholder result;
    result.value = ParseValueReference(value, source, path);
    if (value.contains("format")) {
        result.format = ParseNumberFormat(value["format"], source, path + "/format");
    }
    return result;
}

[[nodiscard]] UiTextSource ParseTextSource(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    if (value.contains("literal")) {
        RejectUnknownKeys(value, {"literal"}, source, path);
        UiTextSource result;
        result.literal = ReadString(value["literal"], source, path + "/literal");
        return result;
    }
    if (value.contains("compose")) {
        RejectUnknownKeys(value, {"compose"}, source, path);
        const Json& compose = value["compose"];
        RejectUnknownKeys(compose, {"format", "placeholders"}, source, path + "/compose");
        UiTextSource result;
        result.kind = UiTextSourceKind::Compose;
        result.composeFormat = ReadString(
            Required(compose, "format", source, path + "/compose"),
            source,
            path + "/compose/format");
        const Json& placeholders = Required(
            compose,
            "placeholders",
            source,
            path + "/compose");
        RequireObject(placeholders, source, path + "/compose/placeholders");
        for (const auto& [name, placeholder] : placeholders.items()) {
            result.placeholders.emplace(
                name,
                ParseTextPlaceholder(
                    placeholder,
                    source,
                    path + "/compose/placeholders/" + name));
        }
        return result;
    }
    if (value.contains("select")) {
        RejectUnknownKeys(value, {"select"}, source, path);
        const Json& selection = value["select"];
        RejectUnknownKeys(selection, {"binding", "cases"}, source, path + "/select");
        UiTextSource result;
        result.kind = UiTextSourceKind::Select;
        result.selectionBinding = ReadString(
            Required(selection, "binding", source, path + "/select"),
            source,
            path + "/select/binding");
        const Json& cases = Required(selection, "cases", source, path + "/select");
        RequireObject(cases, source, path + "/select/cases");
        for (const auto& [key, text] : cases.items()) {
            result.cases.emplace(
                key,
                ReadString(text, source, path + "/select/cases/" + key));
        }
        return result;
    }
    RejectUnknownKeys(value, {"binding", "item_field", "format"}, source, path);
    UiTextSource result;
    result.kind = UiTextSourceKind::Value;
    result.value = ParseValueReference(value, source, path);
    if (value.contains("format")) {
        result.format = ParseNumberFormat(value["format"], source, path + "/format");
    }
    return result;
}

[[nodiscard]] UiColorSource ParseColorSource(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    if (value.contains("color")) {
        RejectUnknownKeys(value, {"color"}, source, path);
        UiColorSource result;
        result.color = ReadString(value["color"], source, path + "/color");
        return result;
    }
    RejectUnknownKeys(value, {"select"}, source, path);
    const Json& selection = Required(value, "select", source, path);
    RejectUnknownKeys(selection, {"binding", "cases"}, source, path + "/select");
    UiColorSource result;
    result.kind = UiColorSourceKind::Select;
    result.selectionBinding = ReadString(
        Required(selection, "binding", source, path + "/select"),
        source,
        path + "/select/binding");
    const Json& cases = Required(selection, "cases", source, path + "/select");
    RequireObject(cases, source, path + "/select/cases");
    for (const auto& [key, color] : cases.items()) {
        result.cases.emplace(
            key,
            ReadString(color, source, path + "/select/cases/" + key));
    }
    return result;
}

[[nodiscard]] UiHorizontalAlign ParseHorizontalAlign(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    const std::string text = ReadString(value, source, path);
    if (text == "left") return UiHorizontalAlign::Left;
    if (text == "center") return UiHorizontalAlign::Center;
    if (text == "right") return UiHorizontalAlign::Right;
    Fail(UiErrorCode::ValidationFailed, "unknown horizontal alignment", source, path);
}

[[nodiscard]] UiVerticalAlign ParseVerticalAlign(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    const std::string text = ReadString(value, source, path);
    if (text == "top") return UiVerticalAlign::Top;
    if (text == "center") return UiVerticalAlign::Center;
    if (text == "bottom") return UiVerticalAlign::Bottom;
    Fail(UiErrorCode::ValidationFailed, "unknown vertical alignment", source, path);
}

[[nodiscard]] UiButtonColors ParseButtonColors(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RejectUnknownKeys(value, {"normal", "focused", "pressed"}, source, path);
    UiButtonColors result;
    result.normal = ReadString(
        Required(value, "normal", source, path), source, path + "/normal");
    result.focused = value.contains("focused")
        ? ReadString(value["focused"], source, path + "/focused")
        : result.normal;
    result.pressed = value.contains("pressed")
        ? ReadString(value["pressed"], source, path + "/pressed")
        : result.focused;
    return result;
}

void ParseTextVisual(
    const Json& value,
    UiElement& result,
    std::string_view source,
    const std::string& path,
    bool centeredByDefault) {
    result.text = ParseTextSource(
        Required(value, "text", source, path), source, path + "/text");
    if (value.contains("font")) {
        result.textStyle.font = ReadString(value["font"], source, path + "/font");
    }
    if (value.contains("point_size")) {
        result.textStyle.pointSize = static_cast<float>(
            ReadNumber(value["point_size"], source, path + "/point_size"));
    }
    result.textStyle.color = ParseColorSource(
        Required(value, "text_color", source, path),
        source,
        path + "/text_color");
    result.textStyle.horizontalAlign = centeredByDefault
        ? UiHorizontalAlign::Center
        : UiHorizontalAlign::Left;
    result.textStyle.verticalAlign = centeredByDefault
        ? UiVerticalAlign::Center
        : UiVerticalAlign::Top;
    if (value.contains("horizontal_align")) {
        result.textStyle.horizontalAlign = ParseHorizontalAlign(
            value["horizontal_align"], source, path + "/horizontal_align");
    }
    if (value.contains("vertical_align")) {
        result.textStyle.verticalAlign = ParseVerticalAlign(
            value["vertical_align"], source, path + "/vertical_align");
    }
}

[[nodiscard]] UiElementType ParseElementType(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    const std::string type = ReadString(value, source, path);
    if (type == "container") return UiElementType::Container;
    if (type == "panel") return UiElementType::Panel;
    if (type == "image") return UiElementType::Image;
    if (type == "text") return UiElementType::Text;
    if (type == "button") return UiElementType::Button;
    if (type == "horizontal_slider") return UiElementType::HorizontalSlider;
    if (type == "fixed_step_list") return UiElementType::FixedStepList;
    Fail(UiErrorCode::ValidationFailed, "unknown element type '" + type + "'", source, path);
}

[[nodiscard]] UiElement ParseElement(
    const Json& value,
    std::string_view source,
    const std::string& path);

[[nodiscard]] std::vector<UiElement> ParseElements(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    if (!value.is_array()) {
        Fail(UiErrorCode::ValidationFailed, "expected an element array", source, path);
    }
    std::vector<UiElement> result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        result.push_back(ParseElement(
            value[index], source, path + "/" + std::to_string(index)));
    }
    return result;
}

UiElement ParseElement(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    UiElement result;
    result.type = ParseElementType(
        Required(value, "type", source, path), source, path + "/type");

    switch (result.type) {
    case UiElementType::Container:
        RejectUnknownKeys(value, {"type", "id", "rect", "children"}, source, path);
        break;
    case UiElementType::Panel:
        RejectUnknownKeys(value, {"type", "id", "rect", "color", "children"}, source, path);
        break;
    case UiElementType::Image:
        RejectUnknownKeys(
            value,
            {"type", "id", "rect", "texture_asset", "source_uv", "tint", "children"},
            source,
            path);
        break;
    case UiElementType::Text:
        RejectUnknownKeys(
            value,
            {"type", "id", "rect", "children", "text", "font", "point_size",
             "text_color", "horizontal_align", "vertical_align"},
            source,
            path);
        break;
    case UiElementType::Button:
        RejectUnknownKeys(
            value,
            {"type", "id", "rect", "children", "text", "font", "point_size",
             "text_color", "horizontal_align", "vertical_align", "action", "background"},
            source,
            path);
        break;
    case UiElementType::HorizontalSlider:
        RejectUnknownKeys(
            value,
            {"type", "id", "rect", "children", "text", "font", "point_size",
             "text_color", "horizontal_align", "vertical_align", "action", "background",
             "binding", "minimum", "maximum", "step", "value_format", "track_color",
             "fill_color", "thumb_color"},
            source,
            path);
        break;
    case UiElementType::FixedStepList:
        RejectUnknownKeys(
            value,
            {"type", "id", "rect", "children", "binding", "max_items", "item_step",
             "template"},
            source,
            path);
        break;
    }

    result.id = ReadString(
        Required(value, "id", source, path), source, path + "/id");
    result.rect = ParseRectTransform(
        Required(value, "rect", source, path), source, path + "/rect");
    if (value.contains("children")) {
        result.children = ParseElements(value["children"], source, path + "/children");
    }

    switch (result.type) {
    case UiElementType::Container:
        break;
    case UiElementType::Panel:
        result.color = ParseColorSource(
            Required(value, "color", source, path), source, path + "/color");
        break;
    case UiElementType::Image:
        result.textureAsset = ReadString(
            Required(value, "texture_asset", source, path),
            source,
            path + "/texture_asset");
        if (value.contains("source_uv")) {
            result.sourceUv = ParseRect4(value["source_uv"], source, path + "/source_uv");
        }
        result.color = ParseColorSource(
            Required(value, "tint", source, path), source, path + "/tint");
        break;
    case UiElementType::Text:
        ParseTextVisual(value, result, source, path, false);
        break;
    case UiElementType::Button:
        ParseTextVisual(value, result, source, path, true);
        result.action = ReadString(
            Required(value, "action", source, path), source, path + "/action");
        result.background = ParseButtonColors(
            Required(value, "background", source, path), source, path + "/background");
        break;
    case UiElementType::HorizontalSlider:
        ParseTextVisual(value, result, source, path, false);
        result.action = ReadString(
            Required(value, "action", source, path), source, path + "/action");
        result.background = ParseButtonColors(
            Required(value, "background", source, path), source, path + "/background");
        result.binding = ReadString(
            Required(value, "binding", source, path), source, path + "/binding");
        result.minimum = ReadNumber(
            Required(value, "minimum", source, path), source, path + "/minimum");
        result.maximum = ReadNumber(
            Required(value, "maximum", source, path), source, path + "/maximum");
        result.step = ReadNumber(
            Required(value, "step", source, path), source, path + "/step");
        result.valueFormat = ParseNumberFormat(
            Required(value, "value_format", source, path), source, path + "/value_format");
        result.trackColor = ReadString(
            Required(value, "track_color", source, path), source, path + "/track_color");
        result.fillColor = ReadString(
            Required(value, "fill_color", source, path), source, path + "/fill_color");
        result.thumbColor = ReadString(
            Required(value, "thumb_color", source, path), source, path + "/thumb_color");
        break;
    case UiElementType::FixedStepList: {
        result.binding = ReadString(
            Required(value, "binding", source, path), source, path + "/binding");
        const std::int64_t maximumItems = ReadInteger(
            Required(value, "max_items", source, path), source, path + "/max_items");
        if (maximumItems < 0) {
            Fail(UiErrorCode::ValidationFailed, "max_items cannot be negative", source, path + "/max_items");
        }
        result.maxItems = static_cast<std::size_t>(maximumItems);
        result.itemStep = ParseFloat2(
            Required(value, "item_step", source, path), source, path + "/item_step");
        result.itemTemplate = ParseElements(
            Required(value, "template", source, path), source, path + "/template");
        break;
    }
    }
    return result;
}

[[nodiscard]] UiScalarType ParseScalarType(
    std::string_view value,
    std::string_view source,
    const std::string& path) {
    if (value == "string") return UiScalarType::String;
    if (value == "integer") return UiScalarType::Integer;
    if (value == "number") return UiScalarType::Number;
    if (value == "boolean") return UiScalarType::Boolean;
    if (value == "enum") return UiScalarType::Enum;
    Fail(UiErrorCode::ValidationFailed, "unknown scalar type", source, path);
}

[[nodiscard]] UiBindingType ParseBindingType(
    std::string_view value,
    std::string_view source,
    const std::string& path) {
    if (value == "string") return UiBindingType::String;
    if (value == "integer") return UiBindingType::Integer;
    if (value == "number") return UiBindingType::Number;
    if (value == "boolean") return UiBindingType::Boolean;
    if (value == "enum") return UiBindingType::Enum;
    if (value == "list<object>") return UiBindingType::List;
    Fail(UiErrorCode::ValidationFailed, "unknown binding type", source, path);
}

[[nodiscard]] UiScalarValue ParseScalar(
    const Json& value,
    UiScalarType type,
    std::string_view source,
    const std::string& path) {
    switch (type) {
    case UiScalarType::String:
    case UiScalarType::Enum:
        return ReadString(value, source, path);
    case UiScalarType::Integer:
        return ReadInteger(value, source, path);
    case UiScalarType::Number:
        return ReadNumber(value, source, path);
    case UiScalarType::Boolean:
        return ReadBool(value, source, path);
    }
    Fail(UiErrorCode::ValidationFailed, "unknown scalar type", source, path);
}

[[nodiscard]] UiBindingValue ParsePreview(
    const Json& value,
    const UiBindingDeclaration& declaration,
    std::string_view source,
    const std::string& path) {
    switch (declaration.type) {
    case UiBindingType::String:
        return ReadString(value, source, path);
    case UiBindingType::Integer:
        return ReadInteger(value, source, path);
    case UiBindingType::Number:
        return ReadNumber(value, source, path);
    case UiBindingType::Boolean:
        return ReadBool(value, source, path);
    case UiBindingType::Enum:
        return ReadString(value, source, path);
    case UiBindingType::List: {
        if (!value.is_array()) {
            Fail(UiErrorCode::ValidationFailed, "list preview must be an array", source, path);
        }
        UiList list;
        for (std::size_t index = 0; index < value.size(); ++index) {
            const Json& item = value[index];
            const std::string itemPath = path + "/" + std::to_string(index);
            RequireObject(item, source, itemPath);
            for (const auto& [key, ignored] : item.items()) {
                static_cast<void>(ignored);
                if (!declaration.itemFields.contains(key)) {
                    Fail(
                        UiErrorCode::ValidationFailed,
                        "unknown list item field '" + key + "'",
                        source,
                        itemPath + "/" + key);
                }
            }
            UiListItem parsed;
            for (const auto& [name, fieldType] : declaration.itemFields) {
                parsed.fields.emplace(
                    name,
                    ParseScalar(
                        Required(item, name, source, itemPath),
                        fieldType,
                        source,
                        itemPath + "/" + name));
            }
            list.items.push_back(std::move(parsed));
        }
        return list;
    }
    }
    Fail(UiErrorCode::ValidationFailed, "unknown binding type", source, path);
}

[[nodiscard]] UiBindingDeclaration ParseBinding(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RequireObject(value, source, path);
    const UiBindingType type = ParseBindingType(
        ReadString(Required(value, "type", source, path), source, path + "/type"),
        source,
        path + "/type");
    if (type == UiBindingType::Enum) {
        RejectUnknownKeys(value, {"id", "type", "values", "preview"}, source, path);
    } else if (type == UiBindingType::List) {
        RejectUnknownKeys(value, {"id", "type", "item_fields", "preview"}, source, path);
    } else {
        RejectUnknownKeys(value, {"id", "type", "preview"}, source, path);
    }

    UiBindingDeclaration result;
    result.id = ReadString(Required(value, "id", source, path), source, path + "/id");
    result.type = type;
    if (type == UiBindingType::Enum) {
        const Json& values = Required(value, "values", source, path);
        if (!values.is_array()) {
            Fail(UiErrorCode::ValidationFailed, "values must be an array", source, path + "/values");
        }
        for (std::size_t index = 0; index < values.size(); ++index) {
            result.enumValues.push_back(ReadString(
                values[index], source, path + "/values/" + std::to_string(index)));
        }
    }
    if (type == UiBindingType::List) {
        const Json& fields = Required(value, "item_fields", source, path);
        RequireObject(fields, source, path + "/item_fields");
        for (const auto& [name, fieldType] : fields.items()) {
            const UiScalarType parsed = ParseScalarType(
                ReadString(fieldType, source, path + "/item_fields/" + name),
                source,
                path + "/item_fields/" + name);
            if (parsed == UiScalarType::Enum) {
                Fail(
                    UiErrorCode::ValidationFailed,
                    "list item enum fields are not supported in v1",
                    source,
                    path + "/item_fields/" + name);
            }
            result.itemFields.emplace(name, parsed);
        }
    }
    result.preview = ParsePreview(
        Required(value, "preview", source, path), result, source, path + "/preview");
    return result;
}

[[nodiscard]] UiCanvas ParseCanvas(
    const Json& value,
    std::string_view source,
    const std::string& path) {
    RejectUnknownKeys(
        value,
        {"id", "backdrop_color", "default_focus", "cancel_action", "focus_order", "children"},
        source,
        path);
    UiCanvas result;
    result.id = ReadString(Required(value, "id", source, path), source, path + "/id");
    result.backdropColor = ReadString(
        Required(value, "backdrop_color", source, path), source, path + "/backdrop_color");
    if (value.contains("default_focus")) {
        result.defaultFocus = ReadString(value["default_focus"], source, path + "/default_focus");
    }
    if (value.contains("cancel_action")) {
        result.cancelAction = ReadString(value["cancel_action"], source, path + "/cancel_action");
    }
    const Json& focusOrder = Required(value, "focus_order", source, path);
    if (!focusOrder.is_array()) {
        Fail(UiErrorCode::ValidationFailed, "focus_order must be an array", source, path + "/focus_order");
    }
    for (std::size_t index = 0; index < focusOrder.size(); ++index) {
        result.focusOrder.push_back(ReadString(
            focusOrder[index], source, path + "/focus_order/" + std::to_string(index)));
    }
    result.children = ParseElements(
        Required(value, "children", source, path), source, path + "/children");
    return result;
}

[[nodiscard]] UiDocument ParseDocument(
    const Json& root,
    std::string_view source) {
    RejectUnknownKeys(
        root,
        {"schema", "version", "design_canvas", "fonts", "default_font", "colors",
         "actions", "bindings", "canvases"},
        source,
        "");

    const std::string schema = ReadString(
        Required(root, "schema", source, ""), source, "/schema");
    if (schema != kUiSchemaName) {
        Fail(
            UiErrorCode::UnsupportedSchema,
            "unsupported UI schema '" + schema + "'",
            source,
            "/schema");
    }
    const std::int64_t version = ReadInteger(
        Required(root, "version", source, ""), source, "/version");
    if (version != kUiSchemaVersion) {
        Fail(
            UiErrorCode::UnsupportedVersion,
            "unsupported UI schema version " + std::to_string(version),
            source,
            "/version");
    }

    UiDocument result;
    const Json& design = Required(root, "design_canvas", source, "");
    RejectUnknownKeys(design, {"size", "scale_mode"}, source, "/design_canvas");
    result.designCanvas.size = ParseFloat2(
        Required(design, "size", source, "/design_canvas"),
        source,
        "/design_canvas/size");
    const std::string scaleMode = ReadString(
        Required(design, "scale_mode", source, "/design_canvas"),
        source,
        "/design_canvas/scale_mode");
    if (scaleMode != "fit") {
        Fail(
            UiErrorCode::ValidationFailed,
            "v1 supports only the fit scale mode",
            source,
            "/design_canvas/scale_mode");
    }

    const Json& fonts = Required(root, "fonts", source, "");
    RequireObject(fonts, source, "/fonts");
    for (const auto& [alias, assetId] : fonts.items()) {
        result.fonts.emplace(alias, ReadString(assetId, source, "/fonts/" + alias));
    }
    result.defaultFont = ReadString(
        Required(root, "default_font", source, ""), source, "/default_font");

    const Json& colors = Required(root, "colors", source, "");
    RequireObject(colors, source, "/colors");
    for (const auto& [name, encoded] : colors.items()) {
        const std::string path = "/colors/" + name;
        auto decoded = DecodeSrgbHexColor(ReadString(encoded, source, path), source, path);
        if (!decoded) {
            throw ParseFailure{std::move(decoded).error()};
        }
        result.colors.emplace(name, std::move(decoded).value());
    }

    const Json& actions = Required(root, "actions", source, "");
    if (!actions.is_array()) {
        Fail(UiErrorCode::ValidationFailed, "actions must be an array", source, "/actions");
    }
    for (std::size_t index = 0; index < actions.size(); ++index) {
        const std::string path = "/actions/" + std::to_string(index);
        const Json& action = actions[index];
        RejectUnknownKeys(action, {"id", "payload"}, source, path);
        UiActionDeclaration declaration;
        declaration.id = ReadString(Required(action, "id", source, path), source, path + "/id");
        const std::string payload = ReadString(
            Required(action, "payload", source, path), source, path + "/payload");
        if (payload == "none") {
            declaration.payload = UiActionPayloadType::None;
        } else if (payload == "number") {
            declaration.payload = UiActionPayloadType::Number;
        } else {
            Fail(UiErrorCode::ValidationFailed, "unknown action payload type", source, path + "/payload");
        }
        result.actions.push_back(std::move(declaration));
    }

    const Json& bindings = Required(root, "bindings", source, "");
    if (!bindings.is_array()) {
        Fail(UiErrorCode::ValidationFailed, "bindings must be an array", source, "/bindings");
    }
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        result.bindings.push_back(ParseBinding(
            bindings[index], source, "/bindings/" + std::to_string(index)));
    }

    const Json& canvases = Required(root, "canvases", source, "");
    if (!canvases.is_array()) {
        Fail(UiErrorCode::ValidationFailed, "canvases must be an array", source, "/canvases");
    }
    for (std::size_t index = 0; index < canvases.size(); ++index) {
        result.canvases.push_back(ParseCanvas(
            canvases[index], source, "/canvases/" + std::to_string(index)));
    }
    return result;
}

} // namespace

UiResult<UiDocument> UiDocumentCodec::Parse(
    std::string_view utf8Json,
    std::string_view source) {
    if (!IsValidUtf8(utf8Json)) {
        return UiResult<UiDocument>::Err({
            UiErrorCode::InvalidJson,
            "UI document is not valid UTF-8",
            std::string(source),
            {},
        });
    }

    try {
        const Json json = Json::parse(utf8Json.begin(), utf8Json.end());
        UiDocument document = ParseDocument(json, source);
        auto validation = Validate(document);
        if (!validation) {
            UiError error = std::move(validation).error();
            error.source = std::string(source);
            return UiResult<UiDocument>::Err(std::move(error));
        }
        return UiResult<UiDocument>::Ok(std::move(document));
    } catch (const ParseFailure& failure) {
        return UiResult<UiDocument>::Err(failure.error);
    } catch (const nlohmann::json::exception& error) {
        return UiResult<UiDocument>::Err({
            UiErrorCode::InvalidJson,
            error.what(),
            std::string(source),
            {},
        });
    } catch (const std::exception& error) {
        return UiResult<UiDocument>::Err({
            UiErrorCode::InvalidJson,
            error.what(),
            std::string(source),
            {},
        });
    }
}

} // namespace Engine::Ui
