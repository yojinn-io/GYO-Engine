#pragma once

#include "gyo/ui_editor/Diagnostic.hpp"
#include "gyo/ui_editor/EditorError.hpp"
#include "gyo/ui_editor/FileService.hpp"
#include "gyo/ui_editor/UndoStack.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

class ReadOnlyAssetCatalog;

class DocumentSession final {
public:
    DocumentSession();

    void NewDocument();
    [[nodiscard]] Result<SessionReport, SessionError> Open(const std::filesystem::path& path);

    [[nodiscard]] const nlohmann::json& Document() const noexcept;
    [[nodiscard]] nlohmann::json& EditDocument() noexcept;
    [[nodiscard]] const std::optional<std::filesystem::path>& InputPath() const noexcept;
    [[nodiscard]] const std::optional<std::filesystem::path>& OutputPath() const noexcept;
    void SetOutputPath(std::optional<std::filesystem::path> path);

    [[nodiscard]] bool IsDirty() const noexcept;
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;

    void CommitEdit(std::string label);
    void BeginEdit(std::string label);
    void UpdateEdit();
    void EndEdit();
    void CancelEdit();
    // False when there is nothing to undo or redo.
    [[nodiscard]] bool Undo();
    [[nodiscard]] bool Redo();

    [[nodiscard]] std::vector<Diagnostic> Validate(
        const ReadOnlyAssetCatalog* catalog) const;
    [[nodiscard]] Result<SessionReport, SessionError> Save(
        const ReadOnlyAssetCatalog* catalog,
        bool overwriteExternalModification = false);
    [[nodiscard]] Result<SessionReport, SessionError> Export(
        const std::filesystem::path& path,
        const ReadOnlyAssetCatalog* catalog,
        bool overwriteExternalModification = false);

    // Whether the observed output file changed since it was opened or saved.
    [[nodiscard]] Result<bool, FileError> HasExternalModification() const;

private:
    void Restore(std::string_view canonicalJson);
    void ResetHistory(bool saved);
    [[nodiscard]] Result<SessionReport, SessionError> SaveTo(
        const std::filesystem::path& path,
        const ReadOnlyAssetCatalog* catalog,
        bool overwriteExternalModification);

    nlohmann::json document_;
    UndoStack history_;
    std::optional<std::filesystem::path> inputPath_;
    std::optional<std::filesystem::path> outputPath_;
    std::optional<std::filesystem::path> observedPath_;
    std::optional<FileStamp> observedStamp_;
};

} // namespace Gyo::Tools::UiEditor
