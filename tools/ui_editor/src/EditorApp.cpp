#include "gyo/ui_editor/EditorApp.hpp"

#include "EditorDocumentEdits.hpp"
#include "gyo/ui_editor/ElementClipboard.hpp"

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

// EditorApp: setup, menu, diagnostics panel, status bar and document commands.

namespace Gyo::Tools::UiEditor {
namespace {

using Detail::Json;
using Detail::CopyPath;
using Detail::FieldPath;
using Detail::FindSelectedNode;

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

} // namespace

EditorApp::EditorApp(
    CommandLineOptions options,
    AssetPreviewContext& previewAssets,
    PathDialogs* dialogs)
    : options_(std::move(options)),
      previewAssets_(previewAssets),
      dialogs_(dialogs) {}

Result<void, EditorError> EditorApp::Initialize() {
    singleCatalogMode_ = options_.catalogPath.has_value();
    if (options_.assetRoot.has_value()) {
        const auto mounted = MountCatalog(
            options_.catalogPath.value_or(std::filesystem::path{}), *options_.assetRoot);
        if (!mounted) {
            return Engine::Base::Err(EditorError::Make(EditorErrorCode::CatalogUnavailable,
                mounted.error().message, Engine::Base::CauseDetail(mounted.error())));
        }
        CopyPath(catalogPath_, catalog_.CatalogPath());
        CopyPath(assetRoot_, catalog_.AssetRoot());
    }
    if (options_.inputPath.has_value()) {
        auto opened = session_.Open(*options_.inputPath);
        if (!opened) {
            diagnostics_ = std::move(opened.error().diagnostics);
            return Engine::Base::Err(EditorError::Make(EditorErrorCode::DocumentUnavailable,
                opened.error().message, Engine::Base::CauseDetail(opened.error())));
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
    return {};
}

void EditorApp::ApplyPendingAssetChanges() {
    if (pendingCatalogMount_) {
        pendingCatalogMount_ = false;
        const auto mounted = MountCatalog(pendingCatalogPath_, pendingAssetRoot_);
        if (mounted) {
            statusMessage_ = "Mounted read-only content: " + Engine::Os::Files::PathForMessage(catalog_.AssetRoot());
        } else {
            statusMessage_ = "Asset mount failed: " + Engine::Base::Describe(mounted.error());
        }
        RefreshDiagnostics();
    }
    if (pendingCatalogUnmount_) {
        pendingCatalogUnmount_ = false;
        previewAssets_.Unmount();
        catalog_.Unmount();
        statusMessage_ = "Unmounted content";
        RefreshDiagnostics();
    }
}

void EditorApp::ApplyPathDialogResults() {
    if (dialogs_ == nullptr) return;
    for (const PathDialogResult& result : dialogs_->TakeResults()) {
        pathDialogOpen_ = false;
        if (result.outcome == PathDialogOutcome::Cancelled) continue;
        if (result.outcome == PathDialogOutcome::Failed) {
            statusMessage_ = "The file dialog is unavailable (" + result.error + "); type the path instead";
            if (result.purpose == PathDialogPurpose::OpenDocument) showOpenDialog_ = true;
            if (result.purpose == PathDialogPurpose::ExportDocument) showExportDialog_ = true;
            if (result.purpose == PathDialogPurpose::AssetFolder) showCatalogDialog_ = true;
            continue;
        }
        const auto path = FieldPath(result.path.c_str());
        if (!path) {
            statusMessage_ = "The chosen path is not UTF-8 text";
            continue;
        }
        switch (result.purpose) {
        case PathDialogPurpose::OpenDocument:
            CopyPath(openPath_, *path);
            OpenDocument(result.path);
            break;
        case PathDialogPurpose::ExportDocument:
            CopyPath(exportPath_, *path);
            ExportDocument(result.path);
            break;
        case PathDialogPurpose::AssetFolder:
            // A whole root with its content.json, as the mount dialog's default mode.
            singleCatalogMode_ = false;
            CopyPath(assetRoot_, *path);
            RequestAssetMount({}, *path);
            break;
        case PathDialogPurpose::AssetRootField:
            CopyPath(assetRoot_, *path);
            break;
        case PathDialogPurpose::CatalogField:
            CopyPath(catalogPath_, *path);
            break;
        }
    }
}

void EditorApp::RequestPath(
    const PathDialogPurpose purpose, const std::array<char, 1024>& field, bool& manualPrompt) {
    if (dialogs_ == nullptr) {
        manualPrompt = true;
        return;
    }
    if (pathDialogOpen_) return;
    pathDialogOpen_ = true;
    dialogs_->Show(purpose, field.data());
}

void EditorApp::RequestAssetMount(std::filesystem::path catalogPath, std::filesystem::path assetRoot) {
    pendingCatalogPath_ = std::move(catalogPath);
    pendingAssetRoot_ = std::move(assetRoot);
    pendingCatalogMount_ = true;
    pendingCatalogUnmount_ = false;
}

void EditorApp::RequestAssetUnmount() {
    pendingCatalogMount_ = false;
    pendingCatalogUnmount_ = true;
}

void EditorApp::Draw() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.WantTextInput) {
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            RequestPath(PathDialogPurpose::ExportDocument, exportPath_, showExportDialog_);
        } else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            SaveDocument();
        } else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
            RequestPath(PathDialogPurpose::OpenDocument, openPath_, showOpenDialog_);
        } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (session_.Undo()) RefreshDiagnostics();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            if (session_.Redo()) RefreshDiagnostics();
        } else if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            CopySelectedElement();
        } else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            PasteClipboardElement();
        } else if (ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            DuplicateSelectedElement();
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
        if (ImGui::MenuItem("Open JSON...", "Ctrl+O", false, !pathDialogOpen_)) {
            RequestPath(PathDialogPurpose::OpenDocument, openPath_, showOpenDialog_);
        }
        if (ImGui::MenuItem("Save JSON", "Ctrl+S")) {
            SaveDocument();
        }
        if (ImGui::MenuItem("Export As...", "Ctrl+Shift+S", false, !pathDialogOpen_)) {
            RequestPath(PathDialogPurpose::ExportDocument, exportPath_, showExportDialog_);
        }
        ImGui::Separator();
        if (dialogs_ != nullptr &&
            ImGui::MenuItem("Open Asset Folder...", nullptr, false, !pathDialogOpen_)) {
            pathDialogOpen_ = true;
            dialogs_->Show(PathDialogPurpose::AssetFolder, assetRoot_.data());
        }
        if (ImGui::MenuItem("Mount Read-Only Content...")) {
            showCatalogDialog_ = true;
        }
        if (catalog_.IsMounted() && ImGui::MenuItem("Unmount Content")) {
            RequestAssetUnmount();
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
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Element", "Ctrl+C", false, !selectedNode_.empty())) {
            CopySelectedElement();
        }
        if (ImGui::MenuItem("Paste Element", "Ctrl+V", false, clipboard_.has_value())) {
            PasteClipboardElement();
        }
        if (ImGui::MenuItem("Duplicate Element", "Ctrl+D", false, !selectedNode_.empty())) {
            DuplicateSelectedElement();
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
        ? Engine::Os::Files::PathForMessage(session_.OutputPath()->filename())
        : "Untitled UI";
    ImGui::Text("%s%s", title.c_str(), session_.IsDirty() ? " *" : "");
    ImGui::EndMainMenuBar();
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

void EditorApp::CopySelectedElement() {
    if (auto copied = CopyElement(session_.Document(), selectedNode_)) {
        clipboard_ = std::move(*copied);
        statusMessage_ = "Copied " + selectedNode_;
    }
}

void EditorApp::PasteClipboardElement() {
    if (!clipboard_.has_value()) return;
    if (auto pasted = PasteElement(session_.EditDocument(), selectedCanvas_, selectedNode_, *clipboard_)) {
        selectedNode_ = std::move(*pasted);
        session_.CommitEdit("Paste element");
        RefreshDiagnostics();
    }
}

void EditorApp::DuplicateSelectedElement() {
    auto copied = CopyElement(session_.Document(), selectedNode_);
    if (!copied) return;
    if (auto pasted = PasteElement(session_.EditDocument(), selectedCanvas_, selectedNode_, std::move(*copied))) {
        selectedNode_ = std::move(*pasted);
        session_.CommitEdit("Duplicate element");
        RefreshDiagnostics();
    }
}

void EditorApp::NudgeSelectedElement(const bool canvasFocused) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool held = ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_RightArrow) ||
        ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow);
    if (nudgeTransactionActive_ && !held) {
        // One undo step per continuous press, however long the key repeats.
        session_.EndEdit();
        nudgeTransactionActive_ = false;
    }
    if (!canvasFocused || selectedNode_.empty() || io.WantTextInput || io.KeyCtrl || gizmoTransactionActive_) {
        return;
    }
    const float step = io.KeyShift ? 10.0F : 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) x -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) x += step;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) y -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) y += step;
    if (x == 0.0F && y == 0.0F) return;
    if (!nudgeTransactionActive_) {
        session_.BeginEdit("Nudge element");
        nudgeTransactionActive_ = true;
    }
    if (Json* node = FindSelectedNode(session_.EditDocument(), selectedNode_);
        node != nullptr && node->contains("rect") && (*node)["rect"].contains("position")) {
        Json& position = (*node)["rect"]["position"];
        position = {position[0].get<float>() + x, position[1].get<float>() + y};
        session_.UpdateEdit();
        RefreshDiagnostics();
    }
}

