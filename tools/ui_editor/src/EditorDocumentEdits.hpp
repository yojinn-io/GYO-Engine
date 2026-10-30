#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Document and path helpers shared by the editor's sources (the panel files
// and core operations such as ElementClipboard). Not part of the public
// include directory.
namespace Gyo::Tools::UiEditor::Detail {

using Json = nlohmann::json;

void CopyPath(std::array<char, 1024>& destination, const std::filesystem::path& path);
[[nodiscard]] std::optional<std::filesystem::path> FieldPath(const char* text);
Json* FindNode(Json& node, std::string_view id);
Json* FindSelectedNode(Json& document, std::string_view id);
Json* FindCanvas(Json& document, std::string_view id);
void RenameReferences(Json& value, std::string_view oldId, std::string_view newId,
    std::initializer_list<std::string_view> keys);
// The array that holds element id, searching children recursively.
Json* FindParentChildren(Json& children, std::string_view id);
// Every element id in value (an element or an array), children and list templates included.
void CollectAllNodeIds(const Json& value, std::unordered_set<std::string>& ids);
// Renames every element id under node to "<id>_copy_<n>", unused in used, and
// records each new id in used and old -> new in remap.
void RegenerateNodeIds(Json& node, std::unordered_set<std::string>& used,
    std::unordered_map<std::string, std::string>& remap);

} // namespace Gyo::Tools::UiEditor::Detail
