#pragma once

#include "RetroFPS/Pvp/Combat.hpp"

#include <cstdint>
#include <optional>

namespace fps::pvp {

// The Client's local shot gate (pv7 §6 Client rule; a1-shot-step retired the
// FireGate). The authority judges a shot at T(S), the tick that resolves the
// command S it claims, and resolves one command per tick, so within one epoch
// T(S) = snapshot tick + (S - the snapshot's last resolved command). A click
// is sent only when that estimate reaches every shot cooldown the Client knows
// of: the snapshot's next allowed tick, each accepted decision's tick plus the
// cooldown, and each shot still awaiting its decision plus the cooldown. The
// cooldown comes from the Match's rules, so a weapon's fire rate needs no
// constant here. A blocked click is dropped, never queued; without an active,
// unfrozen prediction nothing is sent.
struct ShotGateView final {
    // The newest reconciled snapshot: its tick and the own player's epoch and
    // last resolved command.
    std::uint64_t snapshotTick{};
    std::uint64_t snapshotEpoch{};
    std::uint64_t snapshotLastResolved{};
    // E and S of the click: the prediction's epoch and its newest command N.
    std::uint64_t epoch{};
    std::uint64_t sequence{};
    bool operator==(const ShotGateView&) const = default;
};

// T^(S). Another epoch than the snapshot's, or an S the snapshot already
// resolved, falls back to the snapshot tick: no later than the real decision.
[[nodiscard]] constexpr std::uint64_t EstimatedShotTick(const ShotGateView& view) noexcept {
    if (view.epoch != view.snapshotEpoch || view.sequence <= view.snapshotLastResolved) return view.snapshotTick;
    return view.snapshotTick + (view.sequence - view.snapshotLastResolved);
}

class LocalShotGate final {
public:
    void Reset() noexcept;
    // The own combat state of the newest snapshot of the current life.
    void ObserveSnapshot(std::uint64_t snapshotTick, const CombatState& combat) noexcept;
    // A decision of the current life.
    void ObserveDecision(const ShotDecision& decision, std::uint64_t cooldownTicks) noexcept;
    // Ticks to wait before a click may be sent; zero allows it. Without a view
    // (no active, unfrozen prediction) at least one tick remains.
    [[nodiscard]] std::uint64_t RemainingTicks(const std::optional<ShotGateView>& view) const noexcept;
    [[nodiscard]] bool Allows(const std::optional<ShotGateView>& view) const noexcept {
        return view && RemainingTicks(view) == 0;
    }
    // Records a sent shot until its decision or a snapshot that includes it.
    void Submitted(ActionId id, const ShotGateView& view, std::uint64_t cooldownTicks) noexcept;

private:
    [[nodiscard]] std::uint64_t Required() const noexcept;

    std::uint64_t snapshotTick_{};
    std::uint64_t snapshotNextAllowed_{};
    std::uint64_t decidedNextAllowed_{};
    // The latest next allowed tick a sent, undecided shot can cause.
    std::uint64_t pendingNextAllowed_{};
    ActionId pendingThrough_{};
};

} // namespace fps::pvp
