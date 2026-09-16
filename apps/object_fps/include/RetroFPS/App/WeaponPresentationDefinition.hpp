#pragma once

#include "RetroFPS/Gameplay/Weapon/WeaponShotGeometry.hpp"
#include "engine/asset/AssetId.hpp"
#include "model/Animation.hpp"
#include "render/RenderTypes.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Asset { class AssetManager; }

namespace fps {

// Immutable application binding shared by content loading and presentation.
// GPU meshes/textures remain owned by each WeaponViewModel instance.
struct WeaponPresentationDefinition final {
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    // Idle, Shoot, Reload, Draw, Hide.
    std::array<std::size_t, 5> clips{};
    Engine::Model::Vec3 idleAnchor{};
    Engine::Render::Transform3D placement{};
    Engine::Render::PerspectiveCamera3D camera{};
    Engine::Render::SamplerMode sampler{Engine::Render::SamplerMode::LinearClamp};
    std::vector<Engine::Asset::AssetId> materialTextureAssetIds;
    std::size_t muzzleNodeIndex{};
    Engine::Model::Vec3 muzzleLocalPosition{};
    WeaponShotGeometry shotGeometry{};
};

[[nodiscard]] std::shared_ptr<const WeaponPresentationDefinition>
LoadWeaponPresentationDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& presentationId,
    std::string& error);

// Applies the same scale -> X/Y/Z rotation -> translation contract used by
// the render submission, after removing the fixed Idle anchor.
[[nodiscard]] Engine::Render::Float3 EvaluateWeaponMuzzleViewCameraPosition(
    const WeaponPresentationDefinition& definition,
    const Engine::Model::Pose& pose);

} // namespace fps
