// Reading gyo.ui v1/v2 (ported from master's engine/ui/src/UiDocumentCodec.cpp). The text is parsed by the JSON
// Module (strict RFC 8259: a repeated key, a byte order mark or invalid UTF-8 is InvalidJson); the tree is then read
// into a UiDocument with master's rules: the same required and optional keys, defaults, unknown keys rejected, and
// failures located by the same JSON Pointers. Parse validates what it read (UiValidation.cpp).
//
// Master threw on the first failure; here a Reader keeps the first failure and every later read returns a default,
// so the failure reported is the one master reported and the half-read document is thrown away.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/json/Document.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Engine::Ui {
namespace {

using OptionalValue = std::optional<Json::Value>;
using OptionalObject = std::optional<Json::ObjectView>;
using OptionalArray = std::optional<Json::ArrayView>;

class Reader final {
public:
    explicit Reader(const std::string_view source) : source_(source) {}

    [[nodiscard]] bool Failed() const noexcept { return failure_.has_value(); }
    [[nodiscard]] UiFailure TakeFailure() && { return std::move(*failure_); }

    // Only the first failure is kept: it is the one master threw.
    void Fail(const UiFailureCode code, std::string message, std::string path) {
        if (!failure_) failure_.emplace(code, std::move(message), std::string(source_), std::move(path));
    }
    void Fail(UiFailure failure) {
        if (!failure_) failure_.emplace(std::move(failure));
    }
    void Invalid(std::string message, std::string path) {
        Fail(UiFailureCode::ValidationFailed, std::move(message), std::move(path));
    }

    [[nodiscard]] std::string_view Source() const noexcept { return source_; }

    [[nodiscard]] OptionalObject Object(const OptionalValue& value, const std::string& path) {
        if (!Usable(value)) return std::nullopt;
        auto object = value->AsObject();
        if (!object) {
            Invalid("expected an object", path);
            return std::nullopt;
        }
        return *object;
    }

    [[nodiscard]] OptionalObject ObjectWithKeys(const OptionalValue& value,
                                                const std::initializer_list<std::string_view> allowed,
                                                const std::string& path) {
        return ObjectWithKeys(value, std::span<const std::string_view>(allowed.begin(), allowed.size()), path);
    }

    // The object, or nullopt and ValidationFailed at the first key not in allowed (master's RejectUnknownKeys).
    [[nodiscard]] OptionalObject ObjectWithKeys(const OptionalValue& value,
                                                const std::span<const std::string_view> allowed,
                                                const std::string& path) {
        OptionalObject object = Object(value, path);
        if (!object) return std::nullopt;
        for (const Json::Member member : *object) {
            bool known = false;
            for (const std::string_view key : allowed) {
                if (key == member.key) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                Invalid("unknown property '" + std::string(member.key) + "'", path + "/" + std::string(member.key));
                return std::nullopt;
            }
        }
        return object;
    }

    // The member, or nullopt and ValidationFailed at path/key.
    [[nodiscard]] OptionalValue Required(const OptionalObject& object, const std::string_view key,
                                         const std::string_view path) {
        if (!object) {
            GYO_ASSERT(Failed());
            return std::nullopt;
        }
        OptionalValue found = object->Find(key);
        if (!found) {
            Invalid("missing required property '" + std::string(key) + "'",
                    std::string(path) + "/" + std::string(key));
        }
        return found;
    }

    // The member if present; no failure when it is not.
    [[nodiscard]] static OptionalValue Find(const OptionalObject& object, const std::string_view key) {
        if (!object) return std::nullopt;
        return object->Find(key);
    }

    [[nodiscard]] OptionalArray Array(const OptionalValue& value, const std::string& message, const std::string& path) {
        if (!Usable(value)) return std::nullopt;
        auto array = value->AsArray();
        if (!array) {
            Invalid(message, path);
            return std::nullopt;
        }
        return *array;
    }

    [[nodiscard]] std::string String(const OptionalValue& value, const std::string& path) {
        if (!Usable(value)) return {};
        auto text = value->AsString();
        if (!text) {
            Invalid("expected a string", path);
            return {};
        }
        return std::string(*text);
    }

    // Any JSON number; the JSON Module reads only finite ones.
    [[nodiscard]] double Number(const OptionalValue& value, const std::string& path) {
        if (!Usable(value)) return 0.0;
        auto number = value->AsDouble();
        if (!number) {
            Invalid("expected a number", path);
            return 0.0;
        }
        return *number;
    }

