#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fps::pvp {
namespace {
// A phase correction is slewed: each frame moves the fixed-step clock by at
// most this share of its elapsed time, so the display never steps backwards.
constexpr double PhaseSlewFraction = 0.25;
// Share of a step a slew keeps clear of the runtime's catch-up capacity, well
// above FixedTickRuntime's own step tolerance (1e-9).
constexpr double PhaseCatchUpMargin = 1.0e-6;
// The first decision after a reset waits for this many positive frame
// intervals, so command ages no longer come from the first frames.
constexpr std::size_t PhaseFrameEvidence = 8;
double Percentile(const std::deque<double>& samples) {
    std::vector<double> sorted(samples.begin(), samples.end());
    const auto index = Engine::Math::Min(sorted.size() - 1,
        static_cast<std::size_t>(static_cast<double>(sorted.size()) * MovementPhasePercentile));
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(index), sorted.end());
    return sorted[index];
}
} // namespace

LocalPlayerPrediction::LocalPlayerPrediction(const Arena& arena) : arena_(arena) {
    std::string error;
    if (!arena_.Validate(error)) throw std::invalid_argument(error);
}

void LocalPlayerPrediction::SetMovementRules(MovementRules rules) {
    if (!std::isfinite(rules.jumpHeight) || rules.jumpHeight <= 0 ||
        !std::isfinite(rules.gravity) || rules.gravity <= 0)
        throw std::invalid_argument("Invalid authoritative movement rules");
    arena_.jumpHeight = rules.jumpHeight;
    arena_.gravity = rules.gravity;
}

void LocalPlayerPrediction::Reset() noexcept {
    ticks_.Reset();
    secondsUntilNextStep_ = MovementTickSeconds;
    pending_.clear();
    current_ = previous_ = {};
    correction_ = {};
    correctionSeconds_ = 0;
    alpha_ = 0;
    sendPending_ = false;
    freshSeed_ = false;
    pendingJump_ = false;
    phaseShiftSeconds_ = phaseAppliedSeconds_ = 0;
    frameEvidence_ = 0;
    commandAges_ = {};
    phaseSamples_.clear();
    settledAfter_ = lastSlackSequence_ = 0;
    lateSamples_ = 0;
    settling_ = phaseDecided_ = reacquiring_ = false;
    observation_ = {};
}

void LocalPlayerPrediction::SeedLead(const PlayerState& authority) {
    ticks_.Reset();
    secondsUntilNextStep_ = MovementTickSeconds;
    pendingJump_ = false;
    alpha_ = 0;
    pending_.clear();
    current_ = previous_ = authority;
    for (std::size_t index = 0; index < InitialCommandLead &&
         current_.lastResolvedCommand < (std::numeric_limits<std::uint64_t>::max)(); ++index) {
        MovementCommand command{current_.lastResolvedCommand + 1, 0, 0, authority.yaw, authority.pitch};
        pending_.push_back(command);
        previous_ = current_;
        current_ = StepMovement(arena_, current_, command);
        TraceMovement({.kind = MovementTraceKind::Generated, .playerId = current_.playerId,
            .epoch = current_.movementEpoch, .sequence = command.sequence, .seededNeutral = true, .lifeGeneration = current_.lifeGeneration});
    }
    sendPending_ = true;
    freshSeed_ = true;
    // Every seed (epoch start or stall reseed) starts a new fixed-step phase.
    // Tracking measures it from its first real command; the remaining part of
    // any correction described the old phase, and so does every older
    // command's applied shift.
    phaseShiftSeconds_ = phaseAppliedSeconds_ = 0;
    commandAges_ = {};
    phaseSamples_.clear();
    settledAfter_ = current_.lastResolvedCommand;
    lateSamples_ = 0;
    settling_ = phaseDecided_ = reacquiring_ = false;
    observation_.phaseTracking = PhaseTrackingState::Acquiring;
    observation_.phaseErrorSeconds.reset();
}

