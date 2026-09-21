#include "ui/UiRuntime.hpp"

#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Engine::Ui {
namespace {

[[nodiscard]] UiError RuntimeError(
    UiErrorCode code,
    std::string message,
    std::string path = {}) {
    return {code, std::move(message), {}, std::move(path)};
}

[[nodiscard]] bool IsValidViewport(UiViewport viewport) noexcept {
    return std::isfinite(viewport.width) && std::isfinite(viewport.height) &&
           viewport.width > 0.0F && viewport.height > 0.0F;
}

[[nodiscard]] bool MatchesScalar(
    const UiScalarValue& value,
    UiScalarType type) noexcept {
    switch (type) {
    case UiScalarType::String:
    case UiScalarType::Enum: return std::holds_alternative<std::string>(value);
    case UiScalarType::Integer: return std::holds_alternative<std::int64_t>(value);
    case UiScalarType::Number: return std::holds_alternative<double>(value);
    case UiScalarType::Boolean: return std::holds_alternative<bool>(value);
    }
    return false;
}

[[nodiscard]] bool MatchesBinding(
    const UiBindingValue& value,
    UiBindingType type) noexcept {
    switch (type) {
    case UiBindingType::String:
    case UiBindingType::Enum: return std::holds_alternative<std::string>(value);
    case UiBindingType::Integer: return std::holds_alternative<std::int64_t>(value);
    case UiBindingType::Number: return std::holds_alternative<double>(value);
    case UiBindingType::Boolean: return std::holds_alternative<bool>(value);
    case UiBindingType::List: return std::holds_alternative<UiList>(value);
    }
    return false;
}

[[nodiscard]] const UiBindingDeclaration* FindBindingDeclaration(
    const UiDocument& document,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        document.bindings.begin(),
        document.bindings.end(),
        [id](const UiBindingDeclaration& candidate) { return candidate.id == id; });
    return found == document.bindings.end() ? nullptr : &*found;
}

[[nodiscard]] UiResult<const UiBindingValue*> ResolveBinding(
    const UiDocument& document,
    const UiBindingTable& bindings,
    std::string_view id) {
    const UiBindingDeclaration* declaration = FindBindingDeclaration(document, id);
    if (declaration == nullptr) {
        return UiResult<const UiBindingValue*>::Err(RuntimeError(
            UiErrorCode::MissingReference,
            "UI document refers to unknown binding '" + std::string(id) + "'"));
    }
    const auto found = bindings.find(std::string(id));
    if (found == bindings.end()) {
        return UiResult<const UiBindingValue*>::Err(RuntimeError(
            UiErrorCode::MissingBinding,
            "binding value '" + std::string(id) + "' was not supplied"));
    }
    if (!MatchesBinding(found->second, declaration->type)) {
        return UiResult<const UiBindingValue*>::Err(RuntimeError(
            UiErrorCode::BindingTypeMismatch,
            "binding value '" + std::string(id) + "' has the wrong type"));
    }
    if (declaration->type == UiBindingType::Enum) {
        const std::string& selected = std::get<std::string>(found->second);
        if (std::find(declaration->enumValues.begin(), declaration->enumValues.end(), selected) ==
            declaration->enumValues.end()) {
            return UiResult<const UiBindingValue*>::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "enum binding '" + std::string(id) + "' is outside its declared domain"));
        }
    }
    if (declaration->type == UiBindingType::List) {
        const UiList& list = std::get<UiList>(found->second);
        for (const UiListItem& item : list.items) {
            if (item.fields.size() != declaration->itemFields.size()) {
                return UiResult<const UiBindingValue*>::Err(RuntimeError(
                    UiErrorCode::BindingTypeMismatch,
                    "list binding '" + std::string(id) + "' has an invalid item shape"));
            }
            for (const auto& [name, type] : declaration->itemFields) {
                const auto field = item.fields.find(name);
                if (field == item.fields.end() || !MatchesScalar(field->second, type)) {
                    return UiResult<const UiBindingValue*>::Err(RuntimeError(
                        UiErrorCode::BindingTypeMismatch,
                        "list binding '" + std::string(id) + "' field '" + name + "' has the wrong type"));
                }
            }
        }
    }
    return UiResult<const UiBindingValue*>::Ok(&found->second);
}

