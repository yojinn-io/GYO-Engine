#include "RetroFPS/App/WeaponViewModel.hpp"
#include "render/RenderQueue.hpp"
#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponState.hpp"

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "model/Animation.hpp"
#include "model_renderer/ModelRenderer.hpp"
#include "render/IRenderDevice.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace fps {
namespace {

std::size_t ClipSlot(WeaponAction action) {
    switch (action) {
    case WeaponAction::Shoot: return 1;
    case WeaponAction::Reload: return 2;
    case WeaponAction::Draw: return 3;
    case WeaponAction::Hide: return 4;
    default: return 0;
    }
}

std::size_t ClipSlot(WeaponViewModelAction action) {
    switch (action) {
    case WeaponViewModelAction::Idle: return 0;
    case WeaponViewModelAction::Shoot: return 1;
    case WeaponViewModelAction::Reload: return 2;
    case WeaponViewModelAction::Draw: return 3;
    }
    return static_cast<std::size_t>(-1);
}

} // namespace

struct WeaponViewModel::Impl final {
    Engine::Render::IRenderDevice* device{};
    Engine::Asset::AssetManager* assets{};
    std::vector<Engine::Asset::AssetHandle> assetHandles;
    std::shared_ptr<Engine::ModelRenderer::ModelResource> modelResource;
    std::unique_ptr<Engine::ModelRenderer::ModelInstance> modelInstance;
    std::shared_ptr<const WeaponPresentationDefinition> definition;
    Engine::Model::Pose pose;
    Engine::Render::Float3 muzzleViewCameraPosition{};
    std::size_t lastClip{static_cast<std::size_t>(-1)};
    double lastTime{-1.0};
    std::uint64_t poseRevision{}, submissionCount{};
    std::size_t submittedMeshCount{};
    float recoilRadians{};

    ~Impl() {
        modelInstance.reset();
        modelResource.reset();
        if (assets) for (auto handle : assetHandles) assets->Release(handle);
    }

    template<class T>
    std::shared_ptr<const T> Load(const Engine::Asset::AssetId& id,
                                  Engine::Asset::AssetType type) {
        const auto result = assets->Load(
            id, Engine::Asset::AssetRequest::WithTypeHint(type));
        if (!result) {
            throw std::runtime_error("asset '" + id.debugName + "': " +
                                     result.error().message + " " + result.error().detail);
        }
        assetHandles.push_back(result.value());
        auto data = assets->GetSharedConst<T>(result.value());
        if (!data) throw std::runtime_error("asset '" + id.debugName + "' has the wrong payload");
        return data;
    }

    void Evaluate(std::size_t clip, double time) {
        const auto sampled = Engine::Model::SamplePose(
            *definition->model, clip, time, Engine::Model::PlaybackMode::Clamp, pose);
        if (!sampled) throw std::runtime_error(sampled.error());
        muzzleViewCameraPosition = EvaluateWeaponMuzzleViewCameraPosition(*definition, pose);
    }

    void Initialize(const Engine::Asset::AssetId& id) {
        std::string error;
        definition = LoadWeaponPresentationDefinition(*assets, id, error);
        if (!definition) throw std::runtime_error(error);
        Evaluate(definition->clips[0], 0.0);
        std::vector<Engine::ModelRenderer::ModelMaterial> materials;
        for (const auto& textureId : definition->materialTextureAssetIds) {
            const auto texture = Load<Engine::Asset::Loaders::TextureAsset>(
                textureId, Engine::Asset::AssetType::Texture());
            if (!texture->width || !texture->height || texture->rgba.size() !=
                static_cast<std::size_t>(texture->width) * texture->height * 4U) {
                throw std::runtime_error("viewmodel texture has invalid decoded pixels");
            }
            Engine::ModelRenderer::ModelMaterial material;
            material.texture = Engine::Render::ImageView{
                texture->width, texture->height, texture->width * 4U,
                std::as_bytes(std::span<const std::uint8_t>(texture->rgba)),
                Engine::Render::TextureColorSpace::SRgb};
            material.sampler = definition->sampler;
            materials.push_back(material);
        }
        auto resource = Engine::ModelRenderer::ModelResource::Create(*device, definition->model, materials);
        if (!resource) throw std::runtime_error(resource.error());
        modelResource = std::move(resource.value());
        auto instance = Engine::ModelRenderer::ModelInstance::Create(modelResource, pose,
            {-definition->idleAnchor.x, -definition->idleAnchor.y, -definition->idleAnchor.z});
        if (!instance) throw std::runtime_error(instance.error());
        modelInstance = std::move(instance.value());
        lastClip = definition->clips[0];
        lastTime = 0.0;
        poseRevision = 1;
    }

