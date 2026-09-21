#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "engine/asset/AssetType.hpp"
#include "engine/asset/core/AnyAsset.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/asset/loading/IAssetLoader.hpp"
#include "engine/asset/loading/LoadContext.hpp"

namespace Engine::Asset::Loaders {
    using AssetError = Base::Error<AssetErrorCode>;

    // 最小のサウンド表現（PCM16）
    struct SoundAsset final {
        std::uint32_t sampleRate = 0;
        std::uint16_t channels = 0;
        std::vector<std::int16_t> pcm16; // interleaved
    };

    class SoundLoader final : public Loading::IAssetLoader {
    public:
        AssetType GetType() const noexcept override;

        Base::Result<Core::AnyAsset, AssetError>
        Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) override;
    };

} // namespace Engine::Asset::Loaders