// Called only when the previous correction has fully slewed in
// (phaseShiftSeconds_ == 0, not settling): the caller's guard in TrackPhase
// ensures it. A decided error is relative to the phase as it will be once every
// correction so far has slewed in, so it replaces a remainder of zero. Called
// mid-slew it would have to add to the remainder: overwriting it drops the part
// not slewed in yet, and the rebase of every sample, which takes the decided
// total as the applied part plus the remainder, would be wrong too.
void LocalPlayerPrediction::Correct(double error, bool late) {
    phaseShiftSeconds_ = Engine::Math::Clamp(error, -MovementPhaseMaximumCorrectionSeconds, MovementPhaseMaximumCorrectionSeconds);
    settling_ = true;
    phaseSamples_.clear();
    lateSamples_ = 0;
    observation_.phaseCorrectionSeconds = phaseShiftSeconds_;
    observation_.phaseTracking = PhaseTrackingState::Settling;
    ++observation_.phaseCorrections;
    if (late) ++observation_.phaseLateCorrections;
}

void LocalPlayerPrediction::TrackPhase(const PlayerState& authority) {
    // A Host slack sample for one of this phase's real commands, plus that
    // command's age when its window was published, is the slack it would have
    // had if sent at its fixed-step boundary. Less the sequence lead and the
    // target it is how far the phase may move later (positive) or must move
    // earlier (negative). The same quantity A1 took once per epoch. A command
    // generated before part of the corrections so far was slewed in arrives
    // that much earlier or later than one generated now: its slack is rebased
    // by that part, so samples taken while a correction slews describe the
    // corrected phase. Samples keep arriving while a correction slews: if
    // commands were still late after the rebase, the next correction follows
    // as soon as this one settles, so a phase more than one correction behind
    // recovers before the Host's Starvation fuse. After a late correction the
    // next decision is a first decision again (reacquiring_), on every rebased
    // sample of the seed: a delay that has passed is withdrawn within one slew,
    // less the send wait of the commands produced during the late slew (about
    // 3.3 ms, Movement.hpp InputSendDrainRate), and only as far as the samples
    // at the moment of that decision show it (known limitation, v7 batch 09b).
    if (authority.movementSlackSequence && authority.movementSlackMicros &&
        (*authority.movementSlackSequence > settledAfter_ || reacquiring_) &&
        *authority.movementSlackSequence != lastSlackSequence_) {
        const auto sequence = *authority.movementSlackSequence;
        lastSlackSequence_ = sequence;
        const auto& age = commandAges_[sequence % commandAges_.size()];
        if (age.sequence == sequence) {
            const double slack = *authority.movementSlackMicros * 1.0e-6 -
                (phaseAppliedSeconds_ + phaseShiftSeconds_ - age.appliedSeconds);
            phaseSamples_.push_back(slack + age.seconds -
                static_cast<double>(InitialCommandLead) * MovementTickSeconds - MovementPhaseTargetSeconds);
            ++observation_.phaseSamples;
            observation_.phaseSampleSequence = sequence;
            if (phaseSamples_.size() > MovementPhaseWindowSamples) phaseSamples_.pop_front();
            lateSamples_ = slack < 0 ? lateSamples_ + 1 : 0;
        }
    }
    if (settling_ || phaseShiftSeconds_ != 0) return;
    // Commands arriving after their sequence was substituted: correct at once
    // from the latest of them instead of waiting for a window.
    if (lateSamples_ >= MovementPhaseLateSamples && phaseSamples_.size() >= lateSamples_) {
        const double error = *std::min_element(phaseSamples_.end() - static_cast<std::ptrdiff_t>(lateSamples_), phaseSamples_.end());
        const std::size_t used = lateSamples_;
        Correct(error, true);
        phaseDecided_ = reacquiring_ = true;
        TracePhaseDecision(MovementPhasePath::Late, error, 0, used);
        return;
    }
    if (frameEvidence_ < PhaseFrameEvidence) return;
    const bool first = !phaseDecided_ || reacquiring_;
    const auto needed = first ? MovementPhaseFirstSamples : MovementPhaseWindowSamples;
    if (phaseSamples_.size() < needed) return;
    const double error = Percentile(phaseSamples_);
    const double deadband = first ? MovementPhaseFirstDeadbandSeconds : MovementPhaseDeadbandSeconds;
    phaseDecided_ = true;
    reacquiring_ = false;
    observation_.phaseErrorSeconds = error;
    observation_.phaseTracking = PhaseTrackingState::Tracking;
    const auto used = phaseSamples_.size();
    const bool corrects = std::abs(error) > deadband;
    if (corrects) Correct(error, false);
    // Every first decision, but only the window decisions that start a
    // correction: once tracking, the window is decided again on every new
    // sample (about 40 a second), almost always inside the deadband.
    if (first || corrects)
        TracePhaseDecision(first ? MovementPhasePath::First : MovementPhasePath::Window, error, deadband, used);
}

