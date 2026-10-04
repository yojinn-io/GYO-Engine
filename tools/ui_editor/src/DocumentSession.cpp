#include "gyo/ui_editor/DocumentSession.hpp"

#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include "engine/base/Assert.hpp"

#include <iterator>
#include <system_error>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

[[nodiscard]] SessionError IoFailure(const FileError& cause, std::vector<Diagnostic> diagnostics = {}) {
    return SessionError(SessionErrorCode::IoFailure, cause.message, std::move(diagnostics),
                        Engine::Base::CauseDetail(cause));
}

} // namespace

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

Result<SessionReport, SessionError> DocumentSession::Open(const std::filesystem::path& path) {
    const auto file = ReadTextFile(path);
    if (!file) {
        return Engine::Base::Err(IoFailure(file.error()));
    }
    DocumentParseResult parsed = UiDocumentBridge::Parse(*file);
    if (!parsed) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::InvalidDocument,
            "UI document failed GYO::Ui codec validation",
            std::move(parsed.diagnostics)));
    }

    const auto stamp = ProbeFileStamp(path);
    if (!stamp) {
        return Engine::Base::Err(IoFailure(stamp.error()));
    }

    document_ = std::move(*parsed.document);
    inputPath_ = AbsolutePath(path);
    outputPath_ = inputPath_;
    observedPath_ = inputPath_;
    observedStamp_ = *stamp;
    ResetHistory(true);
    return SessionReport{UiDocumentBridge::Validate(document_)};
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
        *path = AbsolutePath(*path);
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
    Restore(history_.Current());
}

bool DocumentSession::Undo() {
    const std::string* state = history_.Undo();
    if (state == nullptr) return false;
    Restore(*state);
    return true;
}

bool DocumentSession::Redo() {
    const std::string* state = history_.Redo();
    if (state == nullptr) return false;
    Restore(*state);
    return true;
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

Result<SessionReport, SessionError> DocumentSession::Save(
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    if (!outputPath_.has_value()) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::MissingOutputPath,
            "no output path is selected; use Export As"));
    }
    return SaveTo(*outputPath_, catalog, overwriteExternalModification);
}

Result<SessionReport, SessionError> DocumentSession::Export(
    const std::filesystem::path& path,
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    SetOutputPath(path);
    return SaveTo(*outputPath_, catalog, overwriteExternalModification);
}

Result<bool, FileError> DocumentSession::HasExternalModification() const {
    if (!outputPath_.has_value() || !observedStamp_.has_value()) {
        return false;
    }
    if (!observedPath_.has_value() || AbsolutePath(*outputPath_) != *observedPath_) {
        return false;
    }
    const auto current = ProbeFileStamp(*outputPath_);
    if (!current) {
        return Engine::Base::Err(current.error());
    }
    return *current != *observedStamp_;
}

void DocumentSession::Restore(const std::string_view canonicalJson) {
    // Snapshots come from SerializeCanonical, so they always parse.
    document_ = nlohmann::json::parse(canonicalJson.begin(), canonicalJson.end(), nullptr, false);
    GYO_ASSERT(!document_.is_discarded());
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

Result<SessionReport, SessionError> DocumentSession::SaveTo(
    const std::filesystem::path& path,
    const ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    std::vector<Diagnostic> diagnostics = Validate(catalog);
    if (HasErrors(diagnostics)) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::InvalidDocument,
            "document validation failed",
            std::move(diagnostics)));
    }

    if (catalog != nullptr && catalog->IsMounted() &&
        IsPathWithin(path, catalog->AssetRoot())) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::OutputInsideMountedAssetRoot,
            "export target is inside the mounted app asset root; export to a "
            "working location, then copy and register it manually",
            std::move(diagnostics)));
    }

    const std::filesystem::path absolutePath = AbsolutePath(path);
    std::optional<FileError> externalError;
    bool externalModification = false;
    if (observedPath_.has_value() && absolutePath == *observedPath_) {
        auto modified = HasExternalModification();
        if (modified) {
            externalModification = *modified;
        } else {
            externalError = std::move(modified).error();
        }
    } else {
        std::error_code existenceError;
        externalModification = std::filesystem::exists(absolutePath, existenceError);
        if (existenceError) {
            externalError = FileError::Make(
                FileErrorCode::ProbeFailed,
                "cannot inspect output path '" + absolutePath.string() + "': " + existenceError.message(),
                absolutePath.string());
        }
    }
    if (!overwriteExternalModification && externalModification) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::ExternalModification,
            "output changed outside the editor",
            std::move(diagnostics)));
    }
    if (externalError) {
        return Engine::Base::Err(IoFailure(*externalError, std::move(diagnostics)));
    }

    const std::string serialized = UiDocumentBridge::SerializeCanonical(document_);
    const auto written = WriteTextFileAtomically(path, serialized);
    if (!written) {
        return Engine::Base::Err(IoFailure(written.error(), std::move(diagnostics)));
    }

    outputPath_ = absolutePath;
    observedPath_ = absolutePath;
    auto stamp = ProbeFileStamp(*outputPath_);
    if (!stamp) {
        observedStamp_.reset();
        return Engine::Base::Err(IoFailure(stamp.error(), std::move(diagnostics)));
    }
    observedStamp_ = *stamp;
    history_.MarkSaved();
    return SessionReport{std::move(diagnostics)};
}

} // namespace Gyo::Tools::UiEditor
