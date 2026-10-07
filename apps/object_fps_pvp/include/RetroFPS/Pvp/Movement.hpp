#pragma once

#include "RetroFPS/Pvp/Arena.hpp"
#include "engine/math/scalar/Angle.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace fps::pvp {

using PlayerId = std::uint64_t;

inline constexpr std::uint32_t AuthorityTickRate = 60;
// Room capacity (pv6 contract §1, 2026-10-07 revision): the Match's join limit,
// Ready.max_players and the Client's snapshot bound. The Gateway, acceptance C++
// and acceptance Python keep their own definitions; test_max_players.py checks
// that all four agree.
inline constexpr std::uint32_t MaxPlayers = 4;
inline constexpr std::uint32_t SnapshotIntervalTicks = 1;
inline constexpr std::uint32_t InputSendRate = 60;
// Input sends draw from an InputSendRate token bucket of this many tokens. A
// window carrying a command never sent goes at the worker's next poll once a
// token exists; an unchanged window is resent at the 1/InputSendRate deadline
// only while one token stays in reserve for the next new command. The average
// stays at InputSendRate, bursts are two packets, nothing is repaid.
inline constexpr double InputSendBurst = 2;
inline constexpr double MovementTickSeconds = 1.0 / AuthorityTickRate;
inline constexpr float MovementMaximumPitch = Engine::Math::DegreesToRadians(89.0F);
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
// Phase tracking: the Host reports, with each snapshot, how long before its
// executing tick a recent command arrived (movement slack). For a command sent
// at its fixed-step boundary the Client keeps the slack beyond the sequence
// lead at this target; lead, interpolation and thresholds stay.
inline constexpr double MovementPhaseTargetSeconds = 0.004;
// The error of the latest commands is the 90th percentile of a window of
// slack samples: commands the worker sent at once, not the ones that also
// waited for a frame. The first decision after a seed uses the first samples
// and a narrow deadband; tracking then uses 240 samples (about four seconds at
// 60 FPS; about eight at 30 FPS, where only the latest snapshot of a frame is
// reconciled) and corrects only beyond the wider deadband. Two late (negative) samples in a
// row correct at once. Each correction is at most two ticks and is slewed.
inline constexpr std::size_t MovementPhaseFirstSamples = 8;
inline constexpr std::size_t MovementPhaseWindowSamples = 240;
inline constexpr double MovementPhasePercentile = 0.9;
inline constexpr double MovementPhaseFirstDeadbandSeconds = 0.0005;
inline constexpr double MovementPhaseDeadbandSeconds = 0.002;
inline constexpr std::size_t MovementPhaseLateSamples = 2;
inline constexpr double MovementPhaseMaximumCorrectionSeconds = 2 * MovementTickSeconds;
inline constexpr std::int32_t MaxMovementSlackMicros = 1'000'000;
// Connection quality: the Match judges each 10-second window after the first
// one following a join. A window fails when the median input reference age
// (the age of the snapshot a window says the Client applied, about RTT plus
// the Client's publication delay) is above 160 ms, more than 5 % of resolved
// movement steps were substituted (Held or Neutral), or a Starvation/Backlog
// movement reset happened. Three failed windows in a row evict the player.
// 160 ms is where the 12-command Client window starts to fill at 60 FPS; below
// 30 FPS a frame is longer than the two-command lead, and 25 FPS already loses
// 7 % of its steps; ordinary 60 FPS play stays below 1 %. 30 FPS is the design
// boundary: the oldest of three steps published in one frame keeps about 4 ms,
// and a host oversleeping about 8 ms loses 1.5-9 % of steps (v5 contract §1).
inline constexpr std::uint64_t ConnectionQualityWindowTicks = 10 * AuthorityTickRate;
inline constexpr std::uint32_t ConnectionQualityFailedWindows = 3;
inline constexpr std::uint32_t ConnectionQualityMaximumReferenceAgeMillis = 160;
inline constexpr std::uint32_t ConnectionQualityMaximumSubstitutedPermille = 50;

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
    // Latest snapshot tick the Client applied when publishing (0: none).
    // Connection-quality timing only, never simulation input.
    std::uint64_t observedAuthorityTick{};
};

struct PlayerState final {
    PlayerId playerId{};
    Engine::Math::Vec3 position{}; // feet, in arena world units
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
    // Host timing observation, never simulation input: the smallest movement
    // slack since the previous published snapshot and its sequence. Both or neither.
    std::optional<std::uint64_t> movementSlackSequence{};
    std::optional<std::int32_t> movementSlackMicros{};
    // Consecutive failed connection-quality windows; eviction at ConnectionQualityFailedWindows.
    std::uint32_t connectionQualityFailures{};
};

[[nodiscard]] bool ValidMovementCommand(const MovementCommand& command) noexcept;
// Shared product policy used by prediction, replay and authority. No clock or I/O.
[[nodiscard]] PlayerState StepMovement(
    const Arena& arena, const PlayerState& state, const MovementCommand& command);

} // namespace fps::pvp
