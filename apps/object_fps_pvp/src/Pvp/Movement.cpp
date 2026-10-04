#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <stdexcept>

namespace fps::pvp {

bool ValidMovementCommand(const MovementCommand& command) noexcept {
    return command.sequence != 0 &&
        std::isfinite(command.moveForward) && std::abs(command.moveForward) <= 1 &&
        std::isfinite(command.moveRight) && std::abs(command.moveRight) <= 1 &&
        std::isfinite(command.yaw) && std::abs(command.yaw) <= 1.0e6F &&
        std::isfinite(command.pitch) && std::abs(command.pitch) <= Engine::Math::HalfPi;
}

PlayerState StepMovement(const Arena& arena, const PlayerState& state, const MovementCommand& command) {
    if (!ValidMovementCommand(command)) throw std::invalid_argument("Invalid movement command");
    auto result = state;
    const bool alive = state.lifeState == LifeState::Alive;
    if (alive) {
        result.yaw = Engine::Math::WrapRadians(command.yaw);
        result.pitch = Engine::Math::Clamp(command.pitch, -MovementMaximumPitch, MovementMaximumPitch);
    }
    const auto displacement = ComputePlanarDisplacement(alive ? command.moveForward : 0,
        alive ? command.moveRight : 0,
        result.yaw, arena.movementSpeed, static_cast<float>(MovementTickSeconds));
    const auto p = state.position;
    const auto supported = [&](const Engine::Math::Vec3& feet) {
        if (feet.y <= 0.0001F) return true;
        const Engine::Collision::VerticalCapsule body{feet, arena.bodyHeight, arena.radius};
        for (const auto& wall : arena.walls) {
            const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstAabb(
                body, {0, -0.002F, 0}, wall);
            if (contact && contact->normal.y > 0.5F) return true;
        }
        return false;
    };
    if (result.grounded) result.grounded = supported(p);
    if (alive && command.jumpRequested && result.grounded) {
        result.verticalVelocity = std::sqrt(2 * arena.gravity * arena.jumpHeight);
        result.grounded = false;
    }
    float desiredY = p.y;
    if (!result.grounded) {
        // Analytic constant acceleration preserves the configured jump height.
        desiredY = static_cast<float>(p.y + result.verticalVelocity * MovementTickSeconds -
            0.5 * arena.gravity * MovementTickSeconds * MovementTickSeconds);
        result.verticalVelocity -= static_cast<float>(arena.gravity * MovementTickSeconds);
        if (desiredY <= 0) {
            desiredY = 0;
            result.verticalVelocity = 0;
        }
    }
    result.position = MoveCharacterBody({p, arena.bodyHeight, arena.radius},
        {displacement.x, desiredY - p.y, displacement.z}, arena.walls, {}, false);
    if (std::abs(result.position.y - desiredY) > 0.0001F) result.verticalVelocity = 0;
    result.grounded = result.verticalVelocity <= 0 && supported(result.position);
    if (result.grounded) result.verticalVelocity = 0;
    result.lastResolvedCommand = command.sequence;
    return result;
}

} // namespace fps::pvp
