#include "ui/UiDocumentCodec.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace Engine::Ui {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] Json Float2Json(UiFloat2 value) {
    return Json::array({value.x, value.y});
}

[[nodiscard]] Json RectJson(const UiRectTransform& rect) {
    Json result = Json::object();
    result["anchor_min"] = Float2Json(rect.anchorMin);
    result["anchor_max"] = Float2Json(rect.anchorMax);
    result["pivot"] = Float2Json(rect.pivot);
    result["position"] = Float2Json(rect.position);
    result["size_delta"] = Float2Json(rect.sizeDelta);
    return result;
}

[[nodiscard]] Json NumberFormatJson(const UiNumberFormat& format) {
    Json result = Json::object();
    result["decimals"] = format.decimals;
    result["show_plus"] = format.showPlus;
    return result;
}

void WriteValueReference(
    Json& result,
    const UiValueReference& reference) {
    result[reference.kind == UiValueReferenceKind::Binding
               ? "binding"
               : "item_field"] = reference.id;
}

[[nodiscard]] Json TextPlaceholderJson(const UiTextPlaceholder& placeholder) {
    Json result = Json::object();
    WriteValueReference(result, placeholder.value);
    result["format"] = NumberFormatJson(placeholder.format);
    return result;
}

[[nodiscard]] Json TextSourceJson(const UiTextSource& source) {
    Json result = Json::object();
    switch (source.kind) {
    case UiTextSourceKind::Literal:
        result["literal"] = source.literal;
        break;
    case UiTextSourceKind::Value:
        WriteValueReference(result, source.value);
        result["format"] = NumberFormatJson(source.format);
        break;
    case UiTextSourceKind::Compose: {
        Json compose = Json::object();
        compose["format"] = source.composeFormat;
        Json placeholders = Json::object();
        for (const auto& [name, placeholder] : source.placeholders) {
            placeholders[name] = TextPlaceholderJson(placeholder);
        }
        compose["placeholders"] = std::move(placeholders);
        result["compose"] = std::move(compose);
        break;
    }
    case UiTextSourceKind::Select: {
        Json selection = Json::object();
        selection["binding"] = source.selectionBinding;
        Json cases = Json::object();
        for (const auto& [key, text] : source.cases) {
            cases[key] = text;
        }
        selection["cases"] = std::move(cases);
        result["select"] = std::move(selection);
        break;
    }
    }
    return result;
}

[[nodiscard]] Json ColorSourceJson(const UiColorSource& source) {
    Json result = Json::object();
    if (source.kind == UiColorSourceKind::Color) {
        result["color"] = source.color;
        return result;
    }
    Json selection = Json::object();
    selection["binding"] = source.selectionBinding;
    Json cases = Json::object();
    for (const auto& [key, color] : source.cases) {
        cases[key] = color;
    }
    selection["cases"] = std::move(cases);
    result["select"] = std::move(selection);
    return result;
}

[[nodiscard]] std::string_view HorizontalAlignName(
    UiHorizontalAlign value) noexcept {
    switch (value) {
    case UiHorizontalAlign::Left: return "left";
    case UiHorizontalAlign::Center: return "center";
    case UiHorizontalAlign::Right: return "right";
    }
    return "left";
}

[[nodiscard]] std::string_view VerticalAlignName(
    UiVerticalAlign value) noexcept {
    switch (value) {
    case UiVerticalAlign::Top: return "top";
    case UiVerticalAlign::Center: return "center";
    case UiVerticalAlign::Bottom: return "bottom";
    }
    return "top";
}

[[nodiscard]] std::string_view ElementTypeName(UiElementType value) noexcept {
    switch (value) {
    case UiElementType::Container: return "container";
    case UiElementType::Panel: return "panel";
    case UiElementType::Image: return "image";
    case UiElementType::Text: return "text";
    case UiElementType::Button: return "button";
    case UiElementType::HorizontalSlider: return "horizontal_slider";
    case UiElementType::FixedStepList: return "fixed_step_list";
    }
    return "container";
}

void WriteTextVisual(Json& result, const UiElement& element) {
    result["text"] = TextSourceJson(element.text);
    if (!element.textStyle.font.empty()) {
        result["font"] = element.textStyle.font;
    }
    result["point_size"] = element.textStyle.pointSize;
    result["text_color"] = ColorSourceJson(element.textStyle.color);
    result["horizontal_align"] = HorizontalAlignName(element.textStyle.horizontalAlign);
    result["vertical_align"] = VerticalAlignName(element.textStyle.verticalAlign);
}

[[nodiscard]] Json ButtonColorsJson(const UiButtonColors& colors) {
    Json result = Json::object();
    result["normal"] = colors.normal;
    result["focused"] = colors.focused;
    result["pressed"] = colors.pressed;
    return result;
}

[[nodiscard]] Json ElementsJson(const std::vector<UiElement>& elements);

