#include "../TestSupport.hpp"

#include "RetroFPS/Collision/GridCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"
#include "RetroFPS/Gameplay/Player/Player.hpp"
#include "RetroFPS/Gameplay/Player/PlayerController.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fps::tests {
namespace {

[[nodiscard]] GridMap ParseValidMap(
    TestContext& context, const std::string_view text) {
    MapLoadResult result = GridMapLoader::Parse(text);
    context.Expect(result.Succeeded(), "valid gameplay map should parse");
    if (!result.map.has_value()) {
        throw std::runtime_error("valid gameplay test map failed to parse: " + result.error);
    }
    return std::move(*result.map);
}

void ExpectPlayerInitialized(
    TestContext& context,
    const PlayerController& controller,
    Player& player,
    const GridMap& map,
    const WorldSettings& settings) {
    std::string error;
    const bool initialized = controller.Initialize(player, map, settings, error);
    context.Expect(initialized, "PlayerController initializes a player on a valid spawn");
    context.Expect(error.empty(), "successful player initialization has no error");
    if (!initialized) {
        throw std::runtime_error("player initialization failed: " + error);
    }
}

[[nodiscard]] GridMap MakeOpenMovementMap(TestContext& context) {
    return ParseValidMap(
        context,
        "#########\n"
        "#D......#\n"
        "#.......#\n"
        "#...P...#\n"
        "#.......#\n"
        "#.......#\n"
        "#########");
}

void TestPlanarMovement(TestContext& context) {
    const Float2 forward = ComputePlanarInput(1.0f, 0.0f, 0.0f);
    context.Expect(
        NearlyEqual(forward.x, 0.0f) && NearlyEqual(forward.z, 1.0f),
        "yaw zero faces +Z");

    const Float2 right = ComputePlanarInput(0.0f, 1.0f, 0.0f);
    context.Expect(
        NearlyEqual(right.x, 1.0f) && NearlyEqual(right.z, 0.0f),
        "right input faces +X");

    const Float2 turned =
        ComputePlanarInput(1.0f, 0.0f, std::numbers::pi_v<float> * 0.5f);
    context.Expect(
        NearlyEqual(turned.x, 1.0f) && NearlyEqual(turned.z, 0.0f),
        "yaw rotates planar input");

    const Float2 diagonal = ComputePlanarInput(1.0f, 1.0f, 0.0f);
    context.Expect(
        NearlyEqual(std::hypot(diagonal.x, diagonal.z), 1.0f),
        "diagonal planar input is normalized");

    const Float2 pitchedUp = ComputePlanarInput(1.0f, 0.5f, 0.7f, 1.2f);
    const Float2 pitchedDown = ComputePlanarInput(1.0f, 0.5f, 0.7f, -1.2f);
    context.Expect(
        NearlyEqual(pitchedUp.x, pitchedDown.x) &&
            NearlyEqual(pitchedUp.z, pitchedDown.z),
        "pitch does not affect planar movement");
}

void TestSemanticMovementInput(TestContext& context) {
    const GridMap map = MakeOpenMovementMap(context);
    const WorldSettings worldSettings{};
    PlayerController controller;
    std::string configureError;
    context.Expect(
        controller.Configure({}, configureError),
        "default PlayerSettings configure successfully");
    context.Expect(configureError.empty(), "valid PlayerSettings leave no configuration error");

    const Float2 spawn = map.GetSpawnPosition(worldSettings.cellSize);
    const auto runInput = [&](const PlayerControlInput& input) {
        Player player;
        ExpectPlayerInitialized(context, controller, player, map, worldSettings);
        controller.Update(player, input, 0.1f, map, worldSettings);
        return player.GetPositionXZ();
    };

    PlayerControlInput input{};
    input.moveForward = 1.0F;
    const Float2 forward = runInput(input);
    context.Expect(NearlyEqual(forward.x, spawn.x), "W does not strafe on yaw zero");
    context.Expect(NearlyEqual(forward.z, spawn.z + 0.3f), "W moves toward +Z");

    input = {};
    input.moveForward = -1.0F;
    const Float2 backward = runInput(input);
    context.Expect(NearlyEqual(backward.z, spawn.z - 0.3f), "S moves toward -Z");

    input = {};
    input.moveRight = -1.0F;
    const Float2 left = runInput(input);
    context.Expect(NearlyEqual(left.x, spawn.x - 0.3f), "A moves toward -X");

    input = {};
    input.moveRight = 1.0F;
    const Float2 right = runInput(input);
    context.Expect(NearlyEqual(right.x, spawn.x + 0.3f), "D moves toward +X");

    input = {};
    input.moveForward = 1.0F;
    input.moveRight = 1.0F;
    const Float2 diagonal = runInput(input);
    context.Expect(
        NearlyEqual(std::hypot(diagonal.x - spawn.x, diagonal.z - spawn.z), 0.3f),
        "semantic diagonal action input keeps configured movement speed");
}

void TestYawAndPitch(TestContext& context) {
    const GridMap map = MakeOpenMovementMap(context);
    const WorldSettings worldSettings{};
    PlayerController controller;
    const PlayerSettings settings = controller.GetSettings();

    Player turnedPlayer;
    ExpectPlayerInitialized(context, controller, turnedPlayer, map, worldSettings);
    const Float2 spawn = turnedPlayer.GetPositionXZ();

    PlayerControlInput lookRight{};
    lookRight.lookEnabled = true;
    lookRight.lookDeltaX =
        (std::numbers::pi_v<float> * 0.5f) / settings.mouseSensitivity;
    controller.Update(turnedPlayer, lookRight, 0.0f, map, worldSettings);
    context.Expect(
        NearlyEqual(turnedPlayer.GetYawRadians(), std::numbers::pi_v<float> * 0.5f),
        "semantic look X updates yaw");
    context.Expect(
        NearlyEqual(turnedPlayer.GetPositionXZ().x, spawn.x) &&
            NearlyEqual(turnedPlayer.GetPositionXZ().z, spawn.z),
        "zero delta time applies look without movement");

    PlayerControlInput forward{};
    forward.moveForward = 1.0F;
    controller.Update(turnedPlayer, forward, 0.1f, map, worldSettings);
    context.Expect(
        NearlyEqual(turnedPlayer.GetPositionXZ().x, spawn.x + 0.3f),
        "yaw rotates forward movement toward +X");
    context.Expect(
        NearlyEqual(turnedPlayer.GetPositionXZ().z, spawn.z),
        "yaw-rotated forward movement has no +Z component at ninety degrees");

    Player pitchedUp;
    Player pitchedDown;
    ExpectPlayerInitialized(context, controller, pitchedUp, map, worldSettings);
    ExpectPlayerInitialized(context, controller, pitchedDown, map, worldSettings);

    PlayerControlInput lookUp{};
    lookUp.lookEnabled = true;
    lookUp.lookDeltaY = 100000.0f;
    controller.Update(pitchedUp, lookUp, 0.0f, map, worldSettings);

    PlayerControlInput lookDown{};
    lookDown.lookEnabled = true;
    lookDown.lookDeltaY = -100000.0f;
    controller.Update(pitchedDown, lookDown, 0.0f, map, worldSettings);

    const float maximumPitch =
        settings.maxPitchDegrees * std::numbers::pi_v<float> / 180.0f;
    context.Expect(
        NearlyEqual(pitchedUp.GetPitchRadians(), maximumPitch),
        "positive mouse Y is clamped to maximum pitch");
    context.Expect(
        NearlyEqual(pitchedDown.GetPitchRadians(), -maximumPitch),
        "negative mouse Y is clamped to minimum pitch");

    controller.Update(pitchedUp, forward, 0.1f, map, worldSettings);
    controller.Update(pitchedDown, forward, 0.1f, map, worldSettings);
    context.Expect(
        NearlyEqual(pitchedUp.GetPositionXZ().x, pitchedDown.GetPositionXZ().x) &&
            NearlyEqual(pitchedUp.GetPositionXZ().z, pitchedDown.GetPositionXZ().z),
        "opposite pitch values do not change horizontal movement");
}

void TestPlayerCollision(TestContext& context) {
    const GridMap map = ParseValidMap(
        context,
        "#####\n"
        "#P..#\n"
        "##..#\n"
        "#..D#\n"
        "#####");
    const WorldSettings worldSettings{};
    PlayerController controller;
    Player player;
    ExpectPlayerInitialized(context, controller, player, map, worldSettings);

    PlayerControlInput input{};
    input.moveForward = 1.0F;
    controller.Update(player, input, 1.0f, map, worldSettings);

    const Float2 position = player.GetPositionXZ();
    context.Expect(position.z <= 1.7501f, "PlayerController stops movement at a wall");
    context.Expect(
        !GridCollision::OverlapsSolid(
            map,
            position,
            controller.GetSettings().collisionRadius,
            worldSettings.cellSize),
        "PlayerController collision result never penetrates a solid cell");
}

void TestPlayerDynamicCollision(TestContext& context) {
    const GridMap map = MakeOpenMovementMap(context);
    const WorldSettings worldSettings{};
    PlayerController controller;
    Player player;
    ExpectPlayerInitialized(context, controller, player, map, worldSettings);

    const Float2 spawn = player.GetPositionXZ();
    const std::array blockers{
        CircleObstacle{{spawn.x, spawn.z + 1.0f}, 0.2f},
    };
    PlayerControlInput input{};
    input.moveForward = 1.0F;
    controller.Update(player, input, 1.0f, map, worldSettings, blockers);

    const Float2 position = player.GetPositionXZ();
    context.Expect(
        NearlyEqual(position.x, spawn.x),
        "dynamic blocker does not introduce lateral player movement");
    context.Expect(
        NearlyEqual(position.z, spawn.z + 0.55f, 0.0002f),
        "PlayerController stops at the dynamic blocker's surface");
    context.Expect(
        !GridCollision::OverlapsCircle(
            position,
            controller.GetSettings().collisionRadius,
            blockers.front()),
        "PlayerController never penetrates a dynamic blocker");
}

void TestInvalidPlayerSettings(TestContext& context) {
    const auto expectInvalid = [&context](
                                   const PlayerSettings settings,
                                   const std::string_view description) {
        PlayerController controller;
        std::string error = "stale";
        context.Expect(!controller.Configure(settings, error), description);
        context.Expect(!error.empty(), "invalid PlayerSettings produce an error message");
    };

    PlayerSettings settings{};
    settings.eyeHeight = -1.0f;
    expectInvalid(settings, "controller rejects negative eye height");

    settings = {};
    settings.collisionRadius = 0.0f;
    expectInvalid(settings, "controller rejects zero collision radius");

    settings = {};
    settings.movementSpeed = -1.0f;
    expectInvalid(settings, "controller rejects negative movement speed");

    settings = {};
    settings.mouseSensitivity = -1.0f;
    expectInvalid(settings, "controller rejects negative mouse sensitivity");

    settings = {};
    settings.maxPitchDegrees = 90.0f;
    expectInvalid(settings, "controller rejects a ninety-degree pitch limit");

    settings = {};
    settings.movementSpeed = (std::numeric_limits<float>::quiet_NaN)();
    expectInvalid(settings, "controller rejects non-finite settings");

    PlayerController largePlayerController;
    settings = {};
    settings.collisionRadius = 0.6f;
    std::string error;
    context.Expect(
        largePlayerController.Configure(settings, error),
        "large finite collision radius is a valid setting by itself");

    const GridMap map = ParseValidMap(context, "PD");
    Player player;
    context.Expect(
        !largePlayerController.Initialize(player, map, {}, error),
        "controller rejects a configured player that overlaps the spawn walls");
    context.Expect(!error.empty(), "spawn collision initialization reports an error");
}

void TestJumpBodyAndFrameIndependence(TestContext& context) {
    const GridMap map = MakeOpenMovementMap(context);
    PlayerController controller;
    Player coarse, fine;
    ExpectPlayerInitialized(context, controller, coarse, map, {});
    ExpectPlayerInitialized(context, controller, fine, map, {});
    PlayerControlInput jump;
    jump.jumpPressed = true;
    controller.Update(coarse, jump, 0.0f, map, {});
    controller.Update(fine, jump, 0.0f, map, {});
    const float ascent = std::sqrt(2.0f * controller.GetSettings().jumpHeight /
                                  controller.GetSettings().gravity);
    controller.Update(coarse, {}, ascent, map, {});
    for (int index = 0; index < 20; ++index) controller.Update(fine, {}, ascent / 20.0f, map, {});
    context.Expect(!coarse.IsGrounded() && NearlyEqual(coarse.GetFeetY(), 0.6f) &&
                       NearlyEqual(coarse.GetFeetY(), fine.GetFeetY()) &&
                       NearlyEqual(coarse.GetVerticalVelocity(), 0.0f),
                   "capsule feet reach configured jump apex independent of timestep");
    context.Expect(NearlyEqual(coarse.GetEyePosition(1.6f).y, coarse.GetFeetY() + 1.6f),
                   "world camera eye follows capsule feet with unchanged relative eye offset");
    controller.Update(coarse, jump, 0.05f, map, {});
    context.Expect(coarse.GetFeetY() < 0.6f && coarse.GetVerticalVelocity() < 0.0f,
                   "midair jump press cannot create a second impulse");
    const float feetBeforeInvalid = coarse.GetFeetY();
    controller.Update(coarse, jump, -1.0f, map, {});
    context.Expect(NearlyEqual(coarse.GetFeetY(), feetBeforeInvalid),
                   "invalid timestep cannot advance a jumping capsule");
    controller.Update(coarse, {}, 100.0f, map, {});
    context.Expect(coarse.IsGrounded() && coarse.GetFeetY() == 0.0f &&
                       coarse.GetVerticalVelocity() == 0.0f,
                   "large timestep lands without tunneling below the floor");
    controller.Update(coarse, jump, 0.1f, map, {});
    context.Expect(!coarse.IsGrounded() && coarse.GetFeetY() > 0.0f,
                   "a fresh press can jump again after landing");
    const GridMap narrow = ParseValidMap(context, "#####\n#P.D#\n#####");
    Player wallPlayer;
    ExpectPlayerInitialized(context, controller, wallPlayer, narrow, {});
    controller.Update(wallPlayer, jump, 0.0f, narrow, {});
    PlayerControlInput towardWall;
    towardWall.moveRight = -1.0f;
    controller.Update(wallPlayer, towardWall, 0.1f, narrow, {});
    context.Expect(!wallPlayer.IsGrounded() &&
                       wallPlayer.GetPositionXZ().x >= 1.25f &&
                       !GridCollision::OverlapsSolid(narrow, wallPlayer.GetPositionXZ(), 0.25f),
                   "airborne horizontal motion stops at the wall instead of bypassing it");
    Player blockedPlayer;
    ExpectPlayerInitialized(context, controller, blockedPlayer, map, {});
    const Float2 spawn = blockedPlayer.GetPositionXZ();
    const CircleObstacle enemy{{spawn.x + 0.6f, spawn.z}, 0.25f};
    controller.Update(blockedPlayer, jump, 0.0f, map, {});
    towardWall.moveRight = 1.0f;
    controller.Update(blockedPlayer, towardWall, 0.1f, map, {}, {&enemy, 1});
    context.Expect(!blockedPlayer.IsGrounded() && blockedPlayer.GetPositionXZ().x <= spawn.x + 0.101f,
                   "airborne horizontal movement retains dynamic enemy blockers");
    std::string error;
    PlayerSettings invalid;
    invalid.gravity = 0.0f;
    context.Expect(!controller.Configure(invalid, error), "zero jump gravity is rejected");
    invalid = {}; invalid.jumpHeight = (std::numeric_limits<float>::quiet_NaN)();
    context.Expect(!controller.Configure(invalid, error), "non-finite jump height is rejected");
    ExpectPlayerInitialized(context, controller, coarse, map, {});
    context.Expect(coarse.IsGrounded() && coarse.GetFeetY() == 0.0f,
                   "stage spawn resets vertical motion");
}

} // namespace

void RunPlayerControllerTests(TestContext& context) {
    TestJumpBodyAndFrameIndependence(context);
    TestPlanarMovement(context);
    TestSemanticMovementInput(context);
    TestYawAndPitch(context);
    TestPlayerCollision(context);
    TestPlayerDynamicCollision(context);
    TestInvalidPlayerSettings(context);
}

} // namespace fps::tests
