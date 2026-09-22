#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/App/AnimationSetDefinition.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace fps {
namespace {

constexpr std::array<const char*, 5> kActions{"Idle", "Shoot", "Reload", "Draw", "Hide"};

bool Finite(Engine::Render::Float3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Engine::Render::Float3 ReadVector(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 3) {
        throw std::runtime_error("viewmodel vector must contain exactly three numbers");
    }
    const Engine::Render::Float3 result{
        json.at(0).get<float>(), json.at(1).get<float>(), json.at(2).get<float>()};
    if (!Finite(result)) throw std::runtime_error("viewmodel vector must be finite");
    return result;
}

Engine::Asset::AssetId ReadAssetId(const nlohmann::json& json) {
    const auto name = json.get<std::string>();
    if (name.empty()) throw std::runtime_error("viewmodel asset ID must not be empty");
    return Engine::Asset::AssetId::FromString(name);
}

template<class T>
std::shared_ptr<const T> LoadShared(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& id,
    Engine::Asset::AssetType type) {
    const auto loaded = assets.Load(id, Engine::Asset::AssetRequest::WithTypeHint(type));
    if (!loaded) {
        throw std::runtime_error("asset '" + id.debugName + "': " +
                                 loaded.error().message + " " + loaded.error().detail);
    }
    auto payload = assets.GetSharedConst<T>(loaded.value());
    // The immutable payload owns its storage independently of a cache handle.
    assets.Release(loaded.value());
    if (!payload) throw std::runtime_error("asset '" + id.debugName + "' has the wrong payload");
    return payload;
}

} // namespace

Engine::Render::Float3 EvaluateWeaponMuzzleViewCameraPosition(
    const WeaponPresentationDefinition& definition,
    const Engine::Model::Pose& pose) {
    if (definition.muzzleNodeIndex >= pose.globalTransforms.size()) {
        throw std::runtime_error("viewmodel muzzle requires a complete model pose");
    }
    const auto point = Engine::Model::TransformPoint(
        pose.globalTransforms[definition.muzzleNodeIndex], definition.muzzleLocalPosition);
    const auto& transform = definition.placement;
    Engine::Render::Float3 result{
        (point.x - definition.idleAnchor.x) * transform.scale.x,
        (point.y - definition.idleAnchor.y) * transform.scale.y,
        (point.z - definition.idleAnchor.z) * transform.scale.z};
    const float cx = std::cos(transform.rotationRadians.x), sx = std::sin(transform.rotationRadians.x);
    result = {result.x, result.y * cx - result.z * sx, result.y * sx + result.z * cx};
    const float cy = std::cos(transform.rotationRadians.y), sy = std::sin(transform.rotationRadians.y);
    result = {result.x * cy + result.z * sy, result.y, -result.x * sy + result.z * cy};
    const float cz = std::cos(transform.rotationRadians.z), sz = std::sin(transform.rotationRadians.z);
    result = {result.x * cz - result.y * sz, result.x * sz + result.y * cz, result.z};
    return {result.x + transform.translation.x, result.y + transform.translation.y,
            result.z + transform.translation.z};
}

