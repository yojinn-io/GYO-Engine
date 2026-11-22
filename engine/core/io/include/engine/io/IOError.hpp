#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace GYO::Engine::IO {

    // Failure classes of IO. Zero is not a valid code; the numeric values are
    // not a data contract.
    enum class IOErrorCode : std::uint16_t {
        // path / resolve
        InvalidPath = 1,
        PathEscapesRoot,

        // fs
        NotFound,
        AlreadyExists,
        PermissionDenied,
        NotSupported,

        // stream / io
        OpenFailed,
        ReadFailed,
        WriteFailed,
        SeekFailed,
        EndOfStream,
        // A whole-file read met more bytes than its caller's limit.
        TooLarge,

        // internal
        InternalError
    };

    [[nodiscard]] constexpr const char* ToString(const IOErrorCode c) noexcept {
        switch (c) {
        case IOErrorCode::InvalidPath:      return "InvalidPath";
        case IOErrorCode::PathEscapesRoot:  return "PathEscapesRoot";
        case IOErrorCode::NotFound:         return "NotFound";
        case IOErrorCode::AlreadyExists:    return "AlreadyExists";
        case IOErrorCode::PermissionDenied: return "PermissionDenied";
        case IOErrorCode::NotSupported:     return "NotSupported";
        case IOErrorCode::OpenFailed:       return "OpenFailed";
        case IOErrorCode::ReadFailed:       return "ReadFailed";
        case IOErrorCode::WriteFailed:      return "WriteFailed";
        case IOErrorCode::SeekFailed:       return "SeekFailed";
        case IOErrorCode::EndOfStream:      return "EndOfStream";
        case IOErrorCode::TooLarge:         return "TooLarge";
        case IOErrorCode::InternalError:    return "InternalError";
        }
        return "Unknown";
    }

    // IO's error type and its Result, declared once for every IO namespace.
    using IOError = Base::Error<IOErrorCode>;
    static_assert(Base::CodedError<IOError>);

    template <class T>
    using IOResult = Base::Result<T, IOError>;

} // namespace GYO::Engine::IO
