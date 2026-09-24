#include "RetroFPS/App/WeaponViewModel.hpp"
#include "render/RenderQueue.hpp"
#include "RetroFPS/App/WeaponPresentationDefinition.hpp"

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
    }

    bool Submit(const WeaponPresentationSnapshot& snapshot,
                Engine::Render::RenderQueue& queue, std::string& error) {
        if (snapshot.action == WeaponAction::Holstered) return true;
        if (!std::isfinite(snapshot.elapsedSeconds) || snapshot.elapsedSeconds < 0 ||
            !std::isfinite(snapshot.durationSeconds) || snapshot.durationSeconds < 0) {
            throw std::runtime_error("weapon snapshot has an invalid action time");
        }
        const std::size_t clip = definition->clips[ClipSlot(snapshot.action)];
        const double progress = snapshot.durationSeconds > 0
            ? std::clamp(static_cast<double>(snapshot.elapsedSeconds) /
                         snapshot.durationSeconds, 0.0, 1.0) : 0.0;
        const double time = snapshot.action == WeaponAction::Idle
            ? 0.0 : definition->model->clips[clip].durationSeconds * progress;
        if (clip != lastClip || time != lastTime) {
            Evaluate(clip, time);
            const auto updated = modelInstance->UpdatePose(pose);
            if (!updated) throw std::runtime_error(updated.error());
            lastClip = clip;
            lastTime = time;
        }
        queue.SetViewModelCamera(definition->camera);
        const auto submitted = modelInstance->Submit(queue, definition->placement,
            Engine::Render::MeshLayer::ViewModel);
        if (!submitted) {
            error = submitted.error();
            return false;
        }
        return true;
    }
};

WeaponViewModel::WeaponViewModel() = default;
WeaponViewModel::~WeaponViewModel() = default;

Engine::Render::Float3 WeaponViewModel::GetMuzzleViewCameraPosition() const noexcept {
    return impl_ ? impl_->muzzleViewCameraPosition : Engine::Render::Float3{};
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
    if (!impl_) { error = "weapon viewmodel is not initialized"; return false; }
    try {
        return impl_->Submit(snapshot, queue, error);
    } catch (const std::exception& exception) {
        error = "failed to present weapon viewmodel: " + std::string(exception.what());
        return false;
    }
}

} // namespace fps