[[nodiscard]] UiResult<UiScalarValue> ResolveScalar(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiValueReference& reference,
    const UiListItem* item) {
    if (reference.kind == UiValueReferenceKind::ItemField) {
        if (item == nullptr) {
            return UiResult<UiScalarValue>::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "item_field was evaluated outside a list item"));
        }
        const auto found = item->fields.find(reference.id);
        if (found == item->fields.end()) {
            return UiResult<UiScalarValue>::Err(RuntimeError(
                UiErrorCode::MissingBinding,
                "list item field '" + reference.id + "' was not supplied"));
        }
        return UiResult<UiScalarValue>::Ok(found->second);
    }

    auto resolved = ResolveBinding(document, bindings, reference.id);
    if (!resolved) return UiResult<UiScalarValue>::Err(std::move(resolved).error());
    const UiBindingValue& value = *resolved.value();
    return std::visit(
        [](const auto& typed) -> UiResult<UiScalarValue> {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, UiList>) {
                return UiResult<UiScalarValue>::Err(RuntimeError(
                    UiErrorCode::BindingTypeMismatch,
                    "a list cannot be formatted as scalar text"));
            } else {
                return UiResult<UiScalarValue>::Ok(UiScalarValue{typed});
            }
        },
        value);
}

[[nodiscard]] std::string FormatScalar(
    const UiScalarValue& value,
    UiNumberFormat format) {
    return std::visit(
        [format](const auto& typed) -> std::string {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return typed;
            } else if constexpr (std::is_same_v<T, bool>) {
                return typed ? "true" : "false";
            } else {
                std::ostringstream output;
                output.imbue(std::locale::classic());
                if (format.showPlus) output << std::showpos;
                output << std::fixed << std::setprecision(format.decimals)
                       << static_cast<double>(typed);
                return output.str();
            }
        },
        value);
}

[[nodiscard]] UiResult<std::string> SelectionKey(
    const UiDocument& document,
    const UiBindingTable& bindings,
    std::string_view bindingId) {
    auto resolved = ResolveBinding(document, bindings, bindingId);
    if (!resolved) return UiResult<std::string>::Err(std::move(resolved).error());
    const UiBindingValue& value = *resolved.value();
    if (const auto* text = std::get_if<std::string>(&value)) {
        return UiResult<std::string>::Ok(*text);
    }
    if (const auto* boolean = std::get_if<bool>(&value)) {
        return UiResult<std::string>::Ok(*boolean ? "true" : "false");
    }
    return UiResult<std::string>::Err(RuntimeError(
        UiErrorCode::BindingTypeMismatch,
        "selection binding must be boolean or enum"));
}

[[nodiscard]] UiResult<std::string> EvaluateText(
    const UiTextSource& source,
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiListItem* item) {
    switch (source.kind) {
    case UiTextSourceKind::Literal:
        return UiResult<std::string>::Ok(source.literal);
    case UiTextSourceKind::Value: {
        auto value = ResolveScalar(document, bindings, source.value, item);
        if (!value) return UiResult<std::string>::Err(std::move(value).error());
        return UiResult<std::string>::Ok(FormatScalar(value.value(), source.format));
    }
    case UiTextSourceKind::Compose: {
        std::string result;
        for (std::size_t index = 0; index < source.composeFormat.size();) {
            const char current = source.composeFormat[index];
            if (current == '{' && index + 1U < source.composeFormat.size() &&
                source.composeFormat[index + 1U] == '{') {
                result.push_back('{');
                index += 2U;
                continue;
            }
            if (current == '}' && index + 1U < source.composeFormat.size() &&
                source.composeFormat[index + 1U] == '}') {
                result.push_back('}');
                index += 2U;
                continue;
            }
            if (current != '{') {
                result.push_back(current);
                ++index;
                continue;
            }
            const std::size_t end = source.composeFormat.find('}', index + 1U);
            const std::string name = source.composeFormat.substr(index + 1U, end - index - 1U);
            const UiTextPlaceholder& placeholder = source.placeholders.at(name);
            auto value = ResolveScalar(document, bindings, placeholder.value, item);
            if (!value) return UiResult<std::string>::Err(std::move(value).error());
            result += FormatScalar(value.value(), placeholder.format);
            index = end + 1U;
        }
        return UiResult<std::string>::Ok(std::move(result));
    }
    case UiTextSourceKind::Select: {
        auto key = SelectionKey(document, bindings, source.selectionBinding);
        if (!key) return UiResult<std::string>::Err(std::move(key).error());
        const auto selected = source.cases.find(key.value());
        if (selected == source.cases.end()) {
            return UiResult<std::string>::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "text selection has no case for '" + key.value() + "'"));
        }
        return UiResult<std::string>::Ok(selected->second);
    }
    }
    return UiResult<std::string>::Err(RuntimeError(UiErrorCode::RuntimeState, "unknown text source kind"));
}

