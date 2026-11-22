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

namespace GYO::Engine::OS {

enum class OSErrorCode : std::uint16_t {
    NotFound = 1,
    AccessDenied,
    InvalidPath,
    // A path or text argument is not valid UTF-8.
    InvalidEncoding,
    TooLarge,
    IOFailed,
    Unsupported,
};

[[nodiscard]] constexpr const char* ToString(const OSErrorCode code) noexcept {
    switch (code) {
    case OSErrorCode::NotFound:        return "NotFound";
    case OSErrorCode::AccessDenied:    return "AccessDenied";
    case OSErrorCode::InvalidPath:     return "InvalidPath";
    case OSErrorCode::InvalidEncoding: return "InvalidEncoding";
    case OSErrorCode::TooLarge:        return "TooLarge";
    case OSErrorCode::IOFailed:        return "IOFailed";
    case OSErrorCode::Unsupported:     return "Unsupported";
    }
    return "Unknown";
}

using OSError = Base::Error<OSErrorCode>;
static_assert(Base::CodedError<OSError>);

// Where a native error code comes from: errno (POSIX and the C runtime) or a
// Win32 error (GetLastError).
enum class NativeErrorDomain : std::uint8_t { Errno = 1, Win32 };

struct NativeError final {
    NativeErrorDomain domain;
    std::int64_t value;
};

// The detail of an OSError: "errno=<n>" or "win32=<n>", followed by
// "; path_bytes=<n>" when the failure concerns a path (its UTF-8 length).
[[nodiscard]] std::string NativeErrorDetail(NativeError native, std::optional<std::size_t> pathBytes = std::nullopt);

[[nodiscard]] OSError MakeOSError(OSErrorCode code, std::string message, NativeError native,
                                  std::optional<std::size_t> pathBytes = std::nullopt);

} // namespace GYO::Engine::OS
