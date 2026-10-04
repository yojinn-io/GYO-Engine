#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace Engine::IO {

    // Failure classes of IO. Zero is not a valid code; the numeric values are
    // not a data contract.
    enum class IoErrorCode : std::uint16_t {
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

        // internal
        InternalError
    };

    [[nodiscard]] constexpr const char* ToString(const IoErrorCode c) noexcept {
        switch (c) {
        case IoErrorCode::InvalidPath:      return "InvalidPath";
        case IoErrorCode::PathEscapesRoot:  return "PathEscapesRoot";
        case IoErrorCode::NotFound:         return "NotFound";
        case IoErrorCode::AlreadyExists:    return "AlreadyExists";
        case IoErrorCode::PermissionDenied: return "PermissionDenied";
        case IoErrorCode::NotSupported:     return "NotSupported";
        case IoErrorCode::OpenFailed:       return "OpenFailed";
        case IoErrorCode::ReadFailed:       return "ReadFailed";
        case IoErrorCode::WriteFailed:      return "WriteFailed";
        case IoErrorCode::SeekFailed:       return "SeekFailed";
        case IoErrorCode::EndOfStream:      return "EndOfStream";
        case IoErrorCode::InternalError:    return "InternalError";
        }
        return "Unknown";
    }

    // IO's error type and its Result, declared once for every IO namespace.
    using IoError = Base::Error<IoErrorCode>;
    static_assert(Base::CodedError<IoError>);

    template <class T>
    using IoResult = Base::Result<T, IoError>;

} // namespace Engine::IO
