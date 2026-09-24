#include "RetroFPS/App/CharacterPresentationDefinition.hpp"

#include "AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "model/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_set>

namespace fps {
namespace {

// Name selection is product content policy. The engine receives only explicit
// node pairs and applies reference-pose corrections to an owning clip copy.
void BindCompatibleAnimations(const nlohmann::json& config,
                              CharacterPresentationDefinition& definition) {
    if (config.at("mode").get<std::string>() != "compatible_reference_pose")
        throw std::runtime_error("unsupported character animation binding mode");
    const float translationScale = config.at("translation_scale").get<float>();
    std::unordered_set<std::string> ignoredNames;
    if (!config.at("ignored_source_nodes").is_array())
        throw std::runtime_error("ignored_source_nodes must be an array of node names");
    for (const auto& name : config.at("ignored_source_nodes")) {
        if (!ignoredNames.insert(name.get<std::string>()).second)
            throw std::runtime_error("duplicate ignored animation source node");
    }
    auto model = std::make_shared<Engine::Model::ModelAsset>(*definition.model);
    auto boundSet = std::make_shared<AnimationSetDefinition>();
    // Stable semantic order also makes assembled clip indices deterministic.
    std::map<std::string, AnimationClipReference> ordered(
        definition.animationSet->clips.begin(), definition.animationSet->clips.end());
    std::map<std::pair<Engine::Asset::AssetId::ValueType, std::size_t>, std::size_t> transferred;
    for (const auto& [semantic, reference] : ordered) {
        const auto key = std::make_pair(reference.modelAssetId.value, reference.clipIndex);
        auto existing = transferred.find(key);
        std::size_t clipIndex{};
        if (existing != transferred.end()) {
            clipIndex = existing->second;
        } else {
            std::vector<Engine::Model::AnimationNodeBinding> nodes;
            std::vector<std::size_t> excluded;
            std::unordered_set<std::string> sourceNames;
            std::unordered_set<std::string> foundIgnored;
            for (std::size_t source = 0; source < reference.model->nodes.size(); ++source) {
                const auto& name = reference.model->nodes[source].name;
                if (!sourceNames.insert(name).second)
                    throw std::runtime_error("ambiguous animation source node: " + name);
                if (ignoredNames.contains(name)) {
                    excluded.push_back(source);
                    foundIgnored.insert(name);
                    continue;
                }
                const auto target = model->FindNode(name);
                if (!target || std::count_if(model->nodes.begin(), model->nodes.end(),
                        [&](const auto& node) { return node.name == name; }) != 1)
                    throw std::runtime_error("missing or ambiguous animation target node: " + name);
                nodes.push_back({source, *target});
            }
            if (foundIgnored != ignoredNames)
                throw std::runtime_error("ignored animation node is absent from its source");
            auto clip = Engine::Model::TransferCompatibleAnimation(
                *reference.model, *model, reference.clipIndex, nodes, translationScale, excluded);
            if (!clip) throw std::runtime_error("animation '" + semantic + "': " + clip.error());
            if (model->FindClip(clip.value().name))
                throw std::runtime_error("assembled animation clip name is ambiguous: " + clip.value().name);
            clipIndex = model->clips.size();
            model->clips.push_back(std::move(clip.value()));
            transferred.emplace(key, clipIndex);
        }
        boundSet->clips.emplace(semantic,
            AnimationClipReference{definition.modelAssetId, model, clipIndex});
    }
    const auto valid = Engine::Model::ValidateModel(*model);
    if (!valid) throw std::runtime_error(valid.error());
    definition.model = std::move(model);
    definition.animationSet = std::move(boundSet);
}

} // namespace

static std::shared_ptr<const CharacterPresentationDefinition> LoadCharacterDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& presentationId,
    std::string& error, bool allowAccessories) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(
            assets, presentationId, Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        RequireVersionOne(config);
        if (!allowAccessories && config.contains("accessories"))
            throw std::runtime_error("nested character accessories are unsupported");
        auto definition = std::make_shared<CharacterPresentationDefinition>();
        definition->modelAssetId = ReadAssetId(config.at("model_asset_id"));
        definition->model = LoadShared<Engine::Model::ModelAsset>(
            assets, definition->modelAssetId, Engine::Asset::AssetType::FromString("model"));
        if (config.contains("animation_binding") && !config.contains("animation_set_asset_id"))
            throw std::runtime_error("animation_binding requires an animation set");
        if (config.contains("animation_set_asset_id")) {
            definition->animationSetAssetId = ReadAssetId(config.at("animation_set_asset_id"));
            std::string bindingError;
            definition->animationSet = LoadAnimationSetDefinition(
                assets, *definition->animationSetAssetId, bindingError);
            if (definition->animationSet && config.contains("animation_binding"))
                BindCompatibleAnimations(config.at("animation_binding"), *definition);
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
        if (config.contains("accessories")) {
            const auto& entries = config.at("accessories");
            if (!entries.is_array()) throw std::runtime_error("accessories must be an array");
            const auto nodeIndex = [](const Engine::Model::ModelAsset& model, const std::string& name) {
                const auto index = model.FindNode(name);
                if (!index || std::count_if(model.nodes.begin(), model.nodes.end(),
                        [&](const auto& node) { return node.name == name; }) != 1)
                    throw std::runtime_error("missing or ambiguous accessory bone: " + name);
                return *index;
            };
            for (const auto& entry : entries) {
                CharacterAccessoryDefinition accessory;
                accessory.presentation = LoadCharacterDefinition(assets,
                    ReadAssetId(entry.at("character_asset_id")), error, false);
                if (!accessory.presentation) throw std::runtime_error(error);
                const auto& source = *accessory.presentation->model;
                if (accessory.presentation->animationSet || !source.clips.empty())
                    throw std::runtime_error("character accessories use the parent pose, not their own animation");
                accessory.targetNode = nodeIndex(*definition->model, entry.at("node").get<std::string>());
                accessory.sourceNode = nodeIndex(source, entry.at("source_node").get<std::string>());
                // This bounded contract supports rigid single-bone accessories,
                // not an independently animated skeleton or unweighted geometry.
                for (const auto& mesh : source.meshes) for (const auto& vertex : mesh.vertices) {
                    float weight = 0;
                    for (std::size_t i = 0; i < vertex.weights.size(); ++i) {
                        if (vertex.weights[i] == 0) continue;
                        if (mesh.joints.at(vertex.joints[i]).nodeIndex != accessory.sourceNode)
                            throw std::runtime_error("accessory vertices must bind only to source_node");
                        weight += vertex.weights[i];
                    }
                    if (std::abs(weight - 1.0F) > 0.0001F)
                        throw std::runtime_error("accessory vertices require complete source_node weights");
                }
                if (entry.contains("translation")) {
                    const auto& p = entry.at("translation");
                    if (!p.is_array() || p.size() != 3)
                        throw std::runtime_error("accessory translation requires three coordinates");
                    accessory.placement.translation = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                }
                const float scale = entry.value("scale", 1.0F);
                const auto p = accessory.placement.translation;
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    !std::isfinite(scale) || scale <= 0)
                    throw std::runtime_error("accessory placement must be finite with positive scale");
                accessory.placement.scale = {scale, scale, scale};
                const auto made = Engine::Model::MakeDefaultPose(source, accessory.referencePose);
                if (!made) throw std::runtime_error(made.error());
                definition->accessories.push_back(std::move(accessory));
            }
        }
        return definition;
    } catch (const std::exception& exception) {
        error = "failed to load character presentation '" + presentationId.debugName + "': " + exception.what();
        return {};
    }
}

std::shared_ptr<const CharacterPresentationDefinition> LoadCharacterPresentationDefinition(
    Engine::Asset::AssetManager& assets, const Engine::Asset::AssetId& presentationId,
    std::string& error) {
    return LoadCharacterDefinition(assets, presentationId, error, true);
}

Engine::Model::Pose BuildCharacterAccessoryPose(
    const CharacterAccessoryDefinition& accessory, const Engine::Model::Pose& characterPose) {
    if (!accessory.presentation || accessory.sourceNode >= accessory.referencePose.globalTransforms.size() ||
        accessory.targetNode >= characterPose.globalTransforms.size())
        throw std::invalid_argument("accessory requires its resolved bind pose and the character pose");
    auto pose = accessory.referencePose;
    pose.globalTransforms[accessory.sourceNode] = Engine::Model::Multiply(
        characterPose.globalTransforms[accessory.targetNode], Engine::Model::ToMatrix(accessory.placement));
    return pose;
}

} // namespace fps