[[nodiscard]] UiResult<UiColor> EvaluateColor(
    const UiColorSource& source,
    const UiDocument& document,
    const UiBindingTable& bindings) {
    std::string colorId;
    if (source.kind == UiColorSourceKind::Color) {
        colorId = source.color;
    } else {
        auto key = SelectionKey(document, bindings, source.selectionBinding);
        if (!key) return UiResult<UiColor>::Err(std::move(key).error());
        const auto selected = source.cases.find(key.value());
        if (selected == source.cases.end()) {
            return UiResult<UiColor>::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "color selection has no case for '" + key.value() + "'"));
        }
        colorId = selected->second;
    }
    const auto color = document.colors.find(colorId);
    if (color == document.colors.end()) {
        return UiResult<UiColor>::Err(RuntimeError(UiErrorCode::MissingReference, "unknown color '" + colorId + "'"));
    }
    return UiResult<UiColor>::Ok(color->second);
}

[[nodiscard]] UiResult<UiColor> NamedColor(
    const UiDocument& document,
    std::string_view id) {
    const auto color = document.colors.find(std::string(id));
    if (color == document.colors.end()) {
        return UiResult<UiColor>::Err(RuntimeError(UiErrorCode::MissingReference, "unknown color '" + std::string(id) + "'"));
    }
    return UiResult<UiColor>::Ok(color->second);
}

struct FitTransform final {
    float scale{1.0F};
    UiFloat2 offset{};
};

[[nodiscard]] FitTransform MakeFit(const UiDocument& document, UiViewport viewport) noexcept {
    const float scale = std::min(
        viewport.width / document.designCanvas.size.x,
        viewport.height / document.designCanvas.size.y);
    return {scale, {
        (viewport.width - document.designCanvas.size.x * scale) * 0.5F,
        (viewport.height - document.designCanvas.size.y * scale) * 0.5F,
    }};
}

[[nodiscard]] UiRect ResolveDesignRect(
    const UiRectTransform& transform,
    UiRect parent) noexcept {
    const float anchorWidth = transform.anchorMax.x - transform.anchorMin.x;
    const float anchorHeight = transform.anchorMax.y - transform.anchorMin.y;
    const float width = parent.width * anchorWidth + transform.sizeDelta.x;
    const float height = parent.height * anchorHeight + transform.sizeDelta.y;
    const float anchorPivotX = parent.x +
        (transform.anchorMin.x + anchorWidth * transform.pivot.x) * parent.width;
    const float anchorPivotY = parent.y +
        (transform.anchorMin.y + anchorHeight * transform.pivot.y) * parent.height;
    return {
        anchorPivotX + transform.position.x - transform.pivot.x * width,
        anchorPivotY + transform.position.y - transform.pivot.y * height,
        width,
        height,
    };
}

[[nodiscard]] UiRect ToPixels(UiRect design, FitTransform fit) noexcept {
    return {
        fit.offset.x + design.x * fit.scale,
        fit.offset.y + design.y * fit.scale,
        design.width * fit.scale,
        design.height * fit.scale,
    };
}

[[nodiscard]] UiRect IntersectRect(UiRect left, UiRect right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float rightEdge = std::min(left.x + left.width, right.x + right.width);
    const float bottomEdge = std::min(left.y + left.height, right.y + right.height);
    return {
        x,
        y,
        std::max(0.0F, rightEdge - x),
        std::max(0.0F, bottomEdge - y),
    };
}

[[nodiscard]] bool ContainsHalfOpen(UiRect rect, UiFloat2 point) noexcept {
    return point.x >= rect.x && point.y >= rect.y &&
           point.x < rect.x + rect.width && point.y < rect.y + rect.height;
}

[[nodiscard]] const UiElement* FindElement(
    const std::vector<UiElement>& elements,
    std::string_view id) noexcept {
    for (const UiElement& element : elements) {
        if (element.id == id) return &element;
        if (const UiElement* child = FindElement(element.children, id)) return child;
    }
    return nullptr;
}

[[nodiscard]] UiResult<void> EvaluateElements(
    const std::vector<UiElement>& elements,
    UiRect parent,
    UiRect parentClip,
    FitTransform fit,
    const UiDocument& document,
    const UiBindingTable& bindings,
    std::optional<std::size_t> listItemIndex,
    std::vector<UiEvaluatedElement>& result,
    std::size_t& drawOrder) {
    for (const UiElement& element : elements) {
        const UiRect design = ResolveDesignRect(element.rect, parent);
        const UiRect clip = IntersectRect(parentClip, design);
        result.push_back({
            element.id,
            element.type,
            ToPixels(design, fit),
            ToPixels(clip, fit),
            drawOrder++,
            element.type == UiElementType::Button ||
                element.type == UiElementType::HorizontalSlider,
            listItemIndex,
        });
        if (element.type == UiElementType::FixedStepList) {
            auto listValue = ResolveBinding(document, bindings, element.binding);
            if (!listValue) return UiResult<void>::Err(std::move(listValue).error());
            const UiList& list = std::get<UiList>(*listValue.value());
            const std::size_t count = std::min(element.maxItems, list.items.size());
            for (std::size_t index = 0; index < count; ++index) {
                UiRect itemRect = design;
                itemRect.x += element.itemStep.x * static_cast<float>(index);
                itemRect.y += element.itemStep.y * static_cast<float>(index);
                auto itemLayout = EvaluateElements(
                    element.itemTemplate,
                    itemRect,
                    IntersectRect(clip, itemRect),
                    fit,
                    document,
                    bindings,
                    index,
                    result,
                    drawOrder);
                if (!itemLayout) return itemLayout;
            }
        }
        auto children = EvaluateElements(
            element.children,
            design,
            clip,
            fit,
            document,
            bindings,
            listItemIndex,
            result,
            drawOrder);
        if (!children) return children;
    }
    return UiResult<void>::Ok();
}

