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

// EditorApp: the modal dialogs (open, export, mount, rename and confirmations).

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;
using Detail::FieldPath;
using Detail::FindSelectedNode;
using Detail::RenameReferences;

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

} // namespace

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
        ImGui::OpenPopup("Mount Read-Only Content");
        showCatalogDialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            "Mount Read-Only Content", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("Single catalog (partial content)", &singleCatalogMode_);
        // Browse fills a field from the host's dialog; the field stays editable.
        const auto browse = [this](const char* id, const PathDialogPurpose purpose,
                                const std::array<char, 1024>& field) {
            if (dialogs_ == nullptr) return;
            ImGui::SameLine();
            ImGui::BeginDisabled(pathDialogOpen_);
            if (ImGui::Button(id)) {
                pathDialogOpen_ = true;
                dialogs_->Show(purpose, field.data());
            }
            ImGui::EndDisabled();
        };
        if (singleCatalogMode_) {
            ImGui::InputText("Catalog", catalogPath_.data(), catalogPath_.size());
            browse("Browse...##catalog", PathDialogPurpose::CatalogField, catalogPath_);
        } else {
            ImGui::TextUnformatted("Load content.json and all declared catalogs from Asset Root.");
        }
        ImGui::InputText("Asset Root", assetRoot_.data(), assetRoot_.size());
        browse("Browse...##root", PathDialogPurpose::AssetRootField, assetRoot_);
        if (ImGui::Button("Mount")) {
            if (singleCatalogMode_ && catalogPath_[0] == '\0') {
                statusMessage_ = "Single catalog mode requires a catalog path";
            } else {
                const auto catalog = singleCatalogMode_ ? FieldPath(catalogPath_.data())
                                                        : std::optional<std::filesystem::path>{std::in_place};
                const auto root = FieldPath(assetRoot_.data());
                if (catalog && root) {
                    RequestAssetMount(*catalog, *root);
                    ImGui::CloseCurrentPopup();
                } else {
                    statusMessage_ = "Asset paths must be UTF-8 text";
                }
            }
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
            const auto source = catalog_.CatalogPath().empty()
                ? catalog_.AssetRoot() / "content.json" : catalog_.CatalogPath();
            ImGui::Text("Read-only: %s", Engine::Os::Files::PathForMessage(source).c_str());
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

} // namespace Gyo::Tools::UiEditor
