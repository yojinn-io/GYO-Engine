#pragma once
#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/path/Path.hpp"

namespace Engine::IO::Path {
    using IoError = Base::Error<IoErrorCode>;

    enum class UriScheme {
        None,
        Asset,
        File,
        Http,
        Https,
        Unknown
    };

    struct Uri final {
        UriScheme scheme = UriScheme::None;
        Path path;                // 論理パス（正規化済み）

        std::string schemeText;   // Unknown の場合に保持
        std::string authority;    // "scheme://authority/path"
        std::string query;        // "?a=b"
        std::string fragment;     // "#xxx"

        bool HasScheme() const noexcept { return scheme != UriScheme::None; }
        bool IsKnownScheme() const noexcept { return scheme != UriScheme::Unknown; }

        std::string ToString() const;
    };

    // 失敗時は Engine::Error（IoErrorCode::InvalidPath / NotSupported 等）
    Base::Result<Uri, IoError>  ParseUri(std::string_view s);

    // 失敗はしない方針（空文字は呼び出し側で弾く）
    // - scheme があれば剥がして path に入れる
    // - scheme が無ければ scheme=""、path=入力
    Uri ParseUriLoose(std::string_view s);

} // namespace Engine::IO::Path
