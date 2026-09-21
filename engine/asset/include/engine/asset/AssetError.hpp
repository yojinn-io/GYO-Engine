#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace Engine::Asset {

    // AssetErrorCode：失敗の分類（ログ・統計・リトライ方針に使える）
    enum class AssetErrorCode : std::uint16_t {
        None = 0,

        // catalog / resolve
        CatalogNotFound,
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
        InternalError
    };

    // AssetError：AssetRecord に保持する失敗理由
    // - code：機械的に扱える分類
    // - message：人間向け（ログ/デバッグ）
    // - detail：任意補助情報（path/typeなどを埋める用途）
    /**
    struct AssetError final {
        AssetErrorCode code = AssetErrorCode::None;
        std::string message;
        std::string detail;

        constexpr bool ok() const noexcept { return code == AssetErrorCode::None; }
        explicit constexpr operator bool() const noexcept { return !ok(); }

        static AssetError None() { return {}; }

        static AssetError Make(AssetErrorCode c, std::string msg, std::string det = {}) {
            AssetError e;
            e.code = c;
            e.message = std::move(msg);
            e.detail = std::move(det);
            return e;
        }

        void Clear() {
            code = AssetErrorCode::None;
            message.clear();
            detail.clear();
        }
    };
    **/

    inline constexpr const char* ToString(AssetErrorCode c) noexcept {
        switch (c) {
        case AssetErrorCode::None:               return "None";
        case AssetErrorCode::CatalogNotFound:    return "CatalogNotFound";
        case AssetErrorCode::InvalidCatalogEntry:return "InvalidCatalogEntry";
        case AssetErrorCode::InvalidPath:        return "InvalidPath";
        case AssetErrorCode::PathEscapesRoot:    return "PathEscapesRoot";
        case AssetErrorCode::SourceNotFound:     return "SourceNotFound";
        case AssetErrorCode::SourceReadFailed:   return "SourceReadFailed";
        case AssetErrorCode::UnsupportedType:    return "UnsupportedType";
        case AssetErrorCode::UnsupportedFormat:  return "UnsupportedFormat";
        case AssetErrorCode::DecodeFailed:       return "DecodeFailed";
        case AssetErrorCode::ParseFailed:        return "ParseFailed";
        case AssetErrorCode::InternalError:      return "InternalError";
        default:                                 return "Unknown";
        }
    }

} // namespace Engine::Asset