// Diagnostics only (E1-3, observability-tools OT-1): a decision TrackPhase
// just made, after Correct (phaseShiftSeconds_ is the correction it started, 0
// when the error stayed inside the deadband).
void LocalPlayerPrediction::TracePhaseDecision(MovementPhasePath path, double error, double deadband,
                                               std::size_t samples) const {
    TraceMovement({.kind = MovementTraceKind::PhaseDecision, .playerId = current_.playerId,
        .epoch = current_.movementEpoch, .sequence = observation_.phaseSampleSequence,
        .count = static_cast<std::uint64_t>(samples), .lifeGeneration = current_.lifeGeneration, .phasePath = path,
        .phaseErrorSeconds = error, .phaseShiftSeconds = phaseShiftSeconds_, .phaseDeadbandSeconds = deadband});
}

void LocalPlayerPrediction::ObservePhaseSample(const PlayerState& authority) {
    if (!observation_.active || authority.playerId != current_.playerId ||
        authority.movementEpoch != current_.movementEpoch || authority.lifeGeneration != current_.lifeGeneration) return;
    TrackPhase(authority);
}

bool LocalPlayerPrediction::Reseeds(const PlayerState& authority) const noexcept {
    return !observation_.active || current_.playerId != authority.playerId ||
        authority.lifeGeneration != current_.lifeGeneration || authority.movementEpoch != current_.movementEpoch ||
        authority.lastResolvedCommand > current_.lastResolvedCommand;
}

void LocalPlayerPrediction::Reconcile(const PlayerState& authority, std::uint64_t authorityTick) {
    if (authority.playerId == 0 || authority.movementEpoch == 0 || !authority.lifeGeneration ||
        !std::isfinite(authority.verticalVelocity) ||
        (authority.lifeState != LifeState::Alive && authority.lifeState != LifeState::Dead) ||
        authority.contiguousPendingCommands > MaxFutureCommands || !Engine::Math::IsFinite(authority.position) ||
        !ValidMovementCommand({1, 0, 0, authority.yaw, authority.pitch})) return;
    const bool newPlayer = !observation_.active || current_.playerId != authority.playerId;
    if (!newPlayer && (authorityTick <= observation_.authorityTick ||
        authority.lifeGeneration < current_.lifeGeneration || authority.movementEpoch < current_.movementEpoch ||
        (authority.movementEpoch == current_.movementEpoch &&
         authority.lastResolvedCommand < observation_.lastResolvedCommand))) return;
    if (newPlayer || authority.lifeGeneration != current_.lifeGeneration || authority.movementEpoch > current_.movementEpoch) {
        if (!newPlayer && authority.lifeGeneration != current_.lifeGeneration) {
            for (const auto& command : pending_)
                TraceMovement({.kind = MovementTraceKind::LifecycleCancelled, .playerId = current_.playerId,
                    .epoch = current_.movementEpoch, .sequence = command.sequence, .authorityTick = authorityTick,
                    .lifeGeneration = current_.lifeGeneration});
        }
        // New epochs have an independent sequence namespace. No old input,
        // fractional simulation time or correction may leak into this seed.
        // Frame pacing belongs to the display loop and carries over.
        const auto frameEvidence = frameEvidence_;
        Reset();
        frameEvidence_ = frameEvidence;
        observation_.active = true;
        SeedLead(authority);
    } else {
        const auto oldBase = Engine::Math::Lerp(previous_.position, current_.position, alpha_);
        const auto oldPredictedPosition = current_.position;
        const auto oldPreviousState = previous_;
        const auto oldTip = current_.lastResolvedCommand;
        const auto oldPrevious = previous_.lastResolvedCommand;
        while (!pending_.empty() && pending_.front().sequence <= authority.lastResolvedCommand)
            pending_.pop_front();
        if (authority.lastResolvedCommand > oldTip) {
            // Neutral future steps restore sequence lead after a stall. No
            // relationship between client and authority clocks is assumed.
            SeedLead(authority);
            ++observation_.stallReseeds;
        } else {
            current_ = previous_ = authority;
            for (const auto& command : pending_) {
                current_ = StepMovement(arena_, current_, command);
                if (command.sequence <= oldPrevious) previous_ = current_;
            }
            if (authority.lastResolvedCommand > oldPrevious) {
                // A full ACK may consume the older display endpoint while it
                // is still being interpolated. Keep that endpoint's sequence
                // and apply only the replayed tip's actual position error.
                // Collapsing it into authority would turn normal interpolation
                // lag into a correction even for a perfectly predicted ACK.
                previous_ = oldPreviousState;
                const auto delta = current_.position - oldPredictedPosition;
                previous_.position = previous_.position + delta;
            }
        }
        const auto newBase = Engine::Math::Lerp(previous_.position, current_.position, alpha_);
        const auto displacement = oldBase - newBase;
        const auto corrected = correction_ + displacement;
        if (Engine::Math::Length(displacement) >= MovementHardCorrectionDistance ||
            Engine::Math::Length(corrected) >= MovementHardCorrectionDistance) {
            correction_ = {};
            correctionSeconds_ = 0;
            previous_ = current_;
        } else if (Engine::Math::Length(displacement) > 0.000001F) {
            correction_ = corrected;
            correctionSeconds_ = MovementCorrectionSeconds;
        }
    }
    if (authority.movementEpoch == current_.movementEpoch && authority.lifeGeneration == current_.lifeGeneration)
        TrackPhase(authority);
    if (authority.lifeState == LifeState::Dead) pendingJump_ = false;
    observation_.authorityTick = authorityTick;
    observation_.lastResolvedCommand = authority.lastResolvedCommand;
    observation_.movementEpoch = authority.movementEpoch;
    observation_.serverPendingCommands = authority.contiguousPendingCommands;
    UpdatePresentation();
}

