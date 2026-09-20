#pragma once

#include "engine/asset/AssetId.hpp"
#include "model/ModelAsset.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Engine::Asset { class AssetManager; }

namespace fps {

// A clip selector owns the exact immutable model snapshot containing the clip.
// Playback time, sampled poses and GPU resources belong to each instance.
struct AnimationClipReference final {
    Engine::Asset::AssetId modelAssetId;
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    std::size_t clipIndex{};
};

struct AnimationSetDefinition final {
    std::unordered_map<std::string, AnimationClipReference> clips;
};

[[nodiscard]] std::shared_ptr<const AnimationSetDefinition> LoadAnimationSetDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& animationSetId,
    std::string& error);

// Until cross-file skeleton binding exists, an executable assembly requires
// both the canonical source ID and the same model ownership snapshot.
[[nodiscard]] bool ValidateAnimationSetBinding(
    const AnimationSetDefinition& animationSet,
    const Engine::Asset::AssetId& modelAssetId,
    const std::shared_ptr<const Engine::Model::ModelAsset>& model,
    std::string& error);

} // namespace fps