    bool Submit(std::size_t slot, float elapsedSeconds, float durationSeconds,
                float recoil,
                Engine::Render::RenderQueue& queue, std::string& error) {
        submittedMeshCount = 0;
        if (slot >= definition->clips.size()) {
            throw std::runtime_error("weapon frame has an invalid action");
        }
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0 ||
            !std::isfinite(durationSeconds) || durationSeconds < 0 ||
            !std::isfinite(recoil)) {
            throw std::runtime_error("weapon snapshot has an invalid action time");
        }
        const std::size_t clip = definition->clips[slot];
        const double progress = durationSeconds > 0
            ? std::clamp(static_cast<double>(elapsedSeconds) /
                         durationSeconds, 0.0, 1.0) : 0.0;
        const double time = slot == 0
            ? 0.0 : definition->model->clips[clip].durationSeconds * progress;
        if (clip != lastClip || time != lastTime) {
            Evaluate(clip, time);
            const auto updated = modelInstance->UpdatePose(pose);
            if (!updated) throw std::runtime_error(updated.error());
            lastClip = clip;
            lastTime = time;
            ++poseRevision;
        }
        auto placement = definition->placement;
        placement.rotationRadians.x += recoil;
        queue.SetViewModelCamera(definition->camera);
        const auto before = queue.Meshes().size();
        const auto submitted = modelInstance->Submit(queue, placement,
            Engine::Render::MeshLayer::ViewModel);
        if (!submitted) {
            error = submitted.error();
            return false;
        }
        muzzleViewCameraPosition = EvaluateWeaponMuzzleViewCameraPosition(*definition, pose, placement);
        submittedMeshCount = queue.Meshes().size() - before;
        ++submissionCount;
        recoilRadians = recoil;
        return true;
    }
};

WeaponViewModel::WeaponViewModel() = default;
WeaponViewModel::~WeaponViewModel() = default;

Engine::Render::Float3 WeaponViewModel::GetMuzzleViewCameraPosition() const noexcept {
    return impl_ ? impl_->muzzleViewCameraPosition : Engine::Render::Float3{};
}

float WeaponViewModel::GetActionDurationSeconds(WeaponViewModelAction action) const noexcept {
    const auto slot = ClipSlot(action);
    if (!impl_ || slot >= impl_->definition->clips.size()) return 0.0F;
    return static_cast<float>(impl_->definition->model->clips[impl_->definition->clips[slot]].durationSeconds);
}

WeaponViewModelObservation WeaponViewModel::GetObservation() const {
    if (!impl_) return {};
    return {true, impl_->definition->model->meshes.size(),
        impl_->definition->materialTextureAssetIds.size(), impl_->submittedMeshCount,
        impl_->submissionCount, impl_->poseRevision,
        impl_->definition->model->clips[impl_->lastClip].name,
        impl_->lastTime, impl_->recoilRadians};
}

bool WeaponViewModel::Initialize(Engine::Render::IRenderDevice& device,
                                 Engine::Asset::AssetManager& assets,
                                 const Engine::Asset::AssetId& id,
                                 std::string& error) {
    error.clear();
    impl_.reset();
    try {
        auto next = std::make_unique<Impl>();
        next->device = &device;
        next->assets = &assets;
        next->Initialize(id);
        impl_ = std::move(next);
        return true;
    } catch (const std::exception& exception) {
        error = "failed to initialize weapon viewmodel '" + id.debugName + "': " + exception.what();
        return false;
    }
}

bool WeaponViewModel::Submit(const WeaponPresentationSnapshot& snapshot,
                            Engine::Render::RenderQueue& queue, std::string& error) {
    error.clear();
    if (!impl_) { error = "weapon viewmodel is not initialized"; return false; }
    try {
        if (snapshot.action == WeaponAction::Holstered) {
            impl_->submittedMeshCount = 0;
            return true;
        }
        return impl_->Submit(ClipSlot(snapshot.action), snapshot.elapsedSeconds,
            snapshot.durationSeconds, 0.0F, queue, error);
    } catch (const std::exception& exception) {
        error = "failed to present weapon viewmodel: " + std::string(exception.what());
        return false;
    }
}

bool WeaponViewModel::Submit(const WeaponViewModelFrame& frame,
                            Engine::Render::RenderQueue& queue, std::string& error) {
    error.clear();
    if (!impl_) { error = "weapon viewmodel is not initialized"; return false; }
    try {
        return impl_->Submit(ClipSlot(frame.action), frame.elapsedSeconds,
            frame.durationSeconds, frame.recoilRadians, queue, error);
    } catch (const std::exception& exception) {
        error = "failed to present weapon viewmodel: " + std::string(exception.what());
        return false;
    }
}

} // namespace fps
