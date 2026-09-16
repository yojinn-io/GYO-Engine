#include "RetroFPS/App/CampaignContentLoader.hpp"
#include "RetroFPS/App/WeaponPresentationDefinition.hpp"

#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/loaders/TextLoader.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fps {
namespace {

[[nodiscard]] std::string AssetName(const Engine::Asset::AssetId& id) {
    return id.debugName.empty() ? std::to_string(id.value) : id.debugName;
}

[[nodiscard]] std::optional<std::string> LoadText(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& id,
    std::string& error) {
    const auto result = assets.Load(
        id,
        Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text()));
    if (!result) {
        error = "failed to load text asset '" + AssetName(id) + "': " +
                result.error().message;
        if (!result.error().detail.empty()) {
            error += " (" + result.error().detail + ')';
        }
        return std::nullopt;
    }

    const Engine::Asset::AssetHandle handle = result.value();
    const auto text =
        assets.GetSharedConst<Engine::Asset::Loaders::TextAsset>(handle);
    if (!text) {
        assets.Release(handle);
        error = "text asset '" + AssetName(id) +
                "' loaded without a TextAsset payload";
        return std::nullopt;
    }

    std::string copy = text->text;
    assets.Release(handle);
    return copy;
}

} // namespace

CampaignContentBuildResult CampaignContentLoader::Load(
    Engine::Asset::AssetManager& assets,
    const CampaignDataAssetIds& ids) {
    std::string error;
    const std::optional<std::string> enemies = LoadText(assets, ids.enemies, error);
    if (!enemies) {
        return {std::nullopt, std::move(error)};
    }
    const std::optional<std::string> animations =
        LoadText(assets, ids.enemyAnimationClips, error);
    if (!animations) {
        return {std::nullopt, std::move(error)};
    }
    const std::optional<std::string> weapons = LoadText(assets, ids.weapons, error);
    if (!weapons) {
        return {std::nullopt, std::move(error)};
    }
    const std::optional<std::string> levels = LoadText(assets, ids.levels, error);
    if (!levels) {
        return {std::nullopt, std::move(error)};
    }

    GameDataLoadResult data = GameDataLoader::Parse(
        *enemies, *animations, *weapons, *levels);
    if (!data) {
        return {
            std::nullopt,
            "failed to deserialize Object_FPS game data: " + data.error,
        };
    }

    std::vector<GridMap> maps;
    maps.reserve(data.catalog->levels.GetDefinitions().size());
    for (const LevelDefinition& level : data.catalog->levels.GetDefinitions()) {
        const std::optional<std::string> mapText =
            LoadText(assets, level.mapAssetId, error);
        if (!mapText) {
            return {std::nullopt, std::move(error)};
        }
        MapLoadResult map = GridMapLoader::Parse(*mapText);
        if (!map) {
            return {
                std::nullopt,
                "failed to deserialize map asset '" + AssetName(level.mapAssetId) +
                    "' for level '" + level.id + "': " + map.error,
            };
        }
        maps.push_back(std::move(*map.map));
    }

    WeaponShotGeometryMap shotGeometry;
    for (const WeaponDefinition& weapon : data.catalog->weapons.GetDefinitions()) {
        const auto presentation = LoadWeaponPresentationDefinition(
            assets, weapon.presentationAssetId, error);
        if (!presentation) {
            return {std::nullopt, "failed to load weapon '" + weapon.id + "': " + error};
        }
        shotGeometry.emplace(weapon.id, presentation->shotGeometry);
    }
    return CampaignContent::Build(
        std::move(*data.catalog), std::move(maps), std::move(shotGeometry));
}

} // namespace fps
