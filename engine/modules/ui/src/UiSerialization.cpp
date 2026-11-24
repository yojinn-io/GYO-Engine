// Writing gyo.ui (ported from master's engine/ui/src/UiSerialization.cpp): master's field order, the same omitted
// fields and uppercase #RRGGBBAA colours, through the JSON Module's Writer. The Writer has one format, compact on one
// line (Design Invariant J-5); master wrote two-space indentation. The values are the same: floats are widened to
// double as master did, and integers (version, decimals, max_items, max_length, integer previews) stay integers.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "engine/json/Writer.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Engine::Ui {
namespace {

void WriteFloat2(Json::Writer& writer, const Math::Vec2 value) {
    writer.BeginArray();
    writer.Double(static_cast<double>(value.x));
    writer.Double(static_cast<double>(value.y));
    writer.EndArray();
}

void WriteStrings(Json::Writer& writer, const std::vector<std::string>& values) {
    writer.BeginArray();
    for (const std::string& value : values) writer.String(value);
    writer.EndArray();
}

void WriteStringMap(Json::Writer& writer, const std::map<std::string, std::string, std::less<>>& values) {
    writer.BeginObject();
    for (const auto& [key, value] : values) {
        writer.Key(key);
        writer.String(value);
    }
    writer.EndObject();
}

void WriteRect(Json::Writer& writer, const UiRectTransform& rect) {
    writer.BeginObject();
    writer.Key("anchor_min");
    WriteFloat2(writer, rect.anchorMin);
    writer.Key("anchor_max");
    WriteFloat2(writer, rect.anchorMax);
    writer.Key("pivot");
    WriteFloat2(writer, rect.pivot);
    writer.Key("position");
    WriteFloat2(writer, rect.position);
    writer.Key("size_delta");
    WriteFloat2(writer, rect.sizeDelta);
    writer.EndObject();
}

void WriteNumberFormat(Json::Writer& writer, const UiNumberFormat& format) {
    writer.BeginObject();
    writer.Key("decimals");
    writer.Uint(format.decimals);
    writer.Key("show_plus");
    writer.Bool(format.showPlus);
    writer.EndObject();
}

void WriteValueReference(Json::Writer& writer, const UiValueReference& reference) {
    writer.Key(reference.kind == UiValueReferenceKind::Binding ? "binding" : "item_field");
    writer.String(reference.id);
}

void WriteTextSource(Json::Writer& writer, const UiTextSource& source) {
    writer.BeginObject();
    switch (source.kind) {
    case UiTextSourceKind::Literal:
        writer.Key("literal");
        writer.String(source.literal);
        break;
    case UiTextSourceKind::Value:
        WriteValueReference(writer, source.value);
        writer.Key("format");
        WriteNumberFormat(writer, source.format);
        break;
    case UiTextSourceKind::Compose:
        writer.Key("compose");
        writer.BeginObject();
        writer.Key("format");
        writer.String(source.composeFormat);
        writer.Key("placeholders");
        writer.BeginObject();
        for (const auto& [name, placeholder] : source.placeholders) {
            writer.Key(name);
            writer.BeginObject();
            WriteValueReference(writer, placeholder.value);
            writer.Key("format");
            WriteNumberFormat(writer, placeholder.format);
            writer.EndObject();
        }
        writer.EndObject();
        writer.EndObject();
        break;
    case UiTextSourceKind::Select:
        writer.Key("select");
        writer.BeginObject();
        writer.Key("binding");
        writer.String(source.selectionBinding);
        writer.Key("cases");
        WriteStringMap(writer, source.cases);
        writer.EndObject();
        break;
    }
    writer.EndObject();
}

void WriteColorSource(Json::Writer& writer, const UiColorSource& source) {
    writer.BeginObject();
    if (source.kind == UiColorSourceKind::Color) {
        writer.Key("color");
        writer.String(source.color);
    } else {
        writer.Key("select");
        writer.BeginObject();
        writer.Key("binding");
        writer.String(source.selectionBinding);
        writer.Key("cases");
        WriteStringMap(writer, source.cases);
        writer.EndObject();
    }
    writer.EndObject();
}

[[nodiscard]] std::string_view HorizontalAlignName(const UiHorizontalAlign value) noexcept {
    switch (value) {
    case UiHorizontalAlign::Left: return "left";
    case UiHorizontalAlign::Center: return "center";
    case UiHorizontalAlign::Right: return "right";
    }
    return "left";
}

[[nodiscard]] std::string_view VerticalAlignName(const UiVerticalAlign value) noexcept {
    switch (value) {
    case UiVerticalAlign::Top: return "top";
    case UiVerticalAlign::Center: return "center";
    case UiVerticalAlign::Bottom: return "bottom";
    }
    return "top";
}

[[nodiscard]] std::string_view ElementTypeName(const UiElementType value) noexcept {
    switch (value) {
    case UiElementType::Container: return "container";
    case UiElementType::Panel: return "panel";
    case UiElementType::Image: return "image";
    case UiElementType::Text: return "text";
    case UiElementType::Button: return "button";
    case UiElementType::HorizontalSlider: return "horizontal_slider";
    case UiElementType::TextField: return "text_field";
    case UiElementType::FixedStepList: return "fixed_step_list";
    }
    return "container";
}

[[nodiscard]] std::string_view PayloadName(const UiActionPayloadType value) noexcept {
    switch (value) {
    case UiActionPayloadType::None: return "none";
    case UiActionPayloadType::Number: return "number";
    case UiActionPayloadType::String: return "string";
    }
    return "none";
}

[[nodiscard]] std::string_view BindingTypeName(const UiBindingType value) noexcept {
    switch (value) {
    case UiBindingType::String: return "string";
    case UiBindingType::Integer: return "integer";
    case UiBindingType::Number: return "number";
    case UiBindingType::Boolean: return "boolean";
    case UiBindingType::Enum: return "enum";
    case UiBindingType::List: return "list<object>";
    }
    return "string";
}

[[nodiscard]] std::string_view ScalarTypeName(const UiScalarType value) noexcept {
    switch (value) {
    case UiScalarType::String: return "string";
    case UiScalarType::Integer: return "integer";
    case UiScalarType::Number: return "number";
    case UiScalarType::Boolean: return "boolean";
    case UiScalarType::Enum: return "enum";
    }
    return "string";
}

void WriteTextStyle(Json::Writer& writer, const UiElement& element) {
    if (!element.textStyle.font.empty()) {
        writer.Key("font");
        writer.String(element.textStyle.font);
    }
    writer.Key("point_size");
    writer.Double(static_cast<double>(element.textStyle.pointSize));
    writer.Key("text_color");
    WriteColorSource(writer, element.textStyle.color);
    writer.Key("horizontal_align");
    writer.String(HorizontalAlignName(element.textStyle.horizontalAlign));
    writer.Key("vertical_align");
    writer.String(VerticalAlignName(element.textStyle.verticalAlign));
}

void WriteTextVisual(Json::Writer& writer, const UiElement& element) {
    writer.Key("text");
    WriteTextSource(writer, element.text);
    WriteTextStyle(writer, element);
}

void WriteButtonColors(Json::Writer& writer, const UiButtonColors& colors) {
    writer.BeginObject();
    writer.Key("normal");
    writer.String(colors.normal);
    writer.Key("focused");
    writer.String(colors.focused);
    writer.Key("pressed");
    writer.String(colors.pressed);
    writer.EndObject();
}

void WriteActionAndBackground(Json::Writer& writer, const UiElement& element) {
    writer.Key("action");
    writer.String(element.action);
    writer.Key("background");
    WriteButtonColors(writer, element.background);
}

void WriteElements(Json::Writer& writer, const std::vector<UiElement>& elements);

void WriteElement(Json::Writer& writer, const UiElement& element) {
    writer.BeginObject();
    writer.Key("type");
    writer.String(ElementTypeName(element.type));
    writer.Key("id");
    writer.String(element.id);
    writer.Key("rect");
    WriteRect(writer, element.rect);

    switch (element.type) {
    case UiElementType::Container:
        break;
    case UiElementType::Panel:
        writer.Key("color");
        WriteColorSource(writer, element.color);
        break;
    case UiElementType::Image:
        writer.Key("texture_asset");
        writer.String(element.textureAsset);
        writer.Key("source_uv");
        writer.BeginArray();
        writer.Double(static_cast<double>(element.sourceUv.x));
        writer.Double(static_cast<double>(element.sourceUv.y));
        writer.Double(static_cast<double>(element.sourceUv.width));
        writer.Double(static_cast<double>(element.sourceUv.height));
        writer.EndArray();
        writer.Key("tint");
        WriteColorSource(writer, element.color);
        break;
    case UiElementType::Text:
        WriteTextVisual(writer, element);
        break;
    case UiElementType::Button:
        WriteTextVisual(writer, element);
        WriteActionAndBackground(writer, element);
        break;
    case UiElementType::HorizontalSlider:
        WriteTextVisual(writer, element);
        WriteActionAndBackground(writer, element);
        writer.Key("binding");
        writer.String(element.binding);
        writer.Key("minimum");
        writer.Double(element.minimum);
        writer.Key("maximum");
        writer.Double(element.maximum);
        writer.Key("step");
        writer.Double(element.step);
        writer.Key("value_format");
        WriteNumberFormat(writer, element.valueFormat);
        writer.Key("track_color");
        writer.String(element.trackColor);
        writer.Key("fill_color");
        writer.String(element.fillColor);
        writer.Key("thumb_color");
        writer.String(element.thumbColor);
        break;
    case UiElementType::TextField:
        WriteTextStyle(writer, element);
        WriteActionAndBackground(writer, element);
        writer.Key("binding");
        writer.String(element.binding);
        writer.Key("max_length");
        writer.Uint(element.maxLength);
        if (!element.placeholder.empty()) {
            writer.Key("placeholder");
            writer.String(element.placeholder);
            writer.Key("placeholder_color");
            writer.String(element.placeholderColor);
        }
        break;
    case UiElementType::FixedStepList:
        writer.Key("binding");
        writer.String(element.binding);
        writer.Key("max_items");
        writer.Uint(element.maxItems);
        writer.Key("item_step");
        WriteFloat2(writer, element.itemStep);
        writer.Key("template");
        WriteElements(writer, element.itemTemplate);
        break;
    }
    if (!element.children.empty()) {
        writer.Key("children");
        WriteElements(writer, element.children);
    }
    writer.EndObject();
}

void WriteElements(Json::Writer& writer, const std::vector<UiElement>& elements) {
    writer.BeginArray();
    for (const UiElement& element : elements) WriteElement(writer, element);
    writer.EndArray();
}

// One scalar of a preview: strings, integers, numbers and booleans keep their JSON kinds.
template <class Value>
void WriteScalar(Json::Writer& writer, const Value& value) {
    using T = std::decay_t<Value>;
    if constexpr (std::is_same_v<T, std::string>) {
        writer.String(value);
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
        writer.Int(value);
    } else if constexpr (std::is_same_v<T, double>) {
        writer.Double(value);
    } else {
        static_assert(std::is_same_v<T, bool>);
        writer.Bool(value);
    }
}

void WritePreview(Json::Writer& writer, const UiBindingValue& preview) {
    std::visit(
        [&writer](const auto& typed) {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, UiList>) {
                writer.BeginArray();
                for (const UiListItem& item : typed.items) {
                    writer.BeginObject();
                    for (const auto& [name, value] : item.fields) {
                        writer.Key(name);
                        std::visit([&writer](const auto& scalar) { WriteScalar(writer, scalar); }, value);
                    }
                    writer.EndObject();
                }
                writer.EndArray();
            } else {
                WriteScalar(writer, typed);
            }
        },
        preview);
}

