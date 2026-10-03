#include "RetroFPS/Pvp/PlayerPresentation.hpp"

#include "../App/AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "model_renderer/ModelRenderer.hpp"
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"

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

bool Finite(const Engine::Math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::size_t Node(const Engine::Model::ModelAsset& model, const std::string& name) {
    const auto found = model.FindNode(name);
    if (!found || std::count_if(model.nodes.begin(), model.nodes.end(),
            [&](const auto& node) { return node.name == name; }) != 1)
        throw std::runtime_error("missing or ambiguous player bone: " + name);
    return *found;
}

void Require(const Engine::Base::Result<void, std::string>& result) {
    if (!result) throw std::runtime_error(result.error());
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
    if (!made) throw std::runtime_error(made.error());
    return std::move(made.value());
}
} // namespace

std::shared_ptr<const PlayerPresentationDefinition> LoadPlayerPresentationDefinition(
    Engine::Asset::AssetManager& assets, const float bodyHeight, std::string& error) {
    error.clear();
    try {
        using namespace asset_definition_detail;
        if (!std::isfinite(bodyHeight) || bodyHeight <= 0)
            throw std::runtime_error("player body height must be finite and positive");
        const auto text = LoadShared<Engine::Asset::Loaders::TextAsset>(assets,
            Engine::Asset::AssetId::FromString("object_fps_pvp.player.presentation"),
            Engine::Asset::AssetType::Text());
        const auto config = nlohmann::json::parse(text->text);
        RequireVersionOne(config);
        auto definition = std::make_shared<PlayerPresentationDefinition>();
        definition->character = LoadCharacterPresentationDefinition(assets,
            ReadAssetId(config.at("character_asset_id")), error);
        if (!definition->character || !definition->character->animationSet)
            throw std::runtime_error(error.empty() ? "player requires an animation set" : error);
        const auto& character = *definition->character;
        const auto& model = *character.model;
        const auto clip = [&](const std::string& name) {
            const auto found = character.animationSet->clips.find(name);
            if (found == character.animationSet->clips.end())
                throw std::runtime_error("missing player animation: " + name);
            const auto index = found->second.clipIndex;
            if (index >= model.clips.size() || model.clips[index].durationSeconds <= 0)
                throw std::runtime_error("player animation requires positive duration: " + name);
            return index;
        };
        definition->idleClip = clip("idle");
        definition->walkClip = clip("walk");
        definition->jogClip = clip("jog");
        definition->shootClip = clip("shoot");
        definition->reloadClip = clip("reload");
        definition->jumpStartClip = clip("jump_start");
        definition->jumpLoopClip = clip("jump_loop");
        definition->jumpLandClip = clip("jump_land");
        definition->deathClip = clip("death");
        definition->bodyHeight = bodyHeight;
        definition->referenceSpeed = config.at("reference_speed").get<float>();
        const auto& locomotion = config.at("locomotion");
        definition->walkNativeSpeed = locomotion.at("walk_native_speed").get<double>();
        definition->jogNativeSpeed = locomotion.at("jog_native_speed").get<double>();
        definition->transitionSeconds = config.at("transition_seconds").get<double>();
        definition->maxFrameDeltaSeconds = config.at("max_frame_delta_seconds").get<double>();
        if (!std::isfinite(definition->referenceSpeed) || definition->referenceSpeed <= 0 ||
            !std::isfinite(definition->walkNativeSpeed) || definition->walkNativeSpeed <= 0 ||
            !std::isfinite(definition->jogNativeSpeed) || definition->jogNativeSpeed <= definition->walkNativeSpeed ||
            !std::isfinite(definition->transitionSeconds) || definition->transitionSeconds <= 0 ||
            !std::isfinite(definition->maxFrameDeltaSeconds) || definition->maxFrameDeltaSeconds <= 0)
            throw std::runtime_error("player locomotion calibration must be finite and positive, with jog faster than walk");
        const auto& actions = config.at("actions");
        definition->shotSeconds = actions.at("shot_seconds").get<double>();
        definition->jumpStartSeconds = actions.at("jump_start_seconds").get<double>();
        definition->jumpLandSeconds = actions.at("jump_land_seconds").get<double>();
        for (const double span : {definition->shotSeconds, definition->jumpStartSeconds, definition->jumpLandSeconds})
            if (!std::isfinite(span) || span <= 0)
                throw std::runtime_error("player action spans must be finite and positive");

        Pose reference;
        Require(Engine::Model::MakeDefaultPose(model, reference));
        float minimum = std::numeric_limits<float>::max(), maximum = -minimum;
        std::vector<Engine::Model::SkinnedVertex> vertices;
        for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
            Require(Engine::Model::SkinMesh(model, mesh, reference, vertices));
            for (const auto& vertex : vertices) {
                minimum = std::min(minimum, vertex.position.y);
                maximum = std::max(maximum, vertex.position.y);
            }
        }
        if (!std::isfinite(maximum - minimum) || maximum - minimum < 0.001F)
            throw std::runtime_error("player model has no usable reference height");
        definition->anchor = {0, minimum, 0};
        definition->scale = bodyHeight / (maximum - minimum);
        definition->upperBodyRoot = Node(model, config.at("upper_body_root").get<std::string>());
        definition->upperBodyMask.resize(model.nodes.size());
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            const auto parent = model.nodes[i].parentIndex;
            definition->upperBodyMask[i] = i == definition->upperBodyRoot ||
                (parent && definition->upperBodyMask[*parent]);
        }
        if (!definition->upperBodyMask[Node(model, "Head")] ||
            !definition->upperBodyMask[Node(model, "hand_r")] ||
            definition->upperBodyMask[Node(model, "pelvis")] ||
            definition->upperBodyMask[Node(model, "thigh_l")] ||
            definition->upperBodyMask[Node(model, "thigh_r")])
            throw std::runtime_error("player upper-body mask must contain head/hand but exclude pelvis/legs");

        const auto& weapon = config.at("weapon");
        definition->weapon = LoadCharacterPresentationDefinition(assets,
            ReadAssetId(weapon.at("character_asset_id")), error);
        if (!definition->weapon) throw std::runtime_error(error);
        if (definition->weapon->animationSet || !definition->weapon->accessories.empty())
            throw std::runtime_error("player weapon must be a static attachment");
        definition->weaponNode = Node(model, weapon.at("node").get<std::string>());
        if (!definition->upperBodyMask[definition->weaponNode])
            throw std::runtime_error("player weapon bone must be inside the upper-body mask");
        const auto& p = weapon.at("translation");
        const auto& r = weapon.at("rotation_xyzw");
        if (!p.is_array() || p.size() != 3 || !r.is_array() || r.size() != 4)
            throw std::runtime_error("player weapon mount requires translation and quaternion");
        auto& mount = definition->weaponMount;
        mount.translation = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
        auto& q = mount.rotation;
        q = {r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>()};
        const double norm = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
            static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
        const float scale = weapon.at("scale").get<float>();
        if (!Finite({mount.translation.x, mount.translation.y, mount.translation.z}) ||
            !std::isfinite(norm) || norm < 1e-12 || !std::isfinite(scale) || scale <= 0)
            throw std::runtime_error("player weapon mount must be finite with positive scale");
        const auto inverse = static_cast<float>(1 / std::sqrt(norm));
        q = {q.x * inverse, q.y * inverse, q.z * inverse, q.w * inverse};
        mount.scale = {scale, scale, scale};
        Require(Engine::Model::MakeDefaultPose(*definition->weapon->model,
            definition->weaponReferencePose));
        return definition;
    } catch (const std::exception& exception) {
        error = "player presentation definition: " + std::string(exception.what());
        return {};
    }
}

