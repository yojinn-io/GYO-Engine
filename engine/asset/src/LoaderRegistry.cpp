#include "engine/asset/loading/LoaderRegistry.hpp"

#include "engine/base/Assert.hpp"

namespace Engine::Asset::Loading {

    std::uint64_t LoaderRegistry::Key(AssetType type) noexcept {
        // AssetType が uint64 hash を持つ前提（実装に合わせて調整可）
        return static_cast<std::uint64_t>(type.value);
    }

    void LoaderRegistry::Register(std::unique_ptr<IAssetLoader> loader) {
        GYO_ASSERT(loader != nullptr);
        const auto k = Key(loader->GetType());
        GYO_ASSERT(k != 0);
        GYO_ASSERT(!map_.contains(k));
        map_.emplace(k, std::move(loader));
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
