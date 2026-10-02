#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fps::pvp {
namespace {
Float3 Interpolate(Float3 from, Float3 to, float alpha) {
    return {from.x + (to.x - from.x) * alpha,
            from.y + (to.y - from.y) * alpha,
            from.z + (to.z - from.z) * alpha};
}
Float3 Difference(Float3 lhs, Float3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}
// A start-phase shift is slewed: each frame moves the fixed-step clock by at
// most this share of its elapsed time, so the display never steps backwards.
constexpr double StartPhaseSlewFraction = 0.25;
// Share of a step a slew keeps clear of the runtime's catch-up capacity, well
// above FixedTickRuntime's own step tolerance (1e-9).
constexpr double StartPhaseCatchUpMargin = 1.0e-6;
// The start phase is armed once this many frame intervals describe the frame
// rate; the mean then grows to the whole interval window.
constexpr std::size_t StartPhaseFrameEvidence = 8;
// One missed 60 Hz refresh. A longer hitch counts as this, so a single stall
// weighs like one dropped frame while every refresh a vsync-paced display drops
// still counts in full.
constexpr double StartPhaseFrameIntervalLimit = 2 * MovementTickSeconds;
float Length(Float3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}
bool Finite(Float3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
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
    pending_.clear();
    current_ = previous_ = {};
    correction_ = {};
    correctionSeconds_ = 0;
    alpha_ = 0;
    sendPending_ = false;
    freshSeed_ = false;
    pendingJump_ = false;
    startPhasePending_ = false;
    startWindowLagSeconds_.reset();
    phaseShiftSeconds_ = 0;
    armedPhaseShiftSeconds_.reset();
    startPhaseWithdrawn_ = false;
    startPhaseRecoveredFrames_ = 0;
    frameIntervals_ = {};
    observation_ = {};
}

void LocalPlayerPrediction::SeedLead(const PlayerState& authority) {
    // An epoch-start seed follows Reset(), so a start phase still pending or
    // armed here belongs to a stall reseed, which rebases the phase the Host
    // timed. The state below disarms it either way; the diagnostics say so
    // instead of keeping the last shift or withdrawal. A wait already taken
    // stays reported.
    if (startPhasePending_ || armedPhaseShiftSeconds_) {
        observation_.startPhaseShiftSeconds.reset();
        observation_.startPhaseSkip = StartPhaseSkip::CancelledByReseed;
    }
    ticks_.Reset();
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
    // Only an epoch-start seed publishes the first window the Host times. A
    // stall reseed rebases the phase within a running epoch; nothing measures it.
    startPhasePending_ = authority.lastResolvedCommand == 0;
    startWindowLagSeconds_.reset();
    phaseShiftSeconds_ = 0;
    armedPhaseShiftSeconds_.reset();
    startPhaseWithdrawn_ = false;
    startPhaseRecoveredFrames_ = 0;
}

std::optional<double> LocalPlayerPrediction::FramePeriodSeconds() const {
    // A plain mean: a vsync-paced display drops whole refreshes, and discarding
    // the longest intervals as outliers would hide exactly those drops. The
    // two-tick limit already bounds what one long hitch contributes.
    if (frameIntervals_.count < StartPhaseFrameEvidence) return std::nullopt;
    double sum{};
    for (std::size_t index = 0; index < frameIntervals_.count; ++index) sum += frameIntervals_.seconds[index];
    return sum / static_cast<double>(frameIntervals_.count);
}

void LocalPlayerPrediction::Reconcile(const PlayerState& authority, std::uint64_t authorityTick) {
    if (authority.playerId == 0 || authority.movementEpoch == 0 || !authority.lifeGeneration ||
        !std::isfinite(authority.verticalVelocity) ||
        (authority.lifeState != LifeState::Alive && authority.lifeState != LifeState::Dead) ||
        authority.contiguousPendingCommands > MaxFutureCommands || !Finite(authority.position) ||
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
        const auto frameIntervals = frameIntervals_;
        Reset();
        frameIntervals_ = frameIntervals;
        observation_.active = true;
        SeedLead(authority);
    } else {
        const auto oldBase = Interpolate(previous_.position, current_.position, alpha_);
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
                const auto delta = Difference(current_.position, oldPredictedPosition);
                previous_.position = {previous_.position.x + delta.x,
                                      previous_.position.y + delta.y,
                                      previous_.position.z + delta.z};
            }
        }
        const auto newBase = Interpolate(previous_.position, current_.position, alpha_);
        const auto displacement = Difference(oldBase, newBase);
        const Float3 corrected{correction_.x + displacement.x,
                               correction_.y + displacement.y,
                               correction_.z + displacement.z};
        if (Length(displacement) >= MovementHardCorrectionDistance ||
            Length(corrected) >= MovementHardCorrectionDistance) {
            correction_ = {};
            correctionSeconds_ = 0;
            previous_ = current_;
        } else if (Length(displacement) > 0.000001F) {
            correction_ = corrected;
            correctionSeconds_ = MovementCorrectionSeconds;
        }
    }
    if (startPhasePending_ && startWindowLagSeconds_ && authority.epochStartWaitMicros &&
        authority.movementEpoch == current_.movementEpoch && authority.lifeGeneration == current_.lifeGeneration) {
        // Wait plus the first step's age at publication is the slack beyond
        // the sequence lead for a command sent at its fixed-step boundary.
        // Shifting the phase sets that slack to the target for every later
        // command; the sequence-to-tick mapping and lead are unchanged.
        // The shift is armed once frame evidence exists; until then the timed
        // phase stays pending and every snapshot of this epoch repeats the wait.
        // Armed while the frame-rate measure is above the cut (below about
        // 54.5 FPS, MovementStartPhaseMaximumFrameSeconds), it starts withdrawn
        // and Advance applies it once frames recover.
        const double wait = *authority.epochStartWaitMicros * 1.0e-6;
        const auto framePeriod = FramePeriodSeconds();
        if (wait > MovementStartPhaseMaximumWaitSeconds || framePeriod) {
            startPhasePending_ = false;
            observation_.epochStartWaitSeconds = wait;
            if (wait > MovementStartPhaseMaximumWaitSeconds) {
                observation_.startPhaseSkip = StartPhaseSkip::HostLate;
            } else {
                armedPhaseShiftSeconds_ = wait + *startWindowLagSeconds_ - MovementStartPhaseTargetSeconds;
                startPhaseWithdrawn_ = *framePeriod > MovementStartPhaseMaximumFrameSeconds;
                if (startPhaseWithdrawn_) {
                    observation_.startPhaseSkip = StartPhaseSkip::FrameRateBelowTick;
                } else {
                    phaseShiftSeconds_ = *armedPhaseShiftSeconds_;
                    observation_.startPhaseShiftSeconds = phaseShiftSeconds_;
                }
            }
        }
    }
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
    if (frameSeconds > 0) {
        frameIntervals_.seconds[frameIntervals_.next] = (std::min)(frameSeconds, StartPhaseFrameIntervalLimit);
        frameIntervals_.next = (frameIntervals_.next + 1) % frameIntervals_.seconds.size();
        frameIntervals_.count = (std::min)(frameIntervals_.count + 1, frameIntervals_.seconds.size());
    }
    if (armedPhaseShiftSeconds_) {
        // The shift is withdrawn on the first frame whose mean is above the cut
        // (below about 54.5 FPS, 1.1 tick): the same slew takes the clock back
        // to the unshifted phase (cancelling any remainder). Each interval
        // counts as at most two ticks, so a drop from 60 FPS needs up to four
        // frames at 30 FPS and 16-20 frames at a vsync-paced 50 FPS to get
        // there. The shift returns only once the mean has stayed at or below
        // the restore bound (about 56.6 FPS, 1.06 tick) for a 32-frame dwell,
        // so a frame rate between the two bounds keeps whichever state it
        // reached and a short run of whole frames inside a low frame rate
        // (random missed refreshes) does not bring it back.
        const auto framePeriod = FramePeriodSeconds();
        bool withdrawn = startPhaseWithdrawn_;
        if (!withdrawn) {
            withdrawn = framePeriod && *framePeriod > MovementStartPhaseMaximumFrameSeconds;
        } else if (framePeriod && *framePeriod <= MovementStartPhaseRestoreFrameSeconds) {
            if (frameSeconds > 0) withdrawn = ++startPhaseRecoveredFrames_ < frameIntervals_.seconds.size();
        } else {
            startPhaseRecoveredFrames_ = 0;
        }
        if (withdrawn != startPhaseWithdrawn_) {
            startPhaseRecoveredFrames_ = 0;
            phaseShiftSeconds_ += withdrawn ? -*armedPhaseShiftSeconds_ : *armedPhaseShiftSeconds_;
            startPhaseWithdrawn_ = withdrawn;
            if (withdrawn) {
                observation_.startPhaseShiftSeconds.reset();
                observation_.startPhaseSkip = StartPhaseSkip::FrameRateBelowTick;
            } else {
                observation_.startPhaseShiftSeconds = armedPhaseShiftSeconds_;
                observation_.startPhaseSkip.reset();
            }
        }
    }
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
        (std::min)(frameSeconds, MovementTickSeconds) : frameSeconds;
    freshSeed_ = false;
    bool send = sendPending_;
    sendPending_ = false;
    // Time discarded after the first window rebases the phase the Host timed.
    if (startWindowLagSeconds_ && frameSeconds > elapsed) startPhasePending_ = false;
    double advanceSeconds = elapsed;
    if (phaseShiftSeconds_ != 0) {
        const double limit = elapsed * StartPhaseSlewFraction;
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
                step * StartPhaseCatchUpMargin;
            ahead = std::clamp(capacity, 0.0, limit);
        }
        const double shift = std::clamp(phaseShiftSeconds_, -ahead, limit);
        advanceSeconds -= shift;
        phaseShiftSeconds_ -= shift;
    }
    std::uint64_t blockedSteps{};
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
            send = true;
        } else { ++blockedSteps; pendingJump_ = false; }
    });
    alpha_ = std::clamp(static_cast<float>(1.0 - advance.secondsUntilNextTick / MovementTickSeconds),
                        0.0F, 1.0F);
    if (frameSeconds >= 0.1 || frameSeconds > elapsed || advance.droppedSeconds > 0 || blockedSteps > 0)
        TraceMovement({.kind = MovementTraceKind::RuntimeGap, .playerId = current_.playerId,
            .epoch = current_.movementEpoch, .sequence = current_.lastResolvedCommand,
            .authorityTick = observation_.authorityTick,
            .pending = static_cast<std::uint32_t>(pending_.size()),
            .droppedSeconds = advance.droppedSeconds + (frameSeconds - elapsed) +
                blockedSteps * MovementTickSeconds,
            .frameSeconds = frameSeconds, .count = blockedSteps, .lifeGeneration = current_.lifeGeneration});
    if (correctionSeconds_ > 0) {
        const auto remaining = (std::max)(0.0, correctionSeconds_ - elapsed);
        const auto scale = static_cast<float>(remaining / correctionSeconds_);
        correction_ = {correction_.x * scale, correction_.y * scale, correction_.z * scale};
        correctionSeconds_ = remaining;
    }
    UpdatePresentation();
    // Keep both neutral lead commands ahead of the first legitimate fixed
    // step. A neutral-only window would start authority before that step exists.
    const bool neutralOnlyBootstrap = observation_.lastResolvedCommand == 0 &&
        current_.lastResolvedCommand <= InitialCommandLead;
    const bool publish = send && !pending_.empty() && !neutralOnlyBootstrap;
    if (publish && startPhasePending_ && !startWindowLagSeconds_) {
        // The first legitimate step is the oldest generated this frame.
        if (advance.steps > 0)
            startWindowLagSeconds_ = (advance.steps - 1) * MovementTickSeconds +
                (MovementTickSeconds - advance.secondsUntilNextTick);
        else startPhasePending_ = false;
    }
    return publish;
}

