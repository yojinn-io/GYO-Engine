#include "engine/asset/loading/LoaderRegistry.hpp"

namespace Engine::Asset::Loading {

    std::uint64_t LoaderRegistry::Key(AssetType type) noexcept {
        // AssetType が uint64 hash を持つ前提（実装に合わせて調整可）
        return static_cast<std::uint64_t>(type.value);
    }

    Base::Result<void, AssetError>
    LoaderRegistry::Register(std::unique_ptr<IAssetLoader> loader) {
        if (!loader) {
            return Base::Result<void, AssetError>::Err(
                AssetError::Make(AssetErrorCode::InternalError, "Register: loader is null"));
        }

        const auto type = loader->GetType();
        const auto k = Key(type);

        if (k == 0) {
            return Base::Result<void, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedType, "Register: invalid AssetType (0)"));
        }

        auto it = map_.find(k);
        if (it != map_.end()) {
            return Base::Result<void, AssetError>::Err(
                AssetError::Make(AssetErrorCode::InternalError, "Register: loader already exists for type"));
        }

        map_.emplace(k, std::move(loader));
        return Base::Result<void, AssetError>::Ok();
    }

    IAssetLoader* LoaderRegistry::Find(AssetType type) noexcept {
        auto it = map_.find(Key(type));
        return it == map_.end() ? nullptr : it->second.get();
    }

    const IAssetLoader* LoaderRegistry::Find(AssetType type) const noexcept {
        auto it = map_.find(Key(type));
        return it == map_.end() ? nullptr : it->second.get();
    }

    void LoaderRegistry::Clear() {
        map_.clear();
    }

} // namespace Engine::Asset::Loading
