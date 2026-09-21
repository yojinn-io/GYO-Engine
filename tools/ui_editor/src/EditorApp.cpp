#include "gyo/ui_editor/EditorApp.hpp"

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

namespace Gyo::Tools::UiEditor {
namespace {

using Json = nlohmann::json;

void CopyPath(std::array<char, 1024>& destination, const std::filesystem::path& path) {
    const std::string value = path.string();
    std::snprintf(destination.data(), destination.size(), "%s", value.c_str());
}

[[nodiscard]] const char* SeverityLabel(const DiagnosticSeverity severity) noexcept {
    switch (severity) {
    case DiagnosticSeverity::Info:
        return "info";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Error:
        return "error";
    }
    return "unknown";
}

[[nodiscard]] ImVec4 SeverityColor(const DiagnosticSeverity severity) noexcept {
    switch (severity) {
    case DiagnosticSeverity::Info:
        return {0.55F, 0.72F, 0.92F, 1.0F};
    case DiagnosticSeverity::Warning:
        return {0.96F, 0.72F, 0.25F, 1.0F};
    case DiagnosticSeverity::Error:
        return {0.96F, 0.32F, 0.32F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
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

[[nodiscard]] std::string UniqueDeclarationId(
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

[[nodiscard]] std::string UniqueObjectKey(
    const Json& object,
    const std::string_view prefix) {
    for (std::size_t suffix = 1U;; ++suffix) {
        std::string candidate{prefix};
        candidate += "_" + std::to_string(suffix);
        if (!object.contains(candidate)) return candidate;
    }
}

void TrackWidgetEdit(
    DocumentSession& session,
    const std::string_view label,
    const bool changed) {
    if (ImGui::IsItemActivated()) session.BeginEdit(std::string{label});
    if (changed) session.UpdateEdit();
    if (ImGui::IsItemDeactivatedAfterEdit()) session.EndEdit();
}

[[nodiscard]] bool DrawScalarValue(
    const char* label,
    Json& value,
    const std::string_view type,
    DocumentSession& session,
    const std::string_view undoLabel) {
    bool changed = false;
    bool discrete = false;
    if (type == "number") {
        double edited = value.is_number() ? value.get<double>() : 0.0;
        changed = ImGui::InputDouble(label, &edited);
        if (changed) value = edited;
    } else if (type == "integer") {
        std::int64_t edited = value.is_number_integer()
            ? value.get<std::int64_t>()
            : std::int64_t{};
        changed = ImGui::InputScalar(label, ImGuiDataType_S64, &edited);
        if (changed) value = edited;
    } else if (type == "boolean") {
        bool edited = value.is_boolean() ? value.get<bool>() : false;
        changed = ImGui::Checkbox(label, &edited);
        if (changed) value = edited;
        discrete = true;
    } else {
        std::array<char, 256> edited{};
        std::snprintf(
            edited.data(), edited.size(), "%s",
            value.is_string() ? value.get_ref<const std::string&>().c_str() : "");
        changed = ImGui::InputText(label, edited.data(), edited.size());
        if (changed) value = edited.data();
    }
    if (discrete) {
        if (changed) session.CommitEdit(std::string{undoLabel});
    } else {
        TrackWidgetEdit(session, undoLabel, changed);
    }
    return changed;
}

[[nodiscard]] Json DefaultScalarValue(const std::string_view type) {
    if (type == "integer") return std::int64_t{};
    if (type == "number") return 0.0;
    if (type == "boolean") return false;
    return "";
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

void RenameColorSelectCases(
    Json& value,
    const std::string_view oldId,
    const std::string_view newId) {
    if (value.is_object()) {
        for (auto member = value.begin(); member != value.end(); ++member) {
            const bool colorSource = member.key() == "color" ||
                                     member.key() == "tint" ||
                                     member.key() == "text_color";
            if (colorSource && member.value().is_object() &&
                member.value().contains("select") &&
                member.value()["select"].is_object() &&
                member.value()["select"].contains("cases") &&
                member.value()["select"]["cases"].is_object()) {
                for (auto& [caseName, color] :
                     member.value()["select"]["cases"].items()) {
                    static_cast<void>(caseName);
                    if (color.is_string() &&
                        color.get_ref<const std::string&>() == oldId) {
                        color = newId;
                    }
                }
            }
            RenameColorSelectCases(member.value(), oldId, newId);
        }
    } else if (value.is_array()) {
        for (Json& item : value) RenameColorSelectCases(item, oldId, newId);
    }
}

void CollectInteractiveNodeIds(const Json& node, std::vector<std::string>& ids) {
    if (!node.is_object()) return;
    const std::string type = node.value("type", std::string{});
    if ((type == "button" || type == "horizontal_slider") &&
        node.contains("id") && node["id"].is_string()) {
        ids.push_back(node["id"].get<std::string>());
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const Json& child : node["children"]) {
            CollectInteractiveNodeIds(child, ids);
        }
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

EditorApp::EditorApp(
    CommandLineOptions options,
    AssetPreviewContext& previewAssets)
    : options_(std::move(options)),
      previewAssets_(previewAssets) {}

bool EditorApp::Initialize(std::string& error) {
    error.clear();
    if (options_.catalogPath.has_value()) {
        if (!catalog_.Mount(
                *options_.catalogPath,
                options_.assetRoot.value_or(options_.catalogPath->parent_path()),
                error)) {
            return false;
        }
        if (!previewAssets_.Mount(
                *options_.catalogPath,
                options_.assetRoot.value_or(options_.catalogPath->parent_path()),
                error)) {
            catalog_.Unmount();
            return false;
        }
        CopyPath(catalogPath_, *options_.catalogPath);
        CopyPath(assetRoot_, catalog_.AssetRoot());
    }
    if (options_.inputPath.has_value()) {
        SessionResult opened = session_.Open(*options_.inputPath);
        if (!opened) {
            error = opened.error;
            diagnostics_ = std::move(opened.diagnostics);
            return false;
        }
        CopyPath(openPath_, *options_.inputPath);
    }
    if (options_.outputPath.has_value()) {
        session_.SetOutputPath(options_.outputPath);
        CopyPath(exportPath_, *options_.outputPath);
    } else if (session_.OutputPath().has_value()) {
        CopyPath(exportPath_, *session_.OutputPath());
    }
    RefreshDiagnostics();
    return true;
}

void EditorApp::Draw() {
    // Catalog remount destroys transient SDL textures. Apply it at the next
    // frame boundary, before any ImGui draw command can reference the old set.
    if (pendingCatalogMount_) {
        pendingCatalogMount_ = false;
        MountCatalog(pendingCatalogPath_, pendingAssetRoot_);
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.WantTextInput) {
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            showExportDialog_ = true;
        } else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            SaveDocument();
        } else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
            showOpenDialog_ = true;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (session_.Undo()) RefreshDiagnostics();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            if (session_.Redo()) RefreshDiagnostics();
        }
    }
    DrawMainMenu();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr float menuHeight = 22.0F;
    constexpr float statusHeight = 24.0F;
    constexpr float leftWidth = 275.0F;
    constexpr float rightWidth = 330.0F;
    constexpr float bottomHeight = 185.0F;
    const float top = viewport->WorkPos.y + menuHeight;
    const float contentHeight = viewport->WorkSize.y - menuHeight - statusHeight;
    const float centerWidth = viewport->WorkSize.x - leftWidth - rightWidth;

    ImGui::SetNextWindowPos({viewport->WorkPos.x, top});
    ImGui::SetNextWindowSize({leftWidth, contentHeight});
    DrawHierarchy();

    ImGui::SetNextWindowPos({viewport->WorkPos.x + leftWidth, top});
    ImGui::SetNextWindowSize({centerWidth, contentHeight - bottomHeight});
    DrawCanvas();

    ImGui::SetNextWindowPos({viewport->WorkPos.x + leftWidth + centerWidth, top});
    ImGui::SetNextWindowSize({rightWidth, contentHeight});
    DrawInspector();

    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + leftWidth, top + contentHeight - bottomHeight});
    ImGui::SetNextWindowSize({centerWidth, bottomHeight});
    DrawBottomPanel();

    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight});
    ImGui::SetNextWindowSize({viewport->WorkSize.x, statusHeight});
    DrawStatusBar();
    DrawDialogs();
}

bool EditorApp::WantsExit() const noexcept {
    return wantsExit_;
}

void EditorApp::RefreshDiagnostics() {
    diagnostics_ = session_.Validate(catalog_.IsMounted() ? &catalog_ : nullptr);
}

void EditorApp::DrawMainMenu() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
            NewDocument();
        }
        if (ImGui::MenuItem("Open JSON...", "Ctrl+O")) {
            showOpenDialog_ = true;
        }
        if (ImGui::MenuItem("Save JSON", "Ctrl+S")) {
            SaveDocument();
        }
        if (ImGui::MenuItem("Export As...", "Ctrl+Shift+S")) {
            showExportDialog_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Mount Read-Only Catalog...")) {
            showCatalogDialog_ = true;
        }
        if (catalog_.IsMounted() && ImGui::MenuItem("Unmount Catalog")) {
            catalog_.Unmount();
            previewAssets_.Unmount();
            RefreshDiagnostics();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            wantsExit_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, session_.CanUndo())) {
            static_cast<void>(session_.Undo());
            RefreshDiagnostics();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, session_.CanRedo())) {
            static_cast<void>(session_.Redo());
            RefreshDiagnostics();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Preview interaction", nullptr, &previewMode_);
        ImGui::EndMenu();
    }

    ImGui::Separator();
    ImGui::TextUnformatted(previewMode_ ? "PREVIEW" : "EDIT");
    ImGui::SameLine();
    const std::string title = session_.OutputPath().has_value()
        ? session_.OutputPath()->filename().string()
        : "Untitled UI";
    ImGui::Text("%s%s", title.c_str(), session_.IsDirty() ? " *" : "");
    ImGui::EndMainMenuBar();
}

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
                        ? std::min(index + 1U, siblings->size() - 1U)
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

