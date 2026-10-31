#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <cstdint>

// The framework's tool part: Runtime Error types (docs/architecture/error-handling.md).
// Codes exist only for distinctions a caller branches on or a test asserts;
// the cause from an engine module is kept in detail (Base::CauseDetail).
// Zero is not a valid code; the numeric values are not a data contract.

namespace Gyo::Tools::Framework {

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

// Mounting a catalog or content root, and writing a catalog entry.
enum class CatalogErrorCode : std::uint8_t {
    LoadFailed = 1,      // a catalog or content root could not be mounted
    InvalidCatalog,      // the text to edit is not a catalog v1
    InvalidRegistration, // the id, type or path cannot be registered in that catalog
};

[[nodiscard]] constexpr const char* ToString(const CatalogErrorCode code) noexcept {
    switch (code) {
    case CatalogErrorCode::LoadFailed: return "LoadFailed";
    case CatalogErrorCode::InvalidCatalog: return "InvalidCatalog";
    case CatalogErrorCode::InvalidRegistration: return "InvalidRegistration";
    }
    return "Unknown";
}

using CatalogError = Engine::Base::Error<CatalogErrorCode>;
static_assert(Engine::Base::CodedError<CatalogError>);

} // namespace Gyo::Tools::Framework
