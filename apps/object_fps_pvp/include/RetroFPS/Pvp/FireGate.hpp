#pragma once

#include "RetroFPS/Pvp/Combat.hpp"
#include "RetroFPS/Pvp/Movement.hpp"

#include <cstdint>
#include <optional>

namespace fps::pvp {

// The Client's local shot gate. One predicate decides whether a click is
// submitted: the earliest authority tick that can resolve a shot sent now must
// reach every shot cooldown the Client knows of. The authority's 10-tick
// cooldown stays the only rule; this gate only avoids sending a shot that the
// authority would reject, and a blocked click is not queued.
//
// The earliest tick comes from phase tracking. The authority resolves one
// movement command per tick, so the newest command executes at
// authorityTick + (latestCommand - lastResolvedCommand). Tracking keeps a
// command sent at its step boundary arriving InitialCommandLead steps plus
// MovementPhaseTargetSeconds before it executes. An action sent now takes the
// same path in the same frame, so it reaches the Match no earlier than the tick
// InitialCommandLead steps before the newest command, and one step later once
// that step is older than the target plus the guard. The guard is the tracking
// deadband, the bound of the decided phase error; a correction still slewing
// that delays the phase is added to it.
inline constexpr double FireGateGuardSeconds = MovementPhaseDeadbandSeconds;
// The latest resolve tick of a submitted shot, in ticks after its earliest:
// the Client and the Gateway each hold a new action up to one send interval
// (1 / ActionSendRate), the Gateway writer wakes at least once a step, plus
// one step for the guard and one for the tick boundary:
// ceil(2 * 60 / 30 + 1) + 1 + 1 = 7.
inline constexpr std::uint64_t FireGatePendingSpreadTicks = 7;

struct FireGateTiming final {
    // The newest reconciled snapshot and the own command it resolved.
    std::uint64_t authorityTick{};
    std::uint64_t lastResolvedCommand{};
    // The newest predicted command and the time since its step boundary.
    std::uint64_t latestCommand{};
    double secondsSinceStep{};
    // The part of a phase correction not yet slewed (positive delays the phase).
    double phaseShiftSeconds{};
    // Phase tracking has decided since the latest seed.
    bool phaseDecided{};
};

// The earliest authority tick that can resolve a shot sent now, never before
// the tick after the snapshot; nullopt when phase tracking cannot bound it.
[[nodiscard]] std::optional<std::uint64_t> EarliestShotResolveTick(const FireGateTiming& timing) noexcept;

class LocalFireGate final {
public:
    void Reset() noexcept;
    // The own combat state of the newest snapshot of the current life.
    void ObserveSnapshot(std::uint64_t snapshotTick, const CombatState& combat) noexcept;
    // A decision of the current life.
    void ObserveDecision(const ShotDecision& decision, std::uint64_t cooldownTicks) noexcept;
    // Ticks to wait before a shot may be submitted now; zero allows it. Without
    // a bounded timing the snapshot tick stands in for the earliest tick.
    [[nodiscard]] std::uint64_t RemainingTicks(const std::optional<FireGateTiming>& timing) const noexcept;
    [[nodiscard]] bool Allows(const std::optional<FireGateTiming>& timing) const noexcept {
        return RemainingTicks(timing) == 0;
    }
    // Records a submitted shot until its decision or a snapshot that includes it.
    void Submitted(ActionId id, const std::optional<FireGateTiming>& timing, std::uint64_t cooldownTicks) noexcept;

private:
    void ClearPending() noexcept;

    std::uint64_t snapshotTick_{};
    std::uint64_t snapshotNextAllowed_{};
    std::uint64_t decidedNextAllowed_{};
    // The latest next-allowed tick a pending shot can cause.
    std::uint64_t pendingNextAllowed_{};
    ActionId pendingThrough_{};
    // A shot submitted without a bounded timing blocks until it is decided.
    bool pendingUnbounded_{};
};

} // namespace fps::pvp
