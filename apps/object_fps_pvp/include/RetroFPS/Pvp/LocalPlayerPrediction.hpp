#pragma once

#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace fps::pvp {

// Phase tracking: Acquiring until the first decision after a seed (epoch
// start or stall reseed), Settling while a correction slews, Tracking otherwise.
enum class PhaseTrackingState { Acquiring, Settling, Tracking };

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
    PhaseTrackingState phaseTracking{PhaseTrackingState::Acquiring};
    // Latest decided error: the window percentile of slack beyond the lead and
    // target for a command sent at its boundary (positive: the phase may move later).
    std::optional<double> phaseErrorSeconds;
    // Latest correction started (positive delays the fixed-step phase).
    std::optional<double> phaseCorrectionSeconds;
    // Corrections in this epoch and life, and how many late samples forced.
    std::uint32_t phaseCorrections{};
    std::uint32_t phaseLateCorrections{};
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
    void TrackPhase(const PlayerState& authority);
    void Correct(double error, bool late);
    void UpdatePresentation();

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
    // Remaining phase correction, slewed into the fixed-step clock.
    double phaseShiftSeconds_{};
    // Positive frame intervals seen, up to the first decision's evidence. They
    // describe the display loop, so a seed keeps them; Reset() clears them.
    std::size_t frameEvidence_{};
    // Each real command's age when its window was published, by sequence.
    struct CommandAge final {
        std::uint64_t sequence{};
        double seconds{};
    };
    std::array<CommandAge, 64> commandAges_{};
    // Error samples of the current phase (slack beyond the lead and target).
    std::deque<double> phaseSamples_;
    // Samples count only for commands generated after this sequence: the seed
    // lead, or the tip when the latest correction finished slewing.
    std::uint64_t settledAfter_{};
    std::uint64_t lastSlackSequence_{};
    std::size_t lateSamples_{};
    bool settling_{};
    bool phaseDecided_{};
    LocalMovementObservation observation_{};
};

} // namespace fps::pvp
