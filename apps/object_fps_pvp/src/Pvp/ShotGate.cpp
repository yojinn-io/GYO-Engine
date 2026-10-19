#include "RetroFPS/Pvp/ShotGate.hpp"

#include "engine/math/scalar/Scalar.hpp"

namespace fps::pvp {

void LocalShotGate::Reset() noexcept { *this = {}; }

void LocalShotGate::ObserveSnapshot(const std::uint64_t snapshotTick, const CombatState& combat) noexcept {
    snapshotTick_ = snapshotTick;
    snapshotNextAllowed_ = combat.nextAllowedShotTick;
    // The snapshot's cooldown already includes the pending shots.
    if (pendingThrough_ != 0 && combat.lastShotActionId >= pendingThrough_) {
        pendingNextAllowed_ = 0;
        pendingThrough_ = 0;
    }
}

void LocalShotGate::ObserveDecision(const ShotDecision& decision, const std::uint64_t cooldownTicks) noexcept {
    if (decision.kind != ActionKind::Shot) return;
    if (decision.accepted)
        decidedNextAllowed_ = Engine::Math::Max(decidedNextAllowed_, decision.resolvedTick + cooldownTicks);
    if (pendingThrough_ != 0 && decision.actionId >= pendingThrough_) {
        pendingNextAllowed_ = 0;
        pendingThrough_ = 0;
    }
}

std::uint64_t LocalShotGate::Required() const noexcept {
    return Engine::Math::Max(Engine::Math::Max(snapshotNextAllowed_, decidedNextAllowed_), pendingNextAllowed_);
}

std::uint64_t LocalShotGate::RemainingTicks(const std::optional<ShotGateView>& view) const noexcept {
    const auto required = Required();
    if (!view) return Engine::Math::Max<std::uint64_t>(required > snapshotTick_ ? required - snapshotTick_ : 0, 1);
    const auto estimate = EstimatedShotTick(*view);
    return required > estimate ? required - estimate : 0;
}

void LocalShotGate::Submitted(const ActionId id, const ShotGateView& view, const std::uint64_t cooldownTicks) noexcept {
    pendingThrough_ = Engine::Math::Max(pendingThrough_, id);
    pendingNextAllowed_ = Engine::Math::Max(pendingNextAllowed_, EstimatedShotTick(view) + cooldownTicks);
}

} // namespace fps::pvp
