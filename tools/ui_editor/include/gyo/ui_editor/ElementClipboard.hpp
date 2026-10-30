#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace Gyo::Tools::UiEditor {

// Copy and paste of an element subtree within gyo.ui documents. Pure JSON
// operations; the editor keeps the copied element and records undo.

// A copy of element id with its children; nullopt when no canvas has it.
[[nodiscard]] std::optional<nlohmann::json> CopyElement(const nlohmann::json& document, std::string_view id);

// Inserts element into canvas canvasId, right after the element after (as its
// sibling), or at the end of the canvas root when after is empty or not in
// that canvas. Every element id in the copy is renamed to one the document
// does not use ("<id>_copy_<n>"), and each focusable element joins the end of
// the canvas focus order. Returns the pasted element's new id; nullopt when
// the canvas does not exist or element is not an element.
[[nodiscard]] std::optional<std::string> PasteElement(
    nlohmann::json& document, std::string_view canvasId, std::string_view after, nlohmann::json element);

} // namespace Gyo::Tools::UiEditor
