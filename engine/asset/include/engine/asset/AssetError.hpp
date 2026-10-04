#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace Engine::Asset {

    // Failure classes of the asset system. Zero is not a valid code; the
    // numeric values are not a data contract (nothing persists or sends them).
    enum class AssetErrorCode : std::uint16_t {
        // catalog / resolve
        CatalogNotFound = 1,
        InvalidCatalogEntry,
        InvalidPath,
        PathEscapesRoot,

        // source (I/O)
        SourceNotFound,
        SourceReadFailed,

        // decode / format
        UnsupportedType,
        UnsupportedFormat,
        DecodeFailed,
        ParseFailed,

        // internal
        InternalError,

        // request admission
        RequestInProgress,
        GenerationExhausted
    };

    [[nodiscard]] constexpr const char* ToString(const AssetErrorCode code) noexcept {
        switch (code) {
        case AssetErrorCode::CatalogNotFound:     return "CatalogNotFound";
        case AssetErrorCode::InvalidCatalogEntry: return "InvalidCatalogEntry";
        case AssetErrorCode::InvalidPath:         return "InvalidPath";
        case AssetErrorCode::PathEscapesRoot:     return "PathEscapesRoot";
        case AssetErrorCode::SourceNotFound:      return "SourceNotFound";
        case AssetErrorCode::SourceReadFailed:    return "SourceReadFailed";
        case AssetErrorCode::UnsupportedType:     return "UnsupportedType";
        case AssetErrorCode::UnsupportedFormat:   return "UnsupportedFormat";
        case AssetErrorCode::DecodeFailed:        return "DecodeFailed";
        case AssetErrorCode::ParseFailed:         return "ParseFailed";
        case AssetErrorCode::InternalError:       return "InternalError";
        case AssetErrorCode::RequestInProgress:   return "RequestInProgress";
        case AssetErrorCode::GenerationExhausted: return "GenerationExhausted";
        }
        return "Unknown";
    }

    // The asset system's error type: code, message for people, optional detail
    // (for example the path).
    using AssetError = Base::Error<AssetErrorCode>;
    static_assert(Base::CodedError<AssetError>);

} // namespace Engine::Asset
