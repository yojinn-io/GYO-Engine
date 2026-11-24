#pragma once

#include "engine/asset/AssetLoader.hpp"
#include "engine/model/ModelData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace object_fps_pvp::client {

// A baked voxel map (gyo.voxel_mesh v1, the tool's file the game reads itself: data/VoxelMeshFile.hpp) as a model
// asset, so render's Mesh3d draws it: one node, one mesh of the faces, uv into the palette picture
// (data::PaletteImage, which the game gives the Mesh3d as its image). Catalog type "model", format "json".
enum class VoxelModelCode : std::uint8_t {
    // The text is not a gyo.voxel_mesh v1 the game reads (the detail says why).
    NotVoxelMesh = 1,
    // The entry has settings; this loader takes none.
    BadSettings = 2,
};

[[nodiscard]] constexpr const char* ToString(const VoxelModelCode code) noexcept {
    switch (code) {
    case VoxelModelCode::NotVoxelMesh: return "NotVoxelMesh";
    case VoxelModelCode::BadSettings: return "BadSettings";
    }
    return "Unknown";
}

struct VoxelModelSettings final {
    [[nodiscard]] friend bool operator==(const VoxelModelSettings&, const VoxelModelSettings&) = default;
};

class VoxelModelLoader final {
public:
    using Output = GYO::Engine::Model::ModelData;
    using Settings = VoxelModelSettings;
    using Code = VoxelModelCode;

    static constexpr std::string_view kTypeName = "model";
    static constexpr std::array<std::string_view, 1> kFormats{"json"};
    // The voxel contract's limit on the text.
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{16} * 1024u * 1024u;

    [[nodiscard]] GYO::Engine::Base::Result<Settings, GYO::Engine::Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;
    [[nodiscard]] GYO::Engine::Base::Result<Output, GYO::Engine::Asset::LoaderFailure<Code>> Load(
        std::span<const std::byte> bytes, const Settings& settings, GYO::Engine::Asset::LoadContext& context) const;
};

static_assert(GYO::Engine::Asset::AssetLoader<VoxelModelLoader>);

} // namespace object_fps_pvp::client
