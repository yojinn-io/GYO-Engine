#include "RetroFPS/App/AnimationSetDefinition.hpp"

#include "AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"

namespace fps {

std::shared_ptr<const AnimationSetDefinition> LoadAnimationSetDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& animationSetId,
    std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(
            assets, animationSetId, Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        RequireVersionOne(config);
        const auto& clips = config.at("clips");
        if (!clips.is_object() || clips.empty()) {
            throw std::runtime_error("clips must be a nonempty semantic-name object");
        }
        auto definition = std::make_shared<AnimationSetDefinition>();
        for (const auto& [semantic, selector] : clips.items()) {
            try {
                if (semantic.empty()) throw std::runtime_error("semantic name must not be empty");
                AnimationClipReference reference;
                reference.modelAssetId = ReadAssetId(selector.at("model_asset_id"));
                const auto clipName = selector.at("clip").get<std::string>();
                if (clipName.empty()) throw std::runtime_error("clip name must not be empty");
                reference.model = LoadShared<Engine::Model::ModelAsset>(
                    assets, reference.modelAssetId, Engine::Asset::AssetType::FromString("model"));
                const auto clip = reference.model->FindClip(clipName);
                if (!clip) {
                    throw std::runtime_error("model '" + reference.modelAssetId.debugName +
                                             "' is missing animation '" + clipName + "'");
                }
                reference.clipIndex = *clip;
                definition->clips.emplace(semantic, std::move(reference));
            } catch (const std::exception& exception) {
                throw std::runtime_error("clip '" + semantic + "': " + exception.what());
            }
        }
        return definition;
    } catch (const std::exception& exception) {
        error = "failed to load animation set '" + animationSetId.debugName + "': " + exception.what();
        return {};
    }
}

bool ValidateAnimationSetBinding(
    const AnimationSetDefinition& animationSet,
    const Engine::Asset::AssetId& modelAssetId,
    const std::shared_ptr<const Engine::Model::ModelAsset>& model,
    std::string& error) {
    error.clear();
    if (!modelAssetId.IsValid() || !model) {
        error = "animation binding requires a valid model asset ID and model snapshot";
        return false;
    }
    for (const auto& [semantic, reference] : animationSet.clips) {
        const auto location = "clip '" + semantic + "' in model '" + reference.modelAssetId.debugName + "'";
        if (reference.modelAssetId != modelAssetId) {
            error = location + " cannot bind to model '" + modelAssetId.debugName +
                    "': cross-source animation binding is unsupported (no retargeting)";
            return false;
        }
        if (reference.model != model || reference.model.owner_before(model) ||
            model.owner_before(reference.model)) {
            error = location + " does not own the same model snapshot; reload the presentation as a whole";
            return false;
        }
        if (reference.clipIndex >= model->clips.size()) {
            error = location + " has an invalid clip index";
            return false;
        }
    }
    return true;
}

} // namespace fps