void EditorApp::NewDocument() {
    session_.NewDocument();
    selectedCanvas_ = "main";
    selectedNode_.clear();
    statusMessage_ = "Created a new unsaved UI document";
    RefreshDiagnostics();
}

void EditorApp::OpenDocument(const std::string& path) {
    auto native = Engine::Os::Files::PathFromUtf8(path);
    if (!native) {
        statusMessage_ = Engine::Base::Describe(native.error());
        return;
    }
    const auto result = session_.Open(*native);
    diagnostics_ = result ? result->diagnostics : result.error().diagnostics;
    statusMessage_ = result ? "Opened " + path : Engine::Base::Describe(result.error());
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
    const auto result = session_.Save(
        catalog_.IsMounted() ? &catalog_ : nullptr,
        overwriteExternal);
    diagnostics_ = result ? result->diagnostics : result.error().diagnostics;
    if (result) {
        statusMessage_ = "Saved canonical UI JSON";
    } else if (result.error().code == SessionErrorCode::MissingOutputPath) {
        RequestPath(PathDialogPurpose::ExportDocument, exportPath_, showExportDialog_);
    } else if (result.error().code == SessionErrorCode::ExternalModification) {
        // Saving again takes the session's own output path; the dialog needs no copy of it.
        pendingOverwritePath_.clear();
        pendingOverwriteIsExport_ = false;
        showOverwriteDialog_ = true;
    } else {
        statusMessage_ = Engine::Base::Describe(result.error());
    }
}

