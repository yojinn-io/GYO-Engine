#include "RetroFPS/Game/CampaignContent.hpp"

#include <cstddef>
#include <unordered_set>
#include <utility>

namespace fps {

CampaignContentBuildResult CampaignContent::Build(
    GameDataCatalog catalog,
    std::vector<GridMap> orderedMaps,
    WeaponShotGeometryMap weaponShotGeometry) {
    const std::span<const LevelDefinition> definitions =
        catalog.levels.GetDefinitions();
    if (definitions.empty()) {
        return {std::nullopt, "campaign content requires at least one stage"};
    }
    if (orderedMaps.size() != definitions.size()) {
        return {
            std::nullopt,
            "campaign map count must match the data-defined stage count",
        };
    }

    for (const WeaponDefinition& weapon : catalog.weapons.GetDefinitions()) {
        const auto found = weaponShotGeometry.find(weapon.id);
        if (found == weaponShotGeometry.end()) {
            return {std::nullopt, "campaign weapon requires shot geometry: " + weapon.id};
        }
        const WeaponShotGeometry& geometry = found->second;
        const Float3 muzzle = geometry.muzzleViewCameraPosition;
        if (!std::isfinite(muzzle.x) || !std::isfinite(muzzle.y) ||
            !std::isfinite(muzzle.z) || muzzle.z <= 0.0F ||
            !IsValidWeaponVerticalFov(geometry.viewModelVerticalFovRadians)) {
            return {std::nullopt, "campaign weapon shot geometry requires a finite forward muzzle and FOV in (0, pi): " + weapon.id};
        }
    }

    std::unordered_set<LevelDefinitionId> identifiers;
    std::vector<CampaignStageContent> stages;
    stages.reserve(definitions.size());
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const LevelDefinition& definition = definitions[index];
        if (definition.id.empty() || !definition.mapAssetId.IsValid()) {
            return {
                std::nullopt,
                "campaign stage requires a level ID and a valid map AssetId",
            };
        }
        if (!identifiers.insert(definition.id).second) {
            return {std::nullopt, "campaign stage IDs must be unique"};
        }
        stages.push_back({definition, std::move(orderedMaps[index])});
    }

    CampaignContent content;
    content.data_ = std::move(catalog);
    content.stages_ = std::move(stages);
    content.weaponShotGeometry_ = std::move(weaponShotGeometry);
    return {std::move(content), {}};
}

const CampaignStageContent* CampaignContent::FindStage(
    const std::string_view levelId) const noexcept {
    for (const CampaignStageContent& stage : stages_) {
        if (stage.definition.id == levelId) {
            return &stage;
        }
    }
    return nullptr;
}

const WeaponShotGeometry* CampaignContent::FindWeaponShotGeometry(
    const std::string_view weaponId) const noexcept {
    for (const auto& [id, geometry] : weaponShotGeometry_) {
        if (id == weaponId) return &geometry;
    }
    return nullptr;
}

} // namespace fps
