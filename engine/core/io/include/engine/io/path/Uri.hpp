#pragma once
#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"
#include "engine/io/path/Path.hpp"

namespace GYO::Engine::IO::Path {

    // file has the rules of docs/architecture/io.md; any other scheme is Unknown and keeps its name in
    // schemeText.
    enum class UriScheme {
        None,
        File,
        Unknown
    };

    struct Uri final {
        UriScheme scheme = UriScheme::None;
        Path path;                // 論理パス（正規化済み）

        std::string schemeText;   // the scheme's name when Unknown
        std::string authority;    // "scheme://authority/path"
        std::string query;        // "?a=b"
        std::string fragment;     // "#xxx"

        bool HasScheme() const noexcept { return scheme != UriScheme::None; }
        bool IsKnownScheme() const noexcept { return scheme != UriScheme::Unknown; }

        std::string ToString() const;
    };

    // 失敗時は Base::Error（IOErrorCode::InvalidPath / NotSupported 等）
    // file:// follows the one mapping of docs/architecture/io.md: the path is
    // the native absolute path in '/' form ("/Users/x", "C:/x", "//server/x"),
    // kept as written; an empty or localhost authority is the local machine,
    // any other names a UNC host; '?' and '#' belong to the path.
    Base::Result<Uri, IOError>  ParseUri(std::string_view s);


    // A native absolute path in UTF-8 ("/Users/x", "C:/x", "C:\x" or a UNC
    // path "//server/x", "\\server\x") as a file URI, the path kept as written.
    // Relative and drive-relative paths are InvalidPath, as are long-path
    // prefixes.
    Base::Result<Uri, IOError> FileUriFromNativePath(std::string_view utf8AbsolutePath);

    // The native absolute path of a file URI in '/' form, UTF-8.
    Base::Result<std::string, IOError> NativePathFromFileUri(const Uri& uri);

} // namespace GYO::Engine::IO::Path
