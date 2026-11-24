#include "object_fps_pvp/sim/PlayerAnimation.hpp"

#include "data/JsonFields.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

double Positive(const Json::ObjectView& object, const std::string_view key) {
    const double value = data::Number(data::Member(object, key), key);
    if (!std::isfinite(value) || value <= 0) throw std::runtime_error(std::string{key} + " is not a finite positive number");
    return value;
}

float Component(const Json::Value& value, const std::string_view what) {
    const auto number = static_cast<float>(data::Number(value, what));
    if (!std::isfinite(number)) throw std::runtime_error(std::string{what} + " is not finite");
    return number;
}

} // namespace

std::optional<PresentationFile> ParsePresentation(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the presentation");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        PresentationFile file;
        file.referenceSpeed = Positive(json, "reference_speed");
        const auto locomotion = data::AsObject(data::Member(json, "locomotion"), "locomotion");
        file.walkNativeSpeed = Positive(locomotion, "walk_native_speed");
        file.jogNativeSpeed = Positive(locomotion, "jog_native_speed");
        if (file.jogNativeSpeed <= file.walkNativeSpeed) throw std::runtime_error("the jog is not faster than the walk");
        file.transitionSeconds = Positive(json, "transition_seconds");
        file.maxFrameDeltaSeconds = Positive(json, "max_frame_delta_seconds");
        const auto actions = data::AsObject(data::Member(json, "actions"), "actions");
        file.shotSeconds = Positive(actions, "shot_seconds");
        file.jumpStartSeconds = Positive(actions, "jump_start_seconds");
        file.jumpLandSeconds = Positive(actions, "jump_land_seconds");
        file.hitSeconds = Positive(actions, "hit_seconds");
        const auto aim = data::AsObject(data::Member(json, "aim"), "aim");
        file.aimUpRadians = static_cast<float>(Positive(aim, "up_pitch_radians"));
        file.aimDownRadians = static_cast<float>(Positive(aim, "down_pitch_radians"));
        file.upperBodyRoot = data::String(data::Member(json, "upper_body_root"), "upper_body_root");
        const auto weapon = data::AsObject(data::Member(json, "weapon"), "weapon");
        file.weaponNode = data::String(data::Member(weapon, "node"), "weapon node");
        const auto translation = data::Member(weapon, "translation").AsArray();
        const auto rotation = data::Member(weapon, "rotation_xyzw").AsArray();
        if (!translation || translation.value().Size() != 3) throw std::runtime_error("the weapon translation is three numbers");
        if (!rotation || rotation.value().Size() != 4) throw std::runtime_error("the weapon rotation is four numbers");
        float t[3]{};
        std::size_t i = 0;
        for (const auto v : translation.value()) t[i++] = Component(v, "the weapon translation");
        file.weaponTranslation = {t[0], t[1], t[2]};
        i = 0;
        for (const auto v : rotation.value()) file.weaponRotation[i++] = Component(v, "the weapon rotation");
        file.weaponScale = static_cast<float>(Positive(weapon, "scale"));
        return file;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

AnimationRules MakeAnimationRules(const PresentationFile& file, const double walkClipSeconds, const double jogClipSeconds) {
    AnimationRules rules;
    rules.referenceSpeed = file.referenceSpeed;
    rules.walkNativeSpeed = file.walkNativeSpeed;
    rules.jogNativeSpeed = file.jogNativeSpeed;
    rules.walkCycleDistance = file.walkNativeSpeed * walkClipSeconds;
    rules.jogCycleDistance = file.jogNativeSpeed * jogClipSeconds;
    rules.transitionSeconds = file.transitionSeconds;
    rules.maxFrameDeltaSeconds = file.maxFrameDeltaSeconds;
    rules.shotSeconds = file.shotSeconds;
    rules.jumpStartSeconds = file.jumpStartSeconds;
    rules.jumpLandSeconds = file.jumpLandSeconds;
    rules.hitSeconds = file.hitSeconds;
    rules.aimUpRadians = file.aimUpRadians;
    rules.aimDownRadians = file.aimDownRadians;
    return rules;
}

void Advance(Locomotion& state, const AnimationInput& input, const AnimationRules& rules) noexcept {
    if (!state.started) {
        state = Locomotion{};
        state.started = true;
        state.seconds = input.seconds;
        state.feet = input.feet;
        state.grounded = input.grounded;
        state.jump = input.grounded ? JumpPhase::Grounded : JumpPhase::Airborne;
        state.jumpSince = input.seconds;
        return;
    }
    const double dt = std::clamp(input.seconds - state.seconds, 0.0, rules.maxFrameDeltaSeconds);
    const Math::Vec3 delta = input.feet - state.feet;
    const double planar = std::sqrt(static_cast<double>(delta.x) * delta.x + static_cast<double>(delta.z) * delta.z);
    // A respawn or a correction moves further in one step than a stride: not walked.
    const bool teleport = planar > std::max(rules.walkCycleDistance, 0.5);
    const double speed = dt > 0 && !teleport ? planar / dt : 0.0;
    const bool moving = speed > 0.05 * rules.referenceSpeed;
    // Eased toward moving or standing over the transition.
    const double ease = rules.transitionSeconds > 0 ? std::min(1.0, dt / rules.transitionSeconds) : 1.0;
    state.moveWeight += ((moving ? 1.0 : 0.0) - state.moveWeight) * ease;
    if (moving) {
        state.jogWeight = std::clamp((speed - rules.walkNativeSpeed) / (rules.jogNativeSpeed - rules.walkNativeSpeed), 0.0, 1.0);
    }
    if (!teleport && planar > 0) {
        const double cycle = rules.walkCycleDistance + (rules.jogCycleDistance - rules.walkCycleDistance) * state.jogWeight;
        // Backward when the step goes against the view.
        const double forward = std::sin(input.yaw) * delta.x + std::cos(input.yaw) * delta.z;
        state.cycles += (forward < 0 ? -planar : planar) / cycle;
    }
    // The jump: leaving the ground starts it, landing ends it; each phase from the moment it began.
    if (state.grounded && !input.grounded) {
        state.jump = JumpPhase::Start;
        state.jumpSince = input.seconds;
    } else if (!state.grounded && input.grounded) {
        state.jump = JumpPhase::Land;
        state.jumpSince = input.seconds;
    } else if (state.jump == JumpPhase::Start && input.seconds - state.jumpSince >= rules.jumpStartSeconds) {
        state.jump = JumpPhase::Airborne;
        state.jumpSince += rules.jumpStartSeconds;
    } else if (state.jump == JumpPhase::Land && input.seconds - state.jumpSince >= rules.jumpLandSeconds) {
        state.jump = JumpPhase::Grounded;
    }
    state.grounded = input.grounded;
    state.seconds = input.seconds;
    state.feet = input.feet;
}

ActionPose ResolveActions(const Locomotion& state, const AnimationInput& input, const AnimationRules& rules) noexcept {
    ActionPose pose;
    const double now = input.seconds;
    if (input.dead) {
        pose.lower = LowerClip::Death;
        pose.lowerSeconds = std::max(0.0, now - input.lifeSeconds);
        return pose;
    }
    // Negative pitch looks up.
    pose.aim = input.pitch < 0 ? -std::min(1.0F, -input.pitch / rules.aimUpRadians) : std::min(1.0F, input.pitch / rules.aimDownRadians);
    const double inPhase = std::max(0.0, now - state.jumpSince);
    switch (state.jump) {
    case JumpPhase::Start:
        pose.lower = LowerClip::JumpStart;
        pose.lowerSeconds = std::clamp(inPhase / rules.jumpStartSeconds, 0.0, 1.0);
        break;
    case JumpPhase::Airborne:
        pose.lower = LowerClip::JumpLoop;
        pose.lowerSeconds = inPhase;
        break;
    case JumpPhase::Land:
        pose.lower = LowerClip::JumpLand;
        pose.lowerSeconds = std::clamp(inPhase / rules.jumpLandSeconds, 0.0, 1.0);
        break;
    case JumpPhase::Grounded:
        break;
    }
    // Only actions of this life; a hit outranks a reload, a reload a shot.
    const auto within = [&](const std::optional<double>& start, const double span) {
        return start && *start >= input.lifeSeconds && now >= *start && now < *start + span;
    };
    if (within(input.hitSeconds, rules.hitSeconds)) {
        pose.upper = input.hitPart == HitPart::Head ? UpperClip::HitHead : UpperClip::Hit;
        pose.upperProgress = (now - *input.hitSeconds) / rules.hitSeconds;
    } else if (input.reloadStart && input.reloadEnd && *input.reloadEnd > *input.reloadStart &&
               within(input.reloadStart, *input.reloadEnd - *input.reloadStart)) {
        pose.upper = UpperClip::Reload;
        pose.upperProgress = (now - *input.reloadStart) / (*input.reloadEnd - *input.reloadStart);
    } else if (within(input.shotSeconds, rules.shotSeconds)) {
        pose.upper = UpperClip::Shoot;
        pose.upperProgress = (now - *input.shotSeconds) / rules.shotSeconds;
    }
    return pose;
}

} // namespace object_fps_pvp::sim
