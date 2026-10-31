#include "RetroFPS/Pvp/PlayerPresentation.hpp"

#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/PlayerAnimationContent.hpp"
#include "../App/AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "engine/render/model/ModelRenderer.hpp"
#include "engine/render/IRenderDevice.hpp"
#include "engine/render/RenderQueue.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace fps::pvp {
namespace {
using Engine::Model::Pose;
using Engine::Math::Vec3;
using Resource = Engine::ModelRenderer::ModelResource;
using Instance = Engine::ModelRenderer::ModelInstance;

template <class E>
void Require(const Engine::Base::Result<void, E>& result) {
    if (!result) throw std::runtime_error(result.error().message);
}

std::shared_ptr<Resource> CreateResource(Engine::Render::IRenderDevice& device,
    Engine::Asset::AssetManager& assets, const CharacterPresentationDefinition& character) {
    std::vector<Engine::ModelRenderer::ModelMaterial> materials;
    std::vector<std::shared_ptr<const Engine::Asset::Loaders::TextureAsset>> pixels;
    for (const auto& binding : character.materials) {
        const auto& c = binding.baseColorLinear;
        Engine::ModelRenderer::ModelMaterial material;
        material.tint = {c[0], c[1], c[2], c[3]};
        material.sampler = binding.sampler;
        if (binding.textureAssetId) {
            auto texture = asset_definition_detail::LoadShared<Engine::Asset::Loaders::TextureAsset>(
                assets, *binding.textureAssetId, Engine::Asset::AssetType::Texture());
            material.texture = Engine::Render::ImageView{texture->width, texture->height,
                texture->width * 4, std::as_bytes(std::span<const std::uint8_t>(texture->rgba)),
                Engine::Render::TextureColorSpace::SRgb};
            pixels.push_back(std::move(texture));
        }
        materials.push_back(material);
    }
    auto made = Resource::Create(device, character.model, materials);
    if (!made) throw std::runtime_error(made.error().message);
    return std::move(made.value());
}
} // namespace

struct PlayerPresentation::Impl final {
    struct Slot final {
        std::uint64_t playerId{}, poseRevision{1};
        PlayerLocomotionState locomotion;
        PlayerPresentationPose pose;
        std::unique_ptr<Instance> body, weapon;
        std::vector<std::unique_ptr<Instance>> accessories;
        bool idlePrepared{true};
    };
    std::shared_ptr<const PlayerPresentationDefinition> definition;
    std::uint64_t animationDigest{};
    std::shared_ptr<Resource> bodyResource, weaponResource;
    std::vector<std::shared_ptr<Resource>> accessoryResources;
    PlayerPresentationPose initialPose;
    std::vector<Slot> slots;
    std::vector<PlayerPresentationObservation> observations;

    void Upload(Slot& slot, const PlayerPresentationPose& pose) {
        Require(slot.body->UpdatePose(pose.body));
        Require(slot.weapon->UpdatePose(pose.weapon));
        for (std::size_t i = 0; i < slot.accessories.size(); ++i)
            Require(slot.accessories[i]->UpdatePose(pose.accessories[i]));
        ++slot.poseRevision;
    }
    void Idle(Slot& slot) {
        slot.playerId = 0;
        slot.locomotion = {};
        if (!slot.idlePrepared) Upload(slot, initialPose);
        slot.pose = initialPose;
        slot.idlePrepared = true;
    }
    void SubmitSlot(Slot& slot, const Engine::Render::Transform3D& transform,
                    Engine::Render::RenderQueue& queue) {
        Require(slot.body->Submit(queue, transform));
        Require(slot.weapon->Submit(queue, transform));
        for (const auto& accessory : slot.accessories) Require(accessory->Submit(queue, transform));
    }
};

