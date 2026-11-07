#pragma once

#include "gyo/voxel_map_editor/Editing.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/math/linear/Vec3.hpp"

// The free camera (ve-free-camera, docs/tools/plans/v8/voxel-editor-free-camera.md): fly with the right mouse
// button held, W/A/S/D along the ground, E/Q up and down, the wheel for speed. Engine's camera convention
// (CameraPose): rotation {pitch, yaw, 0}, yaw turning +Z toward +X, a positive pitch looking down.
namespace Gyo::Tools::VoxelMapEditor {

inline constexpr float FlyMinSpeedCells = 1.0F;    // cells a second
inline constexpr float FlyMaxSpeedCells = 256.0F;
inline constexpr float FlyDefaultSpeedCells = 8.0F;
inline constexpr float FlySpeedStep = 1.25F;       // per wheel notch
inline constexpr float FlyFastFactor = 4.0F;       // with Shift
inline constexpr float FlyPitchLimit = 1.5F;       // radians, as the orbit camera
inline constexpr float FlyNearCells = 0.5F;        // the near plane: a wall against the camera is cut away
inline constexpr float FlyFarCells = 8192.0F;
inline constexpr float FlyOrbitFallbackCells = 16.0F; // free to orbit with nothing in sight

struct FlyCamera final {
    Engine::Math::Vec3 position{};
    float yawRadians{};
    float pitchRadians{};
    float speedCells{FlyDefaultSpeedCells};
};

struct FlyInput final {
    bool forward{}, back{}, left{}, right{}, up{}, down{}, fast{};
};

// Where the camera looks (unit length).
[[nodiscard]] Engine::Math::Vec3 FlyForward(const FlyCamera& camera) noexcept;
// Moves for seconds: forward and back along the look direction on the ground (x, z), left and right across it,
// up and down along y only.
void FlyMove(FlyCamera& camera, const FlyInput& input, float seconds, float cellSize) noexcept;
// Turns by the mouse; the pitch stays within +-FlyPitchLimit.
void FlyLook(FlyCamera& camera, float yawDelta, float pitchDelta) noexcept;
// The wheel: FlySpeedStep per notch, within FlyMinSpeedCells..FlyMaxSpeedCells.
void FlyScrollSpeed(FlyCamera& camera, float notches) noexcept;
[[nodiscard]] CameraPose FlyPose(const FlyCamera& camera, float verticalFieldOfViewRadians, float cellSize) noexcept;

// Orbit to free: the same position and look.
[[nodiscard]] FlyCamera FlyFromPose(const CameraPose& pose, float speedCells) noexcept;

// Free to orbit: the same position and look; the target on the look ray, as far as the first filled cell's
// centre (within the pick reach), or FlyOrbitFallbackCells ahead.
struct OrbitPlacement final {
    Engine::Math::Vec3 target{};
    float distance{};
};
[[nodiscard]] OrbitPlacement OrbitFromFly(const FlyCamera& camera, const GridDocument& document) noexcept;

// Which use the keys W, A, S, D, Q and E have now: the free camera's while the right button is held in it,
// the editor's (tools, orientation) otherwise.
enum class KeyUse { Editing, Camera };
[[nodiscard]] KeyUse KeysFor(bool freeCamera, bool rightButtonHeld) noexcept;

} // namespace Gyo::Tools::VoxelMapEditor
