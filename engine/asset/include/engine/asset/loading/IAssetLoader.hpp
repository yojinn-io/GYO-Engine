#pragma once

#include <cstddef>

#include "engine/asset/AssetError.hpp"
#include "engine/base/Error.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/core/AnyAsset.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/asset/loading/LoadContext.hpp"


namespace Engine::Asset::Loading {
    using AssetError = Base::Error<AssetErrorCode>;

    // IAssetLoader（アイ・アセット・ローダ）
    // - bytes -> runtime resource（Core::AnyAsset）へ変換する
    // - 具体実装：TextureLoader / SoundLoader / ... がこれを実装
    class IAssetLoader {
    public:
        virtual ~IAssetLoader() = default;

        // このローダが担当する AssetType
        virtual AssetType GetType() const noexcept = 0;

        // bytes を decode/parse して AnyAsset を返す
        virtual Base::Result<Core::AnyAsset, AssetError>
        Load(Base::ConstSpan<std::byte> bytes, const LoadContext& ctx) = 0;
    };

} // namespace Engine::Asset::Loading