[[nodiscard]] double QuantizeSlider(const UiElement& slider, double value) noexcept {
    value = std::clamp(value, slider.minimum, slider.maximum);
    const double steps = std::round((value - slider.minimum) / slider.step);
    return std::clamp(slider.minimum + steps * slider.step, slider.minimum, slider.maximum);
}

[[nodiscard]] UiResult<double> SliderValue(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider) {
    auto value = ResolveBinding(document, bindings, slider.binding);
    if (!value) return UiResult<double>::Err(std::move(value).error());
    return UiResult<double>::Ok(std::get<double>(*value.value()));
}

[[nodiscard]] UiResult<std::optional<UiActionEvent>> SliderEvent(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider,
    double candidate) {
    auto current = SliderValue(document, bindings, slider);
    if (!current) return UiResult<std::optional<UiActionEvent>>::Err(std::move(current).error());
    const double quantized = QuantizeSlider(slider, candidate);
    if (std::abs(quantized - current.value()) <= 1.0e-9) {
        return UiResult<std::optional<UiActionEvent>>::Ok(std::nullopt);
    }
    return UiResult<std::optional<UiActionEvent>>::Ok(UiActionEvent{
        slider.action, slider.id, quantized});
}

[[nodiscard]] UiResult<std::optional<UiActionEvent>> SliderPointerEvent(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider,
    UiRect sliderPixels,
    float pointerX) {
    if (sliderPixels.width <= 0.0F) {
        return UiResult<std::optional<UiActionEvent>>::Err(RuntimeError(
            UiErrorCode::RuntimeState, "slider resolved to a non-positive width"));
    }
    const double ratio = std::clamp(
        static_cast<double>((pointerX - sliderPixels.x) / sliderPixels.width), 0.0, 1.0);
    return SliderEvent(
        document,
        bindings,
        slider,
        slider.minimum + (slider.maximum - slider.minimum) * ratio);
}

struct ComposeContext final {
    const UiDocument& document;
    const UiBindingTable& bindings;
    const UiInteractionState& interaction;
    std::string_view pressedElement;
    FitTransform fit;
    UiDrawList& drawList;
};

[[nodiscard]] UiResult<void> AppendText(
    const UiElement& element,
    UiRect designRect,
    const UiListItem* item,
    UiRect clipPixels,
    ComposeContext& context,
    std::optional<UiHorizontalAlign> alignmentOverride = std::nullopt,
    std::optional<std::string> textOverride = std::nullopt,
    std::optional<UiRect> pixelBoundsOverride = std::nullopt) {
    auto text = textOverride
        ? UiResult<std::string>::Ok(std::move(*textOverride))
        : EvaluateText(element.text, context.document, context.bindings, item);
    if (!text) return UiResult<void>::Err(std::move(text).error());
    auto color = EvaluateColor(element.textStyle.color, context.document, context.bindings);
    if (!color) return UiResult<void>::Err(std::move(color).error());
    const std::string& alias = element.textStyle.font.empty()
        ? context.document.defaultFont
        : element.textStyle.font;
    const auto font = context.document.fonts.find(alias);
    if (font == context.document.fonts.end()) {
        return UiResult<void>::Err(RuntimeError(UiErrorCode::MissingReference, "unknown font alias '" + alias + "'"));
    }
    context.drawList.commands.emplace_back(UiTextDraw{
        pixelBoundsOverride.value_or(ToPixels(designRect, context.fit)),
        std::move(text).value(),
        font->second,
        element.textStyle.pointSize * context.fit.scale,
        color.value(),
        alignmentOverride.value_or(element.textStyle.horizontalAlign),
        element.textStyle.verticalAlign,
        clipPixels,
    });
    return UiResult<void>::Ok();
}

