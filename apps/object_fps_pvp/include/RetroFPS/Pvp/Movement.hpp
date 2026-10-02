#pragma once

#include "RetroFPS/Pvp/Arena.hpp"

#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

namespace fps::pvp {

using PlayerId = std::uint64_t;

inline constexpr std::uint32_t AuthorityTickRate = 60;
inline constexpr std::uint32_t SnapshotIntervalTicks = 1;
inline constexpr std::uint32_t InputSendRate = 60;
inline constexpr double MovementTickSeconds = 1.0 / AuthorityTickRate;
inline constexpr float MovementMaximumPitch = 89.0F * std::numbers::pi_v<float> / 180.0F;
inline constexpr double MovementCorrectionSeconds = 0.1;
inline constexpr float MovementHardCorrectionDistance = 1.0F;
inline constexpr double MovementMaximumRegularFrameSeconds = 3 * MovementTickSeconds;
inline constexpr std::size_t MaxPendingCommands = 12;
inline constexpr std::size_t MaxFutureCommands = 32;
inline constexpr std::size_t InitialCommandLead = 2;
inline constexpr std::uint32_t InputHoldTicks = 15;
inline constexpr std::size_t MovementBacklogSampleTicks = 30;
inline constexpr std::uint32_t MovementBacklogCommandSum = 105;
inline constexpr std::uint64_t MovementResetCooldownTicks = 60;
// Start phase: the Host reports how long an epoch's first window waited for
// the tick that executed sequence 1. The Client shifts its fixed-step phase
// once so that wait equals this target; lead, interpolation and thresholds stay.
// Waits beyond one tick plus the bound below come from a late Host and are ignored.
inline constexpr double MovementStartPhaseTargetSeconds = 0.004;
inline constexpr double MovementStartPhaseMaximumWaitSeconds = MovementTickSeconds + 0.002;
// Below about 54.5 FPS (a mean frame period above 1.1 tick) the shift is not
// applied: a frame longer than a tick generates several commands, so the shift
// lands on whole frames and the older command of each frame depends on the
// send phase, which drifts against the frame clock. The frame rate is the mean
// of the latest 32 frame intervals (eight at least), each counted as at most
// two ticks (one missed 60 Hz refresh); above this cut the shift is withdrawn,
// within four frames of a drop from 60 to 30 FPS and 16-20 frames of a drop to
// a vsync-paced 50 FPS. The 10 % allowance keeps 59.94 Hz, ordinary jitter and
// ~58 FPS desktop frames aligned. A 60 Hz display that drops one refresh in
// 12-30 frames (55-58 FPS) stays within it once the window holds more than ten
// intervals.
inline constexpr double MovementStartPhaseMaximumFrameSeconds = MovementTickSeconds * 1.1;
// A withdrawn shift returns only after the mean has stayed at or below this
// (about 56.6 FPS, 1.06 tick) for a 32-frame dwell, so a frame rate near the
// cut keeps its state instead of toggling. It must stay above the ~1.03-1.04
// tick mean of ~58 FPS desktop frames.
inline constexpr double MovementStartPhaseRestoreFrameSeconds =
    MovementStartPhaseMaximumFrameSeconds - MovementTickSeconds * 0.04;
inline constexpr std::uint32_t MaxEpochStartWaitMicros = 1'000'000;

enum class LifeState { Alive = 0, Dead = 1 };

struct MovementRules final {
    float jumpHeight{};
    float gravity{};
};

// One immutable fixed-duration movement step; angles are absolute radians.
struct MovementCommand final {
    std::uint64_t sequence{};
    float moveForward{};
    float moveRight{};
    float yaw{};
    float pitch{};
    bool jumpRequested{};
    bool operator==(const MovementCommand&) const = default;
};

struct PlayerInput final {
    PlayerId playerId{};
    std::vector<MovementCommand> commands;
    std::uint64_t movementEpoch{1};
    std::uint64_t lifeGeneration{1};
};

struct PlayerState final {
    PlayerId playerId{};
    Float3 position{}; // feet, in arena world units
    float yaw{};
    float pitch{};
    std::uint64_t lastResolvedCommand{};
    std::uint64_t movementEpoch{1};
    std::uint32_t contiguousPendingCommands{};
    float verticalVelocity{};
    bool grounded{true};
    std::uint64_t lifeGeneration{1};
    LifeState lifeState{LifeState::Alive};
    std::uint64_t lifeStateTick{};
    std::uint64_t respawnTick{};
    // Host timing observation for this epoch, never simulation input.
    std::optional<std::uint32_t> epochStartWaitMicros{};
};

[[nodiscard]] bool ValidMovementCommand(const MovementCommand& command) noexcept;
// Shared product policy used by prediction, replay and authority. No clock or I/O.
[[nodiscard]] PlayerState StepMovement(
    const Arena& arena, const PlayerState& state, const MovementCommand& command);

} // namespace fps::pvp
