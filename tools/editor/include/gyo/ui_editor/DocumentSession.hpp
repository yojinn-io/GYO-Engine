#pragma once

#include "gyo/ui_editor/Diagnostic.hpp"
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

enum class SaveFailure {
    None,
    InvalidDocument,
    MissingOutputPath,
    OutputInsideMountedAssetRoot,
    ExternalModification,
    IoFailure,
};

struct SessionResult final {
    bool succeeded{};
    std::string error;
    std::vector<Diagnostic> diagnostics;
    SaveFailure saveFailure{SaveFailure::None};

    [[nodiscard]] explicit operator bool() const noexcept { return succeeded; }
};

class DocumentSession final {
public:
    DocumentSession();

    void NewDocument();
    [[nodiscard]] SessionResult Open(const std::filesystem::path& path);

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
    [[nodiscard]] bool Undo();
    [[nodiscard]] bool Redo();

    [[nodiscard]] std::vector<Diagnostic> Validate(
        const ReadOnlyAssetCatalog* catalog) const;
    [[nodiscard]] SessionResult Save(
        const ReadOnlyAssetCatalog* catalog,
        bool overwriteExternalModification = false);
    [[nodiscard]] SessionResult Export(
        const std::filesystem::path& path,
        const ReadOnlyAssetCatalog* catalog,
        bool overwriteExternalModification = false);

    [[nodiscard]] bool HasExternalModification(std::string& error) const;

private:
    [[nodiscard]] bool Restore(std::string_view canonicalJson);
    void ResetHistory(bool saved);
    [[nodiscard]] SessionResult SaveTo(
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