[[nodiscard]] UiResult<void> ComposeElements(
    const std::vector<UiElement>& elements,
    UiRect parent,
    UiRect parentClip,
    const UiListItem* item,
    ComposeContext& context) {
    for (const UiElement& element : elements) {
        const UiRect design = ResolveDesignRect(element.rect, parent);
        const UiRect pixels = ToPixels(design, context.fit);
        const UiRect designClip = IntersectRect(parentClip, design);
        const UiRect clipPixels = ToPixels(designClip, context.fit);
        if (pixels.width < 0.0F || pixels.height < 0.0F) {
            return UiResult<void>::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "element '" + element.id + "' resolved to a negative size"));
        }

        switch (element.type) {
        case UiElementType::Container:
            break;
        case UiElementType::Panel: {
            auto color = EvaluateColor(element.color, context.document, context.bindings);
            if (!color) return UiResult<void>::Err(std::move(color).error());
            context.drawList.commands.emplace_back(UiQuadDraw{pixels, color.value(), clipPixels});
            break;
        }
        case UiElementType::Image: {
            auto tint = EvaluateColor(element.color, context.document, context.bindings);
            if (!tint) return UiResult<void>::Err(std::move(tint).error());
            context.drawList.commands.emplace_back(UiImageDraw{
                pixels, element.sourceUv, element.textureAsset, tint.value(), clipPixels});
            break;
        }
        case UiElementType::Text: {
            auto appended = AppendText(element, design, item, clipPixels, context);
            if (!appended) return appended;
            break;
        }
        case UiElementType::Button: {
            std::string_view colorId = element.background.normal;
            if (context.interaction.capturedElement == element.id || context.pressedElement == element.id) {
                colorId = element.background.pressed;
            } else if (context.interaction.focusedElement == element.id) {
                colorId = element.background.focused;
            }
            auto background = NamedColor(context.document, colorId);
            if (!background) return UiResult<void>::Err(std::move(background).error());
            context.drawList.commands.emplace_back(UiQuadDraw{pixels, background.value(), clipPixels});
            auto appended = AppendText(element, design, item, clipPixels, context);
            if (!appended) return appended;
            break;
        }
        case UiElementType::HorizontalSlider: {
            std::string_view colorId = element.background.normal;
            if (context.interaction.capturedElement == element.id || context.pressedElement == element.id) {
                colorId = element.background.pressed;
            } else if (context.interaction.focusedElement == element.id) {
                colorId = element.background.focused;
            }
            auto background = NamedColor(context.document, colorId);
            if (!background) return UiResult<void>::Err(std::move(background).error());
            context.drawList.commands.emplace_back(UiQuadDraw{pixels, background.value(), clipPixels});

            auto value = SliderValue(context.document, context.bindings, element);
            if (!value) return UiResult<void>::Err(std::move(value).error());
            const double clamped = std::clamp(value.value(), element.minimum, element.maximum);
            const float ratio = static_cast<float>((clamped - element.minimum) / (element.maximum - element.minimum));
            const UiRect textBounds{
                pixels.x + pixels.width * 0.04F,
                pixels.y + pixels.height * 0.08F,
                pixels.width * 0.92F,
                pixels.height * 0.48F,
            };
            auto label = AppendText(element, design, item, clipPixels, context, UiHorizontalAlign::Left, std::nullopt, textBounds);
            if (!label) return label;
            auto valueText = AppendText(
                element,
                design,
                item,
                clipPixels,
                context,
                UiHorizontalAlign::Right,
                FormatScalar(UiScalarValue{clamped}, element.valueFormat),
                textBounds);
            if (!valueText) return valueText;

            auto trackColor = NamedColor(context.document, element.trackColor);
            if (!trackColor) return UiResult<void>::Err(std::move(trackColor).error());
            auto fillColor = NamedColor(context.document, element.fillColor);
            if (!fillColor) return UiResult<void>::Err(std::move(fillColor).error());
            auto thumbColor = NamedColor(context.document, element.thumbColor);
            if (!thumbColor) return UiResult<void>::Err(std::move(thumbColor).error());
            const UiRect track{
                pixels.x + pixels.width * 0.04F,
                pixels.y + pixels.height * 0.72F,
                pixels.width * 0.92F,
                std::max(2.0F, pixels.height * 0.10F),
            };
            context.drawList.commands.emplace_back(UiQuadDraw{track, trackColor.value(), clipPixels});
            context.drawList.commands.emplace_back(UiQuadDraw{
                {track.x, track.y, track.width * ratio, track.height}, fillColor.value(), clipPixels});
            const float thumbWidth = std::max(6.0F, pixels.height * 0.14F);
            context.drawList.commands.emplace_back(UiQuadDraw{
                {track.x + track.width * ratio - thumbWidth * 0.5F,
                 track.y - track.height,
                 thumbWidth,
                 track.height * 3.0F},
                thumbColor.value(),
                clipPixels});
            break;
        }
        case UiElementType::FixedStepList: {
            auto listValue = ResolveBinding(context.document, context.bindings, element.binding);
            if (!listValue) return UiResult<void>::Err(std::move(listValue).error());
            const UiList& list = std::get<UiList>(*listValue.value());
            const std::size_t count = std::min(element.maxItems, list.items.size());
            for (std::size_t index = 0; index < count; ++index) {
                UiRect itemRect = design;
                itemRect.x += element.itemStep.x * static_cast<float>(index);
                itemRect.y += element.itemStep.y * static_cast<float>(index);
                auto composed = ComposeElements(
                    element.itemTemplate,
                    itemRect,
                    IntersectRect(designClip, itemRect),
                    &list.items[index],
                    context);
                if (!composed) return composed;
            }
            break;
        }
        }

        auto children = ComposeElements(element.children, design, designClip, item, context);
        if (!children) return children;
    }
    return UiResult<void>::Ok();
}

} // namespace

