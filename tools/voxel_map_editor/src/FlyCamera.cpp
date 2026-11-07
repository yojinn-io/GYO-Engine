#include "gyo/voxel_map_editor/FlyCamera.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <cmath>

namespace Gyo::Tools::VoxelMapEditor {

namespace Math = Engine::Math;

Math::Vec3 FlyForward(const FlyCamera& camera) noexcept {
    // Engine's rotation: Y (yaw) after X (pitch) applied to +Z; MakeRotationX takes +Z to (0, -sin, cos).
    const float cosPitch = std::cos(camera.pitchRadians);
    return {std::sin(camera.yawRadians) * cosPitch, -std::sin(camera.pitchRadians),
            std::cos(camera.yawRadians) * cosPitch};
}

void FlyMove(FlyCamera& camera, const FlyInput& input, const float seconds, const float cellSize) noexcept {
    const Math::Vec3 ahead{std::sin(camera.yawRadians), 0.0F, std::cos(camera.yawRadians)};
    const Math::Vec3 side{std::cos(camera.yawRadians), 0.0F, -std::sin(camera.yawRadians)};
    Math::Vec3 step{};
    if (input.forward) step = step + ahead;
    if (input.back) step = step - ahead;
    if (input.right) step = step + side;
    if (input.left) step = step - side;
    if (input.up) step.y += 1.0F;
    if (input.down) step.y -= 1.0F;
    const float speed = camera.speedCells * (input.fast ? FlyFastFactor : 1.0F) * cellSize;
    camera.position = camera.position + Math::NormalizeOrZero(step) * (speed * seconds);
}

void FlyLook(FlyCamera& camera, const float yawDelta, const float pitchDelta) noexcept {
    camera.yawRadians += yawDelta;
    camera.pitchRadians = Math::Clamp(camera.pitchRadians + pitchDelta, -FlyPitchLimit, FlyPitchLimit);
}

void FlyScrollSpeed(FlyCamera& camera, const float notches) noexcept {
    camera.speedCells =
        Math::Clamp(camera.speedCells * std::pow(FlySpeedStep, notches), FlyMinSpeedCells, FlyMaxSpeedCells);
}

CameraPose FlyPose(const FlyCamera& camera, const float verticalFieldOfViewRadians, const float cellSize) noexcept {
    return {camera.position, {camera.pitchRadians, camera.yawRadians, 0.0F}, verticalFieldOfViewRadians,
            FlyNearCells * cellSize, FlyFarCells * cellSize};
}

FlyCamera FlyFromPose(const CameraPose& pose, const float speedCells) noexcept {
    return {pose.position, pose.rotationRadians.y, pose.rotationRadians.x, speedCells};
}

OrbitPlacement OrbitFromFly(const FlyCamera& camera, const GridDocument& document) noexcept {
    const Math::Vec3 forward = FlyForward(camera);
    const auto hit = Pick(document, Ray{camera.position, forward}, EditPlane{}, PickOptions{true, false});
    float distance = FlyOrbitFallbackCells * document.cellSize;
    if (hit) {
        const Math::Vec3 centre = CellCorner(document, hit->cell) + Math::Vec3{0.5F, 0.5F, 0.5F} * document.cellSize;
        distance = Math::Length(centre - camera.position);
    }
    // On the look ray, so the orbit camera (target - forward x distance) stands where the free one did.
    return {camera.position + forward * distance, distance};
}

KeyUse KeysFor(const bool freeCamera, const bool rightButtonHeld) noexcept {
    return freeCamera && rightButtonHeld ? KeyUse::Camera : KeyUse::Editing;
}

} // namespace Gyo::Tools::VoxelMapEditor