void EditorApp::DrawCanvas() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("Canvas", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Grid", &showGrid_);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snapToGrid_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0F);
    ImGui::SliderFloat("Zoom", &canvasZoom_, 0.25F, 4.0F, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        canvasZoom_ = 1.0F;
        canvasPanX_ = 0.0F;
        canvasPanY_ = 0.0F;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton(
        "canvas-input",
        available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasFocused = ImGui::IsItemFocused();
    if (canvasHovered) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.MouseWheel != 0.0F) {
            canvasZoom_ = std::clamp(
                canvasZoom_ * (io.MouseWheel > 0.0F ? 1.1F : 1.0F / 1.1F),
                0.25F,
                4.0F);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            canvasPanX_ += io.MouseDelta.x;
            canvasPanY_ += io.MouseDelta.y;
        }
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        origin,
        {origin.x + available.x, origin.y + available.y},
        IM_COL32(13, 15, 19, 255));
    if (showGrid_) {
        const float step = std::max(gridSize_ * canvasZoom_, 8.0F);
        const float xOffset = std::fmod(canvasPanX_, step);
        const float yOffset = std::fmod(canvasPanY_, step);
        for (float x = origin.x + xOffset; x < origin.x + available.x; x += step) {
            drawList->AddLine(
                {x, origin.y},
                {x, origin.y + available.y},
                IM_COL32(36, 40, 49, 255));
        }
        for (float y = origin.y + yOffset; y < origin.y + available.y; y += step) {
            drawList->AddLine(
                {origin.x, y},
                {origin.x + available.x, y},
                IM_COL32(36, 40, 49, 255));
        }
    }
    const float scaledWidth = available.x * canvasZoom_;
    const float scaledHeight = available.y * canvasZoom_;
    const ImVec2 previewOrigin{
        origin.x + canvasPanX_ + (available.x - scaledWidth) * 0.5F,
        origin.y + canvasPanY_ + (available.y - scaledHeight) * 0.5F,
    };
    const PreviewAdapter::Result result = preview_.Draw(
        *drawList,
        session_.Document(),
        selectedCanvas_,
        {previewOrigin.x, previewOrigin.y, scaledWidth, scaledHeight},
        selectedNode_,
        previewAssets_.IsMounted() ? &previewAssets_ : nullptr,
        previewMode_,
        canvasHovered || canvasFocused,
        snapToGrid_,
        gridSize_);
    if (!result.error.empty()) {
        statusMessage_ = "Preview: " + result.error;
    }
    if (!result.clickedNodeId.empty() && !result.gizmoEdit.has_value()) {
        selectedNode_ = result.clickedNodeId;
    }
    if (result.gizmoEdit.has_value()) {
        const PreviewAdapter::Result::GizmoEdit& edit = *result.gizmoEdit;
        const char* key = edit.field == PreviewAdapter::Result::GizmoField::AnchorMin
            ? "anchor_min"
            : edit.field == PreviewAdapter::Result::GizmoField::AnchorMax
                ? "anchor_max"
                : "pivot";
        if (edit.began) {
            session_.BeginEdit(std::string{"Drag "} + key + " gizmo");
            gizmoTransactionActive_ = true;
        }
        if (Json* edited = FindSelectedNode(
                session_.EditDocument(), selectedNode_);
            edited != nullptr) {
            (*edited)["rect"][key] = {edit.x, edit.y};
            session_.UpdateEdit();
            RefreshDiagnostics();
        }
        if (edit.finished) {
            session_.EndEdit();
            gizmoTransactionActive_ = false;
        }
    } else if (gizmoTransactionActive_ &&
               !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        session_.EndEdit();
        gizmoTransactionActive_ = false;
    }
    for (const PreviewAdapter::Result::Action& action : result.actions) {
        std::string entry = action.id + "  source:" + action.sourceElement;
        entry += action.numberPayload.has_value()
            ? "  payload:" + std::to_string(*action.numberPayload)
            : "  payload:none";
        actionLog_.push_back(std::move(entry));
    }
    ImGui::End();
}

