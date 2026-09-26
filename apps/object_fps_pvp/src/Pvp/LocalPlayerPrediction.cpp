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

void LocalPlayerPrediction::Reset() noexcept {
    ticks_.Reset();
    pending_.clear();
    current_ = previous_ = {};
    correction_ = {};
    correctionSeconds_ = 0;
    alpha_ = 0;
    sendPending_ = false;
    freshSeed_ = false;
    observation_ = {};
}

void LocalPlayerPrediction::SeedLead(const PlayerState& authority) {
    ticks_.Reset();
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
            .epoch = current_.movementEpoch, .sequence = command.sequence, .seededNeutral = true});
    }
    sendPending_ = true;
    freshSeed_ = true;
}

void LocalPlayerPrediction::Reconcile(const PlayerState& authority, std::uint64_t authorityTick) {
    if (authority.playerId == 0 || authority.movementEpoch == 0 ||
        authority.contiguousPendingCommands > MaxFutureCommands || !Finite(authority.position) ||
        !ValidMovementCommand({1, 0, 0, authority.yaw, authority.pitch})) return;
    const bool newPlayer = !observation_.active || current_.playerId != authority.playerId;
    if (!newPlayer && (authorityTick <= observation_.authorityTick ||
        authority.movementEpoch < current_.movementEpoch ||
        (authority.movementEpoch == current_.movementEpoch &&
         authority.lastResolvedCommand < observation_.lastResolvedCommand))) return;
    if (newPlayer || authority.movementEpoch > current_.movementEpoch) {
        // New epochs have an independent sequence namespace. No old input,
        // fractional simulation time or correction may leak into this seed.
        Reset();
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
    observation_.authorityTick = authorityTick;
    observation_.lastResolvedCommand = authority.lastResolvedCommand;
    observation_.movementEpoch = authority.movementEpoch;
    observation_.serverPendingCommands = authority.contiguousPendingCommands;
    UpdatePresentation();
}

bool LocalPlayerPrediction::Advance(double frameSeconds, float forward, float right,
                                    float yaw, float pitch) {
    if (!observation_.active) return false;
    if (!ValidMovementCommand({1, forward, right, yaw, pitch}) ||
        !std::isfinite(frameSeconds) || frameSeconds < 0)
        throw std::invalid_argument("Invalid local movement input");
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
    std::uint64_t blockedSteps{};
    const auto advance = ticks_.Advance(elapsed, [&](const Engine::Runtime::TickContext&) {
        previous_ = current_;
        if (pending_.size() < MaxPendingCommands &&
            current_.lastResolvedCommand < (std::numeric_limits<std::uint64_t>::max)()) {
            MovementCommand command{current_.lastResolvedCommand + 1, forward, right, yaw, pitch};
            current_ = StepMovement(arena_, current_, command);
            pending_.push_back(command);
            TraceMovement({.kind = MovementTraceKind::Generated, .playerId = current_.playerId,
                .epoch = current_.movementEpoch, .sequence = command.sequence});
            send = true;
        } else ++blockedSteps;
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
            .frameSeconds = frameSeconds, .count = blockedSteps});
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
    return send && !pending_.empty() && !neutralOnlyBootstrap;
}

void LocalPlayerPrediction::UpdatePresentation() {
    const auto base = Interpolate(previous_.position, current_.position, alpha_);
    const Float3 target{base.x + correction_.x, base.y + correction_.y, base.z + correction_.z};
    const auto position = current_.position;
    // Sweep from the valid predicted body to the proposed display body. An
    // offset cannot carry the camera through a wall, including around corners.
    const auto render = MoveCharacterBody(
        {{position.x, position.y, position.z}, arena_.bodyHeight, arena_.radius},
        Difference(target, position), arena_.walls, {}, true);
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
    return {current_.playerId, {pending_.begin(), pending_.end()}, current_.movementEpoch};
}

const LocalMovementObservation& LocalPlayerPrediction::Observation() const noexcept {
    return observation_;
}

} // namespace fps::pvp
