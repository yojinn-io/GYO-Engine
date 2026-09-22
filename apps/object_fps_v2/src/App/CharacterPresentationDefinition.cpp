#include "RetroFPS/App/CharacterPresentationDefinition.hpp"

#include "AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"

#include <algorithm>
#include <cmath>

namespace fps {

std::shared_ptr<const CharacterPresentationDefinition> LoadCharacterPresentationDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& presentationId,
    std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(
            assets, presentationId, Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        RequireVersionOne(config);
        auto definition = std::make_shared<CharacterPresentationDefinition>();
        definition->modelAssetId = ReadAssetId(config.at("model_asset_id"));
        definition->model = LoadShared<Engine::Model::ModelAsset>(
            assets, definition->modelAssetId, Engine::Asset::AssetType::FromString("model"));
        if (config.contains("animation_set_asset_id")) {
            definition->animationSetAssetId = ReadAssetId(config.at("animation_set_asset_id"));
            std::string bindingError;
            definition->animationSet = LoadAnimationSetDefinition(
                assets, *definition->animationSetAssetId, bindingError);
            if (!definition->animationSet || !ValidateAnimationSetBinding(
                    *definition->animationSet, definition->modelAssetId, definition->model, bindingError)) {
                throw std::runtime_error(bindingError);
            }
        }
        for (const auto& material : definition->model->materials) {
            MaterialBinding binding;
            binding.baseColorLinear = material.baseColorLinear;
            definition->materials.push_back(binding);
        }
        if (config.contains("material_overrides")) {
            const auto& overrides = config.at("material_overrides");
            if (!overrides.is_object()) throw std::runtime_error("material_overrides must be a material-slot object");
            for (const auto& [slot, values] : overrides.items()) {
                try {
                    const auto& materials = definition->model->materials;
                    const auto match = [&](const auto& material) { return material.name == slot; };
                    const auto material = std::find_if(materials.begin(), materials.end(), match);
                    if (material == materials.end()) throw std::runtime_error("unknown material slot");
                    if (std::count_if(materials.begin(), materials.end(), match) != 1) {
                        throw std::runtime_error("ambiguous material slot name");
                    }
                    if (!values.is_object()) throw std::runtime_error("override must be an object");
                    auto& binding = definition->materials[static_cast<std::size_t>(material - materials.begin())];
                    if (values.contains("base_color_linear")) {
                        const auto& color = values.at("base_color_linear");
                        if (!color.is_array() || color.size() != 4) {
                            throw std::runtime_error("base_color_linear must contain exactly four finite numbers");
                        }
                        for (std::size_t channel = 0; channel < 4; ++channel) {
                            binding.baseColorLinear[channel] = color.at(channel).get<float>();
                            if (!std::isfinite(binding.baseColorLinear[channel])) {
                                throw std::runtime_error("base_color_linear must contain exactly four finite numbers");
                            }
                        }
                    }
                    if (values.contains("texture_asset_id")) {
                        binding.textureAssetId = ReadAssetId(values.at("texture_asset_id"));
                        // Validate existence, declared type and decoding through the normal pipeline.
                        // Only the application AssetId is retained in the binding, never importer paths.
                        (void)LoadShared<Engine::Asset::Loaders::TextureAsset>(
                            assets, *binding.textureAssetId, Engine::Asset::AssetType::Texture());
                    }
                    const auto sampler = values.value("sampler", "linear_clamp");
                    if (sampler == "linear_wrap") binding.sampler = Engine::Render::SamplerMode::LinearWrap;
                    else if (sampler != "linear_clamp") throw std::runtime_error("unsupported sampler '" + sampler + "'");
                } catch (const std::exception& exception) {
                    throw std::runtime_error("model '" + definition->modelAssetId.debugName +
                        "', material slot '" + slot + "': " + exception.what());
                }
            }
        }
        return definition;
    } catch (const std::exception& exception) {
        error = "failed to load character presentation '" + presentationId.debugName + "': " + exception.what();
        return {};
    }
}

} // namespace fps