const UiEvaluatedElement* HitTestUiLayout(
    std::span<const UiEvaluatedElement> layout,
    UiFloat2 pointPixels,
    bool interactiveOnly) noexcept {
    const UiEvaluatedElement* selected = nullptr;
    for (const UiEvaluatedElement& element : layout) {
        if (interactiveOnly && !element.interactive) continue;
        if (!ContainsHalfOpen(element.boundsPixels, pointPixels) ||
            !ContainsHalfOpen(element.clipPixels, pointPixels)) {
            continue;
        }
        if (selected == nullptr || element.drawOrder >= selected->drawOrder) {
            selected = &element;
        }
    }
    return selected;
}

struct UiRuntime::Impl final {
    std::shared_ptr<const UiDocument> document;
    const UiCanvas* canvas{};
    UiInteractionState interaction;
    std::string pressedElement;
    bool pointerWasAvailable{};
    UiFloat2 lastPointerPixels{};

    void TrackPointer(const UiInputFrame& input) noexcept {
        pointerWasAvailable = input.pointerAvailable;
        if (input.pointerAvailable) lastPointerPixels = input.pointerPixels;
    }

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluateInternal(
        const UiBindingTable& bindings,
        UiViewport viewport) const {
        if (!document || canvas == nullptr) {
            return UiResult<std::vector<UiEvaluatedElement>>::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "UiRuntime has no active canvas"));
        }
        if (!IsValidViewport(viewport)) {
            return UiResult<std::vector<UiEvaluatedElement>>::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "UI viewport must be finite and positive"));
        }
        const UiRect root{
            0.0F,
            0.0F,
            document->designCanvas.size.x,
            document->designCanvas.size.y,
        };
        std::vector<UiEvaluatedElement> result;
        std::size_t drawOrder = 0;
        auto evaluated = EvaluateElements(
            canvas->children,
            root,
            root,
            MakeFit(*document, viewport),
            *document,
            bindings,
            std::nullopt,
            result,
            drawOrder);
        if (!evaluated) {
            return UiResult<std::vector<UiEvaluatedElement>>::Err(
                std::move(evaluated).error());
        }
        return UiResult<std::vector<UiEvaluatedElement>>::Ok(std::move(result));
    }

    [[nodiscard]] UiResult<UiDrawList> ComposeInternal(
        const UiBindingTable& bindings,
        UiViewport viewport) const {
        if (!document || canvas == nullptr) {
            return UiResult<UiDrawList>::Err(RuntimeError(UiErrorCode::RuntimeState, "UiRuntime has no active canvas"));
        }
        if (!IsValidViewport(viewport)) {
            return UiResult<UiDrawList>::Err(RuntimeError(UiErrorCode::RuntimeState, "UI viewport must be finite and positive"));
        }
        const auto backdrop = document->colors.find(canvas->backdropColor);
        if (backdrop == document->colors.end()) {
            return UiResult<UiDrawList>::Err(RuntimeError(UiErrorCode::MissingReference, "active canvas backdrop color is missing"));
        }
        UiDrawList result;
        const UiRect viewportRect{0.0F, 0.0F, viewport.width, viewport.height};
        result.commands.emplace_back(UiQuadDraw{viewportRect, backdrop->second, viewportRect});
        ComposeContext context{
            *document, bindings, interaction, pressedElement, MakeFit(*document, viewport), result};
        const UiRect root{0.0F, 0.0F, document->designCanvas.size.x, document->designCanvas.size.y};
        auto composed = ComposeElements(canvas->children, root, root, nullptr, context);
        if (!composed) return UiResult<UiDrawList>::Err(std::move(composed).error());
        return UiResult<UiDrawList>::Ok(std::move(result));
    }
};

UiRuntime::UiRuntime() : impl_(std::make_unique<Impl>()) {}
UiRuntime::~UiRuntime() = default;
UiRuntime::UiRuntime(UiRuntime&&) noexcept = default;
UiRuntime& UiRuntime::operator=(UiRuntime&&) noexcept = default;