void WriteDocument(Json::Writer& writer, const UiDocument& document) {
    writer.BeginObject();
    writer.Key("schema");
    writer.String(kUiSchemaName);
    writer.Key("version");
    writer.Uint(document.version);

    writer.Key("design_canvas");
    writer.BeginObject();
    writer.Key("size");
    WriteFloat2(writer, document.designCanvas.size);
    writer.Key("scale_mode");
    writer.String("fit");
    writer.EndObject();

    writer.Key("fonts");
    WriteStringMap(writer, document.fonts);
    writer.Key("default_font");
    writer.String(document.defaultFont);

    writer.Key("colors");
    writer.BeginObject();
    for (const auto& [name, color] : document.colors) {
        writer.Key(name);
        writer.String(EncodeSrgbHexColor(color));
    }
    writer.EndObject();

    writer.Key("actions");
    writer.BeginArray();
    for (const UiActionDeclaration& action : document.actions) {
        writer.BeginObject();
        writer.Key("id");
        writer.String(action.id);
        writer.Key("payload");
        writer.String(PayloadName(action.payload));
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("bindings");
    writer.BeginArray();
    for (const UiBindingDeclaration& binding : document.bindings) {
        writer.BeginObject();
        writer.Key("id");
        writer.String(binding.id);
        writer.Key("type");
        writer.String(BindingTypeName(binding.type));
        if (binding.type == UiBindingType::Enum) {
            writer.Key("values");
            WriteStrings(writer, binding.enumValues);
        }
        if (binding.type == UiBindingType::List) {
            writer.Key("item_fields");
            writer.BeginObject();
            for (const auto& [name, type] : binding.itemFields) {
                writer.Key(name);
                writer.String(ScalarTypeName(type));
            }
            writer.EndObject();
        }
        writer.Key("preview");
        WritePreview(writer, binding.preview);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("canvases");
    writer.BeginArray();
    for (const UiCanvas& canvas : document.canvases) {
        writer.BeginObject();
        writer.Key("id");
        writer.String(canvas.id);
        writer.Key("backdrop_color");
        writer.String(canvas.backdropColor);
        if (canvas.defaultFocus) {
            writer.Key("default_focus");
            writer.String(*canvas.defaultFocus);
        }
        if (canvas.cancelAction) {
            writer.Key("cancel_action");
            writer.String(*canvas.cancelAction);
        }
        writer.Key("focus_order");
        WriteStrings(writer, canvas.focusOrder);
        writer.Key("children");
        WriteElements(writer, canvas.children);
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
}

} // namespace

UiResult<std::string> UiDocumentCodec::Serialize(const UiDocument& document) {
    auto validation = Validate(document);
    if (!validation) return Base::Err(std::move(validation).error());

    std::string serialized;
    Json::Writer writer(serialized);
    WriteDocument(writer, document);
    // A string that is not UTF-8 or a preview number that is not finite: JSON cannot hold it (master: nlohmann's
    // strict dump threw for the first and wrote null for the second).
    if (auto finished = writer.Finish(); !finished) {
        return Base::Err(UiFailure(UiFailureCode::InvalidJson, finished.error().message, {}, {},
                                   finished.error().detail));
    }
    serialized.push_back('\n');
    return serialized;
}

} // namespace GYO::Engine::Ui