void EditorApp::ExportDocument(
    const std::string& path,
    const bool overwriteExternal) {
    auto native = Engine::Os::Files::PathFromUtf8(path);
    if (!native) {
        statusMessage_ = Engine::Base::Describe(native.error());
        return;
    }
    const auto result = session_.Export(
        *native,
        catalog_.IsMounted() ? &catalog_ : nullptr,
        overwriteExternal);
    diagnostics_ = result ? result->diagnostics : result.error().diagnostics;
    if (result) {
        statusMessage_ = "Exported canonical UI JSON to " + path;
        CopyPath(exportPath_, *session_.OutputPath());
    } else if (result.error().code == SessionErrorCode::ExternalModification) {
        pendingOverwritePath_ = path;
        pendingOverwriteIsExport_ = true;
        showOverwriteDialog_ = true;
    } else {
        statusMessage_ = Engine::Base::Describe(result.error());
    }
}

Result<void, CatalogError> EditorApp::MountCatalog(const std::filesystem::path& catalogPath,
    const std::filesystem::path& assetRoot) {
    Framework::ReadOnlyAssetCatalog candidate;
    auto root = assetRoot;
    if (root.empty() && !catalogPath.empty()) {
        root = catalogPath.parent_path();
        if (root.empty()) root = ".";
    }
    const auto loaded = catalogPath.empty()
        ? candidate.MountRoot(root)
        : candidate.Mount(catalogPath, root);
    if (!loaded) return loaded;
    previewAssets_.Mount(candidate);
    catalog_ = std::move(candidate);
    return {};
}

} // namespace Gyo::Tools::UiEditor