UiResult<void> UiRuntime::Initialize(std::shared_ptr<const UiDocument> document) {
    if (!document) {
        return UiResult<void>::Err(RuntimeError(UiErrorCode::RuntimeState, "UiRuntime requires a non-null document"));
    }
    auto validation = UiDocumentCodec::Validate(*document);
    if (!validation) return validation;
    impl_->document = std::move(document);
    impl_->canvas = nullptr;
    impl_->interaction = {};
    impl_->pressedElement.clear();
    impl_->pointerWasAvailable = false;
    impl_->lastPointerPixels = {};
    return UiResult<void>::Ok();
}

void UiRuntime::Reset() noexcept {
    impl_->document.reset();
    impl_->canvas = nullptr;
    impl_->interaction = {};
    impl_->pressedElement.clear();
    impl_->pointerWasAvailable = false;
    impl_->lastPointerPixels = {};
}

UiResult<void> UiRuntime::ActivateCanvas(std::string_view canvasId) {
    if (!impl_->document) {
        return UiResult<void>::Err(RuntimeError(UiErrorCode::RuntimeState, "UiRuntime is not initialized"));
    }
    const auto found = std::find_if(
        impl_->document->canvases.begin(),
        impl_->document->canvases.end(),
        [canvasId](const UiCanvas& candidate) { return candidate.id == canvasId; });
    if (found == impl_->document->canvases.end()) {
        return UiResult<void>::Err(RuntimeError(UiErrorCode::MissingReference, "unknown canvas '" + std::string(canvasId) + "'"));
    }
    impl_->canvas = &*found;
    impl_->interaction.activeCanvas = found->id;
    impl_->interaction.capturedElement.clear();
    impl_->pressedElement.clear();
    impl_->pointerWasAvailable = false;
    impl_->lastPointerPixels = {};
    if (found->defaultFocus) {
        impl_->interaction.focusedElement = *found->defaultFocus;
    } else if (!found->focusOrder.empty()) {
        impl_->interaction.focusedElement = found->focusOrder.front();
    } else {
        impl_->interaction.focusedElement.clear();
    }
    return UiResult<void>::Ok();
}