std::shared_ptr<const WeaponPresentationDefinition> LoadWeaponPresentationDefinition(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& presentationId,
    std::string& error) {
    error.clear();
    try {
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(
            assets, presentationId, Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        if (config.at("version").get<int>() != 1) {
            throw std::runtime_error("unsupported weapon presentation version");
        }
        auto definition = std::make_shared<WeaponPresentationDefinition>();
        const auto modelId = ReadAssetId(config.at("model_asset_id"));
        definition->model = LoadShared<Engine::Model::ModelAsset>(
            assets, modelId, Engine::Asset::AssetType::FromString("model"));
        if (config.contains("clips") == config.contains("animation_set_asset_id")) {
            throw std::runtime_error("provide exactly one of clips or animation_set_asset_id");
        }
        if (config.contains("animation_set_asset_id")) {
            std::string bindingError;
            const auto animationSet = LoadAnimationSetDefinition(
                assets, ReadAssetId(config.at("animation_set_asset_id")), bindingError);
            if (!animationSet || !ValidateAnimationSetBinding(
                    *animationSet, modelId, definition->model, bindingError)) {
                throw std::runtime_error(bindingError);
            }
            for (std::size_t slot = 0; slot < kActions.size(); ++slot) {
                const auto clip = animationSet->clips.find(kActions[slot]);
                if (clip == animationSet->clips.end()) {
                    throw std::runtime_error("animation set is missing weapon action '" +
                                             std::string(kActions[slot]) + "'");
                }
                definition->clips[slot] = clip->second.clipIndex;
            }
        } else {
            for (std::size_t slot = 0; slot < kActions.size(); ++slot) {
                const auto name = config.at("clips").at(kActions[slot]).get<std::string>();
                const auto clip = definition->model->FindClip(name);
                if (!clip) throw std::runtime_error("model '" + modelId.debugName +
                                                   "' is missing animation '" + name + "'");
                definition->clips[slot] = *clip;
            }
        }
        const auto anchor = definition->model->FindNode(config.at("anchor_node").get<std::string>());
        if (!anchor) throw std::runtime_error("model is missing the viewmodel anchor node");
        const auto muzzle = definition->model->FindNode(config.at("muzzle").at("node").get<std::string>());
        if (!muzzle) throw std::runtime_error("model is missing the viewmodel muzzle node");
        definition->muzzleNodeIndex = *muzzle;
        const auto offset = ReadVector(config.at("muzzle").at("local_position_meters"));
        definition->muzzleLocalPosition = {offset.x, offset.y, offset.z};

        definition->placement.translation = ReadVector(config.at("offset_meters"));
        const auto rotation = ReadVector(config.at("rotation_degrees"));
        constexpr float radians = std::numbers::pi_v<float> / 180.0F;
        definition->placement.rotationRadians = {rotation.x * radians, rotation.y * radians, rotation.z * radians};
        const float scale = config.at("scale").get<float>();
        const float fov = config.at("vertical_fov_degrees").get<float>();
        definition->camera.nearClip = config.at("near_clip").get<float>();
        definition->camera.farClip = config.at("far_clip").get<float>();
        if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(fov) || fov <= 0 || fov >= 179 ||
            !std::isfinite(definition->camera.nearClip) || !std::isfinite(definition->camera.farClip) ||
            definition->camera.nearClip <= 0 || definition->camera.farClip <= definition->camera.nearClip) {
            throw std::runtime_error("invalid viewmodel scale or camera clipping/FOV");
        }
        definition->placement.scale = {scale, scale, scale};
        definition->camera.verticalFieldOfViewRadians = fov * radians;
        const auto sampler = config.value("sampler", "linear_clamp");
        if (sampler == "linear_wrap") definition->sampler = Engine::Render::SamplerMode::LinearWrap;
        else if (sampler != "linear_clamp") throw std::runtime_error("unsupported viewmodel sampler '" + sampler + "'");
        const auto& materials = config.at("materials");
        if (!materials.is_object()) throw std::runtime_error("materials must be a slot-to-texture object");
        for (const auto& [slot, value] : materials.items()) {
            bool found = false;
            for (const auto& material : definition->model->materials) {
                if (material.name == slot) { found = true; break; }
            }
            if (!found) throw std::runtime_error("model '" + modelId.debugName +
                                                "' has no material slot '" + slot + "'");
        }
        for (const auto& material : definition->model->materials) {
            definition->materialTextureAssetIds.push_back(ReadAssetId(materials.at(material.name)));
        }

        Engine::Model::Pose pose;
        const auto idle = Engine::Model::SamplePose(
            *definition->model, definition->clips[0], 0, Engine::Model::PlaybackMode::Clamp, pose);
        if (!idle) throw std::runtime_error(idle.error());
        definition->idleAnchor = Engine::Model::TransformPoint(pose.globalTransforms[*anchor], {});
        const auto shoot = Engine::Model::SamplePose(
            *definition->model, definition->clips[1], 0, Engine::Model::PlaybackMode::Clamp, pose);
        if (!shoot) throw std::runtime_error(shoot.error());
        const auto point = EvaluateWeaponMuzzleViewCameraPosition(*definition, pose);
        if (!Finite(point) || point.z <= definition->camera.nearClip) {
            throw std::runtime_error("viewmodel muzzle must be finite and beyond the camera near clip");
        }
        definition->shotGeometry = {{point.x, point.y, point.z}, definition->camera.verticalFieldOfViewRadians};
        return definition;
    } catch (const std::exception& exception) {
        error = "failed to load weapon presentation '" + presentationId.debugName + "': " + exception.what();
        return {};
    }
}

} // namespace fps