    // An integer literal (master: nlohmann's integer kinds) that fits std::int64_t.
    [[nodiscard]] std::int64_t Integer(const OptionalValue& value, const std::string& path) {
        if (!Usable(value)) return 0;
        if (!value->IsIntegerLiteral()) {
            Invalid("expected an integer", path);
            return 0;
        }
        auto integer = value->AsInt64();
        if (!integer) {
            Invalid("integer is out of range", path);
            return 0;
        }
        return *integer;
    }

    [[nodiscard]] bool Bool(const OptionalValue& value, const std::string& path) {
        if (!Usable(value)) return false;
        auto flag = value->AsBool();
        if (!flag) {
            Invalid("expected a boolean", path);
            return false;
        }
        return *flag;
    }

private:
    [[nodiscard]] bool Usable(const OptionalValue& value) const {
        if (!value) {
            GYO_ASSERT(Failed());
            return false;
        }
        return !Failed();
    }

    std::string_view source_;
    std::optional<UiFailure> failure_;
};

// Geometry is held in float. A finite double that rounds beyond the float range becomes an infinity of its sign, which
// validation then rejects where the value matters. Master's plain cast gave the same on IEEE platforms; the bound is
// spelled out because casting an out-of-range double is undefined in C++. 0x1.ffffffp+127 is halfway between the
// largest float and 2^128: from there on, round to nearest (ties to even) gives infinity.
[[nodiscard]] float ToFloat(const double value) noexcept {
    constexpr double kOverflow = 0x1.ffffffp+127;
    static_assert(kOverflow > static_cast<double>(std::numeric_limits<float>::max()));
    if (value >= kOverflow) return std::numeric_limits<float>::infinity();
    if (value <= -kOverflow) return -std::numeric_limits<float>::infinity();
    return static_cast<float>(value);
}

[[nodiscard]] Math::Vec2 ParseFloat2(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalArray array = reader.Array(value, "expected a two-number array", path);
    if (!array) return {};
    if (array->Size() != 2) {
        reader.Invalid("expected a two-number array", path);
        return {};
    }
    auto element = array->begin();
    const auto x = ToFloat(reader.Number(*element, path + "/0"));
    ++element;
    const auto y = ToFloat(reader.Number(*element, path + "/1"));
    return {x, y};
}

[[nodiscard]] UiRectTransform ParseRectTransform(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object =
        reader.ObjectWithKeys(value, {"anchor_min", "anchor_max", "pivot", "position", "size_delta"}, path);
    UiRectTransform result;
    result.anchorMin = ParseFloat2(reader, reader.Required(object, "anchor_min", path), path + "/anchor_min");
    result.anchorMax = ParseFloat2(reader, reader.Required(object, "anchor_max", path), path + "/anchor_max");
    result.pivot = ParseFloat2(reader, reader.Required(object, "pivot", path), path + "/pivot");
    result.position = ParseFloat2(reader, reader.Required(object, "position", path), path + "/position");
    result.sizeDelta = ParseFloat2(reader, reader.Required(object, "size_delta", path), path + "/size_delta");
    return result;
}

[[nodiscard]] Math::Rect ParseRect4(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalArray array = reader.Array(value, "expected a four-number array", path);
    if (!array) return {};
    if (array->Size() != 4) {
        reader.Invalid("expected a four-number array", path);
        return {};
    }
    float numbers[4]{};
    std::size_t index = 0;
    for (const Json::Value element : *array) {
        numbers[index] = ToFloat(reader.Number(element, path + "/" + std::to_string(index)));
        ++index;
    }
    return {numbers[0], numbers[1], numbers[2], numbers[3]};
}

[[nodiscard]] UiNumberFormat ParseNumberFormat(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object = reader.ObjectWithKeys(value, {"decimals", "show_plus"}, path);
    const std::int64_t decimals = reader.Integer(reader.Required(object, "decimals", path), path + "/decimals");
    if (!reader.Failed() && (decimals < 0 || decimals > 9)) {
        reader.Invalid("decimals must be between 0 and 9", path + "/decimals");
    }
    const bool showPlus = reader.Bool(reader.Required(object, "show_plus", path), path + "/show_plus");
    return {static_cast<std::uint8_t>(decimals), showPlus};
}

[[nodiscard]] UiValueReference ParseValueReference(Reader& reader, const OptionalValue& value,
                                                   const std::string& path) {
    const OptionalObject object = reader.Object(value, path);
    if (!object) return {};
    const OptionalValue binding = Reader::Find(object, "binding");
    const OptionalValue itemField = Reader::Find(object, "item_field");
    if (binding.has_value() == itemField.has_value()) {
        reader.Invalid("value source must contain exactly one of binding or item_field", path);
        return {};
    }
    return {
        binding ? UiValueReferenceKind::Binding : UiValueReferenceKind::ItemField,
        reader.String(binding ? binding : itemField, path + (binding ? "/binding" : "/item_field")),
    };
}

[[nodiscard]] UiTextPlaceholder ParseTextPlaceholder(Reader& reader, const OptionalValue& value,
                                                     const std::string& path) {
    const OptionalObject object = reader.ObjectWithKeys(value, {"binding", "item_field", "format"}, path);
    UiTextPlaceholder result;
    result.value = ParseValueReference(reader, value, path);
    if (const OptionalValue format = Reader::Find(object, "format")) {
        result.format = ParseNumberFormat(reader, format, path + "/format");
    }
    return result;
}

// A selection's cases: an object of strings (texts or colour names).
void ParseCases(Reader& reader, const OptionalObject& selection, const std::string& path,
                std::map<std::string, std::string, std::less<>>& cases) {
    const OptionalObject object = reader.Object(reader.Required(selection, "cases", path), path + "/cases");
    if (!object) return;
    for (const Json::Member member : *object) {
        const std::string key(member.key);
        cases.emplace(key, reader.String(member.value, path + "/cases/" + key));
    }
}

[[nodiscard]] UiTextSource ParseTextSource(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object = reader.Object(value, path);
    if (!object) return {};
    UiTextSource result;
    if (const OptionalValue literal = Reader::Find(object, "literal")) {
        static_cast<void>(reader.ObjectWithKeys(value, {"literal"}, path));
        result.literal = reader.String(literal, path + "/literal");
        return result;
    }
    if (const OptionalValue compose = Reader::Find(object, "compose")) {
        static_cast<void>(reader.ObjectWithKeys(value, {"compose"}, path));
        const std::string composePath = path + "/compose";
        const OptionalObject composeObject = reader.ObjectWithKeys(compose, {"format", "placeholders"}, composePath);
        result.kind = UiTextSourceKind::Compose;
        result.composeFormat = reader.String(reader.Required(composeObject, "format", composePath), composePath + "/format");
        const OptionalObject placeholders = reader.Object(reader.Required(composeObject, "placeholders", composePath),
                                                          composePath + "/placeholders");
        if (!placeholders) return result;
        for (const Json::Member member : *placeholders) {
            const std::string name(member.key);
            result.placeholders.emplace(
                name, ParseTextPlaceholder(reader, member.value, composePath + "/placeholders/" + name));
        }
        return result;
    }
    if (const OptionalValue selection = Reader::Find(object, "select")) {
        static_cast<void>(reader.ObjectWithKeys(value, {"select"}, path));
        const std::string selectPath = path + "/select";
        const OptionalObject selectObject = reader.ObjectWithKeys(selection, {"binding", "cases"}, selectPath);
        result.kind = UiTextSourceKind::Select;
        result.selectionBinding = reader.String(reader.Required(selectObject, "binding", selectPath), selectPath + "/binding");
        ParseCases(reader, selectObject, selectPath, result.cases);
        return result;
    }
    static_cast<void>(reader.ObjectWithKeys(value, {"binding", "item_field", "format"}, path));
    result.kind = UiTextSourceKind::Value;
    result.value = ParseValueReference(reader, value, path);
    if (const OptionalValue format = Reader::Find(object, "format")) {
        result.format = ParseNumberFormat(reader, format, path + "/format");
    }
    return result;
}

[[nodiscard]] UiColorSource ParseColorSource(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object = reader.Object(value, path);
    if (!object) return {};
    UiColorSource result;
    if (const OptionalValue color = Reader::Find(object, "color")) {
        static_cast<void>(reader.ObjectWithKeys(value, {"color"}, path));
        result.color = reader.String(color, path + "/color");
        return result;
    }
    static_cast<void>(reader.ObjectWithKeys(value, {"select"}, path));
    const std::string selectPath = path + "/select";
    const OptionalObject selectObject =
        reader.ObjectWithKeys(reader.Required(object, "select", path), {"binding", "cases"}, selectPath);
    result.kind = UiColorSourceKind::Select;
    result.selectionBinding = reader.String(reader.Required(selectObject, "binding", selectPath), selectPath + "/binding");
    ParseCases(reader, selectObject, selectPath, result.cases);
    return result;
}

[[nodiscard]] UiHorizontalAlign ParseHorizontalAlign(Reader& reader, const OptionalValue& value,
                                                     const std::string& path) {
    const std::string text = reader.String(value, path);
    if (text == "left") return UiHorizontalAlign::Left;
    if (text == "center") return UiHorizontalAlign::Center;
    if (text == "right") return UiHorizontalAlign::Right;
    reader.Invalid("unknown horizontal alignment", path);
    return UiHorizontalAlign::Left;
}

[[nodiscard]] UiVerticalAlign ParseVerticalAlign(Reader& reader, const OptionalValue& value, const std::string& path) {
    const std::string text = reader.String(value, path);
    if (text == "top") return UiVerticalAlign::Top;
    if (text == "center") return UiVerticalAlign::Center;
    if (text == "bottom") return UiVerticalAlign::Bottom;
    reader.Invalid("unknown vertical alignment", path);
    return UiVerticalAlign::Top;
}

[[nodiscard]] UiButtonColors ParseButtonColors(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object = reader.ObjectWithKeys(value, {"normal", "focused", "pressed"}, path);
    UiButtonColors result;
    result.normal = reader.String(reader.Required(object, "normal", path), path + "/normal");
    const OptionalValue focused = Reader::Find(object, "focused");
    result.focused = focused ? reader.String(focused, path + "/focused") : result.normal;
    const OptionalValue pressed = Reader::Find(object, "pressed");
    result.pressed = pressed ? reader.String(pressed, path + "/pressed") : result.focused;
    return result;
}

// The text style keys shared by the text elements (everything but "text").
void ParseTextStyle(Reader& reader, const OptionalObject& object, UiElement& result, const std::string& path,
                    const bool centeredByDefault) {
    if (const OptionalValue font = Reader::Find(object, "font")) {
        result.textStyle.font = reader.String(font, path + "/font");
    }
    if (const OptionalValue pointSize = Reader::Find(object, "point_size")) {
        result.textStyle.pointSize = ToFloat(reader.Number(pointSize, path + "/point_size"));
    }
    result.textStyle.color =
        ParseColorSource(reader, reader.Required(object, "text_color", path), path + "/text_color");
    result.textStyle.horizontalAlign = centeredByDefault ? UiHorizontalAlign::Center : UiHorizontalAlign::Left;
    result.textStyle.verticalAlign = centeredByDefault ? UiVerticalAlign::Center : UiVerticalAlign::Top;
    if (const OptionalValue align = Reader::Find(object, "horizontal_align")) {
        result.textStyle.horizontalAlign = ParseHorizontalAlign(reader, align, path + "/horizontal_align");
    }
    if (const OptionalValue align = Reader::Find(object, "vertical_align")) {
        result.textStyle.verticalAlign = ParseVerticalAlign(reader, align, path + "/vertical_align");
    }
}

void ParseTextVisual(Reader& reader, const OptionalObject& object, UiElement& result, const std::string& path,
                     const bool centeredByDefault) {
    result.text = ParseTextSource(reader, reader.Required(object, "text", path), path + "/text");
    ParseTextStyle(reader, object, result, path, centeredByDefault);
}

[[nodiscard]] UiElementType ParseElementType(Reader& reader, const OptionalValue& value, const std::string& path) {
    const std::string type = reader.String(value, path);
    if (reader.Failed()) return UiElementType::Container;
    if (type == "container") return UiElementType::Container;
    if (type == "panel") return UiElementType::Panel;
    if (type == "image") return UiElementType::Image;
    if (type == "text") return UiElementType::Text;
    if (type == "button") return UiElementType::Button;
    if (type == "horizontal_slider") return UiElementType::HorizontalSlider;
    if (type == "fixed_step_list") return UiElementType::FixedStepList;
    if (type == "text_field") return UiElementType::TextField;
    reader.Invalid("unknown element type '" + type + "'", path);
    return UiElementType::Container;
}

[[nodiscard]] UiElement ParseElement(Reader& reader, const OptionalValue& value, const std::string& path);

[[nodiscard]] std::vector<UiElement> ParseElements(Reader& reader, const OptionalValue& value,
                                                   const std::string& path) {
    const OptionalArray array = reader.Array(value, "expected an element array", path);
    std::vector<UiElement> result;
    if (!array) return result;
    result.reserve(array->Size());
    std::size_t index = 0;
    for (const Json::Value element : *array) {
        result.push_back(ParseElement(reader, element, path + "/" + std::to_string(index)));
        if (reader.Failed()) return result;
        ++index;
    }
    return result;
}

// The keys each element type allows (master's per-type RejectUnknownKeys lists).
constexpr std::string_view kContainerKeys[] = {"type", "id", "rect", "children"};
constexpr std::string_view kPanelKeys[] = {"type", "id", "rect", "color", "children"};
constexpr std::string_view kImageKeys[] = {"type", "id", "rect", "texture_asset", "source_uv", "tint", "children"};
constexpr std::string_view kTextKeys[] = {"type", "id", "rect", "children", "text", "font", "point_size",
                                          "text_color", "horizontal_align", "vertical_align"};
constexpr std::string_view kButtonKeys[] = {"type", "id", "rect", "children", "text", "font", "point_size",
                                            "text_color", "horizontal_align", "vertical_align", "action",
                                            "background"};
constexpr std::string_view kSliderKeys[] = {"type", "id", "rect", "children", "text", "font", "point_size",
                                            "text_color", "horizontal_align", "vertical_align", "action",
                                            "background", "binding", "minimum", "maximum", "step", "value_format",
                                            "track_color", "fill_color", "thumb_color"};
constexpr std::string_view kListKeys[] = {"type", "id", "rect", "children", "binding", "max_items", "item_step",
                                          "template"};
constexpr std::string_view kTextFieldKeys[] = {"type", "id", "rect", "children", "font", "point_size", "text_color",
                                               "horizontal_align", "vertical_align", "action", "background",
                                               "binding", "max_length", "placeholder", "placeholder_color"};

[[nodiscard]] std::span<const std::string_view> ElementKeys(const UiElementType type) noexcept {
    switch (type) {
    case UiElementType::Container: return kContainerKeys;
    case UiElementType::Panel: return kPanelKeys;
    case UiElementType::Image: return kImageKeys;
    case UiElementType::Text: return kTextKeys;
    case UiElementType::Button: return kButtonKeys;
    case UiElementType::HorizontalSlider: return kSliderKeys;
    case UiElementType::FixedStepList: return kListKeys;
    case UiElementType::TextField: return kTextFieldKeys;
    }
    return kContainerKeys;
}

UiElement ParseElement(Reader& reader, const OptionalValue& value, const std::string& path) {
    UiElement result;
    const OptionalObject untyped = reader.Object(value, path);
    if (!untyped) return result;
    result.type = ParseElementType(reader, reader.Required(untyped, "type", path), path + "/type");
    const OptionalObject object = reader.ObjectWithKeys(value, ElementKeys(result.type), path);

    result.id = reader.String(reader.Required(object, "id", path), path + "/id");
    result.rect = ParseRectTransform(reader, reader.Required(object, "rect", path), path + "/rect");
    if (const OptionalValue children = Reader::Find(object, "children")) {
        result.children = ParseElements(reader, children, path + "/children");
    }
    if (reader.Failed()) return result;

    switch (result.type) {
    case UiElementType::Container:
        break;
    case UiElementType::Panel:
        result.color = ParseColorSource(reader, reader.Required(object, "color", path), path + "/color");
        break;
    case UiElementType::Image:
        result.textureAsset = reader.String(reader.Required(object, "texture_asset", path), path + "/texture_asset");
        if (const OptionalValue sourceUv = Reader::Find(object, "source_uv")) {
            result.sourceUv = ParseRect4(reader, sourceUv, path + "/source_uv");
        }
        result.color = ParseColorSource(reader, reader.Required(object, "tint", path), path + "/tint");
        break;
    case UiElementType::Text:
        ParseTextVisual(reader, object, result, path, false);
        break;
    case UiElementType::Button:
        ParseTextVisual(reader, object, result, path, true);
        result.action = reader.String(reader.Required(object, "action", path), path + "/action");
        result.background = ParseButtonColors(reader, reader.Required(object, "background", path), path + "/background");
        break;
    case UiElementType::HorizontalSlider:
        ParseTextVisual(reader, object, result, path, false);
        result.action = reader.String(reader.Required(object, "action", path), path + "/action");
        result.background = ParseButtonColors(reader, reader.Required(object, "background", path), path + "/background");
        result.binding = reader.String(reader.Required(object, "binding", path), path + "/binding");
        result.minimum = reader.Number(reader.Required(object, "minimum", path), path + "/minimum");
        result.maximum = reader.Number(reader.Required(object, "maximum", path), path + "/maximum");
        result.step = reader.Number(reader.Required(object, "step", path), path + "/step");
        result.valueFormat =
            ParseNumberFormat(reader, reader.Required(object, "value_format", path), path + "/value_format");
        result.trackColor = reader.String(reader.Required(object, "track_color", path), path + "/track_color");
        result.fillColor = reader.String(reader.Required(object, "fill_color", path), path + "/fill_color");
        result.thumbColor = reader.String(reader.Required(object, "thumb_color", path), path + "/thumb_color");
        break;
    case UiElementType::FixedStepList: {
        result.binding = reader.String(reader.Required(object, "binding", path), path + "/binding");
        const std::int64_t maximumItems = reader.Integer(reader.Required(object, "max_items", path), path + "/max_items");
        if (!reader.Failed() && maximumItems < 0) {
            reader.Invalid("max_items cannot be negative", path + "/max_items");
        }
        result.maxItems = static_cast<std::size_t>(maximumItems);
        result.itemStep = ParseFloat2(reader, reader.Required(object, "item_step", path), path + "/item_step");
        result.itemTemplate = ParseElements(reader, reader.Required(object, "template", path), path + "/template");
        break;
    }
    case UiElementType::TextField: {
        ParseTextStyle(reader, object, result, path, false);
        if (!Reader::Find(object, "vertical_align")) result.textStyle.verticalAlign = UiVerticalAlign::Center;
        result.action = reader.String(reader.Required(object, "action", path), path + "/action");
        result.background = ParseButtonColors(reader, reader.Required(object, "background", path), path + "/background");
        result.binding = reader.String(reader.Required(object, "binding", path), path + "/binding");
        const std::int64_t maximumLength =
            reader.Integer(reader.Required(object, "max_length", path), path + "/max_length");
        if (!reader.Failed() && maximumLength < 1) {
            reader.Invalid("max_length must be at least 1", path + "/max_length");
        }
        result.maxLength = static_cast<std::size_t>(maximumLength);
        if (const OptionalValue placeholder = Reader::Find(object, "placeholder")) {
            result.placeholder = reader.String(placeholder, path + "/placeholder");
            result.placeholderColor =
                reader.String(reader.Required(object, "placeholder_color", path), path + "/placeholder_color");
        } else if (Reader::Find(object, "placeholder_color")) {
            reader.Invalid("placeholder_color needs a placeholder", path + "/placeholder_color");
        }
        break;
    }
    }
    return result;
}

[[nodiscard]] UiScalarType ParseScalarType(Reader& reader, const std::string_view value, const std::string& path) {
    if (value == "string") return UiScalarType::String;
    if (value == "integer") return UiScalarType::Integer;
    if (value == "number") return UiScalarType::Number;
    if (value == "boolean") return UiScalarType::Boolean;
    if (value == "enum") return UiScalarType::Enum;
    reader.Invalid("unknown scalar type", path);
    return UiScalarType::String;
}

[[nodiscard]] UiBindingType ParseBindingType(Reader& reader, const std::string_view value, const std::string& path) {
    if (value == "string") return UiBindingType::String;
    if (value == "integer") return UiBindingType::Integer;
    if (value == "number") return UiBindingType::Number;
    if (value == "boolean") return UiBindingType::Boolean;
    if (value == "enum") return UiBindingType::Enum;
    if (value == "list<object>") return UiBindingType::List;
    reader.Invalid("unknown binding type", path);
    return UiBindingType::String;
}

[[nodiscard]] UiScalarValue ParseScalar(Reader& reader, const OptionalValue& value, const UiScalarType type,
                                        const std::string& path) {
    switch (type) {
    case UiScalarType::String:
    case UiScalarType::Enum:
        return reader.String(value, path);
    case UiScalarType::Integer:
        return reader.Integer(value, path);
    case UiScalarType::Number:
        return reader.Number(value, path);
    case UiScalarType::Boolean:
        return reader.Bool(value, path);
    }
    reader.Invalid("unknown scalar type", path);
    return std::string{};
}

[[nodiscard]] UiBindingValue ParsePreview(Reader& reader, const OptionalValue& value,
                                          const UiBindingDeclaration& declaration, const std::string& path) {
    switch (declaration.type) {
    case UiBindingType::String:
    case UiBindingType::Enum:
        return reader.String(value, path);
    case UiBindingType::Integer:
        return reader.Integer(value, path);
    case UiBindingType::Number:
        return reader.Number(value, path);
    case UiBindingType::Boolean:
        return reader.Bool(value, path);
    case UiBindingType::List: {
        UiList list;
        const OptionalArray array = reader.Array(value, "list preview must be an array", path);
        if (!array) return list;
        std::size_t index = 0;
        for (const Json::Value item : *array) {
            const std::string itemPath = path + "/" + std::to_string(index++);
            const OptionalObject object = reader.Object(item, itemPath);
            if (!object) return list;
            for (const Json::Member member : *object) {
                if (!declaration.itemFields.contains(member.key)) {
                    reader.Invalid("unknown list item field '" + std::string(member.key) + "'",
                                   itemPath + "/" + std::string(member.key));
                    return list;
                }
            }
            UiListItem parsed;
            for (const auto& [name, fieldType] : declaration.itemFields) {
                parsed.fields.emplace(
                    name, ParseScalar(reader, reader.Required(object, name, itemPath), fieldType, itemPath + "/" + name));
            }
            list.items.push_back(std::move(parsed));
        }
        return list;
    }
    }
    reader.Invalid("unknown binding type", path);
    return std::string{};
}

[[nodiscard]] UiBindingDeclaration ParseBinding(Reader& reader, const OptionalValue& value, const std::string& path) {
    UiBindingDeclaration result;
    const OptionalObject untyped = reader.Object(value, path);
    if (!untyped) return result;
    const std::string typeName = reader.String(reader.Required(untyped, "type", path), path + "/type");
    if (reader.Failed()) return result;
    const UiBindingType type = ParseBindingType(reader, typeName, path + "/type");
    OptionalObject object;
    if (type == UiBindingType::Enum) {
        object = reader.ObjectWithKeys(value, {"id", "type", "values", "preview"}, path);
    } else if (type == UiBindingType::List) {
        object = reader.ObjectWithKeys(value, {"id", "type", "item_fields", "preview"}, path);
    } else {
        object = reader.ObjectWithKeys(value, {"id", "type", "preview"}, path);
    }

    result.id = reader.String(reader.Required(object, "id", path), path + "/id");
    result.type = type;
    if (type == UiBindingType::Enum) {
        const OptionalArray values =
            reader.Array(reader.Required(object, "values", path), "values must be an array", path + "/values");
        if (values) {
            std::size_t index = 0;
            for (const Json::Value entry : *values) {
                result.enumValues.push_back(reader.String(entry, path + "/values/" + std::to_string(index++)));
            }
        }
    }
    if (type == UiBindingType::List) {
        const OptionalObject fields =
            reader.Object(reader.Required(object, "item_fields", path), path + "/item_fields");
        if (fields) {
            for (const Json::Member member : *fields) {
                const std::string name(member.key);
                const std::string fieldPath = path + "/item_fields/" + name;
                const std::string fieldType = reader.String(member.value, fieldPath);
                if (reader.Failed()) break;
                const UiScalarType parsed = ParseScalarType(reader, fieldType, fieldPath);
                if (!reader.Failed() && parsed == UiScalarType::Enum) {
                    reader.Invalid("list item enum fields are not supported in v1", fieldPath);
                }
                result.itemFields.emplace(name, parsed);
            }
        }
    }
    if (reader.Failed()) return result;
    result.preview = ParsePreview(reader, reader.Required(object, "preview", path), result, path + "/preview");
    return result;
}

[[nodiscard]] UiCanvas ParseCanvas(Reader& reader, const OptionalValue& value, const std::string& path) {
    const OptionalObject object = reader.ObjectWithKeys(
        value, {"id", "backdrop_color", "default_focus", "cancel_action", "focus_order", "children"}, path);
    UiCanvas result;
    result.id = reader.String(reader.Required(object, "id", path), path + "/id");
    result.backdropColor = reader.String(reader.Required(object, "backdrop_color", path), path + "/backdrop_color");
    if (const OptionalValue focus = Reader::Find(object, "default_focus")) {
        result.defaultFocus = reader.String(focus, path + "/default_focus");
    }
    if (const OptionalValue cancel = Reader::Find(object, "cancel_action")) {
        result.cancelAction = reader.String(cancel, path + "/cancel_action");
    }
    const OptionalArray focusOrder = reader.Array(reader.Required(object, "focus_order", path),
                                                  "focus_order must be an array", path + "/focus_order");
    if (focusOrder) {
        std::size_t index = 0;
        for (const Json::Value id : *focusOrder) {
            result.focusOrder.push_back(reader.String(id, path + "/focus_order/" + std::to_string(index++)));
        }
    }
    if (reader.Failed()) return result;
    result.children = ParseElements(reader, reader.Required(object, "children", path), path + "/children");
    return result;
}

[[nodiscard]] UiDocument ParseDocument(Reader& reader, const Json::Value root) {
    UiDocument result;
    const OptionalObject object = reader.ObjectWithKeys(
        root,
        {"schema", "version", "design_canvas", "fonts", "default_font", "colors", "actions", "bindings", "canvases"},
        "");

    const std::string schema = reader.String(reader.Required(object, "schema", ""), "/schema");
    if (reader.Failed()) return result;
    if (schema != kUiSchemaName) {
        reader.Fail(UiFailureCode::UnsupportedSchema, "unsupported UI schema '" + schema + "'", "/schema");
        return result;
    }
    const std::int64_t version = reader.Integer(reader.Required(object, "version", ""), "/version");
    if (reader.Failed()) return result;
    if (version < static_cast<std::int64_t>(kUiSchemaMinimumVersion) ||
        version > static_cast<std::int64_t>(kUiSchemaVersion)) {
        reader.Fail(UiFailureCode::UnsupportedVersion, "unsupported UI schema version " + std::to_string(version),
                    "/version");
        return result;
    }
    result.version = static_cast<std::uint32_t>(version);

    const OptionalObject design =
        reader.ObjectWithKeys(reader.Required(object, "design_canvas", ""), {"size", "scale_mode"}, "/design_canvas");
    result.designCanvas.size = ParseFloat2(reader, reader.Required(design, "size", "/design_canvas"), "/design_canvas/size");
    const std::string scaleMode =
        reader.String(reader.Required(design, "scale_mode", "/design_canvas"), "/design_canvas/scale_mode");
    if (!reader.Failed() && scaleMode != "fit") {
        reader.Invalid("v1 supports only the fit scale mode", "/design_canvas/scale_mode");
    }

    const OptionalObject fonts = reader.Object(reader.Required(object, "fonts", ""), "/fonts");
    if (fonts) {
        for (const Json::Member member : *fonts) {
            const std::string alias(member.key);
            result.fonts.emplace(alias, reader.String(member.value, "/fonts/" + alias));
        }
    }
    result.defaultFont = reader.String(reader.Required(object, "default_font", ""), "/default_font");

    const OptionalObject colors = reader.Object(reader.Required(object, "colors", ""), "/colors");
    if (colors) {
        for (const Json::Member member : *colors) {
            const std::string name(member.key);
            const std::string path = "/colors/" + name;
            const std::string encoded = reader.String(member.value, path);
            if (reader.Failed()) break;
            auto decoded = DecodeSrgbHexColor(encoded, reader.Source(), path);
            if (!decoded) {
                reader.Fail(std::move(decoded).error());
                break;
            }
            result.colors.emplace(name, *decoded);
        }
    }
    if (reader.Failed()) return result;

    const OptionalArray actions =
        reader.Array(reader.Required(object, "actions", ""), "actions must be an array", "/actions");
    if (actions) {
        std::size_t index = 0;
        for (const Json::Value action : *actions) {
            const std::string path = "/actions/" + std::to_string(index++);
            const OptionalObject actionObject = reader.ObjectWithKeys(action, {"id", "payload"}, path);
            UiActionDeclaration declaration;
            declaration.id = reader.String(reader.Required(actionObject, "id", path), path + "/id");
            const std::string payload = reader.String(reader.Required(actionObject, "payload", path), path + "/payload");
            if (reader.Failed()) break;
            if (payload == "none") {
                declaration.payload = UiActionPayloadType::None;
            } else if (payload == "number") {
                declaration.payload = UiActionPayloadType::Number;
            } else if (payload == "string") {
                declaration.payload = UiActionPayloadType::String;
            } else {
                reader.Invalid("unknown action payload type", path + "/payload");
                break;
            }
            result.actions.push_back(std::move(declaration));
        }
    }
    if (reader.Failed()) return result;

    const OptionalArray bindings =
        reader.Array(reader.Required(object, "bindings", ""), "bindings must be an array", "/bindings");
    if (bindings) {
        std::size_t index = 0;
        for (const Json::Value binding : *bindings) {
            result.bindings.push_back(ParseBinding(reader, binding, "/bindings/" + std::to_string(index++)));
            if (reader.Failed()) return result;
        }
    }
    if (reader.Failed()) return result;

    const OptionalArray canvases =
        reader.Array(reader.Required(object, "canvases", ""), "canvases must be an array", "/canvases");
    if (canvases) {
        std::size_t index = 0;
        for (const Json::Value canvas : *canvases) {
            result.canvases.push_back(ParseCanvas(reader, canvas, "/canvases/" + std::to_string(index++)));
            if (reader.Failed()) return result;
        }
    }
    return result;
}

} // namespace

UiResult<UiDocument> UiDocumentCodec::Parse(const std::string_view utf8Json, const std::string_view source) {
    auto json = Json::Parse(utf8Json);
    if (!json) {
        return Base::Err(UiFailure(UiFailureCode::InvalidJson, json.error().message, std::string(source), {},
                                   json.error().detail));
    }
    Reader reader(source);
    UiDocument document = ParseDocument(reader, json->Root());
    if (reader.Failed()) return Base::Err(std::move(reader).TakeFailure());

    auto validation = Validate(document);
    if (!validation) {
        UiFailure failure = std::move(validation).error();
        failure.source = std::string(source);
        return Base::Err(std::move(failure));
    }
    return document;
}

} // namespace GYO::Engine::Ui
