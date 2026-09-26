#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace fps::pvp {

bool ValidMovementCommand(const MovementCommand& command) noexcept {
    return command.sequence != 0 &&
        std::isfinite(command.moveForward) && std::abs(command.moveForward) <= 1 &&
        std::isfinite(command.moveRight) && std::abs(command.moveRight) <= 1 &&
        std::isfinite(command.yaw) && std::abs(command.yaw) <= 1.0e6F &&
        std::isfinite(command.pitch) && std::abs(command.pitch) <= std::numbers::pi_v<float> / 2;
}

PlayerState StepMovement(const Arena& arena, const PlayerState& state, const MovementCommand& command) {
    if (!ValidMovementCommand(command)) throw std::invalid_argument("Invalid movement command");
    auto result = state;
    result.yaw = std::remainder(command.yaw, 2 * std::numbers::pi_v<float>);
    result.pitch = std::clamp(command.pitch, -MovementMaximumPitch, MovementMaximumPitch);
    const auto displacement = ComputePlanarDisplacement(command.moveForward, command.moveRight,
        result.yaw, arena.movementSpeed, static_cast<float>(MovementTickSeconds));
    const auto p = state.position;
    result.position = MoveCharacterBody({{p.x, p.y, p.z}, arena.bodyHeight, arena.radius},
        {displacement.x, 0, displacement.z}, arena.walls, {}, true);
    result.lastResolvedCommand = command.sequence;
    return result;
}

} // namespace fps::pvp
