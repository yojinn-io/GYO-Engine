#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/App/AnimationSetDefinition.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Angle.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>

namespace fps {
namespace {

constexpr std::array<const char*, 5> kActions{"Idle", "Shoot", "Reload", "Draw", "Hide"};

Engine::Math::Vec3 ReadVector(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 3) {
        throw std::runtime_error("viewmodel vector must contain exactly three numbers");
    }
    const Engine::Math::Vec3 result{
        json.at(0).get<float>(), json.at(1).get<float>(), json.at(2).get<float>()};
    if (!Engine::Math::IsFinite(result)) throw std::runtime_error("viewmodel vector must be finite");
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

Engine::Math::Vec3 EvaluateWeaponMuzzleViewCameraPosition(
    const WeaponPresentationDefinition& definition,
    const Engine::Model::Pose& pose) {
    return EvaluateWeaponMuzzleViewCameraPosition(definition, pose, definition.placement);
}

Engine::Math::Vec3 EvaluateWeaponMuzzleViewCameraPosition(
    const WeaponPresentationDefinition& definition,
    const Engine::Model::Pose& pose,
    const Engine::Render::Transform3D& transform) {
    if (definition.muzzleNodeIndex >= pose.globalTransforms.size()) {
        throw std::runtime_error("viewmodel muzzle requires a complete model pose");
    }
    const auto point = Engine::Math::TransformPoint(
        pose.globalTransforms[definition.muzzleNodeIndex], definition.muzzleLocalPosition);
    // The render submission contract: the instance offsets vertices by
    // -idleAnchor, then the renderer applies ComposeEulerXYZ(transform).
    return Engine::Math::TransformPoint(
        Engine::Math::ComposeEulerXYZ(transform.translation, transform.rotationRadians, transform.scale),
        point - definition.idleAnchor);
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
        definition->muzzleLocalPosition = offset;

        definition->placement.translation = ReadVector(config.at("offset_meters"));
        const auto rotation = ReadVector(config.at("rotation_degrees"));
        definition->placement.rotationRadians = {Engine::Math::DegreesToRadians(rotation.x),
            Engine::Math::DegreesToRadians(rotation.y), Engine::Math::DegreesToRadians(rotation.z)};
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
        definition->camera.verticalFieldOfViewRadians = Engine::Math::DegreesToRadians(fov);
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
        definition->idleAnchor = Engine::Math::TransformPoint(pose.globalTransforms[*anchor], {});
        const auto shoot = Engine::Model::SamplePose(
            *definition->model, definition->clips[1], 0, Engine::Model::PlaybackMode::Clamp, pose);
        if (!shoot) throw std::runtime_error(shoot.error());
        const auto point = EvaluateWeaponMuzzleViewCameraPosition(*definition, pose);
        if (!Engine::Math::IsFinite(point) || point.z <= definition->camera.nearClip) {
            throw std::runtime_error("viewmodel muzzle must be finite and beyond the camera near clip");
        }
        definition->shotGeometry = {point, definition->camera.verticalFieldOfViewRadians};
        return definition;
    } catch (const std::exception& exception) {
        error = "failed to load weapon presentation '" + presentationId.debugName + "': " + exception.what();
        return {};
    }
}

} // namespace fps