PlayerPresentation::PlayerPresentation() = default;
PlayerPresentation::~PlayerPresentation() = default;
void PlayerPresentation::Reset() noexcept { impl_.reset(); }
bool PlayerPresentation::Ready() const noexcept { return impl_ != nullptr; }
std::uint64_t PlayerPresentation::AnimationDigest() const noexcept { return impl_ ? impl_->animationDigest : 0; }
std::size_t PlayerPresentation::Capacity() const noexcept { return impl_ ? impl_->slots.size() : 0; }
std::span<const PlayerPresentationObservation> PlayerPresentation::Observations() const noexcept {
    return impl_ ? std::span<const PlayerPresentationObservation>(impl_->observations) :
        std::span<const PlayerPresentationObservation>{};
}
void PlayerPresentation::ResetPlayers() noexcept {
    if (!impl_) return;
    impl_->observations.clear();
    for (auto& slot : impl_->slots) { slot.playerId = 0; slot.locomotion = {}; }
}

bool PlayerPresentation::Initialize(Engine::Render::IRenderDevice& device,
    Engine::Asset::AssetManager& assets, const float bodyHeight, const std::size_t capacity,
    std::string& error) {
    Reset();
    error.clear();
    try {
        if (!capacity) throw std::runtime_error("player presentation requires nonzero capacity");
        auto next = std::make_unique<Impl>();
        next->definition = LoadPlayerPresentationDefinition(assets, bodyHeight, error);
        if (!next->definition) throw std::runtime_error(error);
        const auto& definition = *next->definition;
        next->animationDigest = AnimationContentDigest(definition);
        next->bodyResource = CreateResource(device, assets, *definition.character);
        next->weaponResource = CreateResource(device, assets, *definition.weapon);
        for (const auto& accessory : definition.character->accessories)
            next->accessoryResources.push_back(CreateResource(device, assets, *accessory.presentation));
        if (!SamplePlayerPresentationPose(definition, {}, next->initialPose, error))
            throw std::runtime_error(error);
        const Vec3 offset = -definition.anchor;
        const auto instance = [&](const auto& resource, const Pose& pose) {
            auto created = Instance::Create(resource, pose, offset);
            if (!created) throw std::runtime_error(created.error().message);
            return std::move(created.value());
        };
        next->slots.reserve(capacity);
        next->observations.reserve(capacity);
        for (std::size_t i = 0; i < capacity; ++i) {
            Impl::Slot slot;
            slot.pose = next->initialPose;
            slot.body = instance(next->bodyResource, slot.pose.body);
            slot.weapon = instance(next->weaponResource, slot.pose.weapon);
            for (std::size_t accessory = 0; accessory < next->accessoryResources.size(); ++accessory)
                slot.accessories.push_back(instance(next->accessoryResources[accessory],
                    slot.pose.accessories[accessory]));
            next->slots.push_back(std::move(slot));
        }
        impl_ = std::move(next);
        return true;
    } catch (const std::exception& exception) {
        error = "player renderer initialize: " + std::string(exception.what());
        return false;
    }
}

bool PlayerPresentation::SubmitWarmup(Engine::Render::RenderQueue& queue, std::string& error) {
    error.clear();
    try {
        if (!impl_) throw std::runtime_error("player renderer is not initialized");
        const float scale = impl_->definition->scale;
        for (auto& slot : impl_->slots) {
            impl_->Idle(slot);
            impl_->SubmitSlot(slot, {{0, 0, 3}, {}, {scale, scale, scale}}, queue);
        }
        return true;
    } catch (const std::exception& exception) {
        error = "player renderer warmup: " + std::string(exception.what());
        return false;
    }
}

