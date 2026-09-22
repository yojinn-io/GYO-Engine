#pragma once
#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"
namespace Engine::Asset {
class AssetManager;
}
namespace fps {
[[nodiscard]] std::shared_ptr<const EnemyRig> LoadEnemyRig(Engine::Asset::AssetManager& assets,
                                                           const EnemyDefinition& definition,
                                                           std::string& error);
}
