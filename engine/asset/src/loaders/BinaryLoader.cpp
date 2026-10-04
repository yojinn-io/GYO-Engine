#include "engine/asset/loaders/BinaryLoader.hpp"

namespace Engine::Asset::Loaders {

    AssetType BinaryLoader::GetType() const noexcept {
        static const AssetType kType = AssetType::FromString("binary");
        return kType;
    }

    Base::Result<Core::AnyAsset, AssetError>
    BinaryLoader::Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        auto bin = std::make_shared<BinaryAsset>();
        bin->bytes.assign(bytes.begin(), bytes.end());

        (void)ctx;
        return Core::AnyAsset::FromShared<BinaryAsset>(std::move(bin));
    }

} // namespace Engine::Asset::Loaders
