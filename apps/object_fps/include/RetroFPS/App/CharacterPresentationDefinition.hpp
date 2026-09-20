#pragma once

#include "RetroFPS/App/AnimationSetDefinition.hpp"
#include "render/RenderTypes.hpp"

#include <array>
#include <optional>
#include <vector>

namespace fps {

struct MaterialBinding final {
    std::array<float, 4> baseColorLinear{1, 1, 1, 1};
    std::optional<Engine::Asset::AssetId> textureAssetId;
    Engine::Render::SamplerMode sampler{Engine::Render::SamplerMode::LinearClamp};
};

// Immutable assembly data. Material bindings are copied from imported defaults
// and never modify the shared model. Instances supply their own pose and clock.
struct CharacterPresentationDefinition final {
    Engine::Asset::AssetId modelAssetId;
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    std::optional<Engine::Asset::AssetId> animationSetAssetId;
    std::shared_ptr<const AnimationSetDefinition> animationSet;
    // Indexed by ModelAsset::materials, like MeshPart::materialIndex.
    std::vector<MaterialBinding> materials;
};

[[nodiscard]] std::shared_ptr<const CharacterPresentationDefinition>
LoadCharacterPresentationDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& presentationId,
    std::string& error);

} // namespace fps
