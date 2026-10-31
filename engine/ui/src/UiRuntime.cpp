#include "engine/ui/UiRuntime.hpp"

#include "engine/base/Assert.hpp"

#include "engine/math/scalar/Scalar.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

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
        return Base::Err(RuntimeError(
            UiErrorCode::MissingReference,
            "UI document refers to unknown binding '" + std::string(id) + "'"));
    }
    const auto found = bindings.find(std::string(id));
    if (found == bindings.end()) {
        return Base::Err(RuntimeError(
            UiErrorCode::MissingBinding,
            "binding value '" + std::string(id) + "' was not supplied"));
    }
    if (!MatchesBinding(found->second, declaration->type)) {
        return Base::Err(RuntimeError(
            UiErrorCode::BindingTypeMismatch,
            "binding value '" + std::string(id) + "' has the wrong type"));
    }
    if (declaration->type == UiBindingType::Enum) {
        const std::string& selected = std::get<std::string>(found->second);
        if (std::find(declaration->enumValues.begin(), declaration->enumValues.end(), selected) ==
            declaration->enumValues.end()) {
            return Base::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "enum binding '" + std::string(id) + "' is outside its declared domain"));
        }
    }
    if (declaration->type == UiBindingType::List) {
        const UiList& list = std::get<UiList>(found->second);
        for (const UiListItem& item : list.items) {
            if (item.fields.size() != declaration->itemFields.size()) {
                return Base::Err(RuntimeError(
                    UiErrorCode::BindingTypeMismatch,
                    "list binding '" + std::string(id) + "' has an invalid item shape"));
            }
            for (const auto& [name, type] : declaration->itemFields) {
                const auto field = item.fields.find(name);
                if (field == item.fields.end() || !MatchesScalar(field->second, type)) {
                    return Base::Err(RuntimeError(
                        UiErrorCode::BindingTypeMismatch,
                        "list binding '" + std::string(id) + "' field '" + name + "' has the wrong type"));
                }
            }
        }
    }
    return &found->second;
}

[[nodiscard]] UiResult<UiScalarValue> ResolveScalar(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiValueReference& reference,
    const UiListItem* item) {
    if (reference.kind == UiValueReferenceKind::ItemField) {
        if (item == nullptr) {
            return Base::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "item_field was evaluated outside a list item"));
        }
        const auto found = item->fields.find(reference.id);
        if (found == item->fields.end()) {
            return Base::Err(RuntimeError(
                UiErrorCode::MissingBinding,
                "list item field '" + reference.id + "' was not supplied"));
        }
        return found->second;
    }

    auto resolved = ResolveBinding(document, bindings, reference.id);
    if (!resolved) return Base::Err(std::move(resolved).error());
    const UiBindingValue& value = *resolved.value();
    return std::visit(
        [](const auto& typed) -> UiResult<UiScalarValue> {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, UiList>) {
                return Base::Err(RuntimeError(
                    UiErrorCode::BindingTypeMismatch,
                    "a list cannot be formatted as scalar text"));
            } else {
                return UiScalarValue{typed};
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
    if (!resolved) return Base::Err(std::move(resolved).error());
    const UiBindingValue& value = *resolved.value();
    if (const auto* text = std::get_if<std::string>(&value)) {
        return *text;
    }
    if (const auto* boolean = std::get_if<bool>(&value)) {
        return *boolean ? "true" : "false";
    }
    return Base::Err(RuntimeError(
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
        return source.literal;
    case UiTextSourceKind::Value: {
        auto value = ResolveScalar(document, bindings, source.value, item);
        if (!value) return Base::Err(std::move(value).error());
        return FormatScalar(value.value(), source.format);
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
            // Validate guarantees every referenced placeholder exists.
            const auto placeholderEntry = source.placeholders.find(name);
            GYO_ASSERT(placeholderEntry != source.placeholders.end());
            const UiTextPlaceholder& placeholder = placeholderEntry->second;
            auto value = ResolveScalar(document, bindings, placeholder.value, item);
            if (!value) return Base::Err(std::move(value).error());
            result += FormatScalar(value.value(), placeholder.format);
            index = end + 1U;
        }
        return std::move(result);
    }
    case UiTextSourceKind::Select: {
        auto key = SelectionKey(document, bindings, source.selectionBinding);
        if (!key) return Base::Err(std::move(key).error());
        const auto selected = source.cases.find(key.value());
        if (selected == source.cases.end()) {
            return Base::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "text selection has no case for '" + key.value() + "'"));
        }
        return selected->second;
    }
    }
    GYO_UNREACHABLE();
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
        if (!key) return Base::Err(std::move(key).error());
        const auto selected = source.cases.find(key.value());
        if (selected == source.cases.end()) {
            return Base::Err(RuntimeError(
                UiErrorCode::BindingTypeMismatch,
                "color selection has no case for '" + key.value() + "'"));
        }
        colorId = selected->second;
    }
    const auto color = document.colors.find(colorId);
    if (color == document.colors.end()) {
        return Base::Err(RuntimeError(UiErrorCode::MissingReference, "unknown color '" + colorId + "'"));
    }
    return color->second;
}

