#pragma once

#include "engine/base/Error.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The error of the OS layer (docs/architecture/os.md). Every failure keeps the
// native error it came from in its detail, in one fixed form, so a caller that
// converts it with Base::CauseDetail and a log that prints it carry the same
// text on every platform.

namespace Engine::Os {

enum class OsErrorCode : std::uint16_t {
    NotFound = 1,
    AccessDenied,
    InvalidPath,
    // A path or text argument is not valid UTF-8.
    InvalidEncoding,
    TooLarge,
    IoFailed,
    Unsupported,
};

[[nodiscard]] constexpr const char* ToString(const OsErrorCode code) noexcept {
    switch (code) {
    case OsErrorCode::NotFound:        return "NotFound";
    case OsErrorCode::AccessDenied:    return "AccessDenied";
    case OsErrorCode::InvalidPath:     return "InvalidPath";
    case OsErrorCode::InvalidEncoding: return "InvalidEncoding";
    case OsErrorCode::TooLarge:        return "TooLarge";
    case OsErrorCode::IoFailed:        return "IoFailed";
    case OsErrorCode::Unsupported:     return "Unsupported";
    }
    return "Unknown";
}

using OsError = Base::Error<OsErrorCode>;
static_assert(Base::CodedError<OsError>);

// Where a native error code comes from: errno (POSIX and the C runtime) or a
// Win32 error (GetLastError).
enum class NativeErrorDomain : std::uint8_t { Errno = 1, Win32 };

struct NativeError final {
    NativeErrorDomain domain;
    std::int64_t value;
};

// The detail of an OsError: "errno=<n>" or "win32=<n>", followed by
// "; path_bytes=<n>" when the failure concerns a path (its UTF-8 length).
[[nodiscard]] std::string NativeErrorDetail(NativeError native, std::optional<std::size_t> pathBytes = std::nullopt);

[[nodiscard]] OsError MakeOsError(OsErrorCode code, std::string message, NativeError native,
                                  std::optional<std::size_t> pathBytes = std::nullopt);

} // namespace Engine::Os