double PlayerJogWeight(const PlayerPresentationDefinition& definition, const double planarSpeed) {
    return std::clamp((planarSpeed - definition.walkNativeSpeed) /
        (definition.jogNativeSpeed - definition.walkNativeSpeed), 0.0, 1.0);
}

double PlayerCycleDistance(const PlayerPresentationDefinition& definition, const double jogWeight) {
    // Each clip covers its native speed times its authored length per cycle.
    // Both are sampled at one gait phase, so their stance feet blend into a
    // foot that moves, on average, at the presented speed.
    const auto& clips = definition.character->model->clips;
    const double walk = definition.walkNativeSpeed * clips[definition.walkClip].durationSeconds;
    const double jog = definition.jogNativeSpeed * clips[definition.jogClip].durationSeconds;
    return walk + (jog - walk) * jogWeight;
}

bool AdvancePlayerLocomotion(PlayerLocomotionState& state, const PlayerPresentationFrame& frame,
    const PlayerPresentationDefinition& definition, std::string& error) {
    error.clear();
    if (!frame.playerId || !frame.movementEpoch || !Finite(frame.position) ||
        !std::isfinite(frame.yaw) || !std::isfinite(frame.presentationSeconds) ||
        !std::isfinite(frame.deltaSeconds) || frame.deltaSeconds < 0 ||
        !definition.character || !definition.character->model ||
        definition.jogClip >= definition.character->model->clips.size() ||
        definition.walkClip >= definition.character->model->clips.size() ||
        !std::isfinite(definition.referenceSpeed) || definition.referenceSpeed <= 0 ||
        !std::isfinite(definition.walkNativeSpeed) || definition.walkNativeSpeed <= 0 ||
        !std::isfinite(definition.jogNativeSpeed) || definition.jogNativeSpeed <= definition.walkNativeSpeed ||
        !std::isfinite(frame.planarSpeed) || frame.planarSpeed < 0 ||
        !std::isfinite(definition.transitionSeconds) || definition.transitionSeconds <= 0 ||
        !std::isfinite(definition.maxFrameDeltaSeconds) || definition.maxFrameDeltaSeconds <= 0 ||
        !std::isfinite(frame.verticalVelocity) || !std::isfinite(frame.lifeStateSeconds) ||
        !std::isfinite(frame.shotSeconds) || !std::isfinite(frame.reloadStartSeconds) ||
        !std::isfinite(frame.reloadEndSeconds) ||
        !std::isfinite(definition.jumpStartSeconds) || definition.jumpStartSeconds <= 0 ||
        !std::isfinite(definition.jumpLandSeconds) || definition.jumpLandSeconds <= 0) {
        error = "player locomotion requires valid sampled values and calibration";
        return false;
    }
    for (const auto clip : {definition.walkClip, definition.jogClip}) {
        const double duration = definition.character->model->clips[clip].durationSeconds;
        if (!std::isfinite(duration) || duration <= 0) {
            error = "player walk and jog durations must be finite and positive";
            return false;
        }
    }
    const double elapsed = frame.presentationSeconds - state.previousPresentationSeconds;
    const double dx = static_cast<double>(frame.position.x) - state.previousPosition.x;
    const double dz = static_cast<double>(frame.position.z) - state.previousPosition.z;
    const double distance = std::hypot(dx, dz);
    std::string reset;
    if (!state.initialized) reset = "spawn";
    else if (frame.playerId != state.playerId) reset = "player";
    else if (frame.lifeGeneration != state.lifeGeneration || frame.dead != state.dead) reset = "life";
    else if (frame.movementEpoch != state.movementEpoch) reset = "epoch";
    else if (!frame.continuous) reset = "discontinuity";
    else if (frame.deltaSeconds >= definition.maxFrameDeltaSeconds ||
             elapsed >= definition.maxFrameDeltaSeconds) reset = "long_frame";
    else if (elapsed < 0 || (elapsed == 0 && distance > 0.000001)) reset = "time";
    else if (distance > definition.referenceSpeed * definition.maxFrameDeltaSeconds)
        reset = "teleport";
    if (!reset.empty()) {
        const auto count = state.resetCount + 1;
        state = {};
        state.initialized = true;
        state.phaseReset = true;
        state.resetCount = count;
        state.resetReason = std::move(reset);
        state.playerId = frame.playerId;
        state.movementEpoch = frame.movementEpoch;
        state.lifeGeneration = frame.lifeGeneration;
        state.dead = frame.dead;
        // A reanchor never invents a liftoff or landing it did not observe.
        state.grounded = frame.grounded;
        state.jumpPhase = frame.grounded ? PlayerJumpPhase::Grounded : PlayerJumpPhase::Airborne;
        state.jumpPhaseSeconds = frame.presentationSeconds;
        state.jogWeight = PlayerJogWeight(definition, frame.planarSpeed);
        state.cycleDistance = PlayerCycleDistance(definition, state.jogWeight);
    } else {
        state.phaseReset = false;
        state.resetReason.clear();
        state.distanceDelta = distance > 0.000001 ? distance : 0;
        state.jogging = state.distanceDelta > 0;
        state.backward = state.jogging && dx * std::sin(frame.yaw) + dz * std::cos(frame.yaw) < -0.000001;
        // A timeline can label its final moving sample as holding. Consume its
        // real displacement, then stop on the subsequent unchanged sample.
        state.holding = frame.holding && !state.jogging;
        const double signedDelta = state.backward ? -state.distanceDelta : state.distanceDelta;
        state.signedDistance += signedDelta;
        state.totalDistance += state.distanceDelta;
        // The weight follows the authority-sampled speed, never this frame's own
        // displacement, so equal routes reach equal phases at any render rate.
        // Without a speed (held or stopped samples) the last blend is kept.
        if (frame.planarSpeed > 0) state.jogWeight = PlayerJogWeight(definition, frame.planarSpeed);
        state.cycleDistance = PlayerCycleDistance(definition, state.jogWeight);
        state.unwrappedPhaseCycles += signedDelta / state.cycleDistance;
        state.phaseCycles = state.unwrappedPhaseCycles - std::floor(state.unwrappedPhaseCycles);
        state.speed = elapsed > 0 ? state.distanceDelta / elapsed : 0;
        state.playbackRate = (state.backward ? -state.speed : state.speed) / state.cycleDistance;
        state.idleSeconds += elapsed;
        // All pose clocks follow the sampled timeline. A visible hold freezes
        // its last walking pose instead of blending legs while position stalls.
        if (!state.holding) {
            const float weightStep = static_cast<float>(elapsed / definition.transitionSeconds);
            state.moveWeight = state.jogging ? std::min(1.0F, state.moveWeight + weightStep) :
                std::max(0.0F, state.moveWeight - weightStep);
        }
        const double now = frame.presentationSeconds;
        if (state.grounded && !frame.grounded) {
            state.jumpPhase = frame.verticalVelocity > 0 ? PlayerJumpPhase::Start : PlayerJumpPhase::Airborne;
            state.jumpPhaseSeconds = now;
        } else if (!state.grounded && frame.grounded) {
            state.jumpPhase = PlayerJumpPhase::Land;
            state.jumpPhaseSeconds = now;
        }
        // The contract spans end in sampled time; control never waits for them.
        if (state.jumpPhase == PlayerJumpPhase::Start && now - state.jumpPhaseSeconds >= definition.jumpStartSeconds) {
            state.jumpPhase = PlayerJumpPhase::Airborne;
            state.jumpPhaseSeconds += definition.jumpStartSeconds;
        } else if (state.jumpPhase == PlayerJumpPhase::Land &&
                   now - state.jumpPhaseSeconds >= definition.jumpLandSeconds) {
            state.jumpPhase = PlayerJumpPhase::Grounded;
            state.jumpPhaseSeconds += definition.jumpLandSeconds;
        }
        state.grounded = frame.grounded;
    }
    state.previousPosition = frame.position;
    state.previousPresentationSeconds = frame.presentationSeconds;
    return true;
}

