#include "gyo/ui_editor/ElementSchema.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

using Json = nlohmann::json;

constexpr ElementColorField kPanelColors[]{
    {"Panel", "color", true},
};
constexpr ElementColorField kImageColors[]{
    {"Image tint", "tint", true},
};
constexpr ElementColorField kSliderColors[]{
    {"Track", "track_color", false},
    {"Fill", "fill_color", false},
    {"Thumb", "thumb_color", false},
};

constexpr ElementKind kKinds[]{
    {.type = "container"},
    {.type = "panel", .colors = kPanelColors},
    {.type = "image", .colors = kImageColors, .textureAsset = true},
    {.type = "text"},
    {.type = "button", .focusable = true, .actionPayload = "none"},
    {.type = "horizontal_slider", .focusable = true, .actionPayload = "number", .bindingType = "number",
     .colors = kSliderColors},
    {.type = "fixed_step_list", .bindingType = "list<object>"},
};

[[nodiscard]] bool DocumentContainsNodeId(
    const Json& document,
    const std::string_view id) {
    if (!document.contains("canvases") || !document["canvases"].is_array()) {
        return false;
    }
    for (const Json& canvas : document["canvases"]) {
        if (!canvas.contains("children") || !canvas["children"].is_array()) continue;
        for (const Json& child : canvas["children"]) {
            if (ContainsNodeId(child, id)) return true;
        }
    }
    return false;
}

[[nodiscard]] Json DefaultRect(const float width = 240.0F, const float height = 72.0F) {
    return {
        {"anchor_min", {0.5, 0.5}},
        {"anchor_max", {0.5, 0.5}},
        {"pivot", {0.5, 0.5}},
        {"position", {0.0, 0.0}},
        {"size_delta", {width, height}},
    };
}

[[nodiscard]] Json TextVisual(const std::string_view literal) {
    return {
        {"text", {{"literal", literal}}},
        {"point_size", 24.0},
        {"text_color", {{"color", "white"}}},
        {"horizontal_align", "center"},
        {"vertical_align", "center"},
    };
}
} // namespace

std::span<const ElementKind> ElementKinds() noexcept {
    return kKinds;
}

const ElementKind* FindElementKind(const std::string_view type) noexcept {
    const auto found = std::ranges::find(kKinds, type, &ElementKind::type);
    return found == std::ranges::end(kKinds) ? nullptr : &*found;
}

bool ContainsNodeId(const Json& node, const std::string_view id) {
    if (!node.is_object()) return false;
    if (node.value("id", std::string{}) == id) return true;
    for (const char* collection : {"children", "template"}) {
        if (node.contains(collection) && node[collection].is_array()) {
            for (const Json& child : node[collection]) {
                if (ContainsNodeId(child, id)) return true;
            }
        }
    }
    return false;
}

std::string UniqueNodeId(
    const Json& document,
    const std::string_view prefix) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{prefix};
        candidate += "_" + std::to_string(suffix);
        if (!DocumentContainsNodeId(document, candidate)) return candidate;
    }
}

std::string UniqueDeclarationId(
    const Json& declarations,
    const std::string_view prefix) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{prefix};
        candidate += "." + std::to_string(suffix);
        const bool exists = std::ranges::any_of(
            declarations,
            [&](const Json& item) {
                return item.value("id", std::string{}) == candidate;
            });
        if (!exists) return candidate;
    }
}

Json MakeDefaultElement(Json& document, const ElementKind& kind) {
    const std::string_view type = kind.type;
    Json element{
        {"type", type},
        {"id", UniqueNodeId(document, type)},
        {"rect", DefaultRect()},
    };
    if (type == "panel") {
        element["color"] = {{"color", "panel"}};
    } else if (type == "image") {
        element["texture_asset"] = "texture.unresolved";
        element["source_uv"] = {0.0, 0.0, 1.0, 1.0};
        element["tint"] = {{"color", "white"}};
    } else if (type == "text") {
        element.update(TextVisual("Text"));
    } else if (type == "button") {
        element.update(TextVisual("Button"));
        Json& actions = document["actions"];
        const std::string action = UniqueDeclarationId(actions, "action.button");
        actions.push_back({{"id", action}, {"payload", "none"}});
        element["action"] = action;
        element["background"] = {
            {"normal", "button"},
            {"focused", "button_focused"},
            {"pressed", "button_pressed"},
        };
    } else if (type == "horizontal_slider") {
        element.update(TextVisual("Value"));
        Json& actions = document["actions"];
        Json& bindings = document["bindings"];
        const std::string action = UniqueDeclarationId(actions, "action.slider");
        const std::string binding = UniqueDeclarationId(bindings, "binding.number");
        actions.push_back({{"id", action}, {"payload", "number"}});
        bindings.push_back(
            {{"id", binding}, {"type", "number"}, {"preview", 0.5}});
        element["action"] = action;
        element["binding"] = binding;
        element["background"] = {
            {"normal", "button"},
            {"focused", "button_focused"},
            {"pressed", "button_pressed"},
        };
        element["minimum"] = 0.0;
        element["maximum"] = 1.0;
        element["step"] = 0.1;
        element["value_format"] = {{"decimals", 1}, {"show_plus", false}};
        element["track_color"] = "track";
        element["fill_color"] = "accent";
        element["thumb_color"] = "white";
    } else if (type == "fixed_step_list") {
        Json& bindings = document["bindings"];
        const std::string binding = UniqueDeclarationId(bindings, "binding.list");
        bindings.push_back({
            {"id", binding},
            {"type", "list<object>"},
            {"item_fields", {{"label", "string"}}},
            {"preview", Json::array({{{"label", "Item"}}})},
        });
        element["binding"] = binding;
        element["max_items"] = 8;
        element["item_step"] = {0.0, 36.0};
        Json item{
            {"type", "text"},
            {"id", UniqueNodeId(document, "list_item")},
            {"rect", DefaultRect(220.0F, 32.0F)},
            {"text", {{"item_field", "label"},
                      {"format", {{"decimals", 0}, {"show_plus", false}}}}},
            {"point_size", 20.0},
            {"text_color", {{"color", "white"}}},
            {"horizontal_align", "left"},
            {"vertical_align", "center"},
        };
        element["template"] = Json::array({std::move(item)});
    }
    return element;
}
} // namespace Gyo::Tools::UiEditor
