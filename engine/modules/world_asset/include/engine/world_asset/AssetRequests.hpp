#pragma once

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/asset/AssetTypeName.hpp"
#include "engine/asset/Handle.hpp"
#include "engine/kernel/Hash.hpp"

// The request entry of a World (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.4, 5.5;
// Design Invariants AS-1, AS-11): a Resource world_asset puts into every World it is built into. A System holds it
// as Res<AssetRequests> and asks for content by id; the Handle comes back at once and names the id.
//
// What it does not give (AS-11): the state of the store now. Load returns nothing of whether the request hit, and
// there is no way from here to the store itself; the state of an asset is read from the World's AssetView, which
// changes only at the apply point. The store has its own lock, so Load may be called from any System.

namespace GYO::Engine::WorldAsset {

namespace Detail {
struct AssetAccess;
} // namespace Detail

class AssetRequests final {
public:
    // A request (5.5): refused at the request level (UnknownId, TypeMismatch: the Handle carries the failure and
    // holds nothing), otherwise a Handle that holds the id alive. Jobs are submitted after the store's lock is
    // released.
    template <class T>
        requires Asset::HasAssetTypeName<T>
    [[nodiscard]] Asset::Handle<T> Load(const Asset::AssetId& id) const {
        return store_->template Request<T>(id);
    }
    // A request for a reference a loader declared: an author who wants to hold that dependency (5.17).
    template <class T>
        requires Asset::HasAssetTypeName<T>
    [[nodiscard]] Asset::Handle<T> Load(const Asset::AssetRef<T>& reference) const {
        return store_->template Request<T>(reference.Id());
    }

private:
    friend struct Detail::AssetAccess;
    explicit AssetRequests(Asset::AssetStore& store) noexcept : store_(&store) {}

    Asset::AssetStore* store_;
};

// Not hashed (declaration 5.9.1): a request entry has nothing to compare.
inline void HashValue(Kernel::Hasher&, const AssetRequests&) {}

} // namespace GYO::Engine::WorldAsset