bool LocalPlayerPrediction::Advance(double frameSeconds, float forward, float right,
                                    float yaw, float pitch, bool jumpRequested) {
    if (!observation_.active) return false;
    if (!ValidMovementCommand({1, forward, right, yaw, pitch}) ||
        !std::isfinite(frameSeconds) || frameSeconds < 0)
        throw std::invalid_argument("Invalid local movement input");
    if (frameSeconds > 0 && frameEvidence_ < PhaseFrameEvidence) ++frameEvidence_;
    if (current_.lifeState != LifeState::Alive || pending_.size() >= MaxPendingCommands) pendingJump_ = false;
    else pendingJump_ = pendingJump_ || jumpRequested;
    if (current_.lifeState == LifeState::Dead) { forward = right = 0; yaw = current_.yaw; pitch = current_.pitch; }
    // A new seed is based on authority received this frame. The preceding
    // frame's elapsed interval is already represented by that state; replaying
    // its catch-up time would permanently add steps to the command lead. Allow
    // at most one fresh input step so current controls still respond promptly.
    // Full acknowledgement is ordinary at low latency: retain the fractional
    // fixed-step phase. Only an abnormal fully-covered frame gap loses its old
    // simulation debt; normal 30 FPS frames must still produce two commands.
    // A first substep frame may consume freshSeed_ without publishing anything.
    // Preview only the copied scheduler: no commands are generated and the real
    // clock is unchanged. Three accumulated startup steps would leave excess
    // persistent lead; an ordinary 30 FPS pair remains legitimate.
    const bool unpublishedBootstrap = observation_.lastResolvedCommand == 0 &&
        current_.lastResolvedCommand <= InitialCommandLead;
    bool bootstrapCatchUp{};
    if (unpublishedBootstrap) {
        auto preview = ticks_;
        bootstrapCatchUp = preview.Advance(frameSeconds,
            [](const Engine::Runtime::TickContext&) {}).steps >= 3;
    }
    const bool coveredGap = bootstrapCatchUp ||
        (pending_.empty() && frameSeconds > MovementMaximumRegularFrameSeconds);
    const double elapsed = freshSeed_ || coveredGap ?
        Engine::Math::Min(frameSeconds, MovementTickSeconds) : frameSeconds;
    freshSeed_ = false;
    bool send = sendPending_;
    sendPending_ = false;
    double advanceSeconds = elapsed;
    if (phaseShiftSeconds_ != 0) {
        const double limit = elapsed * PhaseSlewFraction;
        // A negative shift runs the clock ahead of the frame. The runtime steps
        // at most CatchUpSteps times per call and drops whole steps beyond
        // that, so on a frame of four ticks or more the slew could lose a step
        // instead of moving the phase. It takes only the time the runtime can
        // still step this frame (none when the frame alone drops steps); the
        // rest waits for later frames.
        double ahead = limit;
        if (phaseShiftSeconds_ < 0) {
            auto preview = ticks_;
            const double step = preview.StepSeconds();
            const double accumulated = step -
                preview.Advance(0.0, [](const Engine::Runtime::TickContext&) {}).secondsUntilNextTick;
            const double capacity = (CatchUpSteps + 1) * step - accumulated - elapsed -
                step * PhaseCatchUpMargin;
            ahead = Engine::Math::Clamp(capacity, 0.0, limit);
        }
        const double shift = Engine::Math::Clamp(phaseShiftSeconds_, -ahead, limit);
        advanceSeconds -= shift;
        phaseShiftSeconds_ -= shift;
        phaseAppliedSeconds_ += shift;
    }
    std::uint64_t blockedSteps{};
    std::array<std::uint64_t, CatchUpSteps + 1> generated{};
    std::size_t generatedCount{};
    const auto advance = ticks_.Advance(advanceSeconds, [&](const Engine::Runtime::TickContext&) {
        previous_ = current_;
        if (pending_.size() < MaxPendingCommands &&
            current_.lastResolvedCommand < (std::numeric_limits<std::uint64_t>::max)()) {
            MovementCommand command{current_.lastResolvedCommand + 1, forward, right, yaw, pitch, pendingJump_};
            pendingJump_ = false;
            current_ = StepMovement(arena_, current_, command);
            pending_.push_back(command);
            TraceMovement({.kind = MovementTraceKind::Generated, .playerId = current_.playerId,
                .epoch = current_.movementEpoch, .sequence = command.sequence, .lifeGeneration = current_.lifeGeneration});
            if (generatedCount < generated.size()) generated[generatedCount++] = command.sequence;
            send = true;
        } else { ++blockedSteps; pendingJump_ = false; }
    });
    // Each real command's age when this frame publishes it: the newest step
    // boundary passed (step - secondsUntilNextTick) ago, older ones a step each
    // before. Their boundaries are on the clock after this frame's slew.
    for (std::size_t index = 0; index < generatedCount; ++index)
        commandAges_[generated[index] % commandAges_.size()] = {generated[index],
            static_cast<double>(generatedCount - 1 - index) * MovementTickSeconds +
            (MovementTickSeconds - advance.secondsUntilNextTick), phaseAppliedSeconds_};
    if (settling_ && phaseShiftSeconds_ == 0) {
        // Only commands generated from now on describe the corrected phase.
        settling_ = false;
        settledAfter_ = current_.lastResolvedCommand;
        observation_.phaseTracking = PhaseTrackingState::Tracking;
    }
    alpha_ = Engine::Math::Clamp(static_cast<float>(1.0 - advance.secondsUntilNextTick / MovementTickSeconds),
                                 0.0F, 1.0F);
    secondsUntilNextStep_ = advance.secondsUntilNextTick;
    if (frameSeconds >= 0.1 || frameSeconds > elapsed || advance.droppedSeconds > 0 || blockedSteps > 0)
        TraceMovement({.kind = MovementTraceKind::RuntimeGap, .playerId = current_.playerId,
            .epoch = current_.movementEpoch, .sequence = current_.lastResolvedCommand,
            .authorityTick = observation_.authorityTick,
            .pending = static_cast<std::uint32_t>(pending_.size()),
            .droppedSeconds = advance.droppedSeconds + (frameSeconds - elapsed) +
                blockedSteps * MovementTickSeconds,
            .frameSeconds = frameSeconds, .count = blockedSteps, .lifeGeneration = current_.lifeGeneration});
    if (correctionSeconds_ > 0) {
        const auto remaining = Engine::Math::Max(0.0, correctionSeconds_ - elapsed);
        const auto scale = static_cast<float>(remaining / correctionSeconds_);
        correction_ = correction_ * scale;
        correctionSeconds_ = remaining;
    }
    UpdatePresentation();
    // Keep both neutral lead commands ahead of the first legitimate fixed
    // step. A neutral-only window would start authority before that step exists.
    const bool neutralOnlyBootstrap = observation_.lastResolvedCommand == 0 &&
        current_.lastResolvedCommand <= InitialCommandLead;
    return send && !pending_.empty() && !neutralOnlyBootstrap;
}

