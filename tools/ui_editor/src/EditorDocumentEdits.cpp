#include "EditorDocumentEdits.hpp"

#include "engine/os/Files.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor::Detail {

// The path fields hold UTF-8 (ImGui's text), never the ANSI code page, with the
// separators the path holds. A path with no UTF-8 form leaves the field empty
// rather than naming another file.
void CopyPath(std::array<char, 1024>& destination, const std::filesystem::path& path) {
    std::string value;
    if (Engine::Os::Files::PathToUtf8(path)) {
        const std::u8string text = path.u8string();  // cannot throw: the path has a UTF-8 form
        value.assign(text.begin(), text.end());
    }
    std::snprintf(destination.data(), destination.size(), "%s", value.c_str());
}

// A path field read back; nullopt when the text is not UTF-8 (a field cut
// inside a character, for one).
[[nodiscard]] std::optional<std::filesystem::path> FieldPath(const char* text) {
    auto path = Engine::Os::Files::PathFromUtf8(text);
    if (!path) return std::nullopt;
    return std::move(*path);
}

Json* FindNode(Json& node, const std::string_view id) {
    if (!node.is_object()) {
        return nullptr;
    }
    if (node.value("id", std::string{}) == id) {
        return &node;
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (Json& child : node["children"]) {
            if (Json* found = FindNode(child, id); found != nullptr) {
                return found;
            }
        }
    }
    return nullptr;
}

Json* FindSelectedNode(Json& document, const std::string_view id) {
    if (id.empty() || !document.contains("canvases") ||
        !document["canvases"].is_array()) {
        return nullptr;
    }
    for (Json& canvas : document["canvases"]) {
        if (canvas.contains("children") && canvas["children"].is_array()) {
            for (Json& child : canvas["children"]) {
                if (Json* found = FindNode(child, id); found != nullptr) {
                    return found;
                }
            }
        }
    }
    return nullptr;
}

Json* FindCanvas(Json& document, const std::string_view id) {
    if (!document.contains("canvases") || !document["canvases"].is_array()) {
        return nullptr;
    }
    for (Json& canvas : document["canvases"]) {
        if (canvas.is_object() && canvas.value("id", std::string{}) == id) {
            return &canvas;
        }
    }
    return nullptr;
}


void RenameReferences(
    Json& value,
    const std::string_view oldId,
    const std::string_view newId,
    const std::initializer_list<std::string_view> keys) {
    if (value.is_object()) {
        for (auto member = value.begin(); member != value.end(); ++member) {
            if (member.value().is_string() &&
                std::ranges::find(keys, std::string_view{member.key()}) != keys.end() &&
                member.value().get_ref<const std::string&>() == oldId) {
                member.value() = newId;
            } else {
                RenameReferences(member.value(), oldId, newId, keys);
            }
        }
    } else if (value.is_array()) {
        for (Json& item : value) RenameReferences(item, oldId, newId, keys);
    }
}

} // namespace Gyo::Tools::UiEditor::Detail