void LocalPlayerPrediction::UpdatePresentation() {
    const auto base = Interpolate(previous_.position, current_.position, alpha_);
    const Float3 target{base.x + correction_.x, base.y + correction_.y, base.z + correction_.z};
    const auto position = current_.position;
    // Sweep from the valid predicted body to the proposed display body. An
    // offset cannot carry the camera through a wall, including around corners.
    auto render = MoveCharacterBody(
        {{position.x, position.y, position.z}, arena_.bodyHeight, arena_.radius},
        Difference(target, position), arena_.walls, {}, false);
    render.y = (std::max)(0.0F, render.y);
    observation_.verticalVelocity = current_.verticalVelocity;
    observation_.grounded = current_.grounded;
    observation_.lifeGeneration = current_.lifeGeneration;
    observation_.lifeState = current_.lifeState;
    observation_.predictedPosition = position;
    observation_.renderPosition = render;
    observation_.correctionOffset = Difference(render, base);
    observation_.latestCommand = current_.lastResolvedCommand;
    observation_.previousCommand = previous_.lastResolvedCommand;
    observation_.currentCommand = current_.lastResolvedCommand;
    observation_.interpolationAlpha = alpha_;
    observation_.pendingCommands = pending_.size();
    observation_.frozen = pending_.size() == MaxPendingCommands ||
        current_.lastResolvedCommand == (std::numeric_limits<std::uint64_t>::max)();
}

PlayerInput LocalPlayerPrediction::PendingInput() const {
    return {current_.playerId, {pending_.begin(), pending_.end()}, current_.movementEpoch, current_.lifeGeneration};
}

const LocalMovementObservation& LocalPlayerPrediction::Observation() const noexcept {
    return observation_;
}

} // namespace fps::pvp