LocalRenderSample InterpolateLocalPresentation(const Arena& arena, const LocalPresentationState& state,
                                               const double secondsLater) {
    auto alpha = state.alpha;
    auto correction = state.correction;
    if (secondsLater > 0) {
        alpha = Engine::Math::Clamp(static_cast<float>(alpha + secondsLater / MovementTickSeconds), 0.0F, 1.0F);
        if (state.correctionSeconds > 0) {
            const auto remaining = Engine::Math::Max(0.0, state.correctionSeconds - secondsLater);
            correction = correction * static_cast<float>(remaining / state.correctionSeconds);
        }
    }
    const auto base = Engine::Math::Lerp(state.previousPosition, state.currentPosition, alpha);
    const auto target = base + correction;
    const auto position = state.currentPosition;
    // Sweep from the valid predicted body to the proposed display body. An
    // offset cannot carry the camera through a wall, including around corners.
    auto render = MoveCharacterBody(
        {position, arena.bodyHeight, arena.radius}, target - position, arena.walls, {}, false);
    render.y = Engine::Math::Max(0.0F, render.y);
    return {render, render - base, alpha};
}

double LocalPlayerPrediction::SecondsUntilNextStep() const noexcept {
    // Advance moves the clock by the elapsed time minus the slew it takes, at
    // most PhaseSlewFraction of that time: a delay stretches the wait, an
    // advance shortens it. Solves elapsed - shift(elapsed) = remaining.
    const double remaining = secondsUntilNextStep_;
    if (phaseShiftSeconds_ > 0)
        return Engine::Math::Min(remaining / (1.0 - PhaseSlewFraction), remaining + phaseShiftSeconds_);
    if (phaseShiftSeconds_ < 0)
        return Engine::Math::Max(remaining / (1.0 + PhaseSlewFraction), remaining + phaseShiftSeconds_);
    return remaining;
}

