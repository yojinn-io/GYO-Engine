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
// that step is older than the target plus the guard; a correction still
// slewing that delays the phase is added to the guard.
//
// The guard covers how much earlier than the tracked phase a shot can arrive:
// the decided phase error, bounded by the tracking deadband, plus the command's
// own delay at the Client (it leaves when the simulation role wakes at its step
// boundary and the worker sends it) and what a shot gains on the command's
// path (up to about 0.1 ms on the Client-to-relay leg; none after the Gateway,
// which writes inputs before actions). By design these add up to about 3.1 ms
// when the decided error sits at the deadband edge, a residual; the v7 batch
// 08b clean runs needed about 1 ms of the 2 ms. The same-path, lead-of-two
// premise has exceptions that no constant at or below the v6 values covers:
// while the Match-side lead exceeds two steps plus the target and the guard,
// the earliest tick can be one or two ticks late (a long Match stall can
// exceed two ticks) and a click at the cooldown edge can be rejected by the
// authority. Known triggers: a late correction slewing after the delay that
// caused it has passed, the residual from its withdrawal on (about 3.3 ms
// until the next window decision, about 4 s), a drop in one-way delay after
// the phase was decided (until the next window decision), a lead jump (D44),
// and host or Match stalls. A pinned input send is handled by the Client's
// token bucket (v7 batch 09a). Frozen at these values in v7 batch 09
// (docs/object_fps_pvp/plans/v7/09-firegate-c2.md); v7.1 revisits both
// constants (candidates A1, B1 and B6 in
// docs/object_fps_pvp/plans/v7.1/README.md).
inline constexpr double FireGateGuardSeconds = MovementPhaseDeadbandSeconds;
// The latest resolve tick of a submitted shot, in ticks after its earliest:
// ceil((guard + lateness + hold) / step), where lateness is how much later
// than the tracked phase the shot arrives (taken as at most 5 ms; not
// measured). The Client and the Gateway link each hold a new action up to one
// send interval (1 / ActionSendRate) on their own pacing, plus the link
// writer's and the Client timer's late wakes; 7 holds while those wakes stay
// within about 43 ms, a bound rather than a margin:
// ceil((2 + 5 + 2 * 1000 / 30 + 43) / (1000 / 60)) = 7.
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
