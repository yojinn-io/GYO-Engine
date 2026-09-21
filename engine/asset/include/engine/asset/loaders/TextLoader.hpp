#pragma once

#include <memory>
#include <string>

#include "engine/asset/AssetType.hpp"
#include "engine/asset/core/AnyAsset.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/asset/loading/IAssetLoader.hpp"
#include "engine/asset/loading/LoadContext.hpp"

namespace Engine::Asset::Loaders {
    using AssetError = Base::Error<AssetErrorCode>;

    struct TextAsset final {
        std::string text; // UTF-8 想定
    };

    class TextLoader final : public Loading::IAssetLoader {
    public:
        AssetType GetType() const noexcept override;

        Base::Result<Core::AnyAsset, AssetError>
        Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) override;
    };

} // namespace Engine::Asset::Loaders
