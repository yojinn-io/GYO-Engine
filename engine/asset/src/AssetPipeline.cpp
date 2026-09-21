#include "engine/asset/loading/AssetPipeline.hpp"

#include "engine/asset/core/AssetStatistics.hpp" // optional（nullptrなら使わない）

namespace Engine::Asset::Loading {

    AssetPipeline::AssetPipeline(IAssetSource& source, LoaderRegistry& registry)
        : source_(source), registry_(registry) {}

    Base::Result<Core::AnyAsset, AssetError>
    AssetPipeline::Load(const LoadContext& ctx) {
        // 0) 基本検証
        if (!ctx.HasPath()) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::InvalidPath, "AssetPipeline: resolvedPath is empty"));
        }

        if (ctx.statistics) {
            ctx.statistics->OnLoadStart();
        }

        // 1) Loader を取得
        IAssetLoader* loader = registry_.Find(ctx.type);
        if (!loader) {
            if (ctx.statistics) {
                ctx.statistics->OnLoadFailure(ctx.id, ctx.type, ctx.nowFrame);
            }
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedType, "AssetPipeline: no loader for type", ctx.resolvedPath));
        }

        // 2) bytes を読む
        auto bytesR = source_.ReadAll(ctx.resolvedPath);
        if (!bytesR) {
            if (ctx.statistics) {
                ctx.statistics->OnLoadFailure(ctx.id, ctx.type, ctx.nowFrame);
            }
            return Base::Result<Core::AnyAsset, AssetError>::Err(std::move(bytesR.error()));
        }

        auto& buf = bytesR.value();
        Base::ConstSpan<std::byte> bytes{ buf.data(), buf.size() };

        // 3) decode/parse
        auto assetR = loader->Load(bytes, ctx);
        if (!assetR) {
            if (ctx.statistics) {
                ctx.statistics->OnLoadFailure(ctx.id, ctx.type, ctx.nowFrame);
            }
            return Base::Result<Core::AnyAsset, AssetError>::Err(std::move(assetR.error()));
        }

        if (ctx.statistics) {
            ctx.statistics->OnLoadSuccess(ctx.id, ctx.type, ctx.nowFrame,
                                          static_cast<std::uint64_t>(buf.size()),
                                          0 /*decodedBytes: 分かるなら loader で埋める*/);
        }

        return Base::Result<Core::AnyAsset, AssetError>::Ok(std::move(assetR.value()));
    }

} // namespace Engine::Asset::Loading