void EditorApp::DrawInspector() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Inspector", nullptr, flags)) {
        ImGui::End();
        return;
    }
    Json* node = FindSelectedNode(session_.EditDocument(), selectedNode_);
    if (node == nullptr) {
        ImGui::TextDisabled("Select a hierarchy or canvas node.");
        ImGui::Separator();
        Json& document = session_.EditDocument();
        if (ImGui::CollapsingHeader("Document", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Schema: %s", session_.Document().value("schema", "?").c_str());
            ImGui::Text("Version: %d", session_.Document().value("version", 0));
            Json& fonts = document["fonts"];
            const std::string defaultFont =
                document.value("default_font", std::string{});
            if (ImGui::BeginCombo("Default font alias", defaultFont.c_str())) {
                for (const auto& [alias, assetId] : fonts.items()) {
                    static_cast<void>(assetId);
                    if (ImGui::Selectable(alias.c_str(), alias == defaultFont)) {
                        document["default_font"] = alias;
                        session_.CommitEdit("Choose default font alias");
                        RefreshDiagnostics();
                    }
                }
                ImGui::EndCombo();
            }
            if (Json* canvas = FindCanvas(document, selectedCanvas_);
                canvas != nullptr) {
                ImGui::SeparatorText("Active canvas");
                std::array<char, 128> canvasId{};
                std::snprintf(
                    canvasId.data(), canvasId.size(), "%s",
                    canvas->value("id", std::string{}).c_str());
                const bool canvasIdChanged = ImGui::InputText(
                    "Canvas id", canvasId.data(), canvasId.size());
                if (canvasIdChanged) {
                    (*canvas)["id"] = canvasId.data();
                    selectedCanvas_ = canvasId.data();
                }
                TrackWidgetEdit(session_, "Rename canvas", canvasIdChanged);
                if (canvasIdChanged) RefreshDiagnostics();

                const std::string backdrop = canvas->value(
                    "backdrop_color", std::string{});
                if (ImGui::BeginCombo("Backdrop", backdrop.c_str())) {
                    for (const auto& [name, encoded] : document["colors"].items()) {
                        static_cast<void>(encoded);
                        if (ImGui::Selectable(name.c_str(), name == backdrop)) {
                            (*canvas)["backdrop_color"] = name;
                            session_.CommitEdit("Choose canvas backdrop");
                            RefreshDiagnostics();
                        }
                    }
                    ImGui::EndCombo();
                }

                std::vector<std::string> interactive;
                for (const Json& child : (*canvas)["children"]) {
                    CollectInteractiveNodeIds(child, interactive);
                }
                Json& focusOrder = (*canvas)["focus_order"];
                const std::string defaultFocus = canvas->value(
                    "default_focus", std::string{"(none)"});
                if (ImGui::BeginCombo("Default focus", defaultFocus.c_str())) {
                    if (ImGui::Selectable("(none)", !canvas->contains("default_focus"))) {
                        canvas->erase("default_focus");
                        session_.CommitEdit("Clear canvas default focus");
                        RefreshDiagnostics();
                    }
                    for (const std::string& id : interactive) {
                        if (ImGui::Selectable(id.c_str(), id == defaultFocus)) {
                            (*canvas)["default_focus"] = id;
                            const bool alreadyPresent = std::ranges::any_of(
                                focusOrder,
                                [&id](const Json& value) {
                                    return value.is_string() &&
                                           value.get_ref<const std::string&>() == id;
                                });
                            if (!alreadyPresent) focusOrder.push_back(id);
                            session_.CommitEdit("Choose canvas default focus");
                            RefreshDiagnostics();
                        }
                    }
                    ImGui::EndCombo();
                }

                const std::string cancelAction = canvas->value(
                    "cancel_action", std::string{"(none)"});
                if (ImGui::BeginCombo("Cancel action", cancelAction.c_str())) {
                    if (ImGui::Selectable("(none)", !canvas->contains("cancel_action"))) {
                        canvas->erase("cancel_action");
                        session_.CommitEdit("Clear canvas cancel action");
                        RefreshDiagnostics();
                    }
                    for (const Json& action : document["actions"]) {
                        if (action.value("payload", std::string{}) != "none") continue;
                        const std::string id = action.value("id", std::string{});
                        if (ImGui::Selectable(id.c_str(), id == cancelAction)) {
                            (*canvas)["cancel_action"] = id;
                            session_.CommitEdit("Choose canvas cancel action");
                            RefreshDiagnostics();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TextUnformatted("Explicit focus order");
                std::optional<std::size_t> removeFocus;
                for (std::size_t index = 0; index < focusOrder.size(); ++index) {
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TextUnformatted(focusOrder[index].get_ref<
                        const std::string&>().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Up") && index > 0U) {
                        std::swap(focusOrder[index], focusOrder[index - 1U]);
                        session_.CommitEdit("Reorder canvas focus");
                        RefreshDiagnostics();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Down") && index + 1U < focusOrder.size()) {
                        std::swap(focusOrder[index], focusOrder[index + 1U]);
                        session_.CommitEdit("Reorder canvas focus");
                        RefreshDiagnostics();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) removeFocus = index;
                    ImGui::PopID();
                }
                if (removeFocus.has_value()) {
                    focusOrder.erase(
                        focusOrder.begin() +
                        static_cast<std::ptrdiff_t>(*removeFocus));
                    session_.CommitEdit("Remove canvas focus entry");
                    RefreshDiagnostics();
                }
                if (ImGui::BeginCombo("Add focus entry", "choose element")) {
                    for (const std::string& id : interactive) {
                        const bool alreadyPresent = std::ranges::any_of(
                            focusOrder,
                            [&id](const Json& value) {
                                return value.is_string() &&
                                       value.get_ref<const std::string&>() == id;
                            });
                        if (!alreadyPresent && ImGui::Selectable(id.c_str())) {
                            focusOrder.push_back(id);
                            session_.CommitEdit("Add canvas focus entry");
                            RefreshDiagnostics();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        if (ImGui::CollapsingHeader("Font aliases", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("document-font-aliases");
            Json& fonts = document["fonts"];
            std::optional<std::string> remove;
            for (auto font = fonts.begin(); font != fonts.end(); ++font) {
                ImGui::PushID(font.key().c_str());
                ImGui::TextUnformatted(font.key().c_str());
                std::array<char, 256> assetId{};
                std::snprintf(
                    assetId.data(), assetId.size(), "%s",
                    font.value().is_string()
                        ? font.value().get_ref<const std::string&>().c_str()
                        : "");
                const bool changed =
                    ImGui::InputText("AssetId", assetId.data(), assetId.size());
                if (changed) font.value() = assetId.data();
                TrackWidgetEdit(session_, "Edit font AssetId", changed);
                if (changed) RefreshDiagnostics();
                if (ImGui::SmallButton("Pick from catalog...")) {
                    assetPickerTarget_ = AssetPickerTarget::FontAlias;
                    assetPickerFontAlias_ = font.key();
                    showAssetPicker_ = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Rename...")) {
                    renameTarget_ = RenameTarget::FontAlias;
                    renameOldValue_ = font.key();
                    std::snprintf(
                        renameValue_.data(), renameValue_.size(), "%s",
                        font.key().c_str());
                    showRenameDialog_ = true;
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(fonts.size() <= 1U);
                if (ImGui::SmallButton("Remove")) remove = font.key();
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::PopID();
            }
            if (remove.has_value()) {
                const bool removedDefault =
                    document.value("default_font", std::string{}) == *remove;
                fonts.erase(*remove);
                if (removedDefault && !fonts.empty()) {
                    document["default_font"] = fonts.begin().key();
                }
                session_.CommitEdit("Remove font alias");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add font alias")) {
                const std::string alias = UniqueObjectKey(fonts, "font");
                fonts[alias] = "font.unresolved";
                session_.CommitEdit("Add font alias");
                RefreshDiagnostics();
            }
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Named colors", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("document-named-colors");
            Json& colors = document["colors"];
            std::optional<std::string> remove;
            for (auto color = colors.begin(); color != colors.end(); ++color) {
                ImGui::PushID(color.key().c_str());
                ImGui::TextUnformatted(color.key().c_str());
                ImGui::SameLine();
                std::array<char, 16> encoded{};
                std::snprintf(
                    encoded.data(), encoded.size(), "%s",
                    color.value().is_string()
                        ? color.value().get_ref<const std::string&>().c_str()
                        : "");
                ImGui::SetNextItemWidth(120.0F);
                const bool changed = ImGui::InputText(
                    "#RRGGBBAA", encoded.data(), encoded.size());
                if (changed) color.value() = encoded.data();
                TrackWidgetEdit(session_, "Edit named color", changed);
                if (changed) RefreshDiagnostics();
                ImGui::SameLine();
                if (ImGui::SmallButton("Rename...")) {
                    renameTarget_ = RenameTarget::NamedColor;
                    renameOldValue_ = color.key();
                    std::snprintf(
                        renameValue_.data(), renameValue_.size(), "%s",
                        color.key().c_str());
                    showRenameDialog_ = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) remove = color.key();
                ImGui::PopID();
            }
            if (remove.has_value()) {
                colors.erase(*remove);
                session_.CommitEdit("Remove named color");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add named color")) {
                colors[UniqueObjectKey(colors, "color")] = "#FFFFFFFF";
                session_.CommitEdit("Add named color");
                RefreshDiagnostics();
            }
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Action declarations", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("document-action-declarations");
            Json& actions = document["actions"];
            std::optional<std::size_t> remove;
            for (std::size_t index = 0; index < actions.size(); ++index) {
                Json& action = actions[index];
                ImGui::PushID(static_cast<int>(index));
                const std::string oldId = action.value("id", std::string{});
                std::array<char, 256> id{};
                std::snprintf(id.data(), id.size(), "%s", oldId.c_str());
                const bool idChanged =
                    ImGui::InputText("Id", id.data(), id.size());
                if (idChanged) {
                    action["id"] = id.data();
                    RenameReferences(
                        document, oldId, id.data(), {"action", "cancel_action"});
                }
                TrackWidgetEdit(session_, "Rename action declaration", idChanged);
                if (idChanged) RefreshDiagnostics();
                const std::string payload = action.value("payload", "none");
                if (ImGui::BeginCombo("Payload", payload.c_str())) {
                    for (const char* candidate : {"none", "number"}) {
                        if (ImGui::Selectable(candidate, payload == candidate)) {
                            action["payload"] = candidate;
                            session_.CommitEdit("Change action payload type");
                            RefreshDiagnostics();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) remove = index;
                ImGui::Separator();
                ImGui::PopID();
            }
            if (remove.has_value()) {
                actions.erase(actions.begin() + static_cast<std::ptrdiff_t>(*remove));
                session_.CommitEdit("Remove action declaration");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add action")) {
                actions.push_back({
                    {"id", UniqueDeclarationId(actions, "action.new")},
                    {"payload", "none"},
                });
                session_.CommitEdit("Add action declaration");
                RefreshDiagnostics();
            }
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Bindings & preview values", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("document-binding-declarations");
            Json& bindings = document["bindings"];
            std::optional<std::size_t> remove;
            for (std::size_t index = 0; index < bindings.size(); ++index) {
                Json& binding = bindings[index];
                ImGui::PushID(static_cast<int>(index));
                const std::string oldId = binding.value("id", std::string{});
                std::array<char, 256> id{};
                std::snprintf(id.data(), id.size(), "%s", oldId.c_str());
                const bool idChanged =
                    ImGui::InputText("Id", id.data(), id.size());
                if (idChanged) {
                    binding["id"] = id.data();
                    RenameReferences(document, oldId, id.data(), {"binding"});
                }
                TrackWidgetEdit(session_, "Rename binding declaration", idChanged);
                if (idChanged) RefreshDiagnostics();
                std::string type = binding.value("type", std::string{});
                if (ImGui::BeginCombo("Type", type.c_str())) {
                    for (const char* candidate : {
                             "string", "integer", "number", "boolean",
                             "enum", "list<object>"}) {
                        if (!ImGui::Selectable(candidate, type == candidate)) continue;
                        type = candidate;
                        binding["type"] = type;
                        binding.erase("values");
                        binding.erase("item_fields");
                        if (type == "enum") {
                            binding["values"] = Json::array({"value_1"});
                            binding["preview"] = "value_1";
                        } else if (type == "list<object>") {
                            binding["item_fields"] = {{"label", "string"}};
                            binding["preview"] = Json::array();
                        } else {
                            binding["preview"] = DefaultScalarValue(type);
                        }
                        session_.CommitEdit("Change binding declaration type");
                        RefreshDiagnostics();
                    }
                    ImGui::EndCombo();
                }
                if (type == "number" || type == "integer" ||
                    type == "boolean" || type == "string") {
                    if (DrawScalarValue(
                            "Preview",
                            binding["preview"],
                            type,
                            session_,
                            "Edit binding preview value")) {
                        RefreshDiagnostics();
                    }
                } else if (type == "enum" && binding.contains("values") &&
                           binding["values"].is_array()) {
                    const std::string preview = binding.value("preview", std::string{});
                    if (ImGui::BeginCombo("Preview", preview.c_str())) {
                        for (const Json& candidate : binding["values"]) {
                            const std::string value = candidate.get<std::string>();
                            if (ImGui::Selectable(value.c_str(), value == preview)) {
                                binding["preview"] = value;
                                session_.CommitEdit("Choose enum preview value");
                                RefreshDiagnostics();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    std::optional<std::size_t> removeValue;
                    for (std::size_t valueIndex = 0;
                         valueIndex < binding["values"].size();
                         ++valueIndex) {
                        ImGui::PushID(static_cast<int>(valueIndex));
                        std::array<char, 128> value{};
                        std::snprintf(
                            value.data(), value.size(), "%s",
                            binding["values"][valueIndex].get_ref<
                                const std::string&>().c_str());
                        const std::string oldValue = value.data();
                        const bool valueChanged = ImGui::InputText(
                            "Value", value.data(), value.size());
                        if (valueChanged) {
                            binding["values"][valueIndex] = value.data();
                            if (binding.value("preview", std::string{}) == oldValue) {
                                binding["preview"] = value.data();
                            }
                        }
                        TrackWidgetEdit(
                            session_, "Edit enum declaration", valueChanged);
                        if (valueChanged) RefreshDiagnostics();
                        ImGui::SameLine();
                        ImGui::BeginDisabled(binding["values"].size() <= 1U);
                        if (ImGui::SmallButton("Remove")) removeValue = valueIndex;
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    if (removeValue.has_value()) {
                        const std::string removed =
                            binding["values"][*removeValue].get<std::string>();
                        binding["values"].erase(
                            binding["values"].begin() +
                            static_cast<std::ptrdiff_t>(*removeValue));
                        if (binding.value("preview", std::string{}) == removed) {
                            binding["preview"] = binding["values"].front();
                        }
                        session_.CommitEdit("Remove enum value");
                        RefreshDiagnostics();
                    }
                    if (ImGui::SmallButton("Add enum value")) {
                        const std::string value =
                            "value_" + std::to_string(binding["values"].size() + 1U);
                        binding["values"].push_back(value);
                        session_.CommitEdit("Add enum value");
                        RefreshDiagnostics();
                    }
                } else if (type == "list<object>") {
                    Json& fields = binding["item_fields"];
                    Json& preview = binding["preview"];
                    if (!fields.is_object()) fields = Json::object();
                    if (!preview.is_array()) preview = Json::array();
                    ImGui::TextUnformatted("Item fields");
                    std::optional<std::string> removeField;
                    for (auto field = fields.begin(); field != fields.end(); ++field) {
                        ImGui::PushID(field.key().c_str());
                        const std::string fieldType = field.value().is_string()
                            ? field.value().get<std::string>()
                            : "string";
                        ImGui::TextUnformatted(field.key().c_str());
                        ImGui::SameLine();
                        if (ImGui::BeginCombo("##field-type", fieldType.c_str())) {
                            for (const char* candidate : {
                                     "string", "integer", "number", "boolean"}) {
                                if (ImGui::Selectable(
                                        candidate, fieldType == candidate)) {
                                    field.value() = candidate;
                                    for (Json& item : preview) {
                                        item[field.key()] =
                                            DefaultScalarValue(candidate);
                                    }
                                    session_.CommitEdit("Change list item field type");
                                    RefreshDiagnostics();
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SameLine();
                        ImGui::BeginDisabled(fields.size() <= 1U);
                        if (ImGui::SmallButton("Remove")) removeField = field.key();
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                    if (removeField.has_value()) {
                        fields.erase(*removeField);
                        for (Json& item : preview) item.erase(*removeField);
                        session_.CommitEdit("Remove list item field");
                        RefreshDiagnostics();
                    }
                    if (ImGui::SmallButton("Add item field")) {
                        const std::string field = UniqueObjectKey(fields, "field");
                        fields[field] = "string";
                        for (Json& item : preview) item[field] = "";
                        session_.CommitEdit("Add list item field");
                        RefreshDiagnostics();
                    }
                    ImGui::Text("Preview items: %zu", preview.size());
                    std::optional<std::size_t> removeItem;
                    for (std::size_t itemIndex = 0;
                         itemIndex < preview.size();
                         ++itemIndex) {
                        ImGui::PushID(static_cast<int>(itemIndex));
                        ImGui::SeparatorText(
                            ("Item " + std::to_string(itemIndex + 1U)).c_str());
                        for (const auto& [fieldName, fieldTypeValue] : fields.items()) {
                            const std::string fieldType =
                                fieldTypeValue.get<std::string>();
                            if (!preview[itemIndex].contains(fieldName)) {
                                preview[itemIndex][fieldName] =
                                    DefaultScalarValue(fieldType);
                            }
                            if (DrawScalarValue(
                                    fieldName.c_str(),
                                    preview[itemIndex][fieldName],
                                    fieldType,
                                    session_,
                                    "Edit list preview item")) {
                                RefreshDiagnostics();
                            }
                        }
                        if (ImGui::SmallButton("Remove preview item")) {
                            removeItem = itemIndex;
                        }
                        ImGui::PopID();
                    }
                    if (removeItem.has_value()) {
                        preview.erase(
                            preview.begin() +
                            static_cast<std::ptrdiff_t>(*removeItem));
                        session_.CommitEdit("Remove list preview item");
                        RefreshDiagnostics();
                    }
                    if (ImGui::SmallButton("Add preview item")) {
                        Json item = Json::object();
                        for (const auto& [fieldName, fieldType] : fields.items()) {
                            item[fieldName] = DefaultScalarValue(
                                fieldType.get<std::string>());
                        }
                        preview.push_back(std::move(item));
                        session_.CommitEdit("Add list preview item");
                        RefreshDiagnostics();
                    }
                }
                if (ImGui::SmallButton("Remove binding")) remove = index;
                ImGui::Separator();
                ImGui::PopID();
            }
            if (remove.has_value()) {
                bindings.erase(bindings.begin() + static_cast<std::ptrdiff_t>(*remove));
                session_.CommitEdit("Remove binding declaration");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add number binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.number")},
                    {"type", "number"},
                    {"preview", 0.0},
                });
                session_.CommitEdit("Add number binding");
                RefreshDiagnostics();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add integer binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.integer")},
                    {"type", "integer"},
                    {"preview", std::int64_t{}},
                });
                session_.CommitEdit("Add integer binding");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add boolean binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.boolean")},
                    {"type", "boolean"},
                    {"preview", false},
                });
                session_.CommitEdit("Add boolean binding");
                RefreshDiagnostics();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add string binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.string")},
                    {"type", "string"},
                    {"preview", ""},
                });
                session_.CommitEdit("Add string binding");
                RefreshDiagnostics();
            }
            if (ImGui::Button("Add enum binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.enum")},
                    {"type", "enum"},
                    {"values", Json::array({"value_1"})},
                    {"preview", "value_1"},
                });
                session_.CommitEdit("Add enum binding");
                RefreshDiagnostics();
            }
            ImGui::SameLine();
            if (ImGui::Button("Add list binding")) {
                bindings.push_back({
                    {"id", UniqueDeclarationId(bindings, "binding.list")},
                    {"type", "list<object>"},
                    {"item_fields", {{"label", "string"}}},
                    {"preview", Json::array()},
                });
                session_.CommitEdit("Add list binding");
                RefreshDiagnostics();
            }
            ImGui::PopID();
        }
        ImGui::End();
        return;
    }

    ImGui::Text("Node: %s", selectedNode_.c_str());
    ImGui::Text("Type: %s", node->value("type", "unknown").c_str());
    ImGui::SeparatorText("RectTransform");
    Json& transform = (*node)["rect"];
    auto editPair = [this, &transform](
                        const char* label,
                        const char* key,
                        const float speed,
                        const bool normalized) {
        if (!transform.contains(key) || !transform[key].is_array() ||
            transform[key].size() != 2U || !transform[key][0].is_number() ||
            !transform[key][1].is_number()) {
            transform[key] = {0.0F, 0.0F};
        }
        float values[2]{
            transform[key][0].get<float>(),
            transform[key][1].get<float>(),
        };
        const bool changed = normalized
            ? ImGui::DragFloat2(label, values, speed, 0.0F, 1.0F)
            : ImGui::DragFloat2(label, values, speed);
        if (ImGui::IsItemActivated()) {
            session_.BeginEdit(std::string{"Edit "} + key);
        }
        if (changed) {
            if (snapToGrid_ && !normalized) {
                values[0] = std::round(values[0] / gridSize_) * gridSize_;
                values[1] = std::round(values[1] / gridSize_) * gridSize_;
            }
            transform[key] = {values[0], values[1]};
            session_.UpdateEdit();
            RefreshDiagnostics();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            session_.EndEdit();
        }
    };
    editPair("Anchor Min", "anchor_min", 0.005F, true);
    editPair("Anchor Max", "anchor_max", 0.005F, true);
    editPair("Pivot", "pivot", 0.005F, true);
    editPair("Position", "position", 0.25F, false);
    editPair("Size Delta", "size_delta", 0.25F, false);

    const std::string nodeType = node->value("type", std::string{});
    if (node->contains("text") && (*node)["text"].is_object() &&
        (*node)["text"].contains("literal")) {
        ImGui::SeparatorText("Text style");
        std::array<char, 512> literal{};
        std::snprintf(
            literal.data(), literal.size(), "%s",
            (*node)["text"].value("literal", std::string{}).c_str());
        const bool literalChanged =
            ImGui::InputText("Literal", literal.data(), literal.size());
        if (literalChanged) {
            (*node)["text"]["literal"] = literal.data();
            RefreshDiagnostics();
        }
        TrackWidgetEdit(session_, "Edit text literal", literalChanged);
        float pointSize = node->value("point_size", 24.0F);
        const bool pointSizeChanged = ImGui::DragFloat(
            "Point size", &pointSize, 0.25F, 1.0F, 256.0F);
        if (pointSizeChanged) {
            (*node)["point_size"] = pointSize;
            RefreshDiagnostics();
        }
        TrackWidgetEdit(session_, "Edit text point size", pointSizeChanged);
        Json& fonts = session_.EditDocument()["fonts"];
        const std::string font = node->value(
            "font", session_.Document().value("default_font", std::string{}));
        if (ImGui::BeginCombo("Font alias", font.c_str())) {
            for (const auto& [alias, asset] : fonts.items()) {
                static_cast<void>(asset);
                if (ImGui::Selectable(alias.c_str(), alias == font)) {
                    (*node)["font"] = alias;
                    session_.CommitEdit("Choose font alias");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SeparatorText("Style references");
    Json& colorDeclarations = session_.EditDocument()["colors"];
    auto colorCombo = [this, &colorDeclarations](
                          const char* label,
                          Json& colorReference) {
        const std::string current = colorReference.is_string()
            ? colorReference.get<std::string>()
            : std::string{};
        if (ImGui::BeginCombo(label, current.c_str())) {
            for (const auto& [name, encoded] : colorDeclarations.items()) {
                static_cast<void>(encoded);
                if (ImGui::Selectable(name.c_str(), name == current)) {
                    colorReference = name;
                    session_.CommitEdit("Choose named color");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
    };
    auto colorSourceCombo = [&colorCombo](const char* label, Json& source) {
        if (source.is_object() && source.contains("color") &&
            source["color"].is_string()) {
            colorCombo(label, source["color"]);
        } else {
            ImGui::TextDisabled("%s: binding-select source", label);
        }
    };
    if (nodeType == "panel") {
        colorSourceCombo("Panel", (*node)["color"]);
    } else if (nodeType == "image") {
        colorSourceCombo("Image tint", (*node)["tint"]);
    }
    if (node->contains("text_color")) {
        colorSourceCombo("Text", (*node)["text_color"]);
        const std::string horizontal =
            node->value("horizontal_align", std::string{"left"});
        if (ImGui::BeginCombo("Horizontal align", horizontal.c_str())) {
            for (const char* candidate : {"left", "center", "right"}) {
                if (ImGui::Selectable(candidate, horizontal == candidate)) {
                    (*node)["horizontal_align"] = candidate;
                    session_.CommitEdit("Change horizontal text alignment");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
        const std::string vertical =
            node->value("vertical_align", std::string{"top"});
        if (ImGui::BeginCombo("Vertical align", vertical.c_str())) {
            for (const char* candidate : {"top", "center", "bottom"}) {
                if (ImGui::Selectable(candidate, vertical == candidate)) {
                    (*node)["vertical_align"] = candidate;
                    session_.CommitEdit("Change vertical text alignment");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (node->contains("background") && (*node)["background"].is_object()) {
        colorCombo("Normal", (*node)["background"]["normal"]);
        colorCombo("Focused", (*node)["background"]["focused"]);
        colorCombo("Pressed", (*node)["background"]["pressed"]);
    }
    if (nodeType == "horizontal_slider") {
        colorCombo("Track", (*node)["track_color"]);
        colorCombo("Fill", (*node)["fill_color"]);
        colorCombo("Thumb", (*node)["thumb_color"]);
    }

    ImGui::SeparatorText("Typed references");
    ImGui::TextDisabled("Opaque IDs are declared by the document; app code owns semantics.");
    if (node->contains("action")) {
        const std::string current = node->value("action", std::string{});
        if (ImGui::BeginCombo("Action", current.c_str())) {
            for (const Json& action : session_.Document()["actions"]) {
                const bool needsNumber = nodeType == "horizontal_slider";
                if (action.value("payload", "none") !=
                    (needsNumber ? "number" : "none")) {
                    continue;
                }
                const std::string id = action.value("id", std::string{});
                if (ImGui::Selectable(id.c_str(), id == current)) {
                    (*node)["action"] = id;
                    session_.CommitEdit("Choose action declaration");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (node->contains("binding")) {
        const std::string current = node->value("binding", std::string{});
        if (ImGui::BeginCombo("Binding", current.c_str())) {
            const std::string expected = nodeType == "fixed_step_list"
                ? "list<object>"
                : "number";
            for (const Json& binding : session_.Document()["bindings"]) {
                if (binding.value("type", std::string{}) != expected) continue;
                const std::string id = binding.value("id", std::string{});
                if (ImGui::Selectable(id.c_str(), id == current)) {
                    (*node)["binding"] = id;
                    session_.CommitEdit("Choose binding declaration");
                    RefreshDiagnostics();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (nodeType == "image" && ImGui::Button("Pick Texture AssetId...")) {
        assetPickerTarget_ = AssetPickerTarget::ImageTexture;
        showAssetPicker_ = true;
    }
    ImGui::End();
}

void EditorApp::DrawBottomPanel() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Diagnostics & Action Log", nullptr, flags)) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("bottom-tabs")) {
        if (ImGui::BeginTabItem("Diagnostics")) {
            if (ImGui::Button("Validate now")) {
                RefreshDiagnostics();
            }
            ImGui::SameLine();
            ImGui::Text("%zu item(s)", diagnostics_.size());
            ImGui::BeginChild("diagnostics-list");
            for (const Diagnostic& diagnostic : diagnostics_) {
                ImGui::TextColored(
                    SeverityColor(diagnostic.severity),
                    "%s  %s  %s",
                    SeverityLabel(diagnostic.severity),
                    diagnostic.path.c_str(),
                    diagnostic.message.c_str());
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Action Log")) {
            if (ImGui::Button("Clear")) {
                actionLog_.clear();
            }
            ImGui::BeginChild("action-list");
            for (const std::string& entry : actionLog_) {
                ImGui::TextUnformatted(entry.c_str());
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void EditorApp::DrawDialogs() {
    if (showOpenDialog_) {
        ImGui::OpenPopup("Open UI JSON");
        showOpenDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Open UI JSON", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", openPath_.data(), openPath_.size());
        if (ImGui::Button("Open")) {
            OpenDocument(openPath_.data());
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showExportDialog_) {
        ImGui::OpenPopup("Export UI JSON");
        showExportDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Export UI JSON", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", exportPath_.data(), exportPath_.size());
        ImGui::TextWrapped(
            "Export must remain outside the mounted app asset root. Copy and "
            "register the JSON manually after authoring.");
        if (ImGui::Button("Export")) {
            ExportDocument(exportPath_.data());
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showCatalogDialog_) {
        ImGui::OpenPopup("Mount Read-Only Catalog");
        showCatalogDialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            "Mount Read-Only Catalog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Catalog", catalogPath_.data(), catalogPath_.size());
        ImGui::InputText("Asset Root", assetRoot_.data(), assetRoot_.size());
        if (ImGui::Button("Mount")) {
            pendingCatalogPath_ = catalogPath_.data();
            pendingAssetRoot_ = assetRoot_.data();
            pendingCatalogMount_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showOverwriteDialog_) {
        ImGui::OpenPopup("Output changed");
        showOverwriteDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Output changed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "The output exists or changed outside the editor. Overwrite it with "
            "the current validated document?");
        if (ImGui::Button("Overwrite")) {
            if (pendingOverwriteIsExport_) {
                ExportDocument(pendingOverwritePath_, true);
            } else {
                SaveDocument(true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (showRenameDialog_) {
        ImGui::OpenPopup("Rename declaration");
        showRenameDialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            "Rename declaration", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", renameValue_.data(), renameValue_.size());
        Json& document = session_.EditDocument();
        Json& declarations = renameTarget_ == RenameTarget::FontAlias
            ? document["fonts"]
            : document["colors"];
        const std::string renamed = renameValue_.data();
        const bool duplicate = renamed != renameOldValue_ &&
                               declarations.contains(renamed);
        const bool canRename = !renamed.empty() && !duplicate &&
                               declarations.contains(renameOldValue_);
        if (duplicate) ImGui::TextDisabled("That name is already declared.");
        ImGui::BeginDisabled(!canRename);
        if (ImGui::Button("Rename")) {
            if (renamed != renameOldValue_) {
                Json declaration = std::move(declarations[renameOldValue_]);
                declarations.erase(renameOldValue_);
                declarations[renamed] = std::move(declaration);
                if (renameTarget_ == RenameTarget::FontAlias) {
                    RenameReferences(
                        document,
                        renameOldValue_,
                        renamed,
                        {"font", "default_font"});
                    if (assetPickerFontAlias_ == renameOldValue_) {
                        assetPickerFontAlias_ = renamed;
                    }
                    session_.CommitEdit("Rename font alias");
                } else {
                    RenameReferences(
                        document,
                        renameOldValue_,
                        renamed,
                        {"color", "backdrop_color", "normal", "focused",
                         "pressed", "track_color", "fill_color", "thumb_color"});
                    RenameColorSelectCases(document, renameOldValue_, renamed);
                    session_.CommitEdit("Rename named color");
                }
                RefreshDiagnostics();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showAssetPicker_) {
        ImGui::OpenPopup("AssetId Picker");
        showAssetPicker_ = false;
    }
    if (ImGui::BeginPopupModal("AssetId Picker", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!catalog_.IsMounted()) {
            ImGui::TextDisabled("Mount an app AssetCatalog first.");
        } else {
            ImGui::Text("Read-only: %s", catalog_.CatalogPath().string().c_str());
            ImGui::BeginChild("assets", {620.0F, 320.0F}, true);
            for (const CatalogAsset& asset : catalog_.Assets()) {
                const char* expectedType =
                    assetPickerTarget_ == AssetPickerTarget::FontAlias
                    ? "font"
                    : "texture";
                if (asset.type != expectedType) {
                    continue;
                }
                const std::string row = asset.id + "  [" + asset.type + "]";
                if (ImGui::Selectable(row.c_str())) {
                    Json& document = session_.EditDocument();
                    if (assetPickerTarget_ == AssetPickerTarget::FontAlias) {
                        document["fonts"][assetPickerFontAlias_] = asset.id;
                        session_.CommitEdit("Choose font alias asset");
                    } else if (Json* node = FindSelectedNode(
                                   document, selectedNode_);
                               node != nullptr) {
                        (*node)["texture_asset"] = asset.id;
                        session_.CommitEdit("Choose image texture asset");
                    }
                    RefreshDiagnostics();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();
        }
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorApp::DrawStatusBar() {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Status", nullptr, flags);
    ImGui::TextUnformatted(statusMessage_.empty() ? "Ready" : statusMessage_.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 310.0F);
    ImGui::TextUnformatted(catalog_.IsMounted() ? "Catalog: read-only" : "Catalog: none");
    ImGui::SameLine();
    ImGui::Text("| %s", session_.IsDirty() ? "Unsaved" : "Saved");
    ImGui::End();
}

void EditorApp::NewDocument() {
    session_.NewDocument();
    selectedCanvas_ = "main";
    selectedNode_.clear();
    statusMessage_ = "Created a new unsaved UI document";
    RefreshDiagnostics();
}

void EditorApp::OpenDocument(const std::string& path) {
    SessionResult result = session_.Open(path);
    diagnostics_ = result.diagnostics;
    statusMessage_ = result ? "Opened " + path : result.error;
    if (result) {
        const Json& document = session_.Document();
        selectedCanvas_ = document.contains("canvases") &&
                document["canvases"].is_array() &&
                !document["canvases"].empty()
            ? document["canvases"].front().value("id", std::string{"main"})
            : "main";
        selectedNode_.clear();
        CopyPath(exportPath_, *session_.OutputPath());
        RefreshDiagnostics();
    }
}

void EditorApp::SaveDocument(const bool overwriteExternal) {
    SessionResult result = session_.Save(
        catalog_.IsMounted() ? &catalog_ : nullptr,
        overwriteExternal);
    diagnostics_ = result.diagnostics;
    if (result) {
        statusMessage_ = "Saved canonical UI JSON";
    } else if (result.saveFailure == SaveFailure::MissingOutputPath) {
        showExportDialog_ = true;
    } else if (result.saveFailure == SaveFailure::ExternalModification) {
        pendingOverwritePath_ = session_.OutputPath()->string();
        pendingOverwriteIsExport_ = false;
        showOverwriteDialog_ = true;
    } else {
        statusMessage_ = result.error;
    }
}

void EditorApp::ExportDocument(
    const std::string& path,
    const bool overwriteExternal) {
    SessionResult result = session_.Export(
        path,
        catalog_.IsMounted() ? &catalog_ : nullptr,
        overwriteExternal);
    diagnostics_ = result.diagnostics;
    if (result) {
        statusMessage_ = "Exported canonical UI JSON to " + path;
        CopyPath(exportPath_, *session_.OutputPath());
    } else if (result.saveFailure == SaveFailure::ExternalModification) {
        pendingOverwritePath_ = path;
        pendingOverwriteIsExport_ = true;
        showOverwriteDialog_ = true;
    } else {
        statusMessage_ = result.error;
    }
}

void EditorApp::MountCatalog(
    const std::string& catalogPath,
    const std::string& assetRoot) {
    std::string error;
    if (!catalog_.Mount(catalogPath, assetRoot, error)) {
        statusMessage_ = std::move(error);
    } else if (!previewAssets_.Mount(catalogPath, assetRoot, error)) {
        catalog_.Unmount();
        statusMessage_ = "Preview asset mount failed: " + error;
    } else {
        statusMessage_ = "Mounted catalog read-only: " + catalogPath;
    }
    RefreshDiagnostics();
}

} // namespace Gyo::Tools::UiEditor
