#pragma once

#include "RetroFPS/Pvp/Arena.hpp"

#include <cstddef>
#include <cstdint>
#include <numbers>
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

// One immutable fixed-duration movement step; angles are absolute radians.
struct MovementCommand final {
    std::uint64_t sequence{};
    float moveForward{};
    float moveRight{};
    float yaw{};
    float pitch{};
    bool operator==(const MovementCommand&) const = default;
};

struct PlayerInput final {
    PlayerId playerId{};
    std::vector<MovementCommand> commands;
    std::uint64_t movementEpoch{1};
};

struct PlayerState final {
    PlayerId playerId{};
    Float3 position{}; // feet, in arena world units
    float yaw{};
    float pitch{};
    std::uint64_t lastResolvedCommand{};
    std::uint64_t movementEpoch{1};
    std::uint32_t contiguousPendingCommands{};
};

[[nodiscard]] bool ValidMovementCommand(const MovementCommand& command) noexcept;
// Shared product policy used by prediction, replay and authority. No clock or I/O.
[[nodiscard]] PlayerState StepMovement(
    const Arena& arena, const PlayerState& state, const MovementCommand& command);

} // namespace fps::pvp
