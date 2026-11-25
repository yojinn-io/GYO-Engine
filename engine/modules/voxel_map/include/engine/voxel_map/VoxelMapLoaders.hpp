#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetTypeName.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/voxel_map/VoxelMap.hpp"

// The asset loaders of the chunked map (docs/architecture/target/v2/voxel-map.zh-Hant.md, "As assets"): type
// "voxel_map" for the index and "voxel_region" for a region, format "json" (version 1 of each). A Product registers
// them with AssetLoaderRegistry::Register<...>(). The index declares each region as a reference by its file (relative
// to the index's directory), in index order, so loading the index loads the whole map; the Product's catalog lists
// every region file. A loader has the bytes only, so a region is checked against its index (CheckVoxelRegion) by the
// reader that holds both.

namespace GYO::Engine::VoxelMap {

// The index with a reference to each region, in the order of document.regions.
struct VoxelMapAsset final {
    VoxelMapDocument document;
    std::vector<Asset::AssetRef<VoxelRegionDocument>> regions;
};

} // namespace GYO::Engine::VoxelMap

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::VoxelMap::VoxelMapAsset> {
    static constexpr std::string_view value = "voxel_map";
};

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::VoxelMap::VoxelRegionDocument> {
    static constexpr std::string_view value = "voxel_region";
};

namespace GYO::Engine::VoxelMap {

enum class VoxelMapLoaderCode : std::uint8_t {
    // The text is not a gyo.voxel_map version 1 (message and detail are ParseVoxelMap's).
    NotVoxelMap = 1,
    // The entry has settings; this loader takes none.
    BadSettings,
};

enum class VoxelRegionLoaderCode : std::uint8_t {
    // The text breaks a rule ParseVoxelRegion checks (message and detail are its).
    NotVoxelRegion = 1,
    // The entry has settings; this loader takes none.
    BadSettings,
};

[[nodiscard]] constexpr const char* ToString(const VoxelMapLoaderCode code) noexcept {
    switch (code) {
    case VoxelMapLoaderCode::NotVoxelMap: return "NotVoxelMap";
    case VoxelMapLoaderCode::BadSettings: return "BadSettings";
    }
    return "Unknown";
}

[[nodiscard]] constexpr const char* ToString(const VoxelRegionLoaderCode code) noexcept {
    switch (code) {
    case VoxelRegionLoaderCode::NotVoxelRegion: return "NotVoxelRegion";
    case VoxelRegionLoaderCode::BadSettings: return "BadSettings";
    }
    return "Unknown";
}

struct VoxelMapLoaderSettings final {
    friend bool operator==(const VoxelMapLoaderSettings&, const VoxelMapLoaderSettings&) = default;
};

class VoxelMapLoader final {
public:
    using Output = VoxelMapAsset;
    using Settings = VoxelMapLoaderSettings;
    using Code = VoxelMapLoaderCode;

    static constexpr std::string_view kTypeName = "voxel_map";
    static constexpr std::array<std::string_view, 1> kFormats{"json"};
    static constexpr std::size_t kDefaultMaxBytes = kMaxMapTextBytes;

    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;
    [[nodiscard]] Base::Result<Output, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                        const Settings& settings,
                                                                        Asset::LoadContext& context) const;
};

class VoxelRegionLoader final {
public:
    using Output = VoxelRegionDocument;
    using Settings = VoxelMapLoaderSettings;
    using Code = VoxelRegionLoaderCode;

    static constexpr std::string_view kTypeName = "voxel_region";
    static constexpr std::array<std::string_view, 1> kFormats{"json"};
    static constexpr std::size_t kDefaultMaxBytes = kMaxRegionTextBytes;

    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;
    [[nodiscard]] Base::Result<Output, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                        const Settings& settings,
                                                                        Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<VoxelMapLoader>);
static_assert(Asset::AssetLoader<VoxelRegionLoader>);

} // namespace GYO::Engine::VoxelMap
