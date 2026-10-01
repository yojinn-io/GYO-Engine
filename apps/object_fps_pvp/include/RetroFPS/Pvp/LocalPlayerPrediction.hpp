#pragma once

#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace fps::pvp {

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
    // shift it produced (positive delays the fixed-step phase). Unset until applied.
    std::optional<double> epochStartWaitSeconds;
    std::optional<double> startPhaseShiftSeconds;
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

    Arena arena_;
    Engine::Runtime::FixedTickRuntime ticks_{AuthorityTickRate, 5};
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
    LocalMovementObservation observation_{};
};

} // namespace fps::pvp