[[nodiscard]] Json ElementJson(const UiElement& element) {
    Json result = Json::object();
    result["type"] = ElementTypeName(element.type);
    result["id"] = element.id;
    result["rect"] = RectJson(element.rect);

    switch (element.type) {
    case UiElementType::Container:
        break;
    case UiElementType::Panel:
        result["color"] = ColorSourceJson(element.color);
        break;
    case UiElementType::Image:
        result["texture_asset"] = element.textureAsset;
        result["source_uv"] = Json::array({
            element.sourceUv.x,
            element.sourceUv.y,
            element.sourceUv.width,
            element.sourceUv.height,
        });
        result["tint"] = ColorSourceJson(element.color);
        break;
    case UiElementType::Text:
        WriteTextVisual(result, element);
        break;
    case UiElementType::Button:
        WriteTextVisual(result, element);
        result["action"] = element.action;
        result["background"] = ButtonColorsJson(element.background);
        break;
    case UiElementType::HorizontalSlider:
        WriteTextVisual(result, element);
        result["action"] = element.action;
        result["background"] = ButtonColorsJson(element.background);
        result["binding"] = element.binding;
        result["minimum"] = element.minimum;
        result["maximum"] = element.maximum;
        result["step"] = element.step;
        result["value_format"] = NumberFormatJson(element.valueFormat);
        result["track_color"] = element.trackColor;
        result["fill_color"] = element.fillColor;
        result["thumb_color"] = element.thumbColor;
        break;
    case UiElementType::FixedStepList:
        result["binding"] = element.binding;
        result["max_items"] = element.maxItems;
        result["item_step"] = Float2Json(element.itemStep);
        result["template"] = ElementsJson(element.itemTemplate);
        break;
    }
    if (!element.children.empty()) {
        result["children"] = ElementsJson(element.children);
    }
    return result;
}

Json ElementsJson(const std::vector<UiElement>& elements) {
    Json result = Json::array();
    for (const UiElement& element : elements) {
        result.push_back(ElementJson(element));
    }
    return result;
}

[[nodiscard]] std::string_view BindingTypeName(UiBindingType value) noexcept {
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

[[nodiscard]] std::string_view ScalarTypeName(UiScalarType value) noexcept {
    switch (value) {
    case UiScalarType::String: return "string";
    case UiScalarType::Integer: return "integer";
    case UiScalarType::Number: return "number";
    case UiScalarType::Boolean: return "boolean";
    case UiScalarType::Enum: return "enum";
    }
    return "string";
}

[[nodiscard]] Json ScalarJson(const UiScalarValue& value) {
    return std::visit([](const auto& typed) -> Json { return typed; }, value);
}

[[nodiscard]] Json PreviewJson(const UiBindingDeclaration& binding) {
    return std::visit(
        [](const auto& typed) -> Json {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, UiList>) {
                Json result = Json::array();
                for (const UiListItem& item : typed.items) {
                    Json itemJson = Json::object();
                    for (const auto& [name, value] : item.fields) {
                        itemJson[name] = ScalarJson(value);
                    }
                    result.push_back(std::move(itemJson));
                }
                return result;
            } else {
                return Json(typed);
            }
        },
        binding.preview);
}

} // namespace

UiResult<std::string> UiDocumentCodec::Serialize(const UiDocument& document) {
    auto validation = Validate(document);
    if (!validation) {
        return UiResult<std::string>::Err(std::move(validation).error());
    }

    try {
        Json root = Json::object();
        root["schema"] = kUiSchemaName;
        root["version"] = kUiSchemaVersion;

        Json design = Json::object();
        design["size"] = Float2Json(document.designCanvas.size);
        design["scale_mode"] = "fit";
        root["design_canvas"] = std::move(design);

        Json fonts = Json::object();
        for (const auto& [alias, assetId] : document.fonts) {
            fonts[alias] = assetId;
        }
        root["fonts"] = std::move(fonts);
        root["default_font"] = document.defaultFont;

        Json colors = Json::object();
        for (const auto& [name, color] : document.colors) {
            colors[name] = EncodeSrgbHexColor(color);
        }
        root["colors"] = std::move(colors);

        Json actions = Json::array();
        for (const UiActionDeclaration& action : document.actions) {
            Json encoded = Json::object();
            encoded["id"] = action.id;
            encoded["payload"] = action.payload == UiActionPayloadType::None
                ? "none"
                : "number";
            actions.push_back(std::move(encoded));
        }
        root["actions"] = std::move(actions);

        Json bindings = Json::array();
        for (const UiBindingDeclaration& binding : document.bindings) {
            Json encoded = Json::object();
            encoded["id"] = binding.id;
            encoded["type"] = BindingTypeName(binding.type);
            if (binding.type == UiBindingType::Enum) {
                encoded["values"] = binding.enumValues;
            }
            if (binding.type == UiBindingType::List) {
                Json fields = Json::object();
                for (const auto& [name, type] : binding.itemFields) {
                    fields[name] = ScalarTypeName(type);
                }
                encoded["item_fields"] = std::move(fields);
            }
            encoded["preview"] = PreviewJson(binding);
            bindings.push_back(std::move(encoded));
        }
        root["bindings"] = std::move(bindings);

        Json canvases = Json::array();
        for (const UiCanvas& canvas : document.canvases) {
            Json encoded = Json::object();
            encoded["id"] = canvas.id;
            encoded["backdrop_color"] = canvas.backdropColor;
            if (canvas.defaultFocus) encoded["default_focus"] = *canvas.defaultFocus;
            if (canvas.cancelAction) encoded["cancel_action"] = *canvas.cancelAction;
            encoded["focus_order"] = canvas.focusOrder;
            encoded["children"] = ElementsJson(canvas.children);
            canvases.push_back(std::move(encoded));
        }
        root["canvases"] = std::move(canvases);

        std::string serialized = root.dump(
            2,
            ' ',
            false,
            nlohmann::json::error_handler_t::strict);
        serialized.push_back('\n');
        return UiResult<std::string>::Ok(std::move(serialized));
    } catch (const std::exception& error) {
        return UiResult<std::string>::Err({
            UiErrorCode::InvalidJson,
            error.what(),
            {},
            {},
        });
    }
}

} // namespace Engine::Ui
