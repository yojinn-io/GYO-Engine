#include "RetroFPS/Pvp/PlayerAnimationContent.hpp"

#include "../App/AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"

#include <stdexcept>

namespace fps::pvp {

std::optional<PlayerAnimationSource> ReadPlayerAnimationSource(std::string_view presentationText,
    const CharacterPresentationDefinition& character, std::string& error) {
    error.clear();
    try {
        const auto config = nlohmann::json::parse(presentationText);
        asset_definition_detail::RequireVersionOne(config);
        if (!character.model || !character.animationSet)
            throw std::runtime_error("player character requires a model and an animation set");
        const auto length = [&](const std::string& name) {
            const auto found = character.animationSet->clips.find(name);
            if (found == character.animationSet->clips.end() || found->second.clipIndex >= character.model->clips.size())
                throw std::runtime_error("missing player animation: " + name);
            return character.model->clips[found->second.clipIndex].durationSeconds;
        };
        PlayerAnimationSource source;
        source.referenceSpeed = config.at("reference_speed").get<double>();
        source.maxFrameDeltaSeconds = config.at("max_frame_delta_seconds").get<double>();
        const auto& locomotion = config.at("locomotion");
        source.walkNativeSpeed = locomotion.at("walk_native_speed").get<double>();
        source.jogNativeSpeed = locomotion.at("jog_native_speed").get<double>();
        source.walkClipSeconds = length("walk");
        source.jogClipSeconds = length("jog");
        source.transitionSeconds = config.at("transition_seconds").get<double>();
        const auto& actions = config.at("actions");
        source.jumpStartSeconds = actions.at("jump_start_seconds").get<double>();
        source.jumpLandSeconds = actions.at("jump_land_seconds").get<double>();
        return source;
    } catch (const std::exception& exception) {
        error = "player animation source: " + std::string(exception.what());
        return std::nullopt;
    }
}

std::optional<PlayerAnimationRules> LoadPlayerAnimationRules(Engine::Asset::AssetManager& assets,
                                                             std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(assets,
            Engine::Asset::AssetId::FromString("object_fps_pvp.player.presentation"),
            Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        const auto character = LoadCharacterAnimationDefinition(assets,
            ReadAssetId(config.at("character_asset_id")), error);
        if (!character) throw std::runtime_error(error);
        const auto source = ReadPlayerAnimationSource(text->text, *character, error);
        if (!source) throw std::runtime_error(error);
        auto rules = MakePlayerAnimationRules(*source, error);
        if (!rules) throw std::runtime_error(error);
        return rules;
    } catch (const std::exception& exception) {
        error = "player animation rules: " + std::string(exception.what());
        return std::nullopt;
    }
}

} // namespace fps::pvp
