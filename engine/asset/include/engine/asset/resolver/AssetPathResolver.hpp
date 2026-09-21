#pragma once

#include <string>
#include <string_view>

#include "engine/asset/AssetError.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include "engine/io/path/Path.hpp"

namespace Engine::Asset::Resolver {

    using AssetError = Base::Error<AssetErrorCode>;

    class AssetPathResolver final {
    public:
        struct Options final {
            std::string assetsRoot = "assets";
            bool allowAbsolutePath = false;
            bool allowEscapeAssetsRoot = false;
            bool normalizeSeparators = true;
            bool squashSlashes = true;
            bool allowSchemes = true;
        };

    public:
        AssetPathResolver() = default;
        explicit AssetPathResolver(Options opt);

        void SetOptions(Options opt);
        const Options& GetOptions() const noexcept;

        Base::Result<IO::Path::Path, AssetError> Resolve(std::string_view catalogPath) const;

        // 便利：正規化のみ（単体テストにも使える）
        static std::string NormalizePath(std::string_view path,
                                         bool normalizeSeparators = true,
                                         bool squashSlashes = true);

    private:
        Options opt_{};
    };

} // namespace Engine::Asset::Resolver
