#pragma once

#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/Diagnostic.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/PreviewAdapter.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"

#include <array>
#include <string>
#include <vector>

namespace Gyo::Tools::UiEditor {

class AssetPreviewContext;

class EditorApp final {
public:
    EditorApp(CommandLineOptions options, AssetPreviewContext& previewAssets);

    [[nodiscard]] bool Initialize(std::string& error);
    void Draw();
    [[nodiscard]] bool WantsExit() const noexcept;

private:
    enum class AssetPickerTarget {
        FontAlias,
        ImageTexture,
    };

    enum class RenameTarget {
        FontAlias,
        NamedColor,
    };

    void RefreshDiagnostics();
    void DrawMainMenu();
    void DrawHierarchy();
    void DrawCanvas();
    void DrawInspector();
    void DrawBottomPanel();
    void DrawDialogs();
    void DrawStatusBar();

    void NewDocument();
    void OpenDocument(const std::string& path);
    void SaveDocument(bool overwriteExternal = false);
    void ExportDocument(const std::string& path, bool overwriteExternal = false);
    void MountCatalog(const std::string& catalogPath, const std::string& assetRoot);

    CommandLineOptions options_;
    AssetPreviewContext& previewAssets_;
    DocumentSession session_;
    ReadOnlyAssetCatalog catalog_;
    PreviewAdapter preview_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::string> actionLog_;
    std::string selectedCanvas_{"main"};
    std::string selectedNode_;
    std::string statusMessage_;
    std::string pendingOverwritePath_;
    std::string pendingCatalogPath_;
    std::string pendingAssetRoot_;
    bool pendingOverwriteIsExport_{};
    bool pendingCatalogMount_{};
    bool previewMode_{};
    bool gizmoTransactionActive_{};
    bool showGrid_{true};
    bool snapToGrid_{true};
    float canvasZoom_{1.0F};
    float canvasPanX_{};
    float canvasPanY_{};
    float gridSize_{8.0F};
    bool wantsExit_{};
    bool showOpenDialog_{};
    bool showExportDialog_{};
    bool showCatalogDialog_{};
    bool showOverwriteDialog_{};
    bool showAssetPicker_{};
    bool showRenameDialog_{};
    AssetPickerTarget assetPickerTarget_{AssetPickerTarget::FontAlias};
    RenameTarget renameTarget_{RenameTarget::FontAlias};
    std::string assetPickerFontAlias_{"default"};
    std::string renameOldValue_;
    std::array<char, 1024> openPath_{};
    std::array<char, 1024> exportPath_{};
    std::array<char, 1024> catalogPath_{};
    std::array<char, 1024> assetRoot_{};
    std::array<char, 128> renameValue_{};
};

} // namespace Gyo::Tools::UiEditor
