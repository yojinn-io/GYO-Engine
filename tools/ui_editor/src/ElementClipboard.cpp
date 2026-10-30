#include "gyo/ui_editor/ElementClipboard.hpp"

#include "EditorDocumentEdits.hpp"

#include "gyo/ui_editor/ElementSchema.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;

// Focusable elements under node in document order; list templates are not
// focus targets, so only children are visited.
void CollectFocusableIds(const Json& node, Json& focusOrder) {
    if (!node.is_object()) return;
    const ElementKind* kind = FindElementKind(node.value("type", std::string{}));
    if (kind != nullptr && kind->focusable && node.contains("id") && node["id"].is_string()) {
        focusOrder.push_back(node["id"]);
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const Json& child : node["children"]) CollectFocusableIds(child, focusOrder);
    }
}

} // namespace

std::optional<Json> CopyElement(const Json& document, const std::string_view id) {
    // The lookup only reads; FindSelectedNode is written for mutable documents.
    const Json* found = Detail::FindSelectedNode(const_cast<Json&>(document), id);
    if (found == nullptr) return std::nullopt;
    return *found;
}

std::optional<std::string> PasteElement(
    Json& document, const std::string_view canvasId, const std::string_view after, Json element) {
    Json* canvas = Detail::FindCanvas(document, canvasId);
    if (canvas == nullptr || !element.is_object() || !element.contains("type")) return std::nullopt;

    std::unordered_set<std::string> used;
    for (const Json& each : document["canvases"]) {
        if (each.contains("children")) Detail::CollectAllNodeIds(each["children"], used);
    }
    std::unordered_map<std::string, std::string> remap;
    Detail::RegenerateNodeIds(element, used, remap);
    std::string id = element.value("id", std::string{});
    CollectFocusableIds(element, (*canvas)["focus_order"]);

    Json& root = (*canvas)["children"];
    Json* siblings = after.empty() ? nullptr : Detail::FindParentChildren(root, after);
    if (siblings == nullptr) {
        root.push_back(std::move(element));
        return id;
    }
    const auto position = std::ranges::find_if(*siblings, [after](const Json& sibling) {
        return sibling.value("id", std::string{}) == after;
    });
    siblings->insert(position + 1, std::move(element));
    return id;
}

} // namespace Gyo::Tools::UiEditor