[[nodiscard]] UiResult<UiColor> NamedColor(
    const UiDocument& document,
    std::string_view id) {
    const auto color = document.colors.find(std::string(id));
    if (color == document.colors.end()) {
        return Base::Err(RuntimeError(UiErrorCode::MissingReference, "unknown color '" + std::string(id) + "'"));
    }
    return color->second;
}

[[nodiscard]] UiCanvasFit MakeFit(const UiDocument& document, UiViewport viewport) noexcept {
    return FitDesignCanvas(document.designCanvas.size, viewport);
}

[[nodiscard]] Math::Rect ResolveDesignRect(
    const UiRectTransform& transform,
    Math::Rect parent) noexcept {
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

[[nodiscard]] Math::Rect ToPixels(Math::Rect design, UiCanvasFit fit) noexcept {
    return {
        fit.offset.x + design.x * fit.scale,
        fit.offset.y + design.y * fit.scale,
        design.width * fit.scale,
        design.height * fit.scale,
    };
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

// The list item an element is evaluated in: the item for text and the index
// for the evaluated layout. Both are empty outside fixed-step list templates.
struct ListPosition final {
    const UiListItem* item{};
    std::optional<std::size_t> index;
};

// The single layout walk shared by evaluation and composition: each element is
// resolved against its parent, visited, then its list items (fixed-step lists
// only) and its children follow in document order.
template <class Visit>
[[nodiscard]] UiResult<void> WalkElements(
    const std::vector<UiElement>& elements,
    Math::Rect parent,
    Math::Rect parentClip,
    ListPosition position,
    const UiDocument& document,
    const UiBindingTable& bindings,
    Visit& visit) {
    for (const UiElement& element : elements) {
        const Math::Rect design = ResolveDesignRect(element.rect, parent);
        const Math::Rect clip = Math::Intersection(parentClip, design);
        auto visited = visit(element, design, clip, position);
        if (!visited) return visited;
        if (element.type == UiElementType::FixedStepList) {
            auto listValue = ResolveBinding(document, bindings, element.binding);
            if (!listValue) return Base::Err(std::move(listValue).error());
            const UiList& list = std::get<UiList>(*listValue.value());
            const std::size_t count = Math::Min(element.maxItems, list.items.size());
            for (std::size_t index = 0; index < count; ++index) {
                Math::Rect itemRect = design;
                itemRect.x += element.itemStep.x * static_cast<float>(index);
                itemRect.y += element.itemStep.y * static_cast<float>(index);
                auto items = WalkElements(
                    element.itemTemplate,
                    itemRect,
                    Math::Intersection(clip, itemRect),
                    ListPosition{&list.items[index], index},
                    document,
                    bindings,
                    visit);
                if (!items) return items;
            }
        }
        auto children = WalkElements(element.children, design, clip, position, document, bindings, visit);
        if (!children) return children;
    }
    return {};
}

[[nodiscard]] double QuantizeSlider(const UiElement& slider, double value) noexcept {
    value = Math::Clamp(value, slider.minimum, slider.maximum);
    const double steps = std::round((value - slider.minimum) / slider.step);
    return Math::Clamp(slider.minimum + steps * slider.step, slider.minimum, slider.maximum);
}

[[nodiscard]] UiResult<double> SliderValue(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider) {
    auto value = ResolveBinding(document, bindings, slider.binding);
    if (!value) return Base::Err(std::move(value).error());
    return std::get<double>(*value.value());
}

[[nodiscard]] UiResult<std::string> FieldValue(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& field) {
    auto value = ResolveBinding(document, bindings, field.binding);
    if (!value) return Base::Err(std::move(value).error());
    return std::get<std::string>(*value.value());
}

[[nodiscard]] UiResult<std::optional<UiActionEvent>> SliderEvent(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider,
    double candidate) {
    auto current = SliderValue(document, bindings, slider);
    if (!current) return Base::Err(std::move(current).error());
    const double quantized = QuantizeSlider(slider, candidate);
    if (std::abs(quantized - current.value()) <= 1.0e-9) {
        return std::nullopt;
    }
    return UiActionEvent{
        slider.action, slider.id, quantized};
}

[[nodiscard]] UiResult<std::optional<UiActionEvent>> SliderPointerEvent(
    const UiDocument& document,
    const UiBindingTable& bindings,
    const UiElement& slider,
    Math::Rect sliderPixels,
    float pointerX) {
    if (sliderPixels.width <= 0.0F) {
        return Base::Err(RuntimeError(
            UiErrorCode::RuntimeState, "slider resolved to a non-positive width"));
    }
    const double ratio = Math::Clamp(
        static_cast<double>((pointerX - sliderPixels.x) / sliderPixels.width), 0.0, 1.0);
    return SliderEvent(
        document,
        bindings,
        slider,
        slider.minimum + (slider.maximum - slider.minimum) * ratio);
}

// The text field being edited (Design Invariant 4): the edit lives only
// until it is submitted, cancelled, unfocused or overtaken by its binding.
struct TextEdit final {
    std::string element;
    std::string text;
    std::size_t cursor{};          // bytes, at a code point boundary
    bool selectAll{};
    std::string composition;       // shown only (Design Invariant 6)
    std::int32_t compositionCursor{-1};
    std::string startValue;        // the binding value when editing began
};

[[nodiscard]] std::size_t SequenceLength(const unsigned char lead) noexcept {
    return lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
}

// The byte offset of code point index (clamped to the end) in valid UTF-8.
[[nodiscard]] std::size_t CodePointOffset(std::string_view utf8, std::size_t index) noexcept {
    std::size_t offset = 0;
    while (index > 0 && offset < utf8.size()) {
        offset = UiNextCodePoint(utf8, offset);
        --index;
    }
    return offset;
}

void ApplyTextInput(TextEdit& edit, const UiTextInput& input, const UiElement& field, const UiTextFilter& filter) {
    const auto clearSelection = [&edit] {
        if (!edit.selectAll) return false;
        edit.text.clear();
        edit.cursor = 0;
        edit.selectAll = false;
        return true;
    };
    switch (input.kind) {
    case UiTextInputKind::Insert: {
        // The filter's result is cleaned too: Ui keeps one line of whole code points.
        std::string accepted = UiSingleLineText(filter ? filter(field.id, input.text) : input.text);
        if (accepted.empty()) break;
        clearSelection();
        const std::size_t used = UiCodePointCount(edit.text);
        const std::size_t room = field.maxLength > used ? field.maxLength - used : 0;
        accepted.resize(CodePointOffset(accepted, room));
        edit.text.insert(edit.cursor, accepted);
        edit.cursor += accepted.size();
        break;
    }
    case UiTextInputKind::Composition:
        edit.composition = UiSingleLineText(input.text);
        edit.compositionCursor = input.cursor;
        break;
    case UiTextInputKind::Backspace:
        if (clearSelection() || edit.cursor == 0) break;
        {
            const std::size_t previous = UiPreviousCodePoint(edit.text, edit.cursor);
            edit.text.erase(previous, edit.cursor - previous);
            edit.cursor = previous;
        }
        break;
    case UiTextInputKind::Delete:
        if (clearSelection() || edit.cursor >= edit.text.size()) break;
        edit.text.erase(edit.cursor, UiNextCodePoint(edit.text, edit.cursor) - edit.cursor);
        break;
    case UiTextInputKind::MoveLeft:
        edit.selectAll = false;
        edit.cursor = UiPreviousCodePoint(edit.text, edit.cursor);
        break;
    case UiTextInputKind::MoveRight:
        edit.selectAll = false;
        edit.cursor = UiNextCodePoint(edit.text, edit.cursor);
        break;
    case UiTextInputKind::MoveHome:
        edit.selectAll = false;
        edit.cursor = 0;
        break;
    case UiTextInputKind::MoveEnd:
        edit.selectAll = false;
        edit.cursor = edit.text.size();
        break;
    case UiTextInputKind::SelectAll:
        edit.selectAll = !edit.text.empty();
        edit.cursor = edit.text.size();
        break;
    }
}

struct ComposeContext final {
    const UiDocument& document;
    const UiBindingTable& bindings;
    const UiInteractionState& interaction;
    std::string_view pressedElement;
    UiCanvasFit fit;
    UiDrawList& drawList;
    const TextEdit* edit{};
};

[[nodiscard]] UiResult<void> AppendText(
    const UiElement& element,
    Math::Rect designRect,
    const UiListItem* item,
    Math::Rect clipPixels,
    ComposeContext& context,
    std::optional<UiHorizontalAlign> alignmentOverride = std::nullopt,
    std::optional<std::string> textOverride = std::nullopt,
    std::optional<Math::Rect> pixelBoundsOverride = std::nullopt) {
    auto text = textOverride
        ? UiResult<std::string>(std::move(*textOverride))
        : EvaluateText(element.text, context.document, context.bindings, item);
    if (!text) return Base::Err(std::move(text).error());
    auto color = EvaluateColor(element.textStyle.color, context.document, context.bindings);
    if (!color) return Base::Err(std::move(color).error());
    const std::string& alias = element.textStyle.font.empty()
        ? context.document.defaultFont
        : element.textStyle.font;
    const auto font = context.document.fonts.find(alias);
    if (font == context.document.fonts.end()) {
        return Base::Err(RuntimeError(UiErrorCode::MissingReference, "unknown font alias '" + alias + "'"));
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
    return {};
}

// Composition of one element; WalkElements supplies its list items and children.
[[nodiscard]] UiResult<void> ComposeElement(
    const UiElement& element,
    Math::Rect design,
    Math::Rect designClip,
    const UiListItem* item,
    ComposeContext& context) {
    const Math::Rect pixels = ToPixels(design, context.fit);
    const Math::Rect clipPixels = ToPixels(designClip, context.fit);
    if (pixels.width < 0.0F || pixels.height < 0.0F) {
        return Base::Err(RuntimeError(
            UiErrorCode::RuntimeState,
            "element '" + element.id + "' resolved to a negative size"));
    }

    switch (element.type) {
    case UiElementType::Container:
        break;
    case UiElementType::Panel: {
        auto color = EvaluateColor(element.color, context.document, context.bindings);
        if (!color) return Base::Err(std::move(color).error());
        context.drawList.commands.emplace_back(UiQuadDraw{pixels, color.value(), clipPixels});
        break;
    }
    case UiElementType::Image: {
        auto tint = EvaluateColor(element.color, context.document, context.bindings);
        if (!tint) return Base::Err(std::move(tint).error());
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
        if (!background) return Base::Err(std::move(background).error());
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
        if (!background) return Base::Err(std::move(background).error());
        context.drawList.commands.emplace_back(UiQuadDraw{pixels, background.value(), clipPixels});

        auto value = SliderValue(context.document, context.bindings, element);
        if (!value) return Base::Err(std::move(value).error());
        const double clamped = Math::Clamp(value.value(), element.minimum, element.maximum);
        const float ratio = static_cast<float>((clamped - element.minimum) / (element.maximum - element.minimum));
        const Math::Rect textBounds{
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
        if (!trackColor) return Base::Err(std::move(trackColor).error());
        auto fillColor = NamedColor(context.document, element.fillColor);
        if (!fillColor) return Base::Err(std::move(fillColor).error());
        auto thumbColor = NamedColor(context.document, element.thumbColor);
        if (!thumbColor) return Base::Err(std::move(thumbColor).error());
        const Math::Rect track{
            pixels.x + pixels.width * 0.04F,
            pixels.y + pixels.height * 0.72F,
            pixels.width * 0.92F,
            Math::Max(2.0F, pixels.height * 0.10F),
        };
        context.drawList.commands.emplace_back(UiQuadDraw{track, trackColor.value(), clipPixels});
        context.drawList.commands.emplace_back(UiQuadDraw{
            {track.x, track.y, track.width * ratio, track.height}, fillColor.value(), clipPixels});
        const float thumbWidth = Math::Max(6.0F, pixels.height * 0.14F);
        context.drawList.commands.emplace_back(UiQuadDraw{
            {track.x + track.width * ratio - thumbWidth * 0.5F,
             track.y - track.height,
             thumbWidth,
             track.height * 3.0F},
            thumbColor.value(),
            clipPixels});
        break;
    }
    case UiElementType::FixedStepList:
        // The walk composes the list items.
        break;
    case UiElementType::TextField: {
        const bool editing = context.edit != nullptr && context.edit->element == element.id;
        std::string_view colorId = element.background.normal;
        if (editing || context.pressedElement == element.id) {
            colorId = element.background.pressed;
        } else if (context.interaction.focusedElement == element.id) {
            colorId = element.background.focused;
        }
        auto background = NamedColor(context.document, colorId);
        if (!background) return Base::Err(std::move(background).error());
        context.drawList.commands.emplace_back(UiQuadDraw{pixels, background.value(), clipPixels});

        auto textColor = EvaluateColor(element.textStyle.color, context.document, context.bindings);
        if (!textColor) return Base::Err(std::move(textColor).error());
        UiColor color = textColor.value();
        std::string shown;
        std::optional<UiTextCaret> caret;
        if (editing) {
            const TextEdit& edit = *context.edit;
            shown = edit.text;
            shown.insert(edit.cursor, edit.composition);
            const std::size_t compositionEnd = edit.cursor + edit.composition.size();
            const std::size_t cursor = edit.compositionCursor < 0
                ? compositionEnd
                : edit.cursor + CodePointOffset(edit.composition, static_cast<std::size_t>(edit.compositionCursor));
            caret = UiTextCaret{cursor, edit.cursor, compositionEnd, 0, edit.selectAll ? shown.size() : 0, color};
        } else {
            auto value = FieldValue(context.document, context.bindings, element);
            if (!value) return Base::Err(std::move(value).error());
            shown = std::move(value).value();
            if (shown.empty() && !element.placeholder.empty()) {
                shown = element.placeholder;
                auto placeholderColor = NamedColor(context.document, element.placeholderColor);
                if (!placeholderColor) return Base::Err(std::move(placeholderColor).error());
                color = placeholderColor.value();
            }
        }
        const std::string& alias = element.textStyle.font.empty() ? context.document.defaultFont : element.textStyle.font;
        const auto font = context.document.fonts.find(alias);
        if (font == context.document.fonts.end()) {
            return Base::Err(RuntimeError(UiErrorCode::MissingReference, "unknown font alias '" + alias + "'"));
        }
        // The text is inset like the slider's label and clipped to the field:
        // a field does not scroll, so it is sized for its max_length.
        const float inset = pixels.width * 0.04F;
        const Math::Rect textBounds{pixels.x + inset, pixels.y, Math::Max(0.0F, pixels.width - inset * 2.0F), pixels.height};
        const float clipLeft = Math::Max(clipPixels.x, pixels.x);
        const float clipTop = Math::Max(clipPixels.y, pixels.y);
        const float clipRight = Math::Min(clipPixels.x + clipPixels.width, pixels.x + pixels.width);
        const float clipBottom = Math::Min(clipPixels.y + clipPixels.height, pixels.y + pixels.height);
        const Math::Rect fieldClip{clipLeft, clipTop, Math::Max(0.0F, clipRight - clipLeft), Math::Max(0.0F, clipBottom - clipTop)};
        context.drawList.commands.emplace_back(UiTextDraw{
            textBounds,
            std::move(shown),
            font->second,
            element.textStyle.pointSize * context.fit.scale,
            color,
            element.textStyle.horizontalAlign,
            element.textStyle.verticalAlign,
            fieldClip,
            caret,
        });
        break;
    }
    }
    return {};
}

} // namespace

std::size_t UiCodePointCount(const std::string_view utf8) noexcept {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < utf8.size(); offset = UiNextCodePoint(utf8, offset)) ++count;
    return count;
}

std::size_t UiPreviousCodePoint(const std::string_view utf8, std::size_t offset) noexcept {
    offset = Math::Min(offset, utf8.size());
    if (offset == 0) return 0;
    --offset;
    while (offset > 0 && (static_cast<unsigned char>(utf8[offset]) & 0xC0) == 0x80) --offset;
    return offset;
}

std::size_t UiNextCodePoint(const std::string_view utf8, std::size_t offset) noexcept {
    if (offset >= utf8.size()) return utf8.size();
    ++offset;
    while (offset < utf8.size() && (static_cast<unsigned char>(utf8[offset]) & 0xC0) == 0x80) ++offset;
    return offset;
}

std::string UiSingleLineText(const std::string_view utf8) {
    std::string result;
    for (std::size_t index = 0; index < utf8.size();) {
        const auto lead = static_cast<unsigned char>(utf8[index]);
        const std::size_t length = SequenceLength(lead);
        bool whole = length != 0 && index + length <= utf8.size();
        for (std::size_t next = 1; whole && next < length; ++next) {
            whole = (static_cast<unsigned char>(utf8[index + next]) & 0xC0) == 0x80;
        }
        if (!whole) {
            ++index;
            continue;
        }
        if (length > 1 || (lead >= 0x20 && lead != 0x7F)) result.append(utf8.substr(index, length));
        index += length;
    }
    return result;
}

UiCanvasFit FitDesignCanvas(Math::Vec2 designSize, UiViewport viewport) noexcept {
    const float scale = Math::Min(viewport.width / designSize.x, viewport.height / designSize.y);
    return {scale, {
        (viewport.width - designSize.x * scale) * 0.5F,
        (viewport.height - designSize.y * scale) * 0.5F,
    }};
}

bool ContainsUiPoint(Math::Rect rect, Math::Vec2 point) noexcept {
    return point.x >= rect.x && point.y >= rect.y &&
           point.x < rect.x + rect.width && point.y < rect.y + rect.height;
}

Math::Vec2 AlignUiText(
    Math::Rect bounds, Math::Vec2 extent,
    UiHorizontalAlign horizontal, UiVerticalAlign vertical) noexcept {
    Math::Vec2 position{bounds.x, bounds.y};
    if (horizontal == UiHorizontalAlign::Center) {
        position.x += (bounds.width - extent.x) * 0.5F;
    } else if (horizontal == UiHorizontalAlign::Right) {
        position.x += bounds.width - extent.x;
    }
    if (vertical == UiVerticalAlign::Center) {
        position.y += (bounds.height - extent.y) * 0.5F;
    } else if (vertical == UiVerticalAlign::Bottom) {
        position.y += bounds.height - extent.y;
    }
    return position;
}

const UiEvaluatedElement* HitTestUiLayout(
    std::span<const UiEvaluatedElement> layout,
    Math::Vec2 pointPixels,
    bool interactiveOnly) noexcept {
    const UiEvaluatedElement* selected = nullptr;
    for (const UiEvaluatedElement& element : layout) {
        if (interactiveOnly && !element.interactive) continue;
        if (!ContainsUiPoint(element.boundsPixels, pointPixels) ||
            !ContainsUiPoint(element.clipPixels, pointPixels)) {
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
    Math::Vec2 lastPointerPixels{};
    std::optional<TextEdit> edit;
    UiTextFilter textFilter;

    [[nodiscard]] UiResult<void> BeginEdit(const UiElement& field, const UiBindingTable& bindings) {
        auto value = FieldValue(*document, bindings, field);
        if (!value) return Base::Err(std::move(value).error());
        TextEdit started;
        started.element = field.id;
        started.text = value.value();
        started.cursor = started.text.size();
        started.startValue = std::move(value).value();
        edit = std::move(started);
        interaction.editingElement = field.id;
        return {};
    }

    void EndEdit() noexcept {
        edit.reset();
        interaction.editingElement.clear();
    }

    void TrackPointer(const UiInputFrame& input) noexcept {
        pointerWasAvailable = input.pointerAvailable;
        if (input.pointerAvailable) lastPointerPixels = input.pointerPixels;
    }

    [[nodiscard]] UiResult<std::vector<UiEvaluatedElement>> EvaluateInternal(
        const UiBindingTable& bindings,
        UiViewport viewport) const {
        // Evaluating before ActivateCanvas is API misuse.
        GYO_ASSERT(document && canvas != nullptr);
        if (!IsValidViewport(viewport)) {
            return Base::Err(RuntimeError(
                UiErrorCode::RuntimeState,
                "UI viewport must be finite and positive"));
        }
        const Math::Rect root{
            0.0F,
            0.0F,
            document->designCanvas.size.x,
            document->designCanvas.size.y,
        };
        std::vector<UiEvaluatedElement> result;
        std::size_t drawOrder = 0;
        const UiCanvasFit fit = MakeFit(*document, viewport);
        auto evaluate = [&](const UiElement& element, Math::Rect design, Math::Rect clip,
                            ListPosition position) -> UiResult<void> {
            result.push_back({
                element.id,
                element.type,
                ToPixels(design, fit),
                ToPixels(clip, fit),
                drawOrder++,
                element.type == UiElementType::Button ||
                    element.type == UiElementType::HorizontalSlider ||
                    element.type == UiElementType::TextField,
                position.index,
            });
            return {};
        };
        auto evaluated = WalkElements(canvas->children, root, root, {}, *document, bindings, evaluate);
        if (!evaluated) {
            return Base::Err(std::move(evaluated).error());
        }
        return std::move(result);
    }

    [[nodiscard]] UiResult<UiDrawList> ComposeInternal(
        const UiBindingTable& bindings,
        UiViewport viewport) const {
        // Composing before ActivateCanvas is API misuse.
        GYO_ASSERT(document && canvas != nullptr);
        if (!IsValidViewport(viewport)) {
            return Base::Err(RuntimeError(UiErrorCode::RuntimeState, "UI viewport must be finite and positive"));
        }
        const auto backdrop = document->colors.find(canvas->backdropColor);
        if (backdrop == document->colors.end()) {
            return Base::Err(RuntimeError(UiErrorCode::MissingReference, "active canvas backdrop color is missing"));
        }
        UiDrawList result;
        const Math::Rect viewportRect{0.0F, 0.0F, viewport.width, viewport.height};
        result.commands.emplace_back(UiQuadDraw{viewportRect, backdrop->second, viewportRect});
        ComposeContext context{
            *document, bindings, interaction, pressedElement, MakeFit(*document, viewport), result,
            edit ? &*edit : nullptr};
        const Math::Rect root{0.0F, 0.0F, document->designCanvas.size.x, document->designCanvas.size.y};
        auto compose = [&context](const UiElement& element, Math::Rect design, Math::Rect clip,
                                  ListPosition position) {
            return ComposeElement(element, design, clip, position.item, context);
        };
        auto composed = WalkElements(canvas->children, root, root, {}, *document, bindings, compose);
        if (!composed) return Base::Err(std::move(composed).error());
        return std::move(result);
    }
};

UiRuntime::UiRuntime() : impl_(std::make_unique<Impl>()) {}
UiRuntime::~UiRuntime() = default;
UiRuntime::UiRuntime(UiRuntime&&) noexcept = default;
UiRuntime& UiRuntime::operator=(UiRuntime&&) noexcept = default;

UiResult<void> UiRuntime::Initialize(std::shared_ptr<const UiDocument> document) {
    GYO_ASSERT(document != nullptr);
    auto validation = UiDocumentCodec::Validate(*document);
    if (!validation) return validation;
    impl_->document = std::move(document);
    impl_->canvas = nullptr;
    impl_->interaction = {};
    impl_->edit.reset();
    impl_->pressedElement.clear();
    impl_->pointerWasAvailable = false;
    impl_->lastPointerPixels = {};
    return {};
}

void UiRuntime::Reset() noexcept {
    impl_->document.reset();
    impl_->canvas = nullptr;
    impl_->interaction = {};
    impl_->edit.reset();
    impl_->pressedElement.clear();
    impl_->pointerWasAvailable = false;
    impl_->lastPointerPixels = {};
}

UiResult<void> UiRuntime::ActivateCanvas(std::string_view canvasId) {
    // Activating a canvas before Initialize is API misuse.
    GYO_ASSERT(impl_->document != nullptr);
    const auto found = std::find_if(
        impl_->document->canvases.begin(),
        impl_->document->canvases.end(),
        [canvasId](const UiCanvas& candidate) { return candidate.id == canvasId; });
    if (found == impl_->document->canvases.end()) {
        return Base::Err(RuntimeError(UiErrorCode::MissingReference, "unknown canvas '" + std::string(canvasId) + "'"));
    }
    impl_->canvas = &*found;
    impl_->interaction.activeCanvas = found->id;
    impl_->interaction.capturedElement.clear();
    impl_->EndEdit();
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
    return {};
}

void UiRuntime::SetTextFilter(UiTextFilter filter) {
    impl_->textFilter = std::move(filter);
}

UiResult<std::vector<UiActionEvent>> UiRuntime::Update(
    const UiInputFrame& input,
    const UiBindingTable& bindings,
    UiViewport viewport) {
    // Updating before ActivateCanvas is API misuse.
    GYO_ASSERT(impl_->document && impl_->canvas != nullptr);
    if (!IsValidViewport(viewport)) {
        return Base::Err(RuntimeError(UiErrorCode::RuntimeState, "UI viewport must be finite and positive"));
    }
    impl_->pressedElement.clear();
    std::vector<UiActionEvent> events;
    const bool pointerMoved = input.pointerAvailable &&
        impl_->pointerWasAvailable &&
        (input.pointerPixels.x != impl_->lastPointerPixels.x ||
         input.pointerPixels.y != impl_->lastPointerPixels.y);

    // The text field being edited (Design Invariant 4): a change of its binding
    // discards the edit; cancel discards it; the text inputs apply in order;
    // activate submits it as the field's string action.
    // An activate that arrives with a discarded edit does not start another.
    bool editDiscarded = false;
    const UiElement* editing = impl_->edit ? FindElement(impl_->canvas->children, impl_->edit->element) : nullptr;
    if (impl_->edit && editing == nullptr) {
        impl_->EndEdit();
        editDiscarded = true;
    }
    if (editing != nullptr) {
        auto current = FieldValue(*impl_->document, bindings, *editing);
        if (!current) return Base::Err(std::move(current).error());
        if (current.value() != impl_->edit->startValue) {
            impl_->EndEdit();
            editing = nullptr;
            editDiscarded = true;
        }
    }
    if (editing != nullptr) {
        if (input.cancelPressed) {
            impl_->EndEdit();
            impl_->TrackPointer(input);
            return std::move(events);
        }
        for (const UiTextInput& text : input.text) {
            ApplyTextInput(*impl_->edit, text, *editing, impl_->textFilter);
        }
        if (input.activatePressed) {
            impl_->pressedElement = editing->id;
            events.push_back({editing->action, editing->id, std::move(impl_->edit->text)});
            impl_->EndEdit();
            impl_->TrackPointer(input);
            return std::move(events);
        }
    }

    if (input.cancelPressed) {
        impl_->interaction.capturedElement.clear();
        if (impl_->canvas->cancelAction) {
            events.push_back({*impl_->canvas->cancelAction, {}, std::monostate{}});
        }
        impl_->TrackPointer(input);
        return std::move(events);
    }

    auto evaluated = impl_->EvaluateInternal(bindings, viewport);
    if (!evaluated) {
        return Base::Err(std::move(evaluated).error());
    }
    const std::vector<UiEvaluatedElement>& layouts = evaluated.value();
    const UiEvaluatedElement* hit = input.pointerAvailable
        ? HitTestUiLayout(layouts, input.pointerPixels, true)
        : nullptr;
    // While a field is being edited only a press moves the focus, so moving
    // the pointer across the canvas does not end the edit.
    if (hit != nullptr && (input.pointerPrimaryPressed || (pointerMoved && !impl_->edit))) {
        impl_->interaction.focusedElement = hit->id;
    }
    if (input.pointerPrimaryPressed && impl_->edit && (hit == nullptr || hit->id != impl_->edit->element)) {
        impl_->EndEdit();
    }

    bool pointerSliderProcessed = false;
    if (input.pointerPrimaryPressed && hit != nullptr) {
        const UiElement* hitElement = FindElement(impl_->canvas->children, hit->id);
        // Hits come from this document's own evaluation.
        GYO_ASSERT(hitElement != nullptr);
        impl_->pressedElement = hitElement->id;
        if (hitElement->type == UiElementType::Button) {
            events.push_back({hitElement->action, hitElement->id, std::monostate{}});
        } else if (hitElement->type == UiElementType::TextField) {
            if (!impl_->edit) {
                auto begun = impl_->BeginEdit(*hitElement, bindings);
                if (!begun) return Base::Err(std::move(begun).error());
            }
        } else {
            impl_->interaction.capturedElement = hitElement->id;
            auto event = SliderPointerEvent(
                *impl_->document,
                bindings,
                *hitElement,
                hit->boundsPixels,
                input.pointerPixels.x);
            if (!event) return Base::Err(std::move(event).error());
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
            // Captures come from this document's own evaluation.
            GYO_ASSERT(capturedElement != nullptr);
            auto event = SliderPointerEvent(
                *impl_->document,
                bindings,
                *capturedElement,
                captured->boundsPixels,
                input.pointerPixels.x);
            if (!event) return Base::Err(std::move(event).error());
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
    if (impl_->edit && impl_->interaction.focusedElement != impl_->edit->element) {
        impl_->EndEdit();
    }

    const UiElement* focused = FindElement(impl_->canvas->children, impl_->interaction.focusedElement);
    if (focused != nullptr && focused->type == UiElementType::HorizontalSlider &&
        input.adjustPreviousPressed != input.adjustNextPressed) {
        auto current = SliderValue(*impl_->document, bindings, *focused);
        if (!current) return Base::Err(std::move(current).error());
        auto event = SliderEvent(
            *impl_->document,
            bindings,
            *focused,
            current.value() + (input.adjustNextPressed ? focused->step : -focused->step));
        if (!event) return Base::Err(std::move(event).error());
        if (event.value()) events.push_back(std::move(*event.value()));
    }
    if (input.activatePressed && focused != nullptr && focused->type == UiElementType::Button) {
        impl_->pressedElement = focused->id;
        events.push_back({focused->action, focused->id, std::monostate{}});
    }
    if (input.activatePressed && focused != nullptr && focused->type == UiElementType::TextField && !impl_->edit &&
        !editDiscarded) {
        auto begun = impl_->BeginEdit(*focused, bindings);
        if (!begun) return Base::Err(std::move(begun).error());
    }
    impl_->TrackPointer(input);
    return std::move(events);
}

UiResult<UiDrawList> UiRuntime::Compose(
    const UiBindingTable& bindings,
    UiViewport viewport) const {
    return impl_->ComposeInternal(bindings, viewport);
}

UiResult<UiDrawList> UiRuntime::ComposePreview(UiViewport viewport) const {
    // Previewing before Initialize is API misuse.
    GYO_ASSERT(impl_->document != nullptr);
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
    // Previewing before Initialize is API misuse.
    GYO_ASSERT(impl_->document != nullptr);
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
