#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlayerController.hpp"

#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"
#include "RetroFPS/Gameplay/Player/Player.hpp"
#include "RetroFPS/World/GridMap.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <numbers>
#include <utility>

namespace fps {
namespace {

constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;

} // namespace

bool PlayerController::Configure(
    PlayerSettings settings, std::string& error) {
    if (!ValidatePlayerSettings(settings, error)) {
        return false;
    }

    settings_ = std::move(settings);
    return true;
}

bool PlayerController::Initialize(
    Player& player,
    const GridMap& map,
    const WorldSettings& worldSettings,
    std::string& error) const {
    error.clear();

    std::string settingsError;
    if (!ValidatePlayerSettings(settings_, settingsError)) {
        error = "Invalid player settings: ";
        error += settingsError;
        return false;
    }

    try {
        const Float2 spawnPosition = map.GetSpawnPosition(worldSettings.cellSize);
        const auto walls = BuildWorldCollisionBoxes(map, worldSettings);
        if (!CanPlaceCharacterBody(
                {{spawnPosition.x, 0, spawnPosition.z}, settings_.bodyHeight, settings_.collisionRadius},
                walls, {})) {
            error = "Player spawn overlaps a solid map cell.";
            return false;
        }

        player.Reset(spawnPosition, 0.0f, 0.0f);
        return true;
    } catch (const std::exception& exception) {
        error = "Failed to initialize the player: ";
        error += exception.what();
        return false;
    } catch (...) {
        error = "Failed to initialize the player because of an unknown error.";
        return false;
    }
}

void PlayerController::Update(
    Player& player,
    const PlayerControlInput& input,
    const float deltaSeconds,
    const GridMap& map,
    const WorldSettings& worldSettings,
    const std::span<const Engine::Collision::VerticalCapsule> dynamicBlockers) const {
    float yawRadians = player.GetYawRadians();
    float pitchRadians = player.GetAimPitchRadians();
    const float recoilDegrees = player.GetRecoilDegrees();

    if (input.lookEnabled) {
        if (std::isfinite(input.lookDeltaX)) {
            yawRadians += input.lookDeltaX * settings_.mouseSensitivity;
            yawRadians = std::remainder(
                yawRadians, 2.0f * std::numbers::pi_v<float>);
        }
        if (std::isfinite(input.lookDeltaY)) {
            const float maxPitchRadians =
                settings_.maxPitchDegrees * kDegreesToRadians;
            pitchRadians = std::clamp(
                pitchRadians + input.lookDeltaY * settings_.mouseSensitivity,
                -maxPitchRadians,
                maxPitchRadians);
        }
        player.SetLookAngles(yawRadians, pitchRadians);
        static_cast<void>(SetVerticalRecoilDegrees(player, recoilDegrees));
    }

    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
        return;
    }

    const auto start = player.GetPositionXZ();
    const auto walls = BuildWorldCollisionBoxes(map, worldSettings);
    const auto isSupported = [&](const Engine::Collision::VerticalCapsule& body) {
        if (body.feet.y <= 0.0001F) {
            return true;
        }
        constexpr Engine::Collision::Float3 probe{0.0F, -0.002F, 0.0F};
        for (const auto& wall : walls) {
            const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstAabb(body, probe, wall);
            if (contact && contact->normal.y > 0.5F) {
                return true;
            }
        }
        for (const auto& actor : dynamicBlockers) {
            const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstCapsule(body, probe, actor);
            if (contact && contact->normal.y > 0.5F) {
                return true;
            }
        }
        return false;
    };
    const float previousFeetY = player.feetY_;
    if (player.grounded_) {
        player.grounded_ = isSupported({{start.x, previousFeetY, start.z},
            settings_.bodyHeight, settings_.collisionRadius});
    }
    // Preserve the jump policy while sweeping the complete body in 3D.
    if (input.jumpPressed && player.grounded_) {
        player.verticalVelocity_ = std::sqrt(2.0f * settings_.gravity * settings_.jumpHeight);
        player.grounded_ = false;
    }
    if (!player.grounded_) {
        // Integrate constant acceleration analytically so jump height and flight
        // time do not depend on render/update cadence.
        const double elapsed = deltaSeconds;
        const double feet = player.feetY_ + player.verticalVelocity_ * elapsed -
                            0.5 * settings_.gravity * elapsed * elapsed;
        const double velocity = player.verticalVelocity_ - settings_.gravity * elapsed;
        if (feet <= 0.0 && deltaSeconds > 0.0f) {
            player.feetY_ = 0.0f;
            player.verticalVelocity_ = 0.0f;
            player.grounded_ = true;
        } else {
            player.feetY_ = static_cast<float>(feet);
            player.verticalVelocity_ = static_cast<float>(velocity);
        }
    }

    const Float2 direction =
        ComputePlanarInput(input.moveForward, input.moveRight, yawRadians, pitchRadians);
    const Float2 displacement{
        direction.x * settings_.movementSpeed * deltaSeconds,
        direction.z * settings_.movementSpeed * deltaSeconds,
    };

    const float desiredY=player.feetY_;
    const auto moved=MoveCharacterBody({{start.x,previousFeetY,start.z},settings_.bodyHeight,settings_.collisionRadius},
        {displacement.x,desiredY-previousFeetY,displacement.z},walls,dynamicBlockers);
    player.SetPositionXZ({moved.x,moved.z});
    player.feetY_=moved.y;
    if (std::abs(moved.y-desiredY)>0.0001F) {
        player.verticalVelocity_=0;
    }
    // A body can lose its support by walking off another actor. A ceiling
    // contact also stops vertical motion, but cannot establish ground contact.
    player.grounded_ = player.verticalVelocity_ <= 0.0F &&
        isSupported({{moved.x, moved.y, moved.z}, settings_.bodyHeight, settings_.collisionRadius});
    if (player.grounded_) {
        player.verticalVelocity_ = 0.0F;
    }

}

bool PlayerController::SetVerticalRecoilDegrees(
    Player& player, const float recoilDegrees) const noexcept {
    if (!std::isfinite(recoilDegrees) || recoilDegrees < 0.0f) {
        return false;
    }

    const float maximumPitchRadians = settings_.maxPitchDegrees * kDegreesToRadians;
    const float requestedRecoilRadians = -recoilDegrees * kDegreesToRadians;
    const float aimPitchRadians = player.GetAimPitchRadians();
    const float effectivePitchRadians = std::clamp(
        aimPitchRadians + requestedRecoilRadians,
        -maximumPitchRadians,
        maximumPitchRadians);
    player.SetRecoilPitchRadians(effectivePitchRadians - aimPitchRadians);
    return true;
}

void PlayerController::ClearVerticalRecoil(Player& player) const noexcept {
    player.SetRecoilPitchRadians(0.0f);
}

} // namespace fps
