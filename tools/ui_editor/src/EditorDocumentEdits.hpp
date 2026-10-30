#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

// Document and path helpers shared by EditorApp's panel files. Internal to
// the editor's sources; not part of the public include directory.
namespace Gyo::Tools::UiEditor::Detail {

using Json = nlohmann::json;

void CopyPath(std::array<char, 1024>& destination, const std::filesystem::path& path);
[[nodiscard]] std::optional<std::filesystem::path> FieldPath(const char* text);
Json* FindNode(Json& node, std::string_view id);
Json* FindSelectedNode(Json& document, std::string_view id);
Json* FindCanvas(Json& document, std::string_view id);
[[nodiscard]] std::string UniqueDeclarationId(const Json& declarations, std::string_view prefix);
void RenameReferences(Json& value, std::string_view oldId, std::string_view newId,
    std::initializer_list<std::string_view> keys);

} // namespace Gyo::Tools::UiEditor::Detail
