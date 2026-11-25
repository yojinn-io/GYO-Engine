#include "engine/voxel_map/VoxelMapLoaders.hpp"

#include <utility>

namespace GYO::Engine::VoxelMap {

namespace {

std::string_view TextOf(const std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// No settings, or an empty object.
bool NoSettings(const std::optional<std::string_view> settingsJson) { return !settingsJson || *settingsJson == "{}"; }

} // namespace

Base::Result<VoxelMapLoaderSettings, Asset::LoaderFailure<VoxelMapLoaderCode>> VoxelMapLoader::ParseSettings(
    const std::optional<std::string_view> settingsJson) const {
    if (!NoSettings(settingsJson)) {
        return Base::Err(Asset::LoaderFailure<VoxelMapLoaderCode>::Make(VoxelMapLoaderCode::BadSettings,
                                                                        "a voxel map takes no settings"));
    }
    return VoxelMapLoaderSettings{};
}

Base::Result<VoxelMapAsset, Asset::LoaderFailure<VoxelMapLoaderCode>> VoxelMapLoader::Load(
    const std::span<const std::byte> bytes, const VoxelMapLoaderSettings&, Asset::LoadContext& context) const {
    auto document = ParseVoxelMap(TextOf(bytes));
    if (!document) {
        VoxelMapError error = std::move(document).error();
        return Base::Err(Asset::LoaderFailure<VoxelMapLoaderCode>::Make(VoxelMapLoaderCode::NotVoxelMap,
                                                                        std::move(error.message), std::move(error.detail)));
    }
    VoxelMapAsset asset;
    asset.document = std::move(*document);
    asset.regions.reserve(asset.document.regions.size());
    for (const VoxelMapRegion& region : asset.document.regions) {
        asset.regions.push_back(context.ReferencePath<VoxelRegionDocument>(region.file));
    }
    return asset;
}

Base::Result<VoxelRegionLoaderSettings, Asset::LoaderFailure<VoxelRegionLoaderCode>> VoxelRegionLoader::ParseSettings(
    const std::optional<std::string_view> settingsJson) const {
    if (!NoSettings(settingsJson)) {
        return Base::Err(Asset::LoaderFailure<VoxelRegionLoaderCode>::Make(VoxelRegionLoaderCode::BadSettings,
                                                                           "a voxel region takes no settings"));
    }
    return VoxelRegionLoaderSettings{};
}

Base::Result<VoxelRegionDocument, Asset::LoaderFailure<VoxelRegionLoaderCode>> VoxelRegionLoader::Load(
    const std::span<const std::byte> bytes, const VoxelRegionLoaderSettings&, Asset::LoadContext&) const {
    auto region = ParseVoxelRegion(TextOf(bytes));
    if (!region) {
        VoxelMapError error = std::move(region).error();
        return Base::Err(Asset::LoaderFailure<VoxelRegionLoaderCode>::Make(
            VoxelRegionLoaderCode::NotVoxelRegion, std::move(error.message), std::move(error.detail)));
    }
    return std::move(*region);
}

} // namespace GYO::Engine::VoxelMap
