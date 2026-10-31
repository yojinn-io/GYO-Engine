#include "RetroFPS/Pvp/PlayerPresentation.hpp"

#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/PlayerAnimationContent.hpp"
#include "../App/AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
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

// The CPU player pose shared by the Client's presentation and the Match
// (v8-product-hitbox-data D2, v8-product-hitbox-pose-sharing (a)): the
// definition, the locomotion state the presentation shows, the action pose
// and the composed skeleton. Moved unchanged from PlayerPresentation.cpp so the
// judged pose and the presented pose are one computation.
namespace fps::pvp {
namespace {
using Engine::Model::Pose;
using Engine::Math::Vec3;

std::size_t Node(const Engine::Model::ModelAsset& model, const std::string& name) {
    const auto found = model.FindNode(name);
    if (!found || std::count_if(model.nodes.begin(), model.nodes.end(),
            [&](const auto& node) { return node.name == name; }) != 1)
        throw std::runtime_error("missing or ambiguous player bone: " + name);
    return *found;
}

template <class E>
void Require(const Engine::Base::Result<void, E>& result) {
    if (!result) throw std::runtime_error(result.error().message);
}

// The base pose plus the motion of one clip since its reference frame, in the
// node's own local space: the base rotation, then the clip's rotation since
// the reference. Translation adds its offset; the base scale stays.
Engine::Model::Transform AddMotion(const Engine::Model::Transform& base, const Engine::Model::Transform& reference,
                                   const Engine::Model::Transform& motion) {
    using namespace Engine::Math;
    const auto delta = Multiply(Inverse(NormalizeOrIdentity(reference.rotation)), NormalizeOrIdentity(motion.rotation));
    return {base.translation + (motion.translation - reference.translation),
            NormalizeOrIdentity(Multiply(NormalizeOrIdentity(base.rotation), delta)), base.scale};
}
} // namespace

namespace {
// presentation false is the Match's path: the character's model and clips
// with no material or accessory, and no weapon (the body pose only).
std::shared_ptr<const PlayerPresentationDefinition> LoadPlayerDefinition(
    Engine::Asset::AssetManager& assets, const float bodyHeight, std::string& error, bool presentation) {
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
        definition->character = presentation ?
            LoadCharacterPresentationDefinition(assets, ReadAssetId(config.at("character_asset_id")), error) :
            LoadCharacterAnimationDefinition(assets, ReadAssetId(config.at("character_asset_id")), error);
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
        definition->aimUpClip = clip("aim_up");
        definition->aimNeutralClip = clip("aim_neutral");
        definition->aimDownClip = clip("aim_down");
        definition->hitClip = clip("hit");
        definition->crouchIdleClip = clip("crouch_idle");
        definition->crouchMoveClip = clip("crouch_move");
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
        const auto& aim = config.at("aim");
        definition->aimUpRadians = aim.at("up_pitch_radians").get<double>();
        definition->aimDownRadians = aim.at("down_pitch_radians").get<double>();
        for (const double limit : {definition->aimUpRadians, definition->aimDownRadians})
            if (!std::isfinite(limit) || limit <= 0 || limit > MovementMaximumPitch)
                throw std::runtime_error("player aim pitch limits must be positive and within the maximum pitch");
        const auto& actions = config.at("actions");
        definition->shotSeconds = actions.at("shot_seconds").get<double>();
        definition->jumpStartSeconds = actions.at("jump_start_seconds").get<double>();
        definition->jumpLandSeconds = actions.at("jump_land_seconds").get<double>();
        definition->hitSeconds = actions.at("hit_seconds").get<double>();
        for (const double span : {definition->shotSeconds, definition->jumpStartSeconds, definition->jumpLandSeconds,
                                  definition->hitSeconds})
            if (!std::isfinite(span) || span <= 0)
                throw std::runtime_error("player action spans must be finite and positive");
        const auto animationSource = ReadPlayerAnimationSource(text->text, character, error);
        if (!animationSource) throw std::runtime_error(error);
        const auto animationRules = MakePlayerAnimationRules(*animationSource, error);
        if (!animationRules) throw std::runtime_error(error);
        definition->animationRules = *animationRules;

        Pose reference;
        Require(Engine::Model::MakeDefaultPose(model, reference));
        float minimum = std::numeric_limits<float>::max(), maximum = -minimum;
        std::vector<Engine::Model::SkinnedVertex> vertices;
        for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
            Require(Engine::Model::SkinMesh(model, mesh, reference, vertices));
            for (const auto& vertex : vertices) {
                minimum = Engine::Math::Min(minimum, vertex.position.y);
                maximum = Engine::Math::Max(maximum, vertex.position.y);
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

        if (!presentation) return definition;
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
        const float lengthSquared = Engine::Math::LengthSquared(q);
        const float scale = weapon.at("scale").get<float>();
        if (!Engine::Math::IsFinite(mount.translation) ||
            !std::isfinite(lengthSquared) || lengthSquared < 1e-12F || !std::isfinite(scale) || scale <= 0)
            throw std::runtime_error("player weapon mount must be finite with positive scale");
        q = Engine::Math::Normalize(q);
        mount.scale = {scale, scale, scale};
        Require(Engine::Model::MakeDefaultPose(*definition->weapon->model,
            definition->weaponReferencePose));
        return definition;
    } catch (const std::exception& exception) {
        error = "player presentation definition: " + std::string(exception.what());
        return {};
    }
}

} // namespace

std::shared_ptr<const PlayerPresentationDefinition> LoadPlayerPresentationDefinition(
    Engine::Asset::AssetManager& assets, const float bodyHeight, std::string& error) {
    return LoadPlayerDefinition(assets, bodyHeight, error, true);
}

std::shared_ptr<const PlayerPresentationDefinition> LoadPlayerPoseDefinition(
    Engine::Asset::AssetManager& assets, const float bodyHeight, std::string& error) {
    return LoadPlayerDefinition(assets, bodyHeight, error, false);
}

std::shared_ptr<const PlayerPresentationDefinition> LoadPlayerPoseDefinition(
    const std::filesystem::path& contentRoot, const float bodyHeight, std::string& error) {
    std::shared_ptr<const PlayerPresentationDefinition> definition;
    if (!WithContentAssets(contentRoot, [&](Engine::Asset::AssetManager& assets, std::string& failure) {
            definition = LoadPlayerPoseDefinition(assets, bodyHeight, failure);
            return definition != nullptr;
        }, error))
        return {};
    return definition;
}

double PlayerJogWeight(const PlayerPresentationDefinition& definition, const double planarSpeed) {
    return Engine::Math::Clamp((planarSpeed - definition.walkNativeSpeed) /
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

namespace {
PlayerJumpPhase PresentedJumpPhase(JumpPhase phase) {
    switch (phase) {
    case JumpPhase::Start: return PlayerJumpPhase::Start;
    case JumpPhase::Airborne: return PlayerJumpPhase::Airborne;
    case JumpPhase::Land: return PlayerJumpPhase::Land;
    case JumpPhase::Grounded:
    case JumpPhase::Unspecified: break;
    }
    return PlayerJumpPhase::Grounded;
}
} // namespace

PlayerPresentationFrame PlayerFrameFromAuthority(const PlayerState& player,
    const std::optional<CombatState>& combat, const std::optional<PlayerAnimationSample>& animation,
    const double presentationTick) {
    PlayerPresentationFrame frame;
    frame.playerId = player.playerId;
    frame.movementEpoch = player.movementEpoch;
    frame.lifeGeneration = player.lifeGeneration;
    frame.dead = player.lifeState == LifeState::Dead;
    frame.position = player.position;
    frame.yaw = player.yaw;
    frame.pitch = player.pitch;
    frame.presentationSeconds = presentationTick / AuthorityTickRate;
    frame.animation = animation; // pv9 §4.4: the Match's animation
    frame.grounded = player.grounded;
    frame.crouched = player.crouched; // pv8 section 4: the stance selects the clips
    frame.verticalVelocity = player.verticalVelocity;
    frame.lifeStateSeconds = static_cast<double>(player.lifeStateTick) / AuthorityTickRate;
    // Actions come only from the sampled interval's own combat state, so
    // newer combat is never paired with an older position or life.
    if (combat) {
        if (combat->lastShotActionId) {
            frame.shotActionId = combat->lastShotActionId;
            frame.shotSeconds = static_cast<double>(combat->lastShotTick) / AuthorityTickRate;
        }
        if (combat->damageCount) {
            frame.damageCount = combat->damageCount;
            frame.damageSeconds = static_cast<double>(combat->lastDamageTick) / AuthorityTickRate;
        }
        if (combat->reloadActionId && combat->reloadEndTick > combat->reloadStartTick) {
            frame.reloadActionId = combat->reloadActionId;
            frame.reloadStartSeconds = static_cast<double>(combat->reloadStartTick) / AuthorityTickRate;
            frame.reloadEndSeconds = static_cast<double>(combat->reloadEndTick) / AuthorityTickRate;
        }
    }
    return frame;
}

Vec3 PlayerBodyPointToWorld(const PlayerPresentationDefinition& definition, const Vec3& position, const float yaw,
                            const Vec3& modelPoint) {
    const float scale = definition.scale;
    return Engine::Math::TransformPoint(
        Engine::Math::ComposeEulerXYZ(position, Vec3{0, yaw, 0}, Vec3{scale, scale, scale}),
        modelPoint - definition.anchor);
}

bool AdvancePlayerLocomotion(PlayerLocomotionState& state, const PlayerPresentationFrame& frame,
    const PlayerPresentationDefinition& definition, std::string& error) {
    error.clear();
    if (!frame.playerId || !frame.movementEpoch || !Engine::Math::IsFinite(frame.position) ||
        !std::isfinite(frame.yaw) || !std::isfinite(frame.presentationSeconds) ||
        !std::isfinite(frame.deltaSeconds) || frame.deltaSeconds < 0 ||
        !definition.character || !definition.character->model ||
        definition.jogClip >= definition.character->model->clips.size() ||
        definition.walkClip >= definition.character->model->clips.size() ||
        !std::isfinite(definition.referenceSpeed) || definition.referenceSpeed <= 0 ||
        !std::isfinite(definition.walkNativeSpeed) || definition.walkNativeSpeed <= 0 ||
        !std::isfinite(definition.jogNativeSpeed) || definition.jogNativeSpeed <= definition.walkNativeSpeed ||
        !std::isfinite(definition.transitionSeconds) || definition.transitionSeconds <= 0 ||
        !std::isfinite(definition.maxFrameDeltaSeconds) || definition.maxFrameDeltaSeconds <= 0 ||
        !std::isfinite(frame.verticalVelocity) || !std::isfinite(frame.lifeStateSeconds) ||
        !std::isfinite(frame.shotSeconds) || !std::isfinite(frame.reloadStartSeconds) ||
        !std::isfinite(frame.reloadEndSeconds) ||
        (frame.animation && (!std::isfinite(frame.animation->gaitPhaseQ16) ||
            !std::isfinite(frame.animation->moveWeight) || !std::isfinite(frame.animation->jogWeight))) ||
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
    // Displacement and resets are observations of this presentation only: the
    // gait phase, the weights and the jump phase come from the Match's sampled
    // animation (protocol-v9 §4.4, player-presentation Design Invariants 1-2).
    const double elapsed = frame.presentationSeconds - state.previousPresentationSeconds;
    const Engine::Math::Vec3d planar{static_cast<double>(frame.position.x) - state.previousPosition.x, 0.0,
                                     static_cast<double>(frame.position.z) - state.previousPosition.z};
    const double distance = Engine::Math::Length(planar);
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
    const double phaseQ16 = frame.animation ? frame.animation->gaitPhaseQ16 : 0;
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
    } else {
        state.phaseReset = false;
        state.resetReason.clear();
        state.distanceDelta = distance > 0.000001 ? distance : 0;
        state.jogging = state.distanceDelta > 0;
        state.backward = state.jogging && Engine::Math::Dot(planar,
            Engine::Math::Vec3d{std::sin(frame.yaw), 0.0, std::cos(frame.yaw)}) < -0.000001;
        // A timeline can label its final moving sample as holding. Consume its
        // real displacement, then stop on the subsequent unchanged sample.
        state.holding = frame.holding && !state.jogging;
        const double signedDelta = state.backward ? -state.distanceDelta : state.distanceDelta;
        state.signedDistance += signedDelta;
        state.totalDistance += state.distanceDelta;
        state.speed = elapsed > 0 ? state.distanceDelta / elapsed : 0;
    }
    // A visible hold freezes its last walking pose instead of blending legs
    // while position stalls; otherwise the authority gait applies as sampled.
    if (!state.holding) {
        if (!state.phaseReset) state.unwrappedPhaseCycles += GaitCycleDifference(phaseQ16, state.authorityPhaseQ16);
        state.authorityPhaseQ16 = phaseQ16;
        state.phaseCycles = GaitCycleFraction(phaseQ16);
        state.moveWeight = frame.animation ? static_cast<float>(frame.animation->moveWeight) : 0.0F;
        state.jogWeight = frame.animation ? frame.animation->jogWeight : 0.0;
    }
    // The hold freezes the gait only: a jump in place moves no planar
    // position, so the jump phase follows the authority even while holding.
    state.jumpPhase = frame.animation ? PresentedJumpPhase(frame.animation->jumpPhase) :
        (frame.grounded ? PlayerJumpPhase::Grounded : PlayerJumpPhase::Airborne);
    state.jumpPhaseSeconds = frame.animation ?
        static_cast<double>(frame.animation->jumpPhaseTick) / AuthorityTickRate : frame.presentationSeconds;
    state.cycleDistance = PlayerCycleDistance(definition, state.jogWeight);
    state.playbackRate = (state.backward ? -state.speed : state.speed) / state.cycleDistance;
    // The idle clock runs from the life's start (protocol-v9 §4.1).
    state.idleSeconds = Engine::Math::Max(0.0, frame.presentationSeconds - frame.lifeStateSeconds);
    state.grounded = frame.grounded;
    state.previousPosition = frame.position;
    state.previousPresentationSeconds = frame.presentationSeconds;
    state.crouched = frame.crouched;
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
            return Engine::Math::Clamp(elapsed / span, 0.0, 1.0) * clip;
        };
        const double now = frame.presentationSeconds;
        if (!std::isfinite(now) || !std::isfinite(frame.lifeStateSeconds) || !std::isfinite(frame.shotSeconds) ||
            !std::isfinite(frame.reloadStartSeconds) || !std::isfinite(frame.reloadEndSeconds) ||
            !std::isfinite(state.jumpPhaseSeconds) || !std::isfinite(definition.shotSeconds) ||
            definition.shotSeconds <= 0 || !std::isfinite(definition.jumpStartSeconds) ||
            definition.jumpStartSeconds <= 0 || !std::isfinite(definition.jumpLandSeconds) ||
            definition.jumpLandSeconds <= 0 || !std::isfinite(frame.damageSeconds) ||
            !std::isfinite(definition.hitSeconds) || definition.hitSeconds <= 0)
            throw std::runtime_error("player actions require finite times and positive spans");
        if (!std::isfinite(frame.pitch))
            throw std::runtime_error("player actions require a finite pitch");
        if (frame.dead) {
            // Full body; after the clip the last pose holds until the new life.
            output.lower = PlayerLowerAction::Death;
            output.lowerClipSeconds = Engine::Math::Clamp(now - frame.lifeStateSeconds, 0.0, duration(definition.deathClip));
            return true;
        }
        // Clamp, then linear between neutral and the pose on that side.
        output.aimWeight = frame.pitch < 0 ?
            static_cast<float>(-Engine::Math::Min(1.0, -frame.pitch / definition.aimUpRadians)) :
            static_cast<float>(Engine::Math::Min(1.0, frame.pitch / definition.aimDownRadians));
        const double inPhase = Engine::Math::Max(0.0, now - state.jumpPhaseSeconds);
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
        // A hit outranks a shot or reload; it is anchored at the latest damage,
        // so repeated or late snapshots and several hits at once play it once.
        if (frame.damageCount && frame.damageSeconds >= frame.lifeStateSeconds &&
            now >= frame.damageSeconds && now < frame.damageSeconds + definition.hitSeconds) {
            output.upper = PlayerUpperAction::Hit;
            output.upperClipSeconds = scaled(now - frame.damageSeconds, definition.hitSeconds, duration(definition.hitClip));
        } else if (frame.reloadActionId && frame.reloadEndSeconds > frame.reloadStartSeconds &&
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

namespace {
// The composed body skeleton, the one computation of the judged and the
// presented pose (v8-product-hitbox-pose-sharing (a)). Throws on invalid input.
void ComposeBody(const PlayerPresentationDefinition& definition, const PlayerLocomotionState& state,
                 const PlayerActionPose& actions, Pose& body) {
    {
        auto& output = body;
        if (!definition.character || !definition.character->model)
            throw std::runtime_error("player pose requires a resolved character");
        const auto& model = *definition.character->model;
        if (definition.upperBodyMask.size() != model.nodes.size())
            throw std::runtime_error("player pose requires a resolved mask");
        if (!std::isfinite(actions.upperClipSeconds) || !std::isfinite(actions.lowerClipSeconds) ||
            !std::isfinite(actions.aimWeight) || actions.aimWeight < -1 || actions.aimWeight > 1)
            throw std::runtime_error("player pose requires finite action clip times and an aim weight in [-1, 1]");
        using Engine::Model::PlaybackMode;
        Pose idle;
        Require(Engine::Model::SamplePose(model, definition.idleClip, state.idleSeconds,
            PlaybackMode::Loop, idle));
        Pose upperAction;
        const Pose* upper = &idle;
        switch (actions.lower) {
        case PlayerLowerAction::Death:
            Require(Engine::Model::SamplePose(model, definition.deathClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output));
            upper = nullptr; // Death owns the whole body, weapon hand included.
            break;
        case PlayerLowerAction::Locomotion: {
            if (state.crouched) {
                // v8-product-crouch-pose: the crouch clips on the standing model.
                Pose crouchIdle, crouchMove;
                Require(Engine::Model::SamplePose(model, definition.crouchIdleClip, state.idleSeconds,
                    PlaybackMode::Loop, crouchIdle));
                Require(Engine::Model::SamplePose(model, definition.crouchMoveClip,
                    state.phaseCycles * model.clips[definition.crouchMoveClip].durationSeconds, PlaybackMode::Loop, crouchMove));
                Require(Engine::Model::BlendPoses(model, crouchIdle, crouchMove, state.moveWeight, output));
                break;
            }
            Pose walk, jog, gait;
            Require(Engine::Model::SamplePose(model, definition.walkClip,
                state.phaseCycles * model.clips[definition.walkClip].durationSeconds, PlaybackMode::Loop, walk));
            Require(Engine::Model::SamplePose(model, definition.jogClip,
                state.phaseCycles * model.clips[definition.jogClip].durationSeconds, PlaybackMode::Loop, jog));
            Require(Engine::Model::BlendPoses(model, walk, jog, static_cast<float>(state.jogWeight), gait));
            Require(Engine::Model::BlendPoses(model, idle, gait, state.moveWeight, output));
            break;
        }
        case PlayerLowerAction::JumpStart:
            Require(Engine::Model::SamplePose(model, definition.jumpStartClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output));
            break;
        case PlayerLowerAction::JumpLoop:
            Require(Engine::Model::SamplePose(model, definition.jumpLoopClip, actions.lowerClipSeconds,
                PlaybackMode::Loop, output));
            break;
        case PlayerLowerAction::JumpLand:
            Require(Engine::Model::SamplePose(model, definition.jumpLandClip, actions.lowerClipSeconds,
                PlaybackMode::Clamp, output));
            break;
        default:
            throw std::runtime_error("player pose has an invalid lower-body action");
        }
        if (upper) {
            // The aim pose by pitch is the upper-body base; the idle clip, or
            // the shot or reload, adds its motion since its own first frame.
            Pose aim, extreme, reference;
            Require(Engine::Model::SamplePose(model, definition.aimNeutralClip, 0, PlaybackMode::Clamp, aim));
            if (actions.aimWeight != 0) {
                Require(Engine::Model::SamplePose(model, actions.aimWeight < 0 ? definition.aimUpClip :
                    definition.aimDownClip, 0, PlaybackMode::Clamp, extreme));
                const Pose neutral = aim;
                Require(Engine::Model::BlendPoses(model, neutral, extreme, std::abs(actions.aimWeight), aim));
            }
            std::size_t motionClip = definition.idleClip;
            const Pose* motion = &idle;
            if (actions.upper != PlayerUpperAction::Hold) {
                if (actions.upper != PlayerUpperAction::Shoot && actions.upper != PlayerUpperAction::Reload &&
                    actions.upper != PlayerUpperAction::Hit)
                    throw std::runtime_error("player pose has an invalid upper-body action");
                motionClip = actions.upper == PlayerUpperAction::Shoot ? definition.shootClip :
                    actions.upper == PlayerUpperAction::Reload ? definition.reloadClip : definition.hitClip;
                Require(Engine::Model::SamplePose(model, motionClip, actions.upperClipSeconds,
                    PlaybackMode::Clamp, upperAction));
                motion = &upperAction;
            }
            Require(Engine::Model::SamplePose(model, motionClip, 0, PlaybackMode::Clamp, reference));
            for (std::size_t i = 0; i < model.nodes.size(); ++i)
                if (definition.upperBodyMask[i])
                    aim.localTransforms[i] = AddMotion(aim.localTransforms[i], reference.localTransforms[i],
                                                       motion->localTransforms[i]);
            upperAction = std::move(aim);
            upper = &upperAction;
        }
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            if (upper && definition.upperBodyMask[i]) output.localTransforms[i] = upper->localTransforms[i];
            const auto local = Engine::Model::ToMatrix(output.localTransforms[i]);
            const auto parent = model.nodes[i].parentIndex;
            output.globalTransforms[i] = parent ?
                Engine::Math::Multiply(output.globalTransforms[*parent], local) : local;
        }
    }
}
} // namespace

bool SamplePlayerBodyPose(const PlayerPresentationDefinition& definition, const PlayerLocomotionState& state,
    const PlayerActionPose& actions, Engine::Model::Pose& body, std::string& error) {
    error.clear();
    try {
        ComposeBody(definition, state, actions, body);
        return true;
    } catch (const std::exception& exception) {
        error = "player pose: " + std::string(exception.what());
        return false;
    }
}

bool SamplePlayerPresentationPose(const PlayerPresentationDefinition& definition,
    const PlayerLocomotionState& state, const PlayerActionPose& actions,
    PlayerPresentationPose& output, std::string& error) {
    error.clear();
    try {
        if (!definition.character || !definition.character->model || !definition.weapon ||
            !definition.weapon->model)
            throw std::runtime_error("player pose requires resolved character and weapon");
        if (definition.weaponNode >= definition.character->model->nodes.size())
            throw std::runtime_error("player pose requires a resolved mask and weapon bone");
        ComposeBody(definition, state, actions, output.body);
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

} // namespace fps::pvp
