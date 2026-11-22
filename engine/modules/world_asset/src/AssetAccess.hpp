#pragma once

#include <cstdint>
#include <utility>

#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

// world_asset's own access to the two Resources (not a public header): making them, the store behind the
// requests, and the version a view was pulled at.

namespace GYO::Engine::WorldAsset::Detail {

struct AssetAccess final {
    [[nodiscard]] static AssetRequests MakeRequests(Asset::AssetStore& store) { return AssetRequests(store); }
    [[nodiscard]] static AssetView MakeView(Asset::AssetSnapshot snapshot) { return AssetView(std::move(snapshot)); }
    [[nodiscard]] static Asset::AssetStore& Store(const AssetRequests& requests) { return *requests.store_; }
    [[nodiscard]] static std::uint64_t Version(const AssetView& view) { return view.snapshot_.Version(); }
};

} // namespace GYO::Engine::WorldAsset::Detail
