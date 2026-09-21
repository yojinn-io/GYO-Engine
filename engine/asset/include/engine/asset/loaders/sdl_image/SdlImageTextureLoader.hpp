#pragma once

#include "engine/asset/AssetType.hpp"
#include "engine/asset/core/AnyAsset.hpp"
#include "engine/asset/loading/IAssetLoader.hpp"
#include "engine/asset/loading/LoadContext.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace Engine::Asset::Loaders::SdlImage {

// Optional infrastructure-backed decoder. The public contract contains no SDL
// types and returns the same CPU-side TextureAsset used by other decoders.
class SdlImageTextureLoader final : public Loading::IAssetLoader {
public:
    AssetType GetType() const noexcept override;

    Base::Result<Core::AnyAsset, Loading::AssetError>
    Load(Base::ConstSpan<std::byte> bytes,
         const Loading::LoadContext& context) override;
};

} // namespace Engine::Asset::Loaders::SdlImage
