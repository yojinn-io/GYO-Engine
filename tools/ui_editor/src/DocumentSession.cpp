#include "gyo/ui_editor/DocumentSession.hpp"

#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <iterator>
#include <utility>

namespace Gyo::Tools::UiEditor {

DocumentSession::DocumentSession() {
    NewDocument();
}

void DocumentSession::NewDocument() {
    document_ = UiDocumentBridge::CreateDefault();
    inputPath_.reset();
    outputPath_.reset();
    observedPath_.reset();
    observedStamp_.reset();
    ResetHistory(false);
}

SessionResult DocumentSession::Open(const std::filesystem::path& path) {
    const TextFileResult file = ReadTextFile(path);
    if (!file) {
        return {false, file.error, {}, SaveFailure::IoFailure};
    }
    DocumentParseResult parsed = UiDocumentBridge::Parse(*file.text);
    if (!parsed) {
        return {
            false,
            "UI document failed GYO::Ui codec validation",
            std::move(parsed.diagnostics),
            SaveFailure::InvalidDocument,
        };
    }

    std::string stampError;
    std::optional<FileStamp> stamp = ProbeFileStamp(path, stampError);
    if (!stamp.has_value()) {
        return {false, std::move(stampError), {}, SaveFailure::IoFailure};
    }

    document_ = std::move(*parsed.document);
    inputPath_ = std::filesystem::absolute(path).lexically_normal();
    outputPath_ = inputPath_;
    observedPath_ = inputPath_;
    observedStamp_ = stamp;
    ResetHistory(true);
    return {true, {}, UiDocumentBridge::Validate(document_), SaveFailure::None};
}

const nlohmann::json& DocumentSession::Document() const noexcept {
    return document_;
}

nlohmann::json& DocumentSession::EditDocument() noexcept {
    return document_;
}

const std::optional<std::filesystem::path>& DocumentSession::InputPath() const noexcept {
    return inputPath_;
}

const std::optional<std::filesystem::path>& DocumentSession::OutputPath() const noexcept {
    return outputPath_;
}

void DocumentSession::SetOutputPath(std::optional<std::filesystem::path> path) {
    if (path.has_value()) {
        *path = std::filesystem::absolute(*path).lexically_normal();
    }
    outputPath_ = std::move(path);
}

bool DocumentSession::IsDirty() const noexcept {
    return history_.IsDirty();
}

bool DocumentSession::CanUndo() const noexcept {
    return history_.CanUndo();
}

bool DocumentSession::CanRedo() const noexcept {
    return history_.CanRedo();
}

void DocumentSession::CommitEdit(std::string label) {
    history_.Apply(
        std::move(label), UiDocumentBridge::SerializeCanonical(document_));
}

void DocumentSession::BeginEdit(std::string label) {
    history_.BeginTransaction(std::move(label));
}

void DocumentSession::UpdateEdit() {
    history_.UpdateTransaction(UiDocumentBridge::SerializeCanonical(document_));
}

void DocumentSession::EndEdit() {
    history_.CommitTransaction();
}

void DocumentSession::CancelEdit() {
    history_.CancelTransaction();
    static_cast<void>(Restore(history_.Current()));
}

bool DocumentSession::Undo() {
    const std::string* state = history_.Undo();
    return state != nullptr && Restore(*state);
}

bool DocumentSession::Redo() {
    const std::string* state = history_.Redo();
    return state != nullptr && Restore(*state);
}

std::vector<Diagnostic> DocumentSession::Validate(
    const ReadOnlyAssetCatalog* catalog) const {
    std::vector<Diagnostic> diagnostics = UiDocumentBridge::Validate(document_);
    const std::vector<AssetReference> references =
        UiDocumentBridge::AssetReferences(document_);
    std::vector<Diagnostic> catalogDiagnostics =
        ValidateCatalogReferences(references, catalog);
    diagnostics.insert(
        diagnostics.end(),
        std::make_move_iterator(catalogDiagnostics.begin()),
        std::make_move_iterator(catalogDiagnostics.end()));
    return diagnostics;
}

SessionResult DocumentSession::Save(
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    if (!outputPath_.has_value()) {
        return {
            false,
            "no output path is selected; use Export As",
            {},
            SaveFailure::MissingOutputPath,
        };
    }
    return SaveTo(*outputPath_, catalog, overwriteExternalModification);
}

SessionResult DocumentSession::Export(
    const std::filesystem::path& path,
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    SetOutputPath(path);
    return SaveTo(*outputPath_, catalog, overwriteExternalModification);
}

bool DocumentSession::HasExternalModification(std::string& error) const {
    error.clear();
    if (!outputPath_.has_value() || !observedStamp_.has_value()) {
        return false;
    }
    if (!observedPath_.has_value() ||
        std::filesystem::absolute(*outputPath_).lexically_normal() !=
            *observedPath_) {
        return false;
    }
    std::optional<FileStamp> current = ProbeFileStamp(*outputPath_, error);
    if (!current.has_value()) {
        return false;
    }
    return *current != *observedStamp_;
}

bool DocumentSession::Restore(const std::string_view canonicalJson) {
    try {
        document_ = nlohmann::json::parse(
            canonicalJson.begin(), canonicalJson.end());
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

void DocumentSession::ResetHistory(const bool saved) {
    history_.Reset(UiDocumentBridge::SerializeCanonical(document_));
    if (saved) {
        history_.MarkSaved();
    } else {
        // An empty saved marker cannot equal a valid JSON document.
        history_.MarkUnsaved();
    }
}

SessionResult DocumentSession::SaveTo(
    const std::filesystem::path& path,
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    std::vector<Diagnostic> diagnostics = Validate(catalog);
    if (HasErrors(diagnostics)) {
        return {
            false,
            "document validation failed",
            std::move(diagnostics),
            SaveFailure::InvalidDocument,
        };
    }

    if (catalog != nullptr && catalog->IsMounted() &&
        IsPathWithin(path, catalog->AssetRoot())) {
        return {
            false,
            "export target is inside the mounted app asset root; export to a "
            "working location, then copy and register it manually",
            std::move(diagnostics),
            SaveFailure::OutputInsideMountedAssetRoot,
        };
    }

    const std::filesystem::path absolutePath =
        std::filesystem::absolute(path).lexically_normal();
    std::string externalError;
    bool externalModification = false;
    if (observedPath_.has_value() && absolutePath == *observedPath_) {
        externalModification = HasExternalModification(externalError);
    } else {
        std::error_code existenceError;
        externalModification = std::filesystem::exists(absolutePath, existenceError);
        if (existenceError) {
            externalError = "cannot inspect output path '" + absolutePath.string() +
                            "': " + existenceError.message();
        }
    }
    if (!overwriteExternalModification && externalModification) {
        return {
            false,
            "output changed outside the editor",
            std::move(diagnostics),
            SaveFailure::ExternalModification,
        };
    }
    if (!externalError.empty()) {
        return {
            false,
            std::move(externalError),
            std::move(diagnostics),
            SaveFailure::IoFailure,
        };
    }

    const std::string serialized = UiDocumentBridge::SerializeCanonical(document_);
    const FileOperationResult written = WriteTextFileAtomically(path, serialized);
    if (!written) {
        return {
            false,
            written.error,
            std::move(diagnostics),
            SaveFailure::IoFailure,
        };
    }

    outputPath_ = absolutePath;
    observedPath_ = absolutePath;
    std::string stampError;
    observedStamp_ = ProbeFileStamp(*outputPath_, stampError);
    if (!observedStamp_.has_value()) {
        return {
            false,
            std::move(stampError),
            std::move(diagnostics),
            SaveFailure::IoFailure,
        };
    }
    history_.MarkSaved();
    return {true, {}, std::move(diagnostics), SaveFailure::None};
}

} // namespace Gyo::Tools::UiEditor
