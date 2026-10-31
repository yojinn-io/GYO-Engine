#pragma once

#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "RetroFPS/Pvp/PlayerAnimation.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace Engine::Asset { class AssetManager; }

namespace fps::pvp {

// The one reader of the animation rule inputs: the player presentation
// contract's values and the walk and jog clip lengths of its character.
[[nodiscard]] std::optional<PlayerAnimationSource> ReadPlayerAnimationSource(std::string_view presentationText,
    const CharacterPresentationDefinition& character, std::string& error);

// The Match's path (v8-product-anim-rules-source-texture-free): the player
// presentation and its character's model and animation set, no texture.
[[nodiscard]] std::optional<PlayerAnimationRules> LoadPlayerAnimationRules(Engine::Asset::AssetManager& assets,
                                                                           std::string& error);

} // namespace fps::pvp
