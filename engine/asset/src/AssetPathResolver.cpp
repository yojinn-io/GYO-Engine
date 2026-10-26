#include "engine/asset/resolver/AssetPathResolver.hpp"

#include "engine/io/path/PathUtils.hpp"
#include "engine/io/path/Uri.hpp"

namespace Engine::Asset::Resolver {

    AssetPathResolver::AssetPathResolver(Options opt) : opt_(std::move(opt)) {}

    void AssetPathResolver::SetOptions(Options opt) { opt_ = std::move(opt); }

    const AssetPathResolver::Options& AssetPathResolver::GetOptions() const noexcept { return opt_; }

    std::string AssetPathResolver::NormalizePath(std::string_view path,
                                                 bool normalizeSeparators,
                                                 bool squashSlashes) {
        return Engine::IO::Path::NormalizeSlashes(path, normalizeSeparators, squashSlashes);
    }

    Base::Result<IO::Path::Path, AssetError> AssetPathResolver::Resolve(std::string_view catalogPath) const {
        if (catalogPath.empty()) {
            return Base::Err(AssetError::Make(
                    AssetErrorCode::InvalidPath,
                    "AssetPathResolver: empty path",
                    std::string(catalogPath)));
        }

        // 1) scheme 剥がし（res://, assets:// など）
        std::string p;
        if (opt_.allowSchemes) {
            auto u = Engine::IO::Path::ParseUriLoose(catalogPath);
            p = u.path.Str();
        } else {
            p = std::string(catalogPath);
        }

        // 2) 正規化（区切り/連続スラッシュ）
        p = Engine::IO::Path::NormalizeSlashes(p, opt_.normalizeSeparators, opt_.squashSlashes);

        // 3) 絶対パス判定
        if (Engine::IO::Path::IsAbsolutePathLike(p)) {
            if (!opt_.allowAbsolutePath) {
                return Base::Err(AssetError::Make(
                        AssetErrorCode::InvalidPath,
                        "AssetPathResolver: absolute path is not allowed",
                        std::string(catalogPath)));
            }

            // 絶対パス許可の場合：dot segments だけ解決（root制約なし）
            bool escaped = false;
            std::string cleaned = Engine::IO::Path::RemoveDotSegments(p, escaped);
            return IO::Path::Path::FromNormalized(std::move(cleaned));
        }

        // 4) assetsRoot と結合する前に、相対パス単体で root 越えを検出する。
        //    結合後だけで判定すると "assets/../outside" が filesystem root 内に
        //    収まるため、assetsRoot からの逸脱を見逃してしまう。
        bool escapedRelativeRoot = false;
        std::string cleanedRelative = Engine::IO::Path::RemoveDotSegments(p, escapedRelativeRoot);
        if (escapedRelativeRoot && !opt_.allowEscapeAssetsRoot) {
            return Base::Err(AssetError::Make(
                    AssetErrorCode::PathEscapesRoot,
                    "AssetPathResolver: path escapes assetsRoot via '..' which is not allowed",
                    std::string(catalogPath)));
        }

        // 5) Join under assetsRoot. The root is a native path and stays as written, '\' turned
        //    into '/' only: its '..' and a leading "//" (UNC) go to the OS, which resolves them
        //    when the file opens (docs/architecture/io.md Invariant 3). Within the root the
        //    relative path is the cleaned one, so "a/../b" stays inside; an escape that
        //    allowEscapeAssetsRoot permits keeps its '..' for the OS as well.
        const std::string& relativeForJoin = escapedRelativeRoot ? p : cleanedRelative;
        return IO::Path::Path::FromNormalized(Engine::IO::Path::JoinRootAndRelative(opt_.assetsRoot, relativeForJoin));
    }

} // namespace Engine::Asset::Resolver
