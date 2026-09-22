#pragma once

#include "RetroFPS/Game/CampaignContent.hpp"

#include "engine/asset/AssetId.hpp"

namespace Engine::Asset {
class AssetManager;
}

namespace fps {

struct CampaignDataAssetIds final {
    Engine::Asset::AssetId enemies =
        Engine::Asset::AssetId::FromString("object_fps_v2.data.enemies");
    Engine::Asset::AssetId weapons =
        Engine::Asset::AssetId::FromString("object_fps_v2.data.weapons");
    Engine::Asset::AssetId levels =
        Engine::Asset::AssetId::FromString("object_fps_v2.data.levels");
};

// Object_FPS-specific deserialization adapter. It consumes GYO AssetManager
// handles and never resolves native paths on its own.
class CampaignContentLoader final {
public:
    [[nodiscard]] static CampaignContentBuildResult Load(
        Engine::Asset::AssetManager& assets,
        const CampaignDataAssetIds& ids = {});
};

} // namespace fps
