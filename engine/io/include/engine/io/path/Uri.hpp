#pragma once
#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/path/Path.hpp"

namespace Engine::IO::Path {

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
    // file:// follows the one mapping of docs/architecture/io.md: the path is
    // the native absolute path in '/' form ("/Users/x", "C:/x"), the
    // authority is empty or localhost, and '?' and '#' belong to the path.
    Base::Result<Uri, IoError>  ParseUri(std::string_view s);

    // 失敗はしない方針（空文字は呼び出し側で弾く）
    // - scheme があれば剥がして path に入れる
    // - scheme が無ければ scheme=""、path=入力
    // - file:// that ParseUri accepts gets ParseUri's path; other schemes keep
    //   the loose meaning (leading slashes dropped)
    Uri ParseUriLoose(std::string_view s);

    // A native absolute path in UTF-8 ("/Users/x", "C:/x" or "C:\x") as a
    // file URI. Relative and drive-relative paths are InvalidPath, as are
    // long-path prefixes; UNC paths are NotSupported in this round.
    Base::Result<Uri, IoError> FileUriFromNativePath(std::string_view utf8AbsolutePath);

    // The native absolute path of a file URI in '/' form, UTF-8.
    Base::Result<std::string, IoError> NativePathFromFileUri(const Uri& uri);

} // namespace Engine::IO::Path
