#include "EditorDocumentEdits.hpp"

#include "engine/os/Files.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

namespace {

[[nodiscard]] std::string UniqueNodeCopyId(
    std::unordered_set<std::string>& used,
    const std::string_view original) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{original};
        candidate += "_copy_" + std::to_string(suffix);
        if (used.emplace(candidate).second) return candidate;
    }
}

} // namespace

void CollectAllNodeIds(const Json& value, std::unordered_set<std::string>& ids) {
    if (value.is_object()) {
        if (value.contains("type") && value.contains("id") &&
            value["id"].is_string()) {
            ids.emplace(value["id"].get<std::string>());
        }
        for (const char* collection : {"children", "template"}) {
            if (value.contains(collection) && value[collection].is_array()) {
                for (const Json& child : value[collection]) {
                    CollectAllNodeIds(child, ids);
                }
            }
        }
    } else if (value.is_array()) {
        for (const Json& item : value) CollectAllNodeIds(item, ids);
    }
}

void RegenerateNodeIds(
    Json& node,
    std::unordered_set<std::string>& used,
    std::unordered_map<std::string, std::string>& remap) {
    if (!node.is_object()) return;
    if (node.contains("type") && node.contains("id") && node["id"].is_string()) {
        const std::string oldId = node["id"].get<std::string>();
        const std::string newId = UniqueNodeCopyId(used, oldId);
        remap.emplace(oldId, newId);
        node["id"] = newId;
    }
    for (const char* collection : {"children", "template"}) {
        if (node.contains(collection) && node[collection].is_array()) {
            for (Json& child : node[collection]) {
                RegenerateNodeIds(child, used, remap);
            }
        }
    }
}

Json* FindParentChildren(Json& children, const std::string_view id) {
    if (!children.is_array()) return nullptr;
    for (Json& child : children) {
        if (child.value("id", std::string{}) == id) return &children;
        if (child.contains("children")) {
            if (Json* found = FindParentChildren(child["children"], id)) return found;
        }
    }
    return nullptr;
}

} // namespace Gyo::Tools::UiEditor::Detail
