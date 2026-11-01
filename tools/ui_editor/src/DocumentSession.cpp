#include "gyo/ui_editor/DocumentSession.hpp"

#include "gyo/framework/AbsolutePath.hpp"
#include "gyo/framework/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include "engine/base/Assert.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"
#include "engine/os/Files.hpp"

#include <cstddef>
#include <iterator>
#include <utility>

// Files go through engine/io (docs/architecture/io.md, "User files"); only
// AbsolutePath, the path the session remembers, is the framework's.

namespace Gyo::Tools::UiEditor {
namespace {

namespace Io = Engine::IO;

// A UI document is far smaller; a larger file is refused before it is read whole.
constexpr std::size_t kMaxDocumentBytes = 64U * 1024U * 1024U;

// The path in UTF-8 for messages; engine paths are UTF-8, not path::string()'s ANSI code page.
[[nodiscard]] std::string Text(const std::filesystem::path& path) {
    auto text = Engine::Os::Files::PathToUtf8(path);
    return text ? std::move(text.value()) : std::string("<path with no UTF-8 form>");
}

[[nodiscard]] FileError Failure(const FileErrorCode code, std::string message, const Io::IoError& cause) {
    return FileError::Make(code, std::move(message), Engine::Base::CauseDetail(cause));
}

[[nodiscard]] Io::IoResult<Io::Path::Uri> NativeUri(const std::filesystem::path& path) {
    auto text = Engine::Os::Files::PathToUtf8(path);
    if (!text) {
        return Engine::Base::Err(Io::IoError::Make(
            Io::IoErrorCode::InvalidPath, text.error().message, Engine::Base::CauseDetail(text.error())));
    }
    return Io::FS::NativeFileUri(text.value());
}

[[nodiscard]] Io::Helpers::ReadAllOptions DocumentRead() noexcept {
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = kMaxDocumentBytes;
    return read;
}

[[nodiscard]] Result<std::string, FileError> ReadText(const std::filesystem::path& path) {
    Io::FS::NativeFileSystem files;
    auto uri = NativeUri(path);
    auto bytes = uri ? Io::Helpers::ReadAllBytes(files, uri.value(), DocumentRead())
                     : Io::IoResult<std::vector<std::byte>>(Engine::Base::Err(uri.error()));
    if (!bytes) {
        return Engine::Base::Err(Failure(FileErrorCode::ReadFailed, "cannot read file: " + Text(path), bytes.error()));
    }
    return std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
}

// A missing file is a stamp with exists == false.
[[nodiscard]] Result<Io::Helpers::FileStamp, FileError> ProbeStamp(const std::filesystem::path& path) {
    Io::FS::NativeFileSystem files;
    auto uri = NativeUri(path);
    auto stamp = uri ? Io::Helpers::ProbeFileStamp(files, uri.value(), DocumentRead())
                     : Io::IoResult<Io::Helpers::FileStamp>(Engine::Base::Err(uri.error()));
    if (!stamp) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         stamp.error()));
    }
    return stamp.value();
}

// Whether anything (a file, a directory) exists at the path.
[[nodiscard]] Result<bool, FileError> Exists(const std::filesystem::path& path) {
    Io::FS::NativeFileSystem files;
    auto uri = NativeUri(path);
    auto exists = uri ? files.Exists(uri.value()) : Io::IoResult<bool>(Engine::Base::Err(uri.error()));
    if (!exists) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         exists.error()));
    }
    return exists.value();
}

// Creates the missing folders above the file, as an export to a new folder needs.
[[nodiscard]] Result<void, FileError> WriteText(const std::filesystem::path& path, const std::string_view text) {
    Io::FS::NativeFileSystem files;
    auto uri = NativeUri(path);
    const Engine::Base::ConstSpan<std::byte> bytes(reinterpret_cast<const std::byte*>(text.data()), text.size());
    auto written = uri ? Io::Helpers::WriteFileAtomically(files, uri.value(), bytes, {.createParentDirectories = true})
                       : Io::IoResult<void>(Engine::Base::Err(uri.error()));
    if (!written) {
        return Engine::Base::Err(Failure(FileErrorCode::WriteFailed, "cannot write output '" + Text(path) + "'",
                                         written.error()));
    }
    return {};
}

// Whether a file written at path lands in root, as the OS places it.
[[nodiscard]] Result<bool, FileError> IsWithin(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto candidate = Engine::Os::Files::PathToUtf8(path);
    auto rootText = Engine::Os::Files::PathToUtf8(root);
    if (!candidate || !rootText) {
        return Engine::Base::Err(FileError::Make(FileErrorCode::ProbeFailed,
            "cannot compare '" + Text(path) + "' with the asset root", Text(root)));
    }
    auto within = Io::FS::IsNativePathWithin(candidate.value(), rootText.value());
    if (!within) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed,
            "cannot compare '" + Text(path) + "' with the asset root", within.error()));
    }
    return within.value();
}

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
    const auto file = ReadText(path);
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

    const auto stamp = ProbeStamp(path);
    if (!stamp) {
        return Engine::Base::Err(IoFailure(stamp.error()));
    }

    document_ = std::move(*parsed.document);
    inputPath_ = Framework::AbsolutePath(path);
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
        *path = Framework::AbsolutePath(*path);
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
    const Framework::ReadOnlyAssetCatalog* catalog) const {
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
    const Framework::ReadOnlyAssetCatalog* catalog,
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
    const Framework::ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    SetOutputPath(path);
    return SaveTo(*outputPath_, catalog, overwriteExternalModification);
}

Result<bool, FileError> DocumentSession::HasExternalModification() const {
    if (!outputPath_.has_value() || !observedStamp_.has_value()) {
        return false;
    }
    if (!observedPath_.has_value() || Framework::AbsolutePath(*outputPath_) != *observedPath_) {
        return false;
    }
    const auto current = ProbeStamp(*outputPath_);
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
    const Framework::ReadOnlyAssetCatalog* catalog,
    const bool overwriteExternalModification) {
    std::vector<Diagnostic> diagnostics = Validate(catalog);
    if (HasErrors(diagnostics)) {
        return Engine::Base::Err(SessionError(
            SessionErrorCode::InvalidDocument,
            "document validation failed",
            std::move(diagnostics)));
    }

    if (catalog != nullptr && catalog->IsMounted()) {
        auto within = IsWithin(path, catalog->AssetRoot());
        if (!within) {
            return Engine::Base::Err(IoFailure(within.error(), std::move(diagnostics)));
        }
        if (*within) {
            return Engine::Base::Err(SessionError(
                SessionErrorCode::OutputInsideMountedAssetRoot,
                "export target is inside the mounted app asset root; export to a "
                "working location, then copy and register it manually",
                std::move(diagnostics)));
        }
    }

    const std::filesystem::path absolutePath = Framework::AbsolutePath(path);
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
        auto exists = Exists(absolutePath);
        if (exists) {
            externalModification = exists.value();
        } else {
            externalError = std::move(exists).error();
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
    const auto written = WriteText(path, serialized);
    if (!written) {
        return Engine::Base::Err(IoFailure(written.error(), std::move(diagnostics)));
    }

    outputPath_ = absolutePath;
    observedPath_ = absolutePath;
    auto stamp = ProbeStamp(*outputPath_);
    if (!stamp) {
        observedStamp_.reset();
        return Engine::Base::Err(IoFailure(stamp.error(), std::move(diagnostics)));
    }
    observedStamp_ = *stamp;
    history_.MarkSaved();
    return SessionReport{std::move(diagnostics)};
}

} // namespace Gyo::Tools::UiEditor
