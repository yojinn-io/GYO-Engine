#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/FlyCamera.hpp"

#include "engine/math/linear/Matrix4.hpp"

#include <cmath>

using namespace Gyo::Tools::VoxelMapEditor;
namespace Math = Engine::Math;

// Judgments FC1 to FC3 of ve-free-camera (docs/tools/plans/v8/voxel-editor-free-camera.md).

namespace {

void CheckNear(const Math::Vec3 a, const Math::Vec3 b, const float tolerance = 1e-4F) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(tolerance));
    CHECK(a.y == doctest::Approx(b.y).epsilon(tolerance));
    CHECK(a.z == doctest::Approx(b.z).epsilon(tolerance));
}

} // namespace

TEST_CASE("ve-free-camera FC1: the look direction is Engine's camera +Z") {
    FlyCamera camera;
    camera.yawRadians = 0.6F;
    camera.pitchRadians = 0.7F;
    // Engine's camera: Y (yaw) after X (pitch) applied to +Z.
    const Math::Matrix4 rotation = Math::Multiply(Math::MakeRotationY(0.6F), Math::MakeRotationX(0.7F));
    CheckNear(FlyForward(camera), Math::TransformVector(rotation, {0.0F, 0.0F, 1.0F}));
}

TEST_CASE("ve-free-camera FC1: moving goes along the ground, across it, and up and down only on y") {
    FlyCamera camera;
    camera.yawRadians = 1.5707964F; // looking along +X
    camera.pitchRadians = 0.9F;     // and down: forward still stays on the ground
    camera.speedCells = 10.0F;
    FlyMove(camera, {.forward = true}, 0.5F, 2.0F); // 10 cells/s, 0.5 s, 2 m a cell: 10 m
    CheckNear(camera.position, {10.0F, 0.0F, 0.0F});
    FlyMove(camera, {.right = true}, 0.5F, 2.0F); // right of +X is -Z
    CheckNear(camera.position, {10.0F, 0.0F, -10.0F});
    FlyMove(camera, {.up = true}, 0.5F, 2.0F);
    CheckNear(camera.position, {10.0F, 10.0F, -10.0F});
    FlyMove(camera, {.back = true, .left = true, .down = true}, 0.0F, 2.0F); // no time, no move
    CheckNear(camera.position, {10.0F, 10.0F, -10.0F});
    // Shift: four times as fast; two keys at once do not go faster than one.
    FlyCamera fast;
    fast.speedCells = 1.0F;
    FlyMove(fast, {.forward = true, .right = true, .fast = true}, 1.0F, 1.0F);
    CHECK(Math::Length(fast.position) == doctest::Approx(FlyFastFactor));
    // Forward and back cancel.
    FlyCamera still;
    FlyMove(still, {.forward = true, .back = true}, 1.0F, 1.0F);
    CheckNear(still.position, {0.0F, 0.0F, 0.0F});
}

TEST_CASE("ve-free-camera FC1: the pitch stays within the limit, the speed within its range") {
    FlyCamera camera;
    FlyLook(camera, 0.25F, 10.0F);
    CHECK(camera.yawRadians == doctest::Approx(0.25F));
    CHECK(camera.pitchRadians == doctest::Approx(FlyPitchLimit));
    FlyLook(camera, 0.0F, -10.0F);
    CHECK(camera.pitchRadians == doctest::Approx(-FlyPitchLimit));
    CHECK(camera.speedCells == doctest::Approx(FlyDefaultSpeedCells));
    FlyScrollSpeed(camera, 1.0F);
    CHECK(camera.speedCells == doctest::Approx(FlyDefaultSpeedCells * FlySpeedStep));
    FlyScrollSpeed(camera, 100.0F);
    CHECK(camera.speedCells == doctest::Approx(FlyMaxSpeedCells));
    FlyScrollSpeed(camera, -100.0F);
    CHECK(camera.speedCells == doctest::Approx(FlyMinSpeedCells));
    // The near plane cuts away a wall against the camera.
    const CameraPose pose = FlyPose(camera, 0.9F, 0.5F);
    CHECK(pose.nearClip == doctest::Approx(FlyNearCells * 0.5F));
    CHECK(pose.rotationRadians.x == doctest::Approx(camera.pitchRadians));
    CHECK(pose.rotationRadians.y == doctest::Approx(camera.yawRadians));
    CHECK(pose.rotationRadians.z == 0.0F);
}

TEST_CASE("ve-free-camera FC2: orbit to free keeps the position and the look") {
    const CameraPose orbit{{3.0F, 40.0F, -7.0F}, {0.4F, -1.2F, 0.0F}, 0.9F, 0.1F, 100.0F};
    const FlyCamera camera = FlyFromPose(orbit, 12.0F);
    CheckNear(camera.position, orbit.position);
    CHECK(camera.pitchRadians == doctest::Approx(0.4F));
    CHECK(camera.yawRadians == doctest::Approx(-1.2F));
    CHECK(camera.speedCells == doctest::Approx(12.0F));
}

TEST_CASE("ve-free-camera FC2: free to orbit keeps the position, the target on the first filled cell") {
    GridDocument document = NewGrid();
    document.cellSize = 1.0F;
    document.Set({0, 0, 10}, {1, 0});
    FlyCamera camera;
    camera.position = {0.5F, 0.5F, 0.5F};
    const auto placed = OrbitFromFly(camera, document); // looking along +Z at the cell's centre (0.5, 0.5, 10.5)
    CHECK(placed.distance == doctest::Approx(10.0F));
    CheckNear(placed.target, {0.5F, 0.5F, 10.5F});
    // The orbit camera stands where the free one did.
    CheckNear(placed.target - FlyForward(camera) * placed.distance, camera.position);
    // Nothing in sight: FlyOrbitFallbackCells ahead.
    camera.yawRadians = 3.1415927F;
    const auto ahead = OrbitFromFly(camera, document);
    CHECK(ahead.distance == doctest::Approx(FlyOrbitFallbackCells));
    CheckNear(ahead.target - FlyForward(camera) * ahead.distance, camera.position);
}

TEST_CASE("ve-free-camera FC3: the keys are the camera's only while the right button is held in the free camera") {
    CHECK(KeysFor(true, true) == KeyUse::Camera);
    CHECK(KeysFor(true, false) == KeyUse::Editing);
    CHECK(KeysFor(false, true) == KeyUse::Editing); // the orbit camera's right drag pans; the keys stay the editor's
    CHECK(KeysFor(false, false) == KeyUse::Editing);
}
