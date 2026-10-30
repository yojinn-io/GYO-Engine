#include "gyo/ui_editor/EditorApp.hpp"

#include "EditorDocumentEdits.hpp"
#include "gyo/ui_editor/ElementSchema.hpp"

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

// EditorApp: the Inspector panel (the selected element and the declarations).

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;
using Detail::FindSelectedNode;
using Detail::FindCanvas;
using Detail::RenameReferences;

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

void CollectInteractiveNodeIds(const Json& node, std::vector<std::string>& ids) {
    if (!node.is_object()) return;
    const ElementKind* kind = FindElementKind(node.value("type", std::string{}));
    if (kind != nullptr && kind->focusable && node.contains("id") && node["id"].is_string()) {
        ids.push_back(node["id"].get<std::string>());
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const Json& child : node["children"]) {
            CollectInteractiveNodeIds(child, ids);
        }
    }
}

} // namespace

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

    const ElementKind* kind = FindElementKind(node->value("type", std::string{}));
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
    // A kind's own color sources come before the shared text styles, its plain
    // color names after the background states.
    const std::span<const ElementColorField> kindColors =
        kind != nullptr ? kind->colors : std::span<const ElementColorField>{};
    for (const ElementColorField& field : kindColors) {
        if (field.source) colorSourceCombo(field.label, (*node)[field.key]);
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
    for (const ElementColorField& field : kindColors) {
        if (!field.source) colorCombo(field.label, (*node)[field.key]);
    }

    ImGui::SeparatorText("Typed references");
    ImGui::TextDisabled("Opaque IDs are declared by the document; app code owns semantics.");
    if (node->contains("action")) {
        const std::string current = node->value("action", std::string{});
        if (ImGui::BeginCombo("Action", current.c_str())) {
            for (const Json& action : session_.Document()["actions"]) {
                const std::string_view payload =
                    kind != nullptr && !kind->actionPayload.empty() ? kind->actionPayload : "none";
                if (action.value("payload", "none") != payload) {
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
            const std::string expected{
                kind != nullptr && !kind->bindingType.empty() ? kind->bindingType : "number"};
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
    if (kind != nullptr && kind->textureAsset && ImGui::Button("Pick Texture AssetId...")) {
        assetPickerTarget_ = AssetPickerTarget::ImageTexture;
        showAssetPicker_ = true;
    }
    ImGui::End();
}

} // namespace Gyo::Tools::UiEditor