bool ResolvePlayerActions(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerPresentationFrame& frame,
    PlayerActionPose& output, std::string& error) {
    error.clear();
    output = {};
    try {
        if (!definition.character || !definition.character->model)
            throw std::runtime_error("player actions require a resolved character");
        const auto& clips = definition.character->model->clips;
        const auto duration = [&](const std::size_t clip) {
            if (clip >= clips.size() || !std::isfinite(clips[clip].durationSeconds) || clips[clip].durationSeconds <= 0)
                throw std::runtime_error("player action clip requires a positive duration");
            return clips[clip].durationSeconds;
        };
        // Contract spans are shorter than the authored clips: progress through
        // the span maps onto the whole clip, which is time-scaled to fit.
        const auto scaled = [](const double elapsed, const double span, const double clip) {
            return std::clamp(elapsed / span, 0.0, 1.0) * clip;
        };
        const double now = frame.presentationSeconds;
        if (!std::isfinite(now) || !std::isfinite(frame.lifeStateSeconds) || !std::isfinite(frame.shotSeconds) ||
            !std::isfinite(frame.reloadStartSeconds) || !std::isfinite(frame.reloadEndSeconds) ||
            !std::isfinite(state.jumpPhaseSeconds) || !std::isfinite(definition.shotSeconds) ||
            definition.shotSeconds <= 0 || !std::isfinite(definition.jumpStartSeconds) ||
            definition.jumpStartSeconds <= 0 || !std::isfinite(definition.jumpLandSeconds) ||
            definition.jumpLandSeconds <= 0)
            throw std::runtime_error("player actions require finite times and positive spans");
        if (frame.dead) {
            // Full body; after the clip the last pose holds until the new life.
            output.lower = PlayerLowerAction::Death;
            output.lowerClipSeconds = std::clamp(now - frame.lifeStateSeconds, 0.0, duration(definition.deathClip));
            return true;
        }
        const double inPhase = std::max(0.0, now - state.jumpPhaseSeconds);
        switch (state.jumpPhase) {
        case PlayerJumpPhase::Start:
            output.lower = PlayerLowerAction::JumpStart;
            output.lowerClipSeconds = scaled(inPhase, definition.jumpStartSeconds, duration(definition.jumpStartClip));
            break;
        case PlayerJumpPhase::Airborne:
            output.lower = PlayerLowerAction::JumpLoop;
            static_cast<void>(duration(definition.jumpLoopClip));
            output.lowerClipSeconds = inPhase;
            break;
        case PlayerJumpPhase::Land:
            output.lower = PlayerLowerAction::JumpLand;
            output.lowerClipSeconds = scaled(inPhase, definition.jumpLandSeconds, duration(definition.jumpLandClip));
            break;
        case PlayerJumpPhase::Grounded:
            break;
        }
        // Only actions of this alive period. The Match restarts combat state
        // on respawn; a tick before the life began cannot belong to it.
        if (frame.reloadActionId && frame.reloadEndSeconds > frame.reloadStartSeconds &&
            frame.reloadStartSeconds >= frame.lifeStateSeconds &&
            now >= frame.reloadStartSeconds && now < frame.reloadEndSeconds) {
            output.upper = PlayerUpperAction::Reload;
            output.upperClipSeconds = scaled(now - frame.reloadStartSeconds,
                frame.reloadEndSeconds - frame.reloadStartSeconds, duration(definition.reloadClip));
            output.reloadActionId = frame.reloadActionId;
        } else if (frame.shotActionId && frame.shotSeconds >= frame.lifeStateSeconds &&
                   now >= frame.shotSeconds && now < frame.shotSeconds + definition.shotSeconds) {
            output.upper = PlayerUpperAction::Shoot;
            output.upperClipSeconds = scaled(now - frame.shotSeconds, definition.shotSeconds,
                duration(definition.shootClip));
            output.shotActionId = frame.shotActionId;
        }
        return true;
    } catch (const std::exception& exception) {
        output = {};
        error = "player actions: " + std::string(exception.what());
        return false;
    }
}

bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, PlayerPresentationPose& output, std::string& error) {
    return SamplePlayerPresentationPose(definition, state, PlayerActionPose{}, output, error);
}

bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerActionPose& actions,
    PlayerPresentationPose& output, std::string& error) {
    error.clear();
    try {
        if (!definition.character || !definition.character->model || !definition.weapon ||
            !definition.weapon->model)
            throw std::runtime_error("player pose requires resolved character and weapon");
        const auto& model = *definition.character->model;
        if (definition.upperBodyMask.size() != model.nodes.size() ||
            definition.weaponNode >= model.nodes.size())
            throw std::runtime_error("player pose requires a resolved mask and weapon bone");
        if (!std::isfinite(actions.upperClipSeconds) || !std::isfinite(actions.lowerClipSeconds))
            throw std::runtime_error("player pose requires finite action clip times");
        using Engine::Model::PlaybackMode;
        Pose idle;
        Require(Engine::Model::SamplePose(model, definition.idleClip, state.idleSeconds,
            PlaybackMode::Loop, idle));
        Pose upperAction;
        const Pose* upper = &idle;
        switch (actions.lower) {
        case PlayerLowerAction::Death:
            Require(Engine::Model::SamplePose(model, definition.deathClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output.body));
            upper = nullptr; // Death owns the whole body, weapon hand included.
            break;
        case PlayerLowerAction::Locomotion: {
            Pose walk, jog, gait;
            Require(Engine::Model::SamplePose(model, definition.walkClip,
                state.phaseCycles * model.clips[definition.walkClip].durationSeconds, PlaybackMode::Loop, walk));
            Require(Engine::Model::SamplePose(model, definition.jogClip,
                state.phaseCycles * model.clips[definition.jogClip].durationSeconds, PlaybackMode::Loop, jog));
            Require(Engine::Model::BlendPoses(model, walk, jog, static_cast<float>(state.jogWeight), gait));
            Require(Engine::Model::BlendPoses(model, idle, gait, state.moveWeight, output.body));
            break;
        }
        case PlayerLowerAction::JumpStart:
            Require(Engine::Model::SamplePose(model, definition.jumpStartClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output.body));
            break;
        case PlayerLowerAction::JumpLoop:
            Require(Engine::Model::SamplePose(model, definition.jumpLoopClip, actions.lowerClipSeconds,
                PlaybackMode::Loop, output.body));
            break;
        case PlayerLowerAction::JumpLand:
            Require(Engine::Model::SamplePose(model, definition.jumpLandClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output.body));
            break;
        default:
            throw std::runtime_error("player pose has an invalid lower-body action");
        }
        if (upper && actions.upper != PlayerUpperAction::Hold) {
            if (actions.upper != PlayerUpperAction::Shoot && actions.upper != PlayerUpperAction::Reload)
                throw std::runtime_error("player pose has an invalid upper-body action");
            Require(Engine::Model::SamplePose(model,
                actions.upper == PlayerUpperAction::Shoot ? definition.shootClip : definition.reloadClip,
                actions.upperClipSeconds, PlaybackMode::Clamp, upperAction));
            upper = &upperAction;
        }
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            if (upper && definition.upperBodyMask[i]) output.body.localTransforms[i] = upper->localTransforms[i];
            const auto local = Engine::Model::ToMatrix(output.body.localTransforms[i]);
            const auto parent = model.nodes[i].parentIndex;
            output.body.globalTransforms[i] = parent ?
                Engine::Math::Multiply(output.body.globalTransforms[*parent], local) : local;
        }
        // The weapon and hair follow the final composed pose.
        output.weapon = definition.weaponReferencePose;
        const auto mount = Engine::Math::Multiply(output.body.globalTransforms[definition.weaponNode],
            Engine::Model::ToMatrix(definition.weaponMount));
        for (auto& transform : output.weapon.globalTransforms)
            transform = Engine::Math::Multiply(mount, transform);
        output.accessories.clear();
        for (const auto& accessory : definition.character->accessories)
            output.accessories.push_back(BuildCharacterAccessoryPose(accessory, output.body));
        return true;
    } catch (const std::exception& exception) {
        error = "player pose: " + std::string(exception.what());
        return false;
    }
}

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
        next->bodyResource = CreateResource(device, assets, *definition.character);
        next->weaponResource = CreateResource(device, assets, *definition.weapon);
        for (const auto& accessory : definition.character->accessories)
            next->accessoryResources.push_back(CreateResource(device, assets, *accessory.presentation));
        if (!SamplePlayerPresentationPose(definition, {}, next->initialPose, error))
            throw std::runtime_error(error);
        const Vec3 offset{-definition.anchor.x, -definition.anchor.y, -definition.anchor.z};
        const auto instance = [&](const auto& resource, const Pose& pose) {
            auto created = Instance::Create(resource, pose, offset);
            if (!created) throw std::runtime_error(created.error());
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
            const bool plain = actions.upper == PlayerUpperAction::Hold &&
                actions.lower == PlayerLowerAction::Locomotion;
            if (slot.locomotion.phaseReset && plain) {
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
            impl.SubmitSlot(slot, {player.position, {0, player.yaw, 0}, {scale, scale, scale}}, queue);
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
            for (const auto& accessory : definition.character->accessories)
                observation.hairSubmittedMeshes += accessory.presentation->model->meshes.size();
            observation.upperBodyMaskCount = static_cast<std::size_t>(std::count(
                definition.upperBodyMask.begin(), definition.upperBodyMask.end(), true));
            observation.preparedInstances = impl.slots.size();
            auto point = Engine::Math::TransformPoint(slot.pose.body.globalTransforms[definition.weaponNode],
                definition.weaponMount.translation);
            point = {(point.x - definition.anchor.x) * scale, (point.y - definition.anchor.y) * scale,
                     (point.z - definition.anchor.z) * scale};
            observation.weaponWorldPosition = {
                player.position.x + std::cos(player.yaw) * point.x + std::sin(player.yaw) * point.z,
                player.position.y + point.y,
                player.position.z - std::sin(player.yaw) * point.x + std::cos(player.yaw) * point.z};
            impl.observations.push_back(std::move(observation));
        }
        return true;
    } catch (const std::exception& exception) {
        error = "player renderer submit: " + std::string(exception.what());
        return false;
    }
}
} // namespace fps::pvp
