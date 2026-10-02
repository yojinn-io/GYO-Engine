#pragma once

#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace fps::pvp {

// Why an epoch's start phase has no shift applied.
enum class StartPhaseSkip {
    // The Host was late for the start tick; its wait does not describe the phase.
    HostLate,
    // The frame-rate measure is above the cut (below about 54.5 FPS); the
    // armed shift is withdrawn and returns once frames recover.
    FrameRateBelowTick,
    // A stall reseed rebased the phase the Host timed, while the decision was
    // pending or after it; nothing returns in this epoch.
    CancelledByReseed,
};

// Read-only presentation diagnostics. These never drive simulation or input.
struct LocalMovementObservation final {
    Float3 predictedPosition{};
    Float3 renderPosition{};
    Float3 correctionOffset{};
    std::uint64_t lastResolvedCommand{};
    std::uint64_t latestCommand{};
    std::uint64_t authorityTick{};
    std::size_t pendingCommands{};
    bool active{};
    bool frozen{};
    std::uint64_t movementEpoch{};
    std::uint32_t serverPendingCommands{};
    std::uint64_t previousCommand{};
    std::uint64_t currentCommand{};
    float interpolationAlpha{};
    float verticalVelocity{};
    bool grounded{};
    std::uint64_t lifeGeneration{};
    LifeState lifeState{LifeState::Alive};
    // Start-phase alignment for this epoch: the Host-reported wait and the
    // shift it produced (positive delays the fixed-step phase). The wait is
    // taken once eight frame intervals describe the frame rate (a late Host's
    // at once); both stay unset until then.
    std::optional<double> epochStartWaitSeconds;
    std::optional<double> startPhaseShiftSeconds;
    // Why the epoch's start phase has no shift applied; unset while shifted,
    // while the decision is pending and when nothing was timed. While the
    // frame-rate measure is above MovementStartPhaseMaximumFrameSeconds (below
    // about 54.5 FPS, 1.1 tick) the shift is withdrawn (FrameRateBelowTick),
    // also when the epoch is armed there; it is applied again once the measure
    // has stayed at or below MovementStartPhaseRestoreFrameSeconds (about
    // 56.6 FPS, 1.06 tick) for a 32-frame dwell. A stall reseed cancels a
    // pending or decided start phase (CancelledByReseed): the shift is cleared,
    // a wait already taken stays, and nothing is taken or restored afterwards.
    // A late Host's epoch armed nothing and keeps HostLate.
    std::optional<StartPhaseSkip> startPhaseSkip;
};

// Product-local movement prediction. The application samples mouse input once
// per frame and supplies absolute aim; commands represent exactly one 60 Hz step.
class LocalPlayerPrediction final {
public:
    explicit LocalPlayerPrediction(const Arena& arena);

    void Reset() noexcept;
    void SetMovementRules(MovementRules rules);
    void ClearJumpRequest() noexcept { pendingJump_ = false; }
    void Reconcile(const PlayerState& authority, std::uint64_t authorityTick);
    // True publishes a changed complete window. The network worker owns its
    // independent 60 Hz retransmission deadlines and never creates commands.
    [[nodiscard]] bool Advance(double frameSeconds, float forward, float right,
                               float yaw, float pitch, bool jumpRequested = false);
    [[nodiscard]] PlayerInput PendingInput() const;
    [[nodiscard]] const LocalMovementObservation& Observation() const noexcept;

private:
    void SeedLead(const PlayerState& authority);
    void UpdatePresentation();
    [[nodiscard]] std::optional<double> FramePeriodSeconds() const;

    // Steps one Advance may run; whole steps beyond them are dropped.
    static constexpr std::uint32_t CatchUpSteps = 5;

    Arena arena_;
    Engine::Runtime::FixedTickRuntime ticks_{AuthorityTickRate, CatchUpSteps};
    std::deque<MovementCommand> pending_;
    PlayerState current_{};
    PlayerState previous_{};
    Float3 correction_{};
    double correctionSeconds_{};
    float alpha_{};
    bool sendPending_{};
    bool freshSeed_{};
    bool pendingJump_{};
    // The current fixed-step phase began at an epoch-start seed and has not
    // since been rebased, so the Host's start wait still describes it.
    bool startPhasePending_{};
    // Age of the first real step when its window was published (frame quantization).
    std::optional<double> startWindowLagSeconds_;
    double phaseShiftSeconds_{};
    // The shift armed for this epoch's phase; it is withdrawn while frames run
    // below about 54.5 FPS (1.1 tick). A stall reseed disarms it.
    std::optional<double> armedPhaseShiftSeconds_;
    bool startPhaseWithdrawn_{};
    // Frames in a row a withdrawn shift has seen frame rates fit to restore it.
    std::size_t startPhaseRecoveredFrames_{};
    // Latest positive frame intervals, each limited to two ticks: the start
    // phase's frame-rate evidence. They describe the display loop, so a reseed
    // keeps them; Reset() clears them. The window is also the restore dwell.
    struct FrameIntervals final {
        std::array<double, 32> seconds{};
        std::size_t count{};
        std::size_t next{};
    } frameIntervals_{};
    LocalMovementObservation observation_{};
};

} // namespace fps::pvp
