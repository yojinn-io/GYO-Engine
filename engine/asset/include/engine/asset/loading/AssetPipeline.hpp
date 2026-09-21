#pragma once

#include "engine/asset/AssetError.hpp"
#include "engine/asset/core/AnyAsset.hpp"
#include "engine/base/Result.hpp"
#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/LoadContext.hpp"

namespace Engine::Asset::Loading {
    using AssetError = Base::Error<AssetErrorCode>;

    // AssetPipeline：
    // - 読む（IAssetSource）
    // - 変換する（IAssetLoader）
    // - 成功/失敗を Result で返す
    class AssetPipeline final {
    public:
        AssetPipeline(IAssetSource& source, LoaderRegistry& registry);

        Base::Result<Core::AnyAsset, AssetError> Load(const LoadContext& ctx);

    private:
        IAssetSource& source_;
        LoaderRegistry& registry_;
    };

} // namespace Engine::Asset::Loading
