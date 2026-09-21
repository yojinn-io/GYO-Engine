#pragma once

#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"

namespace Engine::IO::Path {
    using IoError = Base::Error<IoErrorCode>;

    struct NormalizeOptions {
        bool convertBackslash     = true;  // "\" -> "/"
        bool collapseSlashes      = true;  // "a//b" -> "a/b"
        bool removeDot            = true;  // "./a"  -> "a"
        bool resolveDotDot        = true;  // "a/../b" -> "b"（root越えは失敗）
        bool rejectAbsoluteLike   = true;  // "/a" "C:" "\\server" を拒否
        bool rejectTraversal      = true;  // ".." を含むものを拒否（resolveDotDot=false時にも有効）
        bool rejectNullByte       = true;  // '\0' を拒否
        bool keepTrailingSlash    = false; // 末尾 "/" を保持（必要な用途のみ）
    };

    bool IsAbsoluteLike(std::string_view s) noexcept;     // "/","C:","\\"
    bool ContainsNullByte(std::string_view s) noexcept;
    bool ContainsTraversal(std::string_view s) noexcept;  // ".." セグメント

    Base::Result<std::string, IoError> Normalize(std::string_view raw,
                                          const NormalizeOptions& opt = {});

    // 文字列ユーティリティ（OS非依存）
    std::string Join(std::string_view a, std::string_view b);
    std::string Parent(std::string_view s);
    std::string_view Filename(std::string_view s) noexcept;
    std::string_view Extension(std::string_view s) noexcept;
    std::string ReplaceExtension(std::string_view s, std::string_view ext);


    bool IsSlash(char c) noexcept;

    // 文字列の正規化：区切りを'/'へ、連続スラッシュを圧縮
    std::string NormalizeSlashes(std::string_view path,
                                 bool normalizeSeparators = true,
                                 bool squashSlashes = true);

    // 絶対パス判定（Unix / UNC / Drive）
    bool IsAbsolutePathLike(std::string_view p);

    // scheme を剥がす（res:// / assets:// / asset:// など）
    // - scheme 名は検証しない（現行実装と同じ方針）
    // - "://"" 後の先頭スラッシュは削除して相対寄りにする
    std::string StripSchemeLoose(std::string_view p, bool& stripped);

    // root と相対を結合（rootの末尾/rel先頭のスラッシュを調整）
    std::string JoinRootAndRelative(std::string_view root, std::string_view rel);

    // "." / ".." を解決
    // - escapedAboveRoot=true: ".." によりスタックが空の状態でさらに pop しようとした
    // - drive/UNC/unix absolute の prefix は維持（現行実装を踏襲）
    std::string RemoveDotSegments(std::string_view path, bool& escapedAboveRoot);


} // namespace Engine::IO::Path
