#pragma once

#include "gyo/ui_editor/Diagnostic.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// The UI editor's Runtime Error types (docs/architecture/error-handling.md).
// Codes exist only for distinctions a caller branches on or a test asserts;
// the cause from an engine module is kept in detail (Base::CauseDetail).
// Zero is not a valid code; the numeric values are not a data contract.

namespace Gyo::Tools::UiEditor {

template <class T, class E>
using Result = Engine::Base::Result<T, E>;

// File system access on user paths.
enum class FileErrorCode : std::uint8_t {
    ReadFailed = 1,
    WriteFailed,
    ProbeFailed,
};

[[nodiscard]] constexpr const char* ToString(const FileErrorCode code) noexcept {
    switch (code) {
    case FileErrorCode::ReadFailed: return "ReadFailed";
    case FileErrorCode::WriteFailed: return "WriteFailed";
    case FileErrorCode::ProbeFailed: return "ProbeFailed";
    }
    return "Unknown";
}

using FileError = Engine::Base::Error<FileErrorCode>;
static_assert(Engine::Base::CodedError<FileError>);

// Command-line arguments (exit code 2).
enum class CommandLineErrorCode : std::uint8_t {
    InvalidArguments = 1,
};

[[nodiscard]] constexpr const char* ToString(const CommandLineErrorCode code) noexcept {
    return code == CommandLineErrorCode::InvalidArguments ? "InvalidArguments" : "Unknown";
}

using CommandLineError = Engine::Base::Error<CommandLineErrorCode>;
static_assert(Engine::Base::CodedError<CommandLineError>);

// Mounting a read-only asset catalog or content root.
enum class CatalogErrorCode : std::uint8_t {
    LoadFailed = 1,
};

[[nodiscard]] constexpr const char* ToString(const CatalogErrorCode code) noexcept {
    return code == CatalogErrorCode::LoadFailed ? "LoadFailed" : "Unknown";
}

using CatalogError = Engine::Base::Error<CatalogErrorCode>;
static_assert(Engine::Base::CodedError<CatalogError>);

// The authoring asset preview (text rasterizer, mounted catalog, textures).
enum class PreviewErrorCode : std::uint8_t {
    InitializationFailed = 1,
    MountFailed,
    AssetFailed,
};

[[nodiscard]] constexpr const char* ToString(const PreviewErrorCode code) noexcept {
    switch (code) {
    case PreviewErrorCode::InitializationFailed: return "InitializationFailed";
    case PreviewErrorCode::MountFailed: return "MountFailed";
    case PreviewErrorCode::AssetFailed: return "AssetFailed";
    }
    return "Unknown";
}

using PreviewError = Engine::Base::Error<PreviewErrorCode>;
static_assert(Engine::Base::CodedError<PreviewError>);

// Opening, saving or exporting a document. The editor and the CLI branch on
// these codes (Main.cpp maps InvalidDocument to exit code 4).
enum class SessionErrorCode : std::uint8_t {
    InvalidDocument = 1,
    MissingOutputPath,
    OutputInsideMountedAssetRoot,
    ExternalModification,
    IoFailure,
};

[[nodiscard]] constexpr const char* ToString(const SessionErrorCode code) noexcept {
    switch (code) {
    case SessionErrorCode::InvalidDocument: return "InvalidDocument";
    case SessionErrorCode::MissingOutputPath: return "MissingOutputPath";
    case SessionErrorCode::OutputInsideMountedAssetRoot: return "OutputInsideMountedAssetRoot";
    case SessionErrorCode::ExternalModification: return "ExternalModification";
    case SessionErrorCode::IoFailure: return "IoFailure";
    }
    return "Unknown";
}

// A session failure also carries the document diagnostics found before it.
struct SessionError final {
    SessionError(const SessionErrorCode errorCode, std::string errorMessage,
                 std::vector<Diagnostic> errorDiagnostics = {}, std::string errorDetail = {})
        : code(errorCode),
          message(std::move(errorMessage)),
          detail(std::move(errorDetail)),
          diagnostics(std::move(errorDiagnostics)) {}

    SessionErrorCode code;
    std::string message;
    std::string detail;
    std::vector<Diagnostic> diagnostics;
};
static_assert(Engine::Base::CodedError<SessionError>);

// A successful session operation and the diagnostics it found (warnings).
struct SessionReport final {
    std::vector<Diagnostic> diagnostics;
};

// Starting the interactive editor: mounting the catalog, opening the input
// document or preparing the preview. The cause is in detail.
enum class EditorErrorCode : std::uint8_t {
    CatalogUnavailable = 1,
    DocumentUnavailable,
};

[[nodiscard]] constexpr const char* ToString(const EditorErrorCode code) noexcept {
    switch (code) {
    case EditorErrorCode::CatalogUnavailable: return "CatalogUnavailable";
    case EditorErrorCode::DocumentUnavailable: return "DocumentUnavailable";
    }
    return "Unknown";
}

using EditorError = Engine::Base::Error<EditorErrorCode>;
static_assert(Engine::Base::CodedError<EditorError>);

} // namespace Gyo::Tools::UiEditor
