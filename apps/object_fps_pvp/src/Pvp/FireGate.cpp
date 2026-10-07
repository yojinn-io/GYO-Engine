#include "RetroFPS/Pvp/FireGate.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace fps::pvp {

std::optional<std::uint64_t> EarliestShotResolveTick(const FireGateTiming& timing) noexcept {
    if (!timing.phaseDecided || timing.latestCommand < timing.lastResolvedCommand ||
        !std::isfinite(timing.secondsSinceStep) || !std::isfinite(timing.phaseShiftSeconds)) return std::nullopt;
    const auto ahead = timing.latestCommand - timing.lastResolvedCommand;
    if (ahead > (std::numeric_limits<std::uint64_t>::max)() - timing.authorityTick) return std::nullopt;
    // The tick that resolves the newest command executes the target after the
    // shot would arrive if sent at the step boundary, plus the phase error.
    const double margin = timing.secondsSinceStep - MovementPhaseTargetSeconds - FireGateGuardSeconds -
        Engine::Math::Max(0.0, timing.phaseShiftSeconds);
    const auto steps = static_cast<std::int64_t>(std::ceil(margin / MovementTickSeconds));
    const auto newest = static_cast<std::int64_t>(timing.authorityTick + ahead);
    const auto earliest = newest - static_cast<std::int64_t>(InitialCommandLead) + steps;
    // The authority has already resolved the snapshot's tick.
    const auto floor = static_cast<std::int64_t>(timing.authorityTick) + 1;
    return static_cast<std::uint64_t>(Engine::Math::Max(earliest, floor));
}

void LocalFireGate::Reset() noexcept { *this = {}; }

void LocalFireGate::ClearPending() noexcept {
    pendingNextAllowed_ = 0;
    pendingThrough_ = 0;
    pendingUnbounded_ = false;
}

void LocalFireGate::ObserveSnapshot(const std::uint64_t snapshotTick, const CombatState& combat) noexcept {
    snapshotTick_ = snapshotTick;
    snapshotNextAllowed_ = combat.nextAllowedShotTick;
    // The snapshot's cooldown already includes the pending shot.
    if (pendingThrough_ != 0 && combat.lastShotActionId >= pendingThrough_) ClearPending();
}

void LocalFireGate::ObserveDecision(const ShotDecision& decision, const std::uint64_t cooldownTicks) noexcept {
    if (decision.kind != ActionKind::Shot) return;
    if (decision.accepted)
        decidedNextAllowed_ = Engine::Math::Max(decidedNextAllowed_, decision.resolvedTick + cooldownTicks);
    if (pendingThrough_ != 0 && decision.actionId >= pendingThrough_) ClearPending();
}

std::uint64_t LocalFireGate::RemainingTicks(const std::optional<FireGateTiming>& timing) const noexcept {
    const auto required = Engine::Math::Max(Engine::Math::Max(snapshotNextAllowed_, decidedNextAllowed_),
                                            pendingNextAllowed_);
    if (pendingUnbounded_) return Engine::Math::Max<std::uint64_t>(required > snapshotTick_ ? required - snapshotTick_ : 0, 1);
    const auto earliest = timing ? EarliestShotResolveTick(*timing) : std::nullopt;
    const auto reached = earliest ? *earliest : snapshotTick_;
    return required > reached ? required - reached : 0;
}

void LocalFireGate::Submitted(const ActionId id, const std::optional<FireGateTiming>& timing,
                              const std::uint64_t cooldownTicks) noexcept {
    pendingThrough_ = Engine::Math::Max(pendingThrough_, id);
    const auto earliest = timing ? EarliestShotResolveTick(*timing) : std::nullopt;
    if (!earliest) {
        pendingUnbounded_ = true;
        return;
    }
    pendingNextAllowed_ = Engine::Math::Max(pendingNextAllowed_,
        *earliest + FireGatePendingSpreadTicks + cooldownTicks);
}

} // namespace fps::pvp
