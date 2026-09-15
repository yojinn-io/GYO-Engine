#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Engine::Ui {
namespace {

[[nodiscard]] UiResult<void> Invalid(
    std::string message,
    std::string path,
    UiErrorCode code = UiErrorCode::ValidationFailed) {
    return UiResult<void>::Err({code, std::move(message), {}, std::move(path)});
}

[[nodiscard]] bool IsFinite(UiFloat2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool IsFinite(UiColor value) noexcept {
    return std::isfinite(value.red) && std::isfinite(value.green) &&
           std::isfinite(value.blue) && std::isfinite(value.alpha);
}

[[nodiscard]] bool IsUnit(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool MatchesScalar(
    const UiScalarValue& value,
    UiScalarType type) noexcept {
    switch (type) {
    case UiScalarType::String:
    case UiScalarType::Enum:
        return std::holds_alternative<std::string>(value);
    case UiScalarType::Integer:
        return std::holds_alternative<std::int64_t>(value);
    case UiScalarType::Number:
        return std::holds_alternative<double>(value);
    case UiScalarType::Boolean:
        return std::holds_alternative<bool>(value);
    }
    return false;
}

[[nodiscard]] bool MatchesBinding(
    const UiBindingValue& value,
    UiBindingType type) noexcept {
    switch (type) {
    case UiBindingType::String:
    case UiBindingType::Enum:
        return std::holds_alternative<std::string>(value);
    case UiBindingType::Integer:
        return std::holds_alternative<std::int64_t>(value);
    case UiBindingType::Number:
        return std::holds_alternative<double>(value);
    case UiBindingType::Boolean:
        return std::holds_alternative<bool>(value);
    case UiBindingType::List:
        return std::holds_alternative<UiList>(value);
    }
    return false;
}

struct ValidationContext final {
    const UiDocument& document;
    std::unordered_map<std::string_view, const UiActionDeclaration*> actions;
    std::unordered_map<std::string_view, const UiBindingDeclaration*> bindings;
    std::unordered_set<std::string_view> colors;
    std::unordered_set<std::string_view> fonts;
};

[[nodiscard]] UiResult<void> ValidateCases(
    const std::string& bindingId,
    const std::map<std::string, std::string, std::less<>>& cases,
    const ValidationContext& context,
    const std::string& path,
    bool valuesAreColors) {
    const auto found = context.bindings.find(bindingId);
    if (found == context.bindings.end()) {
        return Invalid("unknown selection binding '" + bindingId + "'", path + "/binding", UiErrorCode::MissingReference);
    }
    const UiBindingDeclaration& binding = *found->second;
    std::vector<std::string> requiredCases;
    if (binding.type == UiBindingType::Boolean) {
        requiredCases = {"false", "true"};
    } else if (binding.type == UiBindingType::Enum) {
        requiredCases = binding.enumValues;
    } else {
        return Invalid("selection binding must be boolean or enum", path + "/binding", UiErrorCode::BindingTypeMismatch);
    }
    if (cases.size() != requiredCases.size()) {
        return Invalid("selection cases must exactly cover the binding domain", path + "/cases");
    }
    for (const std::string& required : requiredCases) {
        const auto selected = cases.find(required);
        if (selected == cases.end()) {
            return Invalid("selection case '" + required + "' is missing", path + "/cases");
        }
        if (valuesAreColors && !context.colors.contains(selected->second)) {
            return Invalid("unknown color '" + selected->second + "'", path + "/cases/" + required, UiErrorCode::MissingReference);
        }
    }
    return UiResult<void>::Ok();
}

[[nodiscard]] UiResult<void> ValidateValueReference(
    const UiValueReference& reference,
    const ValidationContext& context,
    const UiBindingDeclaration* listBinding,
    const std::string& path) {
    if (reference.id.empty()) {
        return Invalid("value reference cannot be empty", path);
    }
    if (reference.kind == UiValueReferenceKind::Binding) {
        const auto found = context.bindings.find(reference.id);
        if (found == context.bindings.end()) {
            return Invalid("unknown binding '" + reference.id + "'", path, UiErrorCode::MissingReference);
        }
        if (found->second->type == UiBindingType::List) {
            return Invalid("text cannot directly format a list binding", path, UiErrorCode::BindingTypeMismatch);
        }
        return UiResult<void>::Ok();
    }
    if (listBinding == nullptr) {
        return Invalid("item_field is valid only inside a fixed_step_list template", path);
    }
    if (!listBinding->itemFields.contains(reference.id)) {
        return Invalid("unknown list item field '" + reference.id + "'", path, UiErrorCode::MissingReference);
    }
    return UiResult<void>::Ok();
}

[[nodiscard]] UiResult<void> ValidateTextSource(
    const UiTextSource& source,
    const ValidationContext& context,
    const UiBindingDeclaration* listBinding,
    const std::string& path) {
    switch (source.kind) {
    case UiTextSourceKind::Literal:
        return UiResult<void>::Ok();
    case UiTextSourceKind::Value:
        return ValidateValueReference(source.value, context, listBinding, path);
    case UiTextSourceKind::Compose: {
        if (source.composeFormat.empty() || source.placeholders.empty()) {
            return Invalid("compose requires a format and at least one placeholder", path);
        }
        std::unordered_set<std::string> used;
        for (std::size_t index = 0; index < source.composeFormat.size();) {
            const char current = source.composeFormat[index];
            if (current == '{') {
                if (index + 1U < source.composeFormat.size() &&
                    source.composeFormat[index + 1U] == '{') {
                    index += 2U;
                    continue;
                }
                const std::size_t end = source.composeFormat.find('}', index + 1U);
                if (end == std::string::npos) {
                    return Invalid("compose format has an unclosed placeholder", path + "/format");
                }
                const std::string name = source.composeFormat.substr(index + 1U, end - index - 1U);
                const bool validName = !name.empty() &&
                    (std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_') &&
                    std::all_of(name.begin() + 1, name.end(), [](char value) {
                        return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
                    });
                if (!validName || !source.placeholders.contains(name)) {
                    return Invalid("compose format refers to an unknown or invalid placeholder '" + name + "'", path + "/format");
                }
                used.emplace(name);
                index = end + 1U;
                continue;
            }
            if (current == '}') {
                if (index + 1U < source.composeFormat.size() &&
                    source.composeFormat[index + 1U] == '}') {
                    index += 2U;
                    continue;
                }
                return Invalid("compose format has an unmatched closing brace", path + "/format");
            }
            ++index;
        }
        if (used.size() != source.placeholders.size()) {
            return Invalid("every compose placeholder must occur in format", path + "/placeholders");
        }
        for (const auto& [name, placeholder] : source.placeholders) {
            auto validation = ValidateValueReference(
                placeholder.value,
                context,
                listBinding,
                path + "/placeholders/" + name);
            if (!validation) return validation;
        }
        return UiResult<void>::Ok();
    }
    case UiTextSourceKind::Select:
        return ValidateCases(
            source.selectionBinding,
            source.cases,
            context,
            path,
            false);
    }
    return Invalid("unknown text source kind", path);
}

[[nodiscard]] UiResult<void> ValidateColorSource(
    const UiColorSource& source,
    const ValidationContext& context,
    const std::string& path) {
    if (source.kind == UiColorSourceKind::Color) {
        if (!context.colors.contains(source.color)) {
            return Invalid("unknown color '" + source.color + "'", path, UiErrorCode::MissingReference);
        }
        return UiResult<void>::Ok();
    }
    return ValidateCases(
        source.selectionBinding,
        source.cases,
        context,
        path,
        true);
}

[[nodiscard]] UiResult<void> ValidateTextStyle(
    const UiTextStyle& style,
    const ValidationContext& context,
    const std::string& path) {
    if (!std::isfinite(style.pointSize) || style.pointSize <= 0.0F) {
        return Invalid("point_size must be finite and positive", path + "/point_size");
    }
    if (!style.font.empty() && !context.fonts.contains(style.font)) {
        return Invalid("unknown font alias '" + style.font + "'", path + "/font", UiErrorCode::MissingReference);
    }
    return ValidateColorSource(style.color, context, path + "/text_color");
}

[[nodiscard]] UiResult<void> ValidateNamedColor(
    std::string_view color,
    const ValidationContext& context,
    const std::string& path) {
    if (!context.colors.contains(color)) {
        return Invalid("unknown color '" + std::string(color) + "'", path, UiErrorCode::MissingReference);
    }
    return UiResult<void>::Ok();
}

struct CanvasElementState final {
    std::unordered_set<std::string> ids;
    std::vector<std::string> interactive;
};

[[nodiscard]] UiResult<void> ValidateElements(
    const std::vector<UiElement>& elements,
    const ValidationContext& context,
    CanvasElementState& state,
    const UiBindingDeclaration* listBinding,
    bool insideListTemplate,
    const std::string& path) {
    for (std::size_t index = 0; index < elements.size(); ++index) {
        const UiElement& element = elements[index];
        const std::string elementPath = path + "/" + std::to_string(index);
        if (element.id.empty()) {
            return Invalid("element id cannot be empty", elementPath + "/id");
        }
        if (!state.ids.emplace(element.id).second) {
            return Invalid("duplicate element id '" + element.id + "'", elementPath + "/id");
        }
        const UiRectTransform& rect = element.rect;
        if (!IsFinite(rect.anchorMin) || !IsFinite(rect.anchorMax) ||
            !IsFinite(rect.pivot) || !IsFinite(rect.position) ||
            !IsFinite(rect.sizeDelta)) {
            return Invalid("rect transform values must be finite", elementPath + "/rect");
        }
        if (!IsUnit(rect.anchorMin.x) || !IsUnit(rect.anchorMin.y) ||
            !IsUnit(rect.anchorMax.x) || !IsUnit(rect.anchorMax.y) ||
            rect.anchorMin.x > rect.anchorMax.x ||
            rect.anchorMin.y > rect.anchorMax.y) {
            return Invalid("anchor_min/anchor_max must be ordered values in [0,1]", elementPath + "/rect");
        }
        if (!IsUnit(rect.pivot.x) || !IsUnit(rect.pivot.y)) {
            return Invalid("pivot must be in [0,1]", elementPath + "/rect/pivot");
        }

        switch (element.type) {
        case UiElementType::Container:
            break;
        case UiElementType::Panel: {
            auto validation = ValidateColorSource(element.color, context, elementPath + "/color");
            if (!validation) return validation;
            break;
        }
        case UiElementType::Image: {
            if (element.textureAsset.empty()) {
                return Invalid("texture_asset cannot be empty", elementPath + "/texture_asset");
            }
            const UiRect uv = element.sourceUv;
            if (!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
                !std::isfinite(uv.width) || !std::isfinite(uv.height) ||
                uv.x < 0.0F || uv.y < 0.0F || uv.width <= 0.0F || uv.height <= 0.0F ||
                uv.x + uv.width > 1.0F || uv.y + uv.height > 1.0F) {
                return Invalid("source_uv must be a positive normalized rectangle", elementPath + "/source_uv");
            }
            auto validation = ValidateColorSource(element.color, context, elementPath + "/tint");
            if (!validation) return validation;
            break;
        }
        case UiElementType::Text: {
            auto validation = ValidateTextSource(element.text, context, listBinding, elementPath + "/text");
            if (!validation) return validation;
            validation = ValidateTextStyle(element.textStyle, context, elementPath);
            if (!validation) return validation;
            break;
        }
        case UiElementType::Button: {
            if (insideListTemplate) {
                return Invalid("interactive elements are forbidden in list templates", elementPath);
            }
            const auto action = context.actions.find(element.action);
            if (action == context.actions.end()) {
                return Invalid("unknown action '" + element.action + "'", elementPath + "/action", UiErrorCode::MissingReference);
            }
            if (action->second->payload != UiActionPayloadType::None) {
                return Invalid("button action payload must be none", elementPath + "/action", UiErrorCode::BindingTypeMismatch);
            }
            auto validation = ValidateTextSource(element.text, context, listBinding, elementPath + "/text");
            if (!validation) return validation;
            validation = ValidateTextStyle(element.textStyle, context, elementPath);
            if (!validation) return validation;
            validation = ValidateNamedColor(element.background.normal, context, elementPath + "/background/normal");
            if (!validation) return validation;
            validation = ValidateNamedColor(element.background.focused, context, elementPath + "/background/focused");
            if (!validation) return validation;
            validation = ValidateNamedColor(element.background.pressed, context, elementPath + "/background/pressed");
            if (!validation) return validation;
            state.interactive.push_back(element.id);
            break;
        }
        case UiElementType::HorizontalSlider: {
            if (insideListTemplate) {
                return Invalid("interactive elements are forbidden in list templates", elementPath);
            }
            const auto action = context.actions.find(element.action);
            if (action == context.actions.end()) {
                return Invalid("unknown action '" + element.action + "'", elementPath + "/action", UiErrorCode::MissingReference);
            }
            if (action->second->payload != UiActionPayloadType::Number) {
                return Invalid("slider action payload must be number", elementPath + "/action", UiErrorCode::BindingTypeMismatch);
            }
            const auto binding = context.bindings.find(element.binding);
            if (binding == context.bindings.end()) {
                return Invalid("unknown binding '" + element.binding + "'", elementPath + "/binding", UiErrorCode::MissingReference);
            }
            if (binding->second->type != UiBindingType::Number) {
                return Invalid("slider binding must be number", elementPath + "/binding", UiErrorCode::BindingTypeMismatch);
            }
            if (!std::isfinite(element.minimum) || !std::isfinite(element.maximum) ||
                !std::isfinite(element.step) || element.minimum >= element.maximum || element.step <= 0.0) {
                return Invalid("slider requires finite minimum < maximum and step > 0", elementPath);
            }
            auto validation = ValidateTextSource(element.text, context, listBinding, elementPath + "/text");
            if (!validation) return validation;
            validation = ValidateTextStyle(element.textStyle, context, elementPath);
            if (!validation) return validation;
            for (const auto& [name, color] : std::initializer_list<std::pair<std::string_view, std::string_view>>{
                     {"background/normal", element.background.normal},
                     {"background/focused", element.background.focused},
                     {"background/pressed", element.background.pressed},
                     {"track_color", element.trackColor},
                     {"fill_color", element.fillColor},
                     {"thumb_color", element.thumbColor}}) {
                validation = ValidateNamedColor(color, context, elementPath + "/" + std::string(name));
                if (!validation) return validation;
            }
            state.interactive.push_back(element.id);
            break;
        }
        case UiElementType::FixedStepList: {
            if (insideListTemplate) {
                return Invalid("nested fixed_step_list is forbidden", elementPath);
            }
            const auto binding = context.bindings.find(element.binding);
            if (binding == context.bindings.end()) {
                return Invalid("unknown binding '" + element.binding + "'", elementPath + "/binding", UiErrorCode::MissingReference);
            }
            if (binding->second->type != UiBindingType::List) {
                return Invalid("fixed_step_list binding must be list", elementPath + "/binding", UiErrorCode::BindingTypeMismatch);
            }
            if (element.maxItems == 0 || element.itemTemplate.empty()) {
                return Invalid("fixed_step_list requires max_items > 0 and a non-empty template", elementPath);
            }
            auto validation = ValidateElements(
                element.itemTemplate,
                context,
                state,
                binding->second,
                true,
                elementPath + "/template");
            if (!validation) return validation;
            break;
        }
        }

        auto childrenValidation = ValidateElements(
            element.children,
            context,
            state,
            listBinding,
            insideListTemplate,
            elementPath + "/children");
        if (!childrenValidation) return childrenValidation;
    }
    return UiResult<void>::Ok();
}

} // namespace

UiResult<void> UiDocumentCodec::Validate(const UiDocument& document) {
    if (!IsFinite(document.designCanvas.size) ||
        document.designCanvas.size.x <= 0.0F || document.designCanvas.size.y <= 0.0F) {
        return Invalid("design canvas size must be finite and positive", "/design_canvas/size");
    }
    if (document.fonts.empty()) {
        return Invalid("at least one font alias is required", "/fonts");
    }
    if (!document.fonts.contains(document.defaultFont)) {
        return Invalid("default_font must name a declared font alias", "/default_font", UiErrorCode::MissingReference);
    }

    ValidationContext context{document, {}, {}, {}, {}};
    for (const auto& [alias, asset] : document.fonts) {
        if (alias.empty() || asset.empty()) {
            return Invalid("font aliases and asset ids cannot be empty", "/fonts");
        }
        context.fonts.emplace(alias);
    }
    for (const auto& [name, color] : document.colors) {
        if (name.empty() || !IsFinite(color) || color.red < 0.0F || color.red > 1.0F ||
            color.green < 0.0F || color.green > 1.0F || color.blue < 0.0F ||
            color.blue > 1.0F || color.alpha < 0.0F || color.alpha > 1.0F) {
            return Invalid("colors must have a name and finite linear channels in [0,1]", "/colors");
        }
        context.colors.emplace(name);
    }

    std::unordered_set<std::string_view> declarationIds;
    for (std::size_t index = 0; index < document.actions.size(); ++index) {
        const UiActionDeclaration& action = document.actions[index];
        if (action.id.empty() || !context.actions.emplace(action.id, &action).second) {
            return Invalid("action ids must be non-empty and unique", "/actions/" + std::to_string(index) + "/id");
        }
        declarationIds.emplace(action.id);
    }

    for (std::size_t index = 0; index < document.bindings.size(); ++index) {
        const UiBindingDeclaration& binding = document.bindings[index];
        const std::string path = "/bindings/" + std::to_string(index);
        if (binding.id.empty() || !context.bindings.emplace(binding.id, &binding).second) {
            return Invalid("binding ids must be non-empty and unique", path + "/id");
        }
        if (!declarationIds.emplace(binding.id).second) {
            return Invalid("action and binding ids must be globally unique", path + "/id");
        }
        if (!MatchesBinding(binding.preview, binding.type)) {
            return Invalid("preview does not match the declared binding type", path + "/preview", UiErrorCode::BindingTypeMismatch);
        }
        if (binding.type == UiBindingType::Enum) {
            std::unordered_set<std::string_view> values;
            if (binding.enumValues.empty()) {
                return Invalid("enum values cannot be empty", path + "/values");
            }
            for (const std::string& value : binding.enumValues) {
                if (value.empty() || !values.emplace(value).second) {
                    return Invalid("enum values must be non-empty and unique", path + "/values");
                }
            }
            if (!values.contains(std::get<std::string>(binding.preview))) {
                return Invalid("enum preview must be one of values", path + "/preview");
            }
        }
        if (binding.type == UiBindingType::List) {
            if (binding.itemFields.empty()) {
                return Invalid("list item_fields cannot be empty", path + "/item_fields");
            }
            const UiList& list = std::get<UiList>(binding.preview);
            for (std::size_t itemIndex = 0; itemIndex < list.items.size(); ++itemIndex) {
                const UiListItem& item = list.items[itemIndex];
                if (item.fields.size() != binding.itemFields.size()) {
                    return Invalid("list preview item fields must exactly match item_fields", path + "/preview/" + std::to_string(itemIndex));
                }
                for (const auto& [name, type] : binding.itemFields) {
                    const auto field = item.fields.find(name);
                    if (field == item.fields.end() || !MatchesScalar(field->second, type)) {
                        return Invalid("list preview field has the wrong type", path + "/preview/" + std::to_string(itemIndex) + "/" + name, UiErrorCode::BindingTypeMismatch);
                    }
                }
            }
        }
    }

    std::unordered_set<std::string_view> canvasIds;
    for (std::size_t index = 0; index < document.canvases.size(); ++index) {
        const UiCanvas& canvas = document.canvases[index];
        const std::string path = "/canvases/" + std::to_string(index);
        if (canvas.id.empty() || !canvasIds.emplace(canvas.id).second) {
            return Invalid("canvas ids must be non-empty and unique", path + "/id");
        }
        if (!context.colors.contains(canvas.backdropColor)) {
            return Invalid("unknown backdrop color '" + canvas.backdropColor + "'", path + "/backdrop_color", UiErrorCode::MissingReference);
        }
        if (canvas.cancelAction) {
            const auto action = context.actions.find(*canvas.cancelAction);
            if (action == context.actions.end()) {
                return Invalid("unknown cancel action", path + "/cancel_action", UiErrorCode::MissingReference);
            }
            if (action->second->payload != UiActionPayloadType::None) {
                return Invalid("cancel action payload must be none", path + "/cancel_action", UiErrorCode::BindingTypeMismatch);
            }
        }

        CanvasElementState elementState;
        auto elements = ValidateElements(
            canvas.children, context, elementState, nullptr, false, path + "/children");
        if (!elements) return elements;

        std::unordered_set<std::string_view> focusIds;
        for (const std::string& id : canvas.focusOrder) {
            if (!focusIds.emplace(id).second) {
                return Invalid("focus_order cannot contain duplicates", path + "/focus_order");
            }
        }
        if (focusIds.size() != elementState.interactive.size()) {
            return Invalid("focus_order must contain every interactive element exactly once", path + "/focus_order");
        }
        for (const std::string& id : elementState.interactive) {
            if (!focusIds.contains(id)) {
                return Invalid("focus_order is missing interactive element '" + id + "'", path + "/focus_order");
            }
        }
        if (canvas.defaultFocus && !focusIds.contains(*canvas.defaultFocus)) {
            return Invalid("default_focus must occur in focus_order", path + "/default_focus");
        }
    }
    if (document.canvases.empty()) {
        return Invalid("at least one canvas is required", "/canvases");
    }
    return UiResult<void>::Ok();
}

} // namespace Engine::Ui
