#pragma once

#include "engine/asset/loading/IAssetLoader.hpp"

namespace Engine::Model::Ufbx {

// FBX bytes -> owning, backend-neutral ModelAsset. All file access belongs to
// AssetManager; material names are exposed for explicit application bindings.
class UfbxModelLoader final : public Asset::Loading::IAssetLoader {
public:
    [[nodiscard]] Asset::AssetType GetType() const noexcept override;
    [[nodiscard]] Base::Result<Asset::Core::AnyAsset, Asset::Loading::AssetError>
    Load(Base::ConstSpan<std::byte> bytes,
         const Asset::Loading::LoadContext& context) override;
};

} // namespace Engine::Model::Ufbx