bool PlayerPresentation::Submit(const std::span<const PlayerPresentationFrame> players,
    Engine::Render::RenderQueue& queue, std::string& error) {
    error.clear();
    try {
        if (!impl_) throw std::runtime_error("player renderer is not initialized");
        auto& impl = *impl_;
        if (players.size() > impl.slots.size())
            throw std::runtime_error("player presentation exceeds prepared capacity");
        std::unordered_set<std::uint64_t> ids;
        for (const auto& player : players)
            if (!player.playerId || !ids.insert(player.playerId).second)
                throw std::runtime_error("player presentation requires unique nonzero IDs");
        impl.observations.clear();
        for (auto& slot : impl.slots)
            if (!ids.contains(slot.playerId)) impl.Idle(slot);
        const auto& definition = *impl.definition;
        const float scale = definition.scale;
        for (const auto& player : players) {
            auto found = std::find_if(impl.slots.begin(), impl.slots.end(),
                [&](const auto& slot) { return slot.playerId == player.playerId; });
            if (found == impl.slots.end()) {
                found = std::find_if(impl.slots.begin(), impl.slots.end(),
                    [](const auto& slot) { return slot.playerId == 0; });
                found->playerId = player.playerId;
            }
            auto& slot = *found;
            if (!AdvancePlayerLocomotion(slot.locomotion, player, definition, error))
                throw std::runtime_error(error);
            PlayerActionPose actions;
            if (!ResolvePlayerActions(definition, slot.locomotion, player, actions, error))
                throw std::runtime_error(error);
            // The prepared initial pose is the level aim pose: any pitch is sampled.
            const bool plain = actions.upper == PlayerUpperAction::Hold &&
                actions.lower == PlayerLowerAction::Locomotion && actions.aimWeight == 0;
            // A reset still shows the authority phase; only a rest pose with
            // no gait to show takes the prepared initial pose.
            if (slot.locomotion.phaseReset && plain && slot.locomotion.moveWeight == 0 &&
                slot.locomotion.idleSeconds == 0) {
                if (!slot.idlePrepared) impl.Upload(slot, impl.initialPose);
                slot.pose = impl.initialPose;
                slot.idlePrepared = true;
            } else {
                // A reset into death or the air still shows that action at once.
                if (!SamplePlayerPresentationPose(definition, slot.locomotion, actions, slot.pose, error))
                    throw std::runtime_error(error);
                impl.Upload(slot, slot.pose);
                slot.idlePrepared = false;
            }
            const Engine::Render::Transform3D transform{player.position, {0, player.yaw, 0}, {scale, scale, scale}};
            impl.SubmitSlot(slot, transform, queue);
            const auto& state = slot.locomotion;
            PlayerPresentationObservation observation;
            observation.ready = true;
            observation.playerId = player.playerId;
            observation.movementEpoch = player.movementEpoch;
            observation.lifeGeneration = player.lifeGeneration;
            observation.dead = player.dead;
            observation.jumpPhase = state.jumpPhase;
            observation.actions = actions;
            observation.jogging = state.jogging;
            observation.holding = state.holding;
            observation.backward = state.backward;
            observation.phaseReset = state.phaseReset;
            observation.phaseCycles = state.phaseCycles;
            observation.authorityPhaseCycles = state.authorityPhaseQ16 / 65536.0;
            observation.unwrappedPhaseCycles = state.unwrappedPhaseCycles;
            observation.signedDistance = state.signedDistance;
            observation.totalDistance = state.totalDistance;
            observation.distanceDelta = state.distanceDelta;
            observation.cycleDistance = state.cycleDistance;
            observation.jogWeight = state.jogWeight;
            observation.speed = state.speed;
            observation.playbackRate = state.playbackRate;
            observation.moveWeight = state.moveWeight;
            observation.scale = scale;
            observation.footAnchor = definition.anchor;
            observation.poseRevision = slot.poseRevision;
            observation.resetCount = state.resetCount;
            observation.resetReason = state.resetReason;
            observation.bodySubmittedMeshes = definition.character->model->meshes.size();
            observation.weaponSubmittedMeshes = definition.weapon->model->meshes.size();
            observation.declaredAccessories = definition.character->accessories.size();
            for (const auto& accessory : definition.character->accessories)
                observation.hairSubmittedMeshes += accessory.presentation->model->meshes.size();
            observation.upperBodyMaskCount = static_cast<std::size_t>(std::count(
                definition.upperBodyMask.begin(), definition.upperBodyMask.end(), true));
            observation.preparedInstances = impl.slots.size();
            // Where the renderer draws the mount: the instance offsets vertices by
            // -anchor, then the renderer applies ComposeEulerXYZ(transform).
            const auto mount = Engine::Math::TransformPoint(slot.pose.body.globalTransforms[definition.weaponNode],
                definition.weaponMount.translation);
            observation.weaponWorldPosition = Engine::Math::TransformPoint(
                Engine::Math::ComposeEulerXYZ(transform.translation, transform.rotationRadians, transform.scale),
                mount - definition.anchor);
            impl.observations.push_back(std::move(observation));
        }
        return true;
    } catch (const std::exception& exception) {
        error = "player renderer submit: " + std::string(exception.what());
        return false;
    }
}
} // namespace fps::pvp
