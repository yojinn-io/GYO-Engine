#pragma once

// The Client's locomotion state machine that v8-product-anim-authority A4
// replaced with the Match's animation authority, frozen from c739bb79
// (PlayerPresentation.cpp, AdvancePlayerLocomotion) with its frame and state
// as they were. Only the E1 equivalence test uses it: the reference the
// Match's integer state machine was accepted against.

#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace PresentationLegacyLocomotion {

struct Frame final {
    std::uint64_t playerId{}, movementEpoch{};
    Engine::Math::Vec3 position{};
    float yaw{}, pitch{};
    double presentationSeconds{}, deltaSeconds{};
    double planarSpeed{};
    bool continuous{true}, holding{};
    std::uint64_t lifeGeneration{1};
    bool dead{};
    bool grounded{true};
    float verticalVelocity{};
    double lifeStateSeconds{};
    std::uint64_t shotActionId{}, reloadActionId{};
    double shotSeconds{}, reloadStartSeconds{}, reloadEndSeconds{};
    std::uint32_t damageCount{};
    double damageSeconds{};
    bool crouched{};
};

struct LocomotionState final {
    bool initialized{}, jogging{}, backward{}, holding{}, phaseReset{};
    std::uint64_t playerId{}, movementEpoch{}, resetCount{};
    Engine::Math::Vec3 previousPosition{};
    double previousPresentationSeconds{};
    double phaseCycles{}, unwrappedPhaseCycles{}, idleSeconds{};
    double signedDistance{}, totalDistance{}, distanceDelta{}, speed{}, playbackRate{};
    double jogWeight{}, cycleDistance{};
    float moveWeight{};
    std::string resetReason;
    std::uint64_t lifeGeneration{1};
    bool dead{};
    bool grounded{true};
    fps::pvp::PlayerJumpPhase jumpPhase{fps::pvp::PlayerJumpPhase::Grounded};
    double jumpPhaseSeconds{};
    bool crouched{};
};

inline bool AdvanceLocomotion(LocomotionState& state, const Frame& frame,
    const fps::pvp::PlayerPresentationDefinition& definition, std::string& error) {
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
        state.jumpPhase = frame.grounded ? fps::pvp::PlayerJumpPhase::Grounded : fps::pvp::PlayerJumpPhase::Airborne;
        state.jumpPhaseSeconds = frame.presentationSeconds;
        state.jogWeight = fps::pvp::PlayerJogWeight(definition, frame.planarSpeed);
        state.cycleDistance = fps::pvp::PlayerCycleDistance(definition, state.jogWeight);
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
        // The weight follows the authority-sampled speed, never this frame's own
        // displacement, so equal routes reach equal phases at any render rate.
        // Without a speed (held or stopped samples) the last blend is kept.
        if (frame.planarSpeed > 0) state.jogWeight = fps::pvp::PlayerJogWeight(definition, frame.planarSpeed);
        state.cycleDistance = fps::pvp::PlayerCycleDistance(definition, state.jogWeight);
        state.unwrappedPhaseCycles += signedDelta / state.cycleDistance;
        state.phaseCycles = state.unwrappedPhaseCycles - std::floor(state.unwrappedPhaseCycles);
        state.speed = elapsed > 0 ? state.distanceDelta / elapsed : 0;
        state.playbackRate = (state.backward ? -state.speed : state.speed) / state.cycleDistance;
        state.idleSeconds += elapsed;
        // All pose clocks follow the sampled timeline. A visible hold freezes
        // its last walking pose instead of blending legs while position stalls.
        if (!state.holding) {
            const float weightStep = static_cast<float>(elapsed / definition.transitionSeconds);
            state.moveWeight = state.jogging ? Engine::Math::Min(1.0F, state.moveWeight + weightStep) :
                Engine::Math::Max(0.0F, state.moveWeight - weightStep);
        }
        const double now = frame.presentationSeconds;
        if (state.grounded && !frame.grounded) {
            state.jumpPhase = frame.verticalVelocity > 0 ? fps::pvp::PlayerJumpPhase::Start : fps::pvp::PlayerJumpPhase::Airborne;
            state.jumpPhaseSeconds = now;
        } else if (!state.grounded && frame.grounded) {
            state.jumpPhase = fps::pvp::PlayerJumpPhase::Land;
            state.jumpPhaseSeconds = now;
        }
        // The contract spans end in sampled time; control never waits for them.
        if (state.jumpPhase == fps::pvp::PlayerJumpPhase::Start && now - state.jumpPhaseSeconds >= definition.jumpStartSeconds) {
            state.jumpPhase = fps::pvp::PlayerJumpPhase::Airborne;
            state.jumpPhaseSeconds += definition.jumpStartSeconds;
        } else if (state.jumpPhase == fps::pvp::PlayerJumpPhase::Land &&
                   now - state.jumpPhaseSeconds >= definition.jumpLandSeconds) {
            state.jumpPhase = fps::pvp::PlayerJumpPhase::Grounded;
            state.jumpPhaseSeconds += definition.jumpLandSeconds;
        }
        state.grounded = frame.grounded;
    }
    state.previousPosition = frame.position;
    state.previousPresentationSeconds = frame.presentationSeconds;
    state.crouched = frame.crouched;
    return true;
}


} // namespace PresentationLegacyLocomotion
