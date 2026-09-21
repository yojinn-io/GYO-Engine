#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "engine/asset/AssetError.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/base/Result.hpp"
#include "engine/asset/loading/IAssetLoader.hpp"

namespace Engine::Asset::Loading {
    using AssetError = Base::Error<AssetErrorCode>;


    // LoaderRegistry：AssetType -> IAssetLoader の登録/検索
    class LoaderRegistry final {
    public:
        LoaderRegistry() = default;

        // 既に登録済みtypeへの上書きはエラー（事故防止）
        Base::Result<void, AssetError> Register(std::unique_ptr<IAssetLoader> loader);

        // 見つからなければ nullptr
        IAssetLoader* Find(AssetType type) noexcept;
        const IAssetLoader* Find(AssetType type) const noexcept;

        void Clear();

    private:
        static std::uint64_t Key(AssetType type) noexcept;

    private:
        std::unordered_map<std::uint64_t, std::unique_ptr<IAssetLoader>> map_;
    };

} // namespace Engine::Asset::Loading
