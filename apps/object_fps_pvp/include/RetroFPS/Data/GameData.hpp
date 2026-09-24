#pragma once

#include "RetroFPS/World/GridMap.hpp"

#include "engine/asset/AssetId.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fps {

using EnemyDefinitionId = std::string;
using WeaponDefinitionId = std::string;
using LevelDefinitionId = std::string;

struct EnemyRig;

struct EnemyDefinition final {
    EnemyDefinitionId id;
    EnemyKind kind = EnemyKind::Melee;
    float damage = 0.0f;
    float attackIntervalSeconds = 0.0f;
    float maxHealth = 0.0f;
    float defense = 0.0f;
    float hitboxRadius = 0.0f;
    float hitboxHeight = 0.0f;
    Engine::Asset::AssetId presentationAssetId;
    std::shared_ptr<const EnemyRig> rig;
};

struct WeaponDefinition final {
    WeaponDefinitionId id;
    float damage = 0.0f;
    std::uint32_t magazineCapacity = 0;
    std::uint32_t reserveAmmo = 0;
    float recoilDegrees = 0.0f;
    bool automatic = false;
    Engine::Asset::AssetId presentationAssetId;
    float fireIntervalSeconds = 0.0f;
    float reloadSeconds = 0.0f;
    float drawSeconds = 25.0f / 30.0f;
    float hideSeconds = 11.0f / 30.0f;
};

struct LevelDefinition final {
    LevelDefinitionId id;
    std::string name;
    Engine::Asset::AssetId mapAssetId;
    std::optional<LevelDefinitionId> nextLevelId;
    std::uint32_t rangedEnemyCount = 0;
    std::uint32_t meleeEnemyCount = 0;
    std::uint32_t activeEnemyLimit = 0;
    std::uint32_t clearKillCount = 0;
};

class EnemyCatalog final {
public:
    [[nodiscard]] std::span<const EnemyDefinition> GetDefinitions() const noexcept {
        return definitions_;
    }
    [[nodiscard]] const EnemyDefinition* FindById(std::string_view id) const noexcept;
    [[nodiscard]] const EnemyDefinition* FindByKind(EnemyKind kind) const noexcept;

private:
    friend class GameDataLoader;
    friend class CampaignContentLoader;
    std::vector<EnemyDefinition> definitions_;
};

class WeaponCatalog final {
public:
    [[nodiscard]] std::span<const WeaponDefinition> GetDefinitions() const noexcept {
        return definitions_;
    }
    [[nodiscard]] const WeaponDefinition* FindById(std::string_view id) const noexcept;
    [[nodiscard]] const WeaponDefinition* GetDefaultWeapon() const noexcept;

private:
    friend class GameDataLoader;
    std::vector<WeaponDefinition> definitions_;
};

class LevelCatalog final {
public:
    [[nodiscard]] std::span<const LevelDefinition> GetDefinitions() const noexcept {
        return definitions_;
    }
    [[nodiscard]] const LevelDefinition* FindById(std::string_view id) const noexcept;
    [[nodiscard]] const LevelDefinition* GetStartLevel() const noexcept;

private:
    friend class GameDataLoader;
    std::vector<LevelDefinition> definitions_;
};

struct GameDataCatalog final {
    EnemyCatalog enemies;
    WeaponCatalog weapons;
    LevelCatalog levels;
};

struct GameDataLoadResult final {
    std::optional<GameDataCatalog> catalog;
    std::string error;

    [[nodiscard]] bool Succeeded() const noexcept { return catalog.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return Succeeded(); }
};

class GameDataLoader final {
public:
    [[nodiscard]] static GameDataLoadResult Parse(
        std::string_view enemiesCsv,
        std::string_view weaponsCsv,
        std::string_view levelsCsv);
};

} // namespace fps
