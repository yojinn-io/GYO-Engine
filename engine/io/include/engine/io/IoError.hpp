#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "engine/base/Error.hpp" // Engine::Error（共通Error）

namespace Engine::IO {

    // IO エラーコード（機械判定用）
    enum class IoErrorCode : std::uint16_t {
        None = 0,

        // path / resolve
        InvalidPath,
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

    inline constexpr const char* ToString(IoErrorCode c) noexcept {
        switch (c) {
        case IoErrorCode::None:             return "None";
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
        default:                            return "Unknown";
        }
    }

} // namespace Engine::IO
