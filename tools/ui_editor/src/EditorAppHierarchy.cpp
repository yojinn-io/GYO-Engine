#include "gyo/ui_editor/EditorApp.hpp"

#include "EditorDocumentEdits.hpp"

#include "engine/math/scalar/Scalar.hpp"
#include "engine/os/Files.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// EditorApp: the Hierarchy panel (canvases and the element tree).

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;
using Detail::FindNode;
using Detail::FindSelectedNode;
using Detail::FindCanvas;
using Detail::UniqueDeclarationId;

[[nodiscard]] bool ContainsNodeId(const Json& node, const std::string_view id) {
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

[[nodiscard]] std::string UniqueId(
    const Json& document,
    const std::string_view prefix) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{prefix};
        candidate += "_" + std::to_string(suffix);
        if (!DocumentContainsNodeId(document, candidate)) return candidate;
    }
}

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

[[nodiscard]] std::string UniqueNodeCopyId(
    std::unordered_set<std::string>& used,
    const std::string_view original) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{original};
        candidate += "_copy_" + std::to_string(suffix);
        if (used.emplace(candidate).second) return candidate;
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

[[nodiscard]] Json MakeElement(Json& document, const std::string_view type) {
    Json element{
        {"type", type},
        {"id", UniqueId(document, type)},
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
            {"id", UniqueId(document, "list_item")},
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

void CollectNodeIds(const Json& node, std::vector<std::string>& ids) {
    if (!node.is_object()) return;
    if (node.contains("id") && node["id"].is_string()) {
        ids.push_back(node["id"].get<std::string>());
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const Json& child : node["children"]) CollectNodeIds(child, ids);
    }
}

[[nodiscard]] bool RemoveNode(Json& children, const std::string_view id) {
    if (!children.is_array()) return false;
    for (auto item = children.begin(); item != children.end(); ++item) {
        if (item->value("id", std::string{}) == id) {
            children.erase(item);
            return true;
        }
        if (item->contains("children") &&
            RemoveNode((*item)["children"], id)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool TakeNode(
    Json& children,
    const std::string_view id,
    Json& result) {
    if (!children.is_array()) return false;
    for (auto item = children.begin(); item != children.end(); ++item) {
        if (item->value("id", std::string{}) == id) {
            result = std::move(*item);
            children.erase(item);
            return true;
        }
        if (item->contains("children") &&
            TakeNode((*item)["children"], id, result)) {
            return true;
        }
    }
    return false;
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

void DrawHierarchyNode(
    const Json& node,
    std::string& selection,
    std::string& reparentSource,
    std::string& reparentTarget) {
    if (!node.is_object()) {
        return;
    }
    const std::string id = node.value("id", std::string{"<unnamed>"});
    const std::string type = node.value("type", std::string{"unknown"});
    const bool hasChildren = node.contains("children") && node["children"].is_array() &&
                             !node["children"].empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (selection == id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const std::string label = id + "  [" + type + "]";
    const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        selection = id;
    }
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(
            "GYO_UI_HIERARCHY_NODE",
            id.c_str(),
            id.size() + 1U);
        ImGui::Text("Move %s", id.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("GYO_UI_HIERARCHY_NODE");
            payload != nullptr && payload->Data != nullptr && payload->DataSize > 1) {
            reparentSource = static_cast<const char*>(payload->Data);
            reparentTarget = id;
        }
        ImGui::EndDragDropTarget();
    }
    if (open) {
        if (hasChildren) {
            for (const Json& child : node["children"]) {
                DrawHierarchyNode(
                    child,
                    selection,
                    reparentSource,
                    reparentTarget);
            }
        }
        ImGui::TreePop();
    }
}

} // namespace

void EditorApp::DrawHierarchy() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Hierarchy", nullptr, flags)) {
        ImGui::End();
        return;
    }
    Json& document = session_.EditDocument();
    std::string reparentSource;
    std::string reparentTarget;
    if (document.contains("canvases") && document["canvases"].is_array()) {
        if (ImGui::BeginCombo("Canvas", selectedCanvas_.c_str())) {
            for (const Json& canvas : document["canvases"]) {
                const std::string id = canvas.value("id", std::string{"<unnamed>"});
                if (ImGui::Selectable(id.c_str(), id == selectedCanvas_)) {
                    selectedCanvas_ = id;
                    selectedNode_.clear();
                }
            }
            ImGui::EndCombo();
        }
        Json& canvases = document["canvases"];
        if (ImGui::Button("+ Canvas")) {
            const std::string id = UniqueDeclarationId(canvases, "canvas");
            canvases.push_back({
                {"id", id},
                {"backdrop_color", "transparent"},
                {"focus_order", Json::array()},
                {"children", Json::array()},
            });
            selectedCanvas_ = id;
            selectedNode_.clear();
            session_.CommitEdit("Add canvas");
            RefreshDiagnostics();
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            const auto source = std::ranges::find_if(
                canvases,
                [this](const Json& canvas) {
                    return canvas.value("id", std::string{}) == selectedCanvas_;
                });
            if (source != canvases.end()) {
                Json duplicate = *source;
                const std::string id = UniqueDeclarationId(canvases, "canvas");
                duplicate["id"] = id;
                std::unordered_set<std::string> used;
                for (const Json& canvas : canvases) {
                    if (canvas.contains("children")) {
                        CollectAllNodeIds(canvas["children"], used);
                    }
                }
                std::unordered_map<std::string, std::string> remap;
                for (Json& child : duplicate["children"]) {
                    RegenerateNodeIds(child, used, remap);
                }
                for (Json& focus : duplicate["focus_order"]) {
                    if (focus.is_string()) {
                        if (const auto mapped = remap.find(focus.get<std::string>());
                            mapped != remap.end()) {
                            focus = mapped->second;
                        }
                    }
                }
                if (duplicate.contains("default_focus") &&
                    duplicate["default_focus"].is_string()) {
                    if (const auto mapped = remap.find(
                            duplicate["default_focus"].get<std::string>());
                        mapped != remap.end()) {
                        duplicate["default_focus"] = mapped->second;
                    }
                }
                canvases.push_back(std::move(duplicate));
                selectedCanvas_ = id;
                selectedNode_.clear();
                session_.CommitEdit("Duplicate canvas");
                RefreshDiagnostics();
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(canvases.size() <= 1U);
        if (ImGui::Button("Delete canvas")) {
            const auto selected = std::ranges::find_if(
                canvases,
                [this](const Json& canvas) {
                    return canvas.value("id", std::string{}) == selectedCanvas_;
                });
            if (selected != canvases.end()) {
                canvases.erase(selected);
                selectedCanvas_ = canvases.front().value(
                    "id", std::string{"main"});
                selectedNode_.clear();
                session_.CommitEdit("Delete canvas");
                RefreshDiagnostics();
            }
        }
        ImGui::EndDisabled();
        for (const Json& canvas : document["canvases"]) {
            if (canvas.value("id", std::string{}) == selectedCanvas_ &&
                canvas.contains("children") && canvas["children"].is_array()) {
                if (ImGui::Selectable(
                        "Canvas root (drop here)", selectedNode_.empty())) {
                    selectedNode_.clear();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                            "GYO_UI_HIERARCHY_NODE");
                        payload != nullptr && payload->Data != nullptr &&
                        payload->DataSize > 1) {
                        reparentSource = static_cast<const char*>(payload->Data);
                        reparentTarget.clear();
                    }
                    ImGui::EndDragDropTarget();
                }
                for (const Json& child : canvas["children"]) {
                    DrawHierarchyNode(
                        child,
                        selectedNode_,
                        reparentSource,
                        reparentTarget);
                }
                break;
            }
        }
    }
    if (!reparentSource.empty()) {
        Json* canvas = FindCanvas(document, selectedCanvas_);
        Json* source = FindSelectedNode(document, reparentSource);
        if (canvas == nullptr || source == nullptr) {
            statusMessage_ = "Cannot reparent: source is no longer present";
        } else if (reparentSource == reparentTarget ||
                   (!reparentTarget.empty() &&
                    ContainsNodeId(*source, reparentTarget))) {
            statusMessage_ = "Cannot reparent an element into itself or its descendant";
        } else {
            Json children = (*canvas)["children"];
            Json moved;
            if (!TakeNode(children, reparentSource, moved)) {
                statusMessage_ = "Cannot reparent: source is outside the active canvas";
            } else if (reparentTarget.empty()) {
                children.push_back(std::move(moved));
                (*canvas)["children"] = std::move(children);
                selectedNode_ = reparentSource;
                session_.CommitEdit("Reparent element to canvas root");
                RefreshDiagnostics();
            } else if (Json* target = FindNode(children, reparentTarget);
                       target != nullptr) {
                if (!target->contains("children") ||
                    !(*target)["children"].is_array()) {
                    (*target)["children"] = Json::array();
                }
                (*target)["children"].push_back(std::move(moved));
                (*canvas)["children"] = std::move(children);
                selectedNode_ = reparentSource;
                session_.CommitEdit("Reparent element");
                RefreshDiagnostics();
            } else {
                statusMessage_ = "Cannot reparent: target is no longer present";
            }
        }
    }
    ImGui::Separator();
    if (ImGui::Button("+ Add element")) {
        ImGui::OpenPopup("add-element");
    }
    if (ImGui::BeginPopup("add-element")) {
        for (const std::string_view type : {
                 "container", "panel", "image", "text", "button",
                 "horizontal_slider", "fixed_step_list"}) {
            if (!ImGui::MenuItem(type.data())) continue;
            Json* canvas = FindCanvas(document, selectedCanvas_);
            if (canvas == nullptr) break;
            Json element = MakeElement(document, type);
            const std::string id = element.value("id", std::string{});
            if (type == "button" || type == "horizontal_slider") {
                (*canvas)["focus_order"].push_back(id);
            }
            Json* destination = &(*canvas)["children"];
            if (Json* selected = FindSelectedNode(document, selectedNode_);
                selected != nullptr) {
                if (!selected->contains("children")) {
                    (*selected)["children"] = Json::array();
                }
                destination = &(*selected)["children"];
            }
            destination->push_back(std::move(element));
            selectedNode_ = id;
            session_.CommitEdit("Add " + std::string{type});
            RefreshDiagnostics();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(selectedNode_.empty());
    if (ImGui::Button("Delete")) {
        Json* canvas = FindCanvas(document, selectedCanvas_);
        Json* selected = FindSelectedNode(document, selectedNode_);
        if (canvas != nullptr && selected != nullptr) {
            std::vector<std::string> removedIds;
            CollectNodeIds(*selected, removedIds);
            if (RemoveNode((*canvas)["children"], selectedNode_)) {
                Json& focusOrder = (*canvas)["focus_order"];
                focusOrder.erase(
                    std::remove_if(
                        focusOrder.begin(),
                        focusOrder.end(),
                        [&](const Json& id) {
                            return id.is_string() &&
                                   std::ranges::find(
                                       removedIds, id.get<std::string>()) !=
                                       removedIds.end();
                        }),
                    focusOrder.end());
                if ((*canvas).contains("default_focus") &&
                    (*canvas)["default_focus"].is_string() &&
                    std::ranges::find(
                        removedIds,
                        (*canvas)["default_focus"].get<std::string>()) !=
                        removedIds.end()) {
                    (*canvas).erase("default_focus");
                }
                selectedNode_.clear();
                session_.CommitEdit("Delete element");
                RefreshDiagnostics();
            }
        }
    }
    ImGui::SameLine();
    const bool moveUp = ImGui::Button("Up");
    ImGui::SameLine();
    const bool moveDown = ImGui::Button("Down");
    if (moveUp || moveDown) {
        Json* canvas = FindCanvas(document, selectedCanvas_);
        if (canvas != nullptr) {
            Json* siblings = FindParentChildren((*canvas)["children"], selectedNode_);
            if (siblings != nullptr) {
                for (std::size_t index = 0; index < siblings->size(); ++index) {
                    if ((*siblings)[index].value("id", std::string{}) != selectedNode_) {
                        continue;
                    }
                    const std::size_t target = moveDown
                        ? Engine::Math::Min(index + 1U, siblings->size() - 1U)
                        : (index > 0U ? index - 1U : 0U);
                    if (target != index) {
                        std::swap((*siblings)[index], (*siblings)[target]);
                        session_.CommitEdit("Reorder element");
                        RefreshDiagnostics();
                    }
                    break;
                }
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}

} // namespace Gyo::Tools::UiEditor