LocalPresentationState LocalPlayerPrediction::Presentation() const noexcept {
    return {previous_.position, current_.position, correction_, correctionSeconds_, alpha_};
}

void LocalPlayerPrediction::UpdatePresentation() {
    const auto sample = InterpolateLocalPresentation(arena_, Presentation(), 0.0);
    const auto position = current_.position;
    const auto render = sample.renderPosition;
    observation_.verticalVelocity = current_.verticalVelocity;
    observation_.grounded = current_.grounded;
    observation_.lifeGeneration = current_.lifeGeneration;
    observation_.lifeState = current_.lifeState;
    observation_.predictedPosition = position;
    observation_.renderPosition = render;
    observation_.correctionOffset = sample.correctionOffset;
    observation_.latestCommand = current_.lastResolvedCommand;
    observation_.previousCommand = previous_.lastResolvedCommand;
    observation_.currentCommand = current_.lastResolvedCommand;
    observation_.interpolationAlpha = alpha_;
    observation_.pendingCommands = pending_.size();
    observation_.frozen = pending_.size() == MaxPendingCommands ||
        current_.lastResolvedCommand == (std::numeric_limits<std::uint64_t>::max)();
}

std::optional<ShotGateView> LocalPlayerPrediction::ShotGate() const noexcept {
    if (!observation_.active || observation_.frozen) return std::nullopt;
    return ShotGateView{.snapshotTick = observation_.authorityTick, .snapshotEpoch = observation_.movementEpoch,
        .snapshotLastResolved = observation_.lastResolvedCommand, .epoch = current_.movementEpoch,
        .sequence = current_.lastResolvedCommand};
}

PlayerInput LocalPlayerPrediction::PendingInput() const {
    return {current_.playerId, {pending_.begin(), pending_.end()}, current_.movementEpoch, current_.lifeGeneration,
            observation_.authorityTick};
}

const LocalMovementObservation& LocalPlayerPrediction::Observation() const noexcept {
    return observation_;
}

} // namespace fps::pvp