UiResult<std::vector<UiActionEvent>> UiRuntime::Update(
    const UiInputFrame& input,
    const UiBindingTable& bindings,
    UiViewport viewport) {
    if (!impl_->document || impl_->canvas == nullptr) {
        return UiResult<std::vector<UiActionEvent>>::Err(RuntimeError(UiErrorCode::RuntimeState, "UiRuntime has no active canvas"));
    }
    if (!IsValidViewport(viewport)) {
        return UiResult<std::vector<UiActionEvent>>::Err(RuntimeError(UiErrorCode::RuntimeState, "UI viewport must be finite and positive"));
    }
    impl_->pressedElement.clear();
    std::vector<UiActionEvent> events;
    const bool pointerMoved = input.pointerAvailable &&
        impl_->pointerWasAvailable &&
        (input.pointerPixels.x != impl_->lastPointerPixels.x ||
         input.pointerPixels.y != impl_->lastPointerPixels.y);

    if (input.cancelPressed) {
        impl_->interaction.capturedElement.clear();
        if (impl_->canvas->cancelAction) {
            events.push_back({*impl_->canvas->cancelAction, {}, std::monostate{}});
        }
        impl_->TrackPointer(input);
        return UiResult<std::vector<UiActionEvent>>::Ok(std::move(events));
    }

    auto evaluated = impl_->EvaluateInternal(bindings, viewport);
    if (!evaluated) {
        return UiResult<std::vector<UiActionEvent>>::Err(std::move(evaluated).error());
    }
    const std::vector<UiEvaluatedElement>& layouts = evaluated.value();
    const UiEvaluatedElement* hit = input.pointerAvailable
        ? HitTestUiLayout(layouts, input.pointerPixels, true)
        : nullptr;
    if (hit != nullptr && (pointerMoved || input.pointerPrimaryPressed)) {
        impl_->interaction.focusedElement = hit->id;
    }

    bool pointerSliderProcessed = false;
    if (input.pointerPrimaryPressed && hit != nullptr) {
        const UiElement* hitElement = FindElement(impl_->canvas->children, hit->id);
        if (hitElement == nullptr) {
            return UiResult<std::vector<UiActionEvent>>::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "evaluated interactive element is missing from the document"));
        }
        impl_->pressedElement = hitElement->id;
        if (hitElement->type == UiElementType::Button) {
            events.push_back({hitElement->action, hitElement->id, std::monostate{}});
        } else {
            impl_->interaction.capturedElement = hitElement->id;
            auto event = SliderPointerEvent(
                *impl_->document,
                bindings,
                *hitElement,
                hit->boundsPixels,
                input.pointerPixels.x);
            if (!event) return UiResult<std::vector<UiActionEvent>>::Err(std::move(event).error());
            if (event.value()) events.push_back(std::move(*event.value()));
            pointerSliderProcessed = true;
        }
    }

    if (!impl_->interaction.capturedElement.empty() && input.pointerAvailable &&
        !pointerSliderProcessed && (input.pointerPrimaryHeld || input.pointerPrimaryReleased)) {
        const auto captured = std::find_if(
            layouts.begin(),
            layouts.end(),
            [this](const UiEvaluatedElement& layout) {
                return layout.id == impl_->interaction.capturedElement;
            });
        if (captured != layouts.end()) {
            const UiElement* capturedElement = FindElement(
                impl_->canvas->children,
                captured->id);
            if (capturedElement == nullptr) {
                return UiResult<std::vector<UiActionEvent>>::Err(RuntimeError(
                    UiErrorCode::RuntimeState,
                    "captured slider is missing from the document"));
            }
            auto event = SliderPointerEvent(
                *impl_->document,
                bindings,
                *capturedElement,
                captured->boundsPixels,
                input.pointerPixels.x);
            if (!event) return UiResult<std::vector<UiActionEvent>>::Err(std::move(event).error());
            if (event.value()) events.push_back(std::move(*event.value()));
        }
    }
    if (input.pointerPrimaryReleased) impl_->interaction.capturedElement.clear();

    if (input.focusPreviousPressed != input.focusNextPressed && !impl_->canvas->focusOrder.empty()) {
        const auto focused = std::find(
            impl_->canvas->focusOrder.begin(),
            impl_->canvas->focusOrder.end(),
            impl_->interaction.focusedElement);
        std::size_t index = focused == impl_->canvas->focusOrder.end()
            ? 0U
            : static_cast<std::size_t>(focused - impl_->canvas->focusOrder.begin());
        if (input.focusPreviousPressed) {
            index = (index + impl_->canvas->focusOrder.size() - 1U) % impl_->canvas->focusOrder.size();
        } else {
            index = (index + 1U) % impl_->canvas->focusOrder.size();
        }
        impl_->interaction.focusedElement = impl_->canvas->focusOrder[index];
    }

    const UiElement* focused = FindElement(impl_->canvas->children, impl_->interaction.focusedElement);
    if (focused != nullptr && focused->type == UiElementType::HorizontalSlider &&
        input.adjustPreviousPressed != input.adjustNextPressed) {
        auto current = SliderValue(*impl_->document, bindings, *focused);
        if (!current) return UiResult<std::vector<UiActionEvent>>::Err(std::move(current).error());
        auto event = SliderEvent(
            *impl_->document,
            bindings,
            *focused,
            current.value() + (input.adjustNextPressed ? focused->step : -focused->step));
        if (!event) return UiResult<std::vector<UiActionEvent>>::Err(std::move(event).error());
        if (event.value()) events.push_back(std::move(*event.value()));
    }
    if (input.activatePressed && focused != nullptr && focused->type == UiElementType::Button) {
        impl_->pressedElement = focused->id;
        events.push_back({focused->action, focused->id, std::monostate{}});
    }
    impl_->TrackPointer(input);
    return UiResult<std::vector<UiActionEvent>>::Ok(std::move(events));
}

UiResult<UiDrawList> UiRuntime::Compose(
    const UiBindingTable& bindings,
    UiViewport viewport) const {
    return impl_->ComposeInternal(bindings, viewport);
}

UiResult<UiDrawList> UiRuntime::ComposePreview(UiViewport viewport) const {
    if (!impl_->document) {
        return UiResult<UiDrawList>::Err(RuntimeError(UiErrorCode::RuntimeState, "UiRuntime is not initialized"));
    }
    UiBindingTable bindings;
    for (const UiBindingDeclaration& declaration : impl_->document->bindings) {
        bindings.emplace(declaration.id, declaration.preview);
    }
    return impl_->ComposeInternal(bindings, viewport);
}

UiResult<std::vector<UiEvaluatedElement>> UiRuntime::EvaluateLayout(
    const UiBindingTable& bindings,
    UiViewport viewport) const {
    return impl_->EvaluateInternal(bindings, viewport);
}

UiResult<std::vector<UiEvaluatedElement>> UiRuntime::EvaluatePreviewLayout(
    UiViewport viewport) const {
    if (!impl_->document) {
        return UiResult<std::vector<UiEvaluatedElement>>::Err(RuntimeError(
            UiErrorCode::RuntimeState,
            "UiRuntime is not initialized"));
    }
    UiBindingTable bindings;
    for (const UiBindingDeclaration& declaration : impl_->document->bindings) {
        bindings.emplace(declaration.id, declaration.preview);
    }
    return impl_->EvaluateInternal(bindings, viewport);
}

const UiInteractionState& UiRuntime::InteractionState() const noexcept {
    return impl_->interaction;
}

} // namespace Engine::Ui
