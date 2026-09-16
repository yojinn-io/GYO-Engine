#include "../TestSupport.hpp"

#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/Game/GameSession.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace fps::tests {
namespace {

constexpr std::string_view kEnemies =
    "enemy_id,kind,damage,attack_interval_seconds,hp,defense,hitbox_radius,hitbox_height,render_width,render_height,texture_asset_id,frame_width_px,frame_height_px\n"
    "melee_basic,melee,15,0.9,50,5,0.2,0.8,0.973913,0.8,object_fps.texture.enemy.blood_dog,560,460\n"
    "ranged_basic,ranged,10,1.25,40,0,0.2,1.6,1.230769,1.6,object_fps.texture.enemy.spitter,700,910\n";

constexpr std::string_view kAnimations =
    "enemy_id,state,origin_x_px,origin_y_px,frame_count,seconds_per_frame,event_frame_index,muzzle_x_px,muzzle_y_px\n"
    "melee_basic,idle,0,0,3,0.1,,,\n"
    "melee_basic,move,0,460,4,0.1,,,\n"
    "melee_basic,attack,0,920,6,0.05,3,,\n"
    "melee_basic,dead,0,1380,4,0.1,,,\n"
    "ranged_basic,idle,0,0,3,0.1,,,\n"
    "ranged_basic,move,0,910,4,0.1,,,\n"
    "ranged_basic,attack,0,1820,5,0.05,2,350,420\n"
    "ranged_basic,dead,0,2730,4,0.1,,,\n";

constexpr std::string_view kWeapons =
    "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,draw_seconds,hide_seconds,presentation_asset_id\n"
    "starter_pistol,25,12,48,1.5,false,0.3333333333,3.7333333333,0.8333333333,0.3666666667,object_fps.weapon.mark23\n";

constexpr std::string_view kMap =
    "##############\n"
    "#P..........R#\n"
    "#..###..#....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#..#...##....#\n"
    "#..#..####...#\n"
    "#.M.........D#\n"
    "##############\n";

// Tests explicitly author their own muzzle; production content has no default
// or legacy-offset fallback when calibration is absent.
constexpr WeaponShotGeometry kFixtureShotGeometry{
    {0.28F, -0.22F, 0.35F}, std::numbers::pi_v<float> / 3.0F};

[[nodiscard]] Float3 ExpectedMuzzle(
    const PlayerSnapshot& player,
    const WeaponShotGeometry& geometry,
    const float worldFov) {
    const float scale = std::tan(worldFov / 2.0F) /
                        std::tan(geometry.viewModelVerticalFovRadians / 2.0F);
    const Float3 local{geometry.muzzleViewCameraPosition.x * scale,
                       geometry.muzzleViewCameraPosition.y * scale,
                       geometry.muzzleViewCameraPosition.z};
    const float sp = std::sin(player.pitchRadians), cp = std::cos(player.pitchRadians);
    const float sy = std::sin(player.yawRadians), cy = std::cos(player.yawRadians);
    return {
        player.position.x + cy * local.x + sy * sp * local.y + sy * cp * local.z,
        player.feetY + player.eyeHeight + cp * local.y - sp * local.z,
        player.position.z - sy * local.x + cy * sp * local.y + cy * cp * local.z,
    };
}

[[nodiscard]] bool SamePoint(const Float3 a, const Float3 b) {
    return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y) && NearlyEqual(a.z, b.z);
}

[[nodiscard]] std::string MakeLevels(const std::size_t count) {
    std::ostringstream csv;
    csv << "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,"
           "melee_enemy_count,active_enemy_limit,clear_kill_count\n";
    for (std::size_t index = 0; index < count; ++index) {
        csv << "room_" << index << ",ROOM " << index << ",object_fps.map.room_"
            << index << ',';
        if (index + 1 < count) {
            csv << "room_" << (index + 1);
        }
        csv << ",1,1,2,1\n";
    }
    return csv.str();
}

[[nodiscard]] std::shared_ptr<const CampaignContent> MakeContent(
    TestContext& context,
    const std::size_t stageCount,
    const std::string_view mapText = kMap,
    const WeaponShotGeometry geometry = kFixtureShotGeometry) {
    GameDataLoadResult data =
        GameDataLoader::Parse(kEnemies, kAnimations, kWeapons, MakeLevels(stageCount));
    context.Expect(data.Succeeded(), "session fixture game data parses");
    if (!data.catalog.has_value()) {
        return {};
    }

    std::vector<GridMap> maps;
    maps.reserve(stageCount);
    for (std::size_t index = 0; index < stageCount; ++index) {
        MapLoadResult map = GridMapLoader::Parse(mapText);
        context.Expect(map.Succeeded(), "session fixture map parses");
        if (!map.map.has_value()) {
            return {};
        }
        maps.push_back(std::move(*map.map));
    }

    CampaignContentBuildResult content =
        CampaignContent::Build(std::move(*data.catalog), std::move(maps),
                               {{"starter_pistol", geometry}});
    context.Expect(content.Succeeded(), "session fixture CampaignContent builds");
    if (!content.content.has_value()) {
        return {};
    }
    return std::make_shared<CampaignContent>(std::move(*content.content));
}

template <class Payload>
[[nodiscard]] bool HasEvent(const std::span<const GameSessionEvent> events) {
    for (const GameSessionEvent& event : events) {
        if (std::holds_alternative<Payload>(event.payload)) {
            return true;
        }
    }
    return false;
}

void TestContentCardinality(TestContext& context) {
    for (const std::size_t stageCount : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        const std::shared_ptr<const CampaignContent> content =
            MakeContent(context, stageCount);
        context.Expect(
            content && content->Stages().size() == stageCount,
            "CampaignContent preserves arbitrary data-defined cardinality");
        if (content) {
            context.Expect(
                content->Stages().back().definition.nextLevelId == std::nullopt,
                "the chain-defined terminal stage is retained");
        }
    }
}

void TestWeaponGeometryAndFovValidation(TestContext& context) {
    const auto build = [&](WeaponShotGeometryMap geometry, const bool secondWeapon = false) {
        std::string weapons{kWeapons};
        if (secondWeapon) {
            weapons += "second_pistol,25,12,48,1.5,false,0.3333333333,3.7333333333,0.8333333333,0.3666666667,object_fps.weapon.mark23\n";
        }
        auto data = GameDataLoader::Parse(kEnemies, kAnimations, weapons, MakeLevels(1));
        auto map = GridMapLoader::Parse(kMap);
        context.Expect(data.Succeeded() && map.Succeeded(), "geometry validation fixture parses");
        if (!data || !map) return CampaignContentBuildResult{};
        std::vector<GridMap> maps;
        maps.push_back(std::move(*map.map));
        return CampaignContent::Build(std::move(*data.catalog), std::move(maps), std::move(geometry));
    };
    context.Expect(!build({}), "campaign rejects missing weapon muzzle calibration");
    context.Expect(!build({{"starter_pistol", kFixtureShotGeometry}}, true),
                   "every catalog weapon requires calibration, including non-starting weapons");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (const WeaponShotGeometry geometry : {
             WeaponShotGeometry{{nan, 0, 1}, 1},
             WeaponShotGeometry{{0, infinity, 1}, 1},
             WeaponShotGeometry{{0, 0, nan}, 1},
             WeaponShotGeometry{{0, 0, 0}, 1},
             WeaponShotGeometry{{0, 0, -1}, 1},
             WeaponShotGeometry{{0, 0, 1}, 0},
             WeaponShotGeometry{{0, 0, 1}, std::numbers::pi_v<float>},
             WeaponShotGeometry{{0, 0, 1}, nan}}) {
        const auto result = build({{"starter_pistol", geometry}});
        context.Expect(!result && !result.error.empty(), "campaign rejects invalid muzzle coordinates or viewmodel FOV");
    }
    const auto content = MakeContent(context, 1);
    if (!content) return;
    const auto* geometry = content->FindWeaponShotGeometry("starter_pistol");
    context.Expect(geometry && SamePoint(geometry->muzzleViewCameraPosition,
                                        kFixtureShotGeometry.muzzleViewCameraPosition) &&
                       !content->FindWeaponShotGeometry("unknown"),
                   "immutable campaign exposes authored geometry only for known weapon IDs");
    GameSession session;
    std::string error;
    for (const float fov : {0.0F, -1.0F, std::numbers::pi_v<float>, infinity, nan}) {
        GameSessionConfig config;
        config.worldVerticalFovRadians = fov;
        context.Expect(!session.Initialize(content, config, error) && !error.empty(),
                       "session rejects invalid world FOV before simulation starts");
    }
    GameSessionConfig valid;
    valid.worldVerticalFovRadians = std::numbers::pi_v<float> / 2.0F;
    context.Expect(session.Initialize(content, valid, error) &&
                       NearlyEqual(session.Snapshot().worldVerticalFovRadians, valid.worldVerticalFovRadians),
                   "query carries the configured world FOV even before entering a stage");
    const Float3 converted = ResolveWeaponMuzzleCameraPosition(kFixtureShotGeometry, valid.worldVerticalFovRadians);
    context.Expect(NearlyEqual(converted.x / converted.z / std::tan(valid.worldVerticalFovRadians / 2),
                               kFixtureShotGeometry.muzzleViewCameraPosition.x /
                                   kFixtureShotGeometry.muzzleViewCameraPosition.z /
                                   std::tan(kFixtureShotGeometry.viewModelVerticalFovRadians / 2)) &&
                       NearlyEqual(converted.z, kFixtureShotGeometry.muzzleViewCameraPosition.z),
                   "FOV conversion preserves projected muzzle location and authored depth");
}

void TestCalibratedTracerBirthAndCameraPose(TestContext& context) {
    struct PoseCase { float yaw; float pitch; float worldFov; bool jump; };
    const WeaponShotGeometry geometry{{0.19F, -0.14F, 0.53F}, 0.91F};
    constexpr std::string_view openMap =
        "###############\n#............D#\n#.............#\n#.............#\n"
        "#.............#\n#.............#\n#.............#\n#......P......#\n"
        "#.............#\n#.............#\n#.............#\n#.............#\n"
        "#.M.........R.#\n#.............#\n###############\n";
    for (const PoseCase pose : {PoseCase{0, 0, 0.78F, false},
                               PoseCase{0.7F, 0.3F, 1.05F, false},
                               PoseCase{-0.6F, -0.25F, 1.57F, true}}) {
        const auto content = MakeContent(context, 1, openMap, geometry);
        if (!content) return;
        GameSession session;
        GameSessionConfig config;
        config.fadeOutSeconds = config.fadeInSeconds = 0.1F;
        config.worldVerticalFovRadians = pose.worldFov;
        config.enemies.meleeSpeed = config.enemies.rangedSpeed = 0.001F;
        std::string error;
        context.Expect(session.Initialize(content, config, error), "calibrated camera-pose session initializes");
        const GameSessionCommand start = StartCampaignCommand{};
        context.Expect(session.Advance(0, {}, std::span{&start, 1}, error), "calibrated camera-pose campaign starts");
        for (const float dt : {0.1F, 0.1F, 25.0F / 30.0F}) {
            context.Expect(session.Advance(dt, {}, {}, error), "calibrated camera-pose weapon becomes ready");
        }
        GameFrameInput aim;
        aim.lookEnabled = true;
        aim.lookDeltaX = pose.yaw / config.player.mouseSensitivity;
        aim.lookDeltaY = pose.pitch / config.player.mouseSensitivity;
        aim.jumpPressed = aim.jumpHeld = pose.jump;
        context.Expect(session.Advance(0.1F, aim, {}, error), "fixture applies authored yaw/pitch and optional body jump");
        const PlayerSnapshot before = *session.Snapshot().player;
        GameFrameInput fire;
        fire.fireHeld = fire.firePressed = true;
        // A nonzero birth dt would have visibly advanced the old tracer path.
        context.Expect(session.Advance(0.02F, fire, {}, error), "calibrated weapon fires on nonzero frame dt");
        const PlayerSnapshot after = *session.Snapshot().player;
        context.Expect(NearlyEqual(after.pitchRadians, before.pitchRadians - 1.5F * std::numbers::pi_v<float> / 180) &&
                           NearlyEqual(session.Snapshot().worldVerticalFovRadians, pose.worldFov),
                       "final snapshot contains new recoil and the shared world FOV");
        if (pose.jump) context.Expect(after.feetY > 0 && !after.grounded, "calibrated tracer scenario uses an airborne capsule");
        const ProjectileSnapshot* tracer = nullptr;
        for (const auto& item : session.Snapshot().projectiles) {
            if (item.kind == ProjectileKind::PlayerTracer) tracer = &item;
        }
        context.Expect(tracer && SamePoint(tracer->position, ExpectedMuzzle(after, geometry, pose.worldFov)),
                       "birth-frame tracer stays at calibrated final camera muzzle across yaw/pitch/jump/FOV");
        if (!tracer) continue;
        const ProjectileSnapshot birth = *tracer;
        context.Expect(session.Advance(0, {}, {}, error), "zero-delta frame follows tracer birth");
        bool stayed = false;
        for (const auto& item : session.Snapshot().projectiles) {
            if (item.id == birth.id) stayed = SamePoint(item.position, birth.position);
        }
        context.Expect(stayed, "zero-delta frame leaves the born tracer at its muzzle");
        context.Expect(session.Advance(0.002F, {}, {}, error), "next simulation frame advances the existing tracer");
        bool advanced = false;
        for (const auto& item : session.Snapshot().projectiles) {
            if (item.id != birth.id) continue;
            const Float3 delta{item.position.x - birth.position.x, item.position.y - birth.position.y,
                               item.position.z - birth.position.z};
            advanced = NearlyEqual(std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z),
                                   config.projectiles.playerTracerSpeed * 0.002F);
        }
        context.Expect(advanced && session.Snapshot().weapon.magazineAmmo == 11,
                       "tracer travels one next-frame time step without another shot or ammo consumption");
    }
}

void TestReticleDamageAndCosmeticTracerSeparation(TestContext& context) {
    const auto content = MakeContent(context, 1);
    if (!content) return;
    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = config.fadeInSeconds = 0.1F;
    config.enemies.meleeSpeed = config.enemies.rangedSpeed = 0.001F;
    std::string error;
    context.Expect(session.Initialize(content, config, error), "reticle damage fixture initializes");
    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(session.Advance(0, {}, std::span{&start, 1}, error), "reticle damage fixture starts");
    for (const float dt : {0.1F, 0.1F, 25.0F / 30.0F}) {
        context.Expect(session.Advance(dt, {}, {}, error), "reticle damage fixture readies weapon");
    }
    EnemySnapshot target;
    for (const auto& item : session.Snapshot().enemies) if (item.kind == EnemyKind::Ranged) target = item;
    const PlayerSnapshot before = *session.Snapshot().player;
    const float dx = target.position.x - before.position.x, dz = target.position.z - before.position.z;
    const float targetY = target.hitboxHeight - target.collisionRadius;
    GameFrameInput aim;
    aim.lookEnabled = true;
    aim.lookDeltaX = std::atan2(dx, dz) / config.player.mouseSensitivity;
    aim.lookDeltaY = std::atan2(before.eyeHeight - targetY, std::hypot(dx, dz)) / config.player.mouseSensitivity;
    context.Expect(session.Advance(0, aim, {}, error), "reticle aims near the far enemy's upper capsule");
    GameFrameInput fire;
    fire.fireHeld = fire.firePressed = true;
    context.Expect(session.Advance(0, fire, {}, error), "reticle shot resolves before adding recoil");
    const PlayerSnapshot fired = *session.Snapshot().player;
    const CombatTarget targetCapsule{target.id, {target.position, target.hitboxHeight, target.collisionRadius}};
    const float cp = std::cos(fired.pitchRadians);
    const auto postRecoilHit = CombatCollision::Raycast(content->Stages()[0].map, config.world,
        {fired.position.x, fired.feetY + fired.eyeHeight, fired.position.z},
        {std::sin(fired.yawRadians) * cp, -std::sin(fired.pitchRadians), std::cos(fired.yawRadians) * cp},
        50, {&targetCapsule, 1});
    context.Expect(!postRecoilHit || postRecoilHit->kind != CombatHitKind::Target,
                   "fixture distinguishes old reticle aim from new recoil ray which misses the enemy");
    bool damageCorrect = false;
    for (const auto& item : session.Snapshot().enemies) {
        if (item.id == target.id) damageCorrect = NearlyEqual(item.health, target.health - 25);
    }
    context.Expect(damageCorrect, "pre-recoil reticle hit applies exactly 25 damage immediately");
    bool cosmeticAtMuzzle = false;
    for (const auto& item : session.Snapshot().projectiles) {
        if (item.kind == ProjectileKind::PlayerTracer) {
            cosmeticAtMuzzle = SamePoint(item.position, ExpectedMuzzle(fired, kFixtureShotGeometry, config.worldVerticalFovRadians));
        }
    }
    context.Expect(cosmeticAtMuzzle, "same shot's cosmetic tracer uses the visible post-recoil muzzle");
    context.Expect(session.Advance(0.5F, {}, {}, error), "cosmetic tracer completes its flight to physical impact");
    damageCorrect = false;
    bool retired = true;
    for (const auto& item : session.Snapshot().enemies) {
        if (item.id == target.id) damageCorrect = NearlyEqual(item.health, target.health - 25);
    }
    for (const auto& item : session.Snapshot().projectiles) if (item.kind == ProjectileKind::PlayerTracer) retired = false;
    context.Expect(damageCorrect && retired && !HasEvent<ShotEvent>(session.Events()),
                   "tracer retirement applies no repeat damage and creates no second shot event");
}

void TestSessionCommandsSnapshotsAndEvents(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 4);
    if (!content) {
        return;
    }

    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = 0.1F;
    config.fadeInSeconds = 0.1F;
    std::string error;
    context.Expect(
        session.Initialize(content, config, error),
        "GameSession initializes from immutable CampaignContent");
    context.Expect(error.empty(), "successful GameSession initialization clears errors");

    const GameSessionCommand illegal = PauseCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&illegal, 1}, error),
        "illegal external command does not corrupt the session");
    context.Expect(
        HasEvent<CommandRejectedEvent>(session.Events()),
        "illegal external command produces a typed rejection event");
    context.Expect(
        session.Snapshot().screen == GameScreen::MainMenu,
        "rejected command leaves query state unchanged");

    const GameSessionCommand openControls = OpenControlsCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&openControls, 1}, error) &&
            session.Snapshot().screen == GameScreen::Controls,
        "OpenControls is accepted as an opaque-UI semantic command");
    const GameSessionCommand closeControls = CloseControlsCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&closeControls, 1}, error) &&
            session.Snapshot().screen == GameScreen::MainMenu,
        "CloseControls returns to MainMenu without a menu-index switch");

    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&start, 1}, error),
        "StartCampaign command enters the transition protocol");
    context.Expect(
        session.Snapshot().transitionPhase == StageTransitionPhase::FadingOut,
        "query exposes the pending runtime transition");

    context.Expect(
        session.Advance(0.1F, {}, {}, error),
        "data-defined first stage commits after fade out");
    const GameSessionSnapshot& playing = session.Snapshot();
    context.Expect(
        playing.screen == GameScreen::Playing && playing.activeStage.has_value(),
        "query exposes the committed playing stage");
    context.Expect(
        playing.activeStage.has_value() && playing.activeStage->levelId == "room_0" &&
            playing.activeStage->stageCount == 4,
        "stage snapshot uses catalog identity and dynamic cardinality");
    context.Expect(
        playing.player.has_value() && playing.enemies.size() == 2,
        "snapshot contains observer-safe player and enemy values");
    context.Expect(
        HasEvent<StageEnteredEvent>(session.Events()) &&
            HasEvent<EnemySpawnedEvent>(session.Events()),
        "stage commit emits typed runtime facts");

    context.Expect(
        session.Advance(0.1F, {}, {}, error),
        "fade in completes without changing stage policy");
    const GameSessionCommand pause = PauseCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&pause, 1}, error),
        "Pause is accepted through the command boundary");
    context.Expect(
        session.Snapshot().screen == GameScreen::Paused &&
            HasEvent<ScreenChangedEvent>(session.Events()),
        "pause updates query state and emits a typed screen event");

    const GameSessionCommand resume = ResumeCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&resume, 1}, error),
        "Resume is accepted through the same command boundary");
    context.Expect(
        session.Snapshot().screen == GameScreen::Playing,
        "resume returns the session to its active stage");

    context.Expect(
        session.Advance(0.0F, {}, std::span{&pause, 1}, error),
        "session can pause again before testing local resume input");
    const std::uint32_t ammoBeforeLocalResume =
        session.Snapshot().weapon.magazineAmmo;
    const Float2 positionBeforeLocalResume = session.Snapshot().player.has_value()
        ? session.Snapshot().player->position
        : Float2{};
    GameFrameInput resumeInput;
    resumeInput.backPressed = true;
    resumeInput.moveForward = 1.0F;
    resumeInput.fireHeld = true;
    resumeInput.firePressed = true;
    context.Expect(
        session.Advance(0.0F, resumeInput, {}, error) &&
            session.Snapshot().screen == GameScreen::Playing,
        "escape resumes through the local GameFlow path");

    GameFrameInput heldAfterResume;
    heldAfterResume.moveForward = 1.0F;
    heldAfterResume.fireHeld = true;
    heldAfterResume.firePressed = true;
    context.Expect(
        session.Advance(0.05F, heldAfterResume, {}, error),
        "first held-input frame after local resume advances safely");
    context.Expect(
        session.Snapshot().weapon.magazineAmmo == ammoBeforeLocalResume,
        "local resume requires fire release before gameplay accepts another shot");
    context.Expect(
        session.Snapshot().player.has_value() &&
            session.Snapshot().player->position.x == positionBeforeLocalResume.x &&
            session.Snapshot().player->position.z == positionBeforeLocalResume.z,
        "local resume requires movement release before gameplay moves the player");

    const GameSessionCommand quit = RequestQuitCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&quit, 1}, error),
        "quit intent is accepted without platform coupling");
    context.Expect(
        session.Snapshot().quitRequested &&
            HasEvent<QuitRequestedEvent>(session.Events()),
        "quit intent is visible through query and event seams");
}

void TestInvalidDeltaDoesNotAdvance(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 1);
    if (!content) {
        return;
    }
    GameSession session;
    std::string error;
    context.Expect(session.Initialize(content, {}, error), "delta test session initializes");
    context.Expect(
        !session.Advance(-0.1F, {}, {}, error) && !error.empty(),
        "negative delta is rejected at the game-policy boundary");
}

void TestFocusLossSurvivesTransitionInputLock(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 1);
    if (!content) {
        return;
    }

    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = 0.1F;
    config.fadeInSeconds = 0.1F;
    std::string error;
    context.Expect(
        session.Initialize(content, config, error),
        "focus-loss transition session initializes");

    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&start, 1}, error),
        "focus-loss transition starts a campaign");

    GameFrameInput focusLost;
    focusLost.focusLost = true;
    context.Expect(
        session.Advance(0.05F, focusLost, {}, error) &&
            session.Snapshot().transitionPhase != StageTransitionPhase::Idle,
        "focus loss is accepted while fade input is locked");
    context.Expect(
        session.Advance(0.05F, {}, {}, error) &&
            session.Snapshot().screen == GameScreen::Playing,
        "the stage commits after the focus-loss edge");
    context.Expect(
        session.Advance(0.1F, {}, {}, error) &&
            session.Snapshot().transitionPhase == StageTransitionPhase::Idle,
        "the fade completes before safety input is delivered");
    context.Expect(
        session.Advance(0.0F, {}, {}, error) &&
            session.Snapshot().screen == GameScreen::Paused,
        "latched focus loss pauses the first stable Playing frame");
}

void TestJumpAndWeaponActionIntegration(TestContext& context) {
    const auto content = MakeContent(context, 1);
    if (!content) return;
    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = config.fadeInSeconds = 0.1f;
    config.enemies.meleeSpeed = config.enemies.rangedSpeed = 0.001f;
    std::string error;
    context.Expect(session.Initialize(content, config, error), "jump/weapon session initializes");
    const auto step = [&](const float dt, const GameFrameInput input = {}) {
        context.Expect(session.Advance(dt, input, {}, error), "headless jump/weapon step succeeds");
    };
    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(session.Advance(0.0f, {}, std::span{&start, 1}, error), "animated campaign starts");
    context.Expect(HasEvent<WeaponActionEvent>(session.Events()), "start publishes Draw action event");
    step(0.1f); step(0.1f); step(0.0f);
    context.Expect(session.Snapshot().weaponPresentation.action == WeaponAction::Draw &&
                       session.Snapshot().weaponPresentation.elapsedSeconds == 0.0f,
                   "fade freezes initial Draw timeline");
    step(25.0f / 30.0f);
    context.Expect(session.Snapshot().weaponPresentation.action == WeaponAction::Idle,
                   "gameplay time completes draw without a renderer");
    GameFrameInput jump;
    jump.jumpPressed = jump.jumpHeld = true;
    step(0.1f, jump);
    const auto player = *session.Snapshot().player;
    context.Expect(player.feetY > 0.0f && !player.grounded && player.verticalVelocity > 0.0f &&
                       NearlyEqual(player.eyeHeight, config.player.eyeHeight),
                   "jump snapshot exposes airborne body while preserving relative eye offset");
    GameFrameInput fire;
    fire.fireHeld = fire.firePressed = true;
    step(0.0f, fire);
    context.Expect(session.Snapshot().weapon.magazineAmmo == 11 &&
                       session.Snapshot().weaponPresentation.action == WeaponAction::Shoot &&
                       HasEvent<ShotEvent>(session.Events()),
                   "airborne fire publishes authoritative shot and consumes one round");
    bool elevatedMuzzle = false;
    for (const auto& projectile : session.Snapshot().projectiles) {
        if (projectile.kind == ProjectileKind::PlayerTracer) {
            elevatedMuzzle = SamePoint(projectile.position, ExpectedMuzzle(
                *session.Snapshot().player, kFixtureShotGeometry, config.worldVerticalFovRadians));
        }
    }
    context.Expect(elevatedMuzzle, "hitscan tracer muzzle follows the actual airborne eye position");
    GameFrameInput reload;
    reload.reloadPressed = true;
    step(0.0f, reload);
    const GameSessionCommand pause = PauseCommand{};
    context.Expect(session.Advance(0.0f, {}, std::span{&pause, 1}, error), "airborne reload pauses");
    const auto frozen = session.Snapshot();
    step(5.0f);
    context.Expect(session.Snapshot().player->feetY == frozen.player->feetY &&
                       session.Snapshot().player->verticalVelocity == frozen.player->verticalVelocity &&
                       session.Snapshot().weaponPresentation.elapsedSeconds == frozen.weaponPresentation.elapsedSeconds &&
                       session.Snapshot().weapon.magazineAmmo == 11,
                   "pause freezes capsule physics and authoritative reload clock together");
    GameFrameInput held = jump;
    held.fireHeld = held.firePressed = true;
    held.holsterToggleHeld = held.holsterTogglePressed = true;
    const GameSessionCommand resume = ResumeCommand{};
    context.Expect(session.Advance(0.0f, held, std::span{&resume, 1}, error), "resume with held actions succeeds");
    step(112.0f / 30.0f, held);
    step(0.1f, held);
    context.Expect(session.Snapshot().player->grounded && session.Snapshot().player->feetY == 0.0f &&
                       session.Snapshot().weaponPresentation.action == WeaponAction::Idle &&
                       session.Snapshot().weapon.magazineAmmo == 12 &&
                       session.Snapshot().weapon.reserveAmmo == 47,
                   "resume-held jump/fire/holster stay blocked through landing and reload completion");
    step(0.0f);
    GameFrameInput holster;
    holster.holsterTogglePressed = holster.holsterToggleHeld = true;
    step(0.0f, holster);
    context.Expect(session.Snapshot().weaponPresentation.action == WeaponAction::Hide,
                   "fresh H starts Hide after release gate");
    step(11.0f / 30.0f);
    step(0.0f, fire);
    context.Expect(session.Snapshot().weaponPresentation.action == WeaponAction::Holstered &&
                       session.Snapshot().weapon.magazineAmmo == 12,
                   "hidden gameplay weapon is inert");
    step(0.0f, holster);
    step(25.0f / 30.0f);
    context.Expect(session.Snapshot().weaponPresentation.action == WeaponAction::Idle,
                   "H draws weapon back to ready state in headless gameplay");
}

void TestStageTransitionPreservesWeaponAndResetsBody(TestContext& context) {
    const auto content = MakeContent(context, 2,
        "#######\n#P....#\n#M....#\n#R....#\n#D....#\n#######");
    if (!content) return;
    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = config.fadeInSeconds = 0.1f;
    config.enemies.meleeSpeed = config.enemies.rangedSpeed = 0.001f;
    std::string error;
    context.Expect(session.Initialize(content, config, error), "transition-review session initializes");
    const auto step = [&](const float dt, const GameFrameInput input = {}) {
        context.Expect(session.Advance(dt, input, {}, error), "transition-review gameplay step succeeds");
    };
    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(session.Advance(0.0f, {}, std::span{&start, 1}, error), "transition-review campaign starts");
    step(0.1f); step(0.1f); step(25.0f / 30.0f);
    GameFrameInput aim;
    aim.lookEnabled = true;
    aim.lookDeltaY = std::atan(1.2f) / config.player.mouseSensitivity;
    step(0.0f, aim);
    GameFrameInput fire;
    fire.firePressed = fire.fireHeld = true;
    for (int index = 0; index < 3; ++index) step(10.0f / 30.0f, fire);
    context.Expect(session.Snapshot().activeStage->doorVisible && session.Snapshot().weapon.magazineAmmo == 9,
                   "three aimed shots defeat the fixture enemy and open the stage exit");
    GameFrameInput reload;
    reload.reloadPressed = true;
    step(0.0f, reload);
    GameFrameInput strafe;
    strafe.moveRight = 1.0f;
    step(1.0f / 3.0f, strafe);
    GameFrameInput forward;
    forward.moveForward = 1.0f;
    step(1.0f, forward);
    GameFrameInput crossExit;
    crossExit.moveRight = -1.0f;
    crossExit.jumpPressed = crossExit.jumpHeld = true;
    step(1.0f / 3.0f, crossExit);
    const auto exiting = session.Snapshot();
    context.Expect(exiting.transitionPhase == StageTransitionPhase::FadingOut &&
                       exiting.player->feetY > 0.0f && exiting.weapon.reloading,
                   "airborne exit begins fade while authoritative reload remains active");
    step(0.05f, crossExit);
    context.Expect(session.Snapshot().player->feetY == exiting.player->feetY &&
                       session.Snapshot().weaponPresentation.elapsedSeconds == exiting.weaponPresentation.elapsedSeconds,
                   "fade freezes both outgoing body and weapon progress");
    step(0.05f, crossExit);
    context.Expect(session.Snapshot().activeStage->levelId == "room_1" &&
                       session.Snapshot().player->grounded && session.Snapshot().player->feetY == 0.0f &&
                       session.Snapshot().player->verticalVelocity == 0.0f &&
                       session.Snapshot().weapon.magazineAmmo == 9 &&
                       session.Snapshot().weaponPresentation.revision == exiting.weaponPresentation.revision &&
                       session.Snapshot().weaponPresentation.elapsedSeconds == exiting.weaponPresentation.elapsedSeconds,
                   "stage commit resets capsule at spawn and preserves weapon ammo/action identity and progress");
    step(0.1f, crossExit); step(0.1f, crossExit);
    context.Expect(session.Snapshot().player->grounded &&
                       NearlyEqual(session.Snapshot().player->position.x, 1.5f),
                   "held movement and jump do not leak through a stage transition");
    const GameSessionCommand menu = ReturnToMainMenuCommand{};
    context.Expect(session.Advance(0.0f, {}, std::span{&menu, 1}, error), "transition-review returns to menu");
    step(0.1f); step(0.1f);
    context.Expect(session.Advance(0.0f, {}, std::span{&start, 1}, error), "new campaign starts after menu");
    context.Expect(session.Snapshot().weapon.magazineAmmo == 12 && session.Snapshot().weapon.reserveAmmo == 48 &&
                       session.Snapshot().weaponPresentation.action == WeaponAction::Draw &&
                       session.Snapshot().weaponPresentation.elapsedSeconds == 0.0f,
                   "new campaign restores ammo and starts a fresh Draw action");
    std::uint64_t previousSequence = 0;
    for (const auto& event : session.Events()) {
        context.Expect(event.sequence > previousSequence, "session fact sequences increase within each event batch");
        previousSequence = event.sequence;
    }
}

void TestWallAdjacentMuzzleGameplay(TestContext& context) {
    const auto scenario = [&](const std::string_view mapText, const bool sideWall) {
        const auto content = MakeContent(context, 1, mapText);
        if (!content) return;
        GameSession session;
        GameSessionConfig config;
        config.fadeOutSeconds = config.fadeInSeconds = 0.1f;
        config.enemies.meleeSpeed = config.enemies.rangedSpeed = 0.001f;
        std::string error;
        context.Expect(session.Initialize(content, config, error), "wall-adjacent weapon session initializes");
        const GameSessionCommand start = StartCampaignCommand{};
        context.Expect(session.Advance(0.0f, {}, std::span{&start, 1}, error), "wall-adjacent weapon campaign starts");
        for (const float dt : {0.1f, 0.1f, 25.0f / 30.0f}) {
            context.Expect(session.Advance(dt, {}, {}, error), "wall-adjacent weapon draw completes");
        }
        GameFrameInput hugWall;
        hugWall.moveRight = sideWall ? 1.0f : 0.0f;
        hugWall.moveForward = sideWall ? 0.0f : 1.0f;
        context.Expect(session.Advance(0.1f, hugWall, {}, error), "player moves to legal capsule contact with wall");
        const auto before = session.Snapshot();
        const EnemySnapshot* target = nullptr;
        for (const auto& enemy : before.enemies) if (enemy.kind == EnemyKind::Ranged) target = &enemy;
        context.Expect(target != nullptr, "wall-adjacent weapon test has a ranged target");
        if (!target) return;
        const float dx = target->position.x - before.player->position.x;
        const float dz = target->position.z - before.player->position.z;
        const float eyeY = before.player->feetY + before.player->eyeHeight;
        GameFrameInput aim;
        aim.lookEnabled = true;
        aim.lookDeltaX = std::atan2(dx, dz) / config.player.mouseSensitivity;
        aim.lookDeltaY = std::atan2(eyeY - target->hitboxHeight * 0.5f, std::hypot(dx, dz)) /
                         config.player.mouseSensitivity;
        context.Expect(session.Advance(0.0f, aim, {}, error), "reticle aims at target from legal wall-adjacent body");
        const CombatTarget probe{target->id, {target->position, target->hitboxHeight, target->collisionRadius}};
        const auto aimHit = CombatCollision::Raycast(content->Stages()[0].map, config.world,
            {before.player->position.x, eyeY, before.player->position.z},
            {dx, target->hitboxHeight * 0.5f - eyeY, dz}, 50, {&probe, 1});
        context.Expect(aimHit && aimHit->kind == (sideWall ? CombatHitKind::Target : CombatHitKind::Wall),
                       "side-wall reticle sees target, front-wall reticle remains occluded");
        GameFrameInput fire;
        fire.firePressed = fire.fireHeld = true;
        context.Expect(session.Advance(0.0f, fire, {}, error), "wall-adjacent firing succeeds without zero-length errors");
        bool healthCorrect = false;
        for (const auto& enemy : session.Snapshot().enemies) {
            if (enemy.id == target->id) healthCorrect = NearlyEqual(enemy.health,
                target->health - (sideWall ? 25.0f : 0.0f));
        }
        context.Expect(healthCorrect && session.Snapshot().weapon.magazineAmmo == 11,
                       sideWall ? "clear reticle shot damages enemy beside wall instead of being absorbed"
                                : "front wall still prevents damage while firing consumes a round");
        bool freeTracer = false;
        for (const auto& projectile : session.Snapshot().projectiles) {
            if (projectile.kind == ProjectileKind::PlayerTracer) {
                freeTracer = !CombatCollision::Raycast(content->Stages()[0].map, config.world,
                    projectile.position, {0, 0, 1}, 0).has_value();
            }
        }
        context.Expect(freeTracer, "physical tracer starts on free-space side of the near wall");
    };
    scenario("####\n#P##\n#.##\n#.##\n#.##\n#R##\n#M##\n#D##\n####", true);
    scenario("######\n#P...#\n###..#\n#....#\n#R...#\n#M..D#\n######", false);
}

} // namespace

void RunGameSessionTests(TestContext& context) {
    TestWeaponGeometryAndFovValidation(context);
    TestCalibratedTracerBirthAndCameraPose(context);
    TestReticleDamageAndCosmeticTracerSeparation(context);
    TestWallAdjacentMuzzleGameplay(context);
    TestStageTransitionPreservesWeaponAndResetsBody(context);
    TestJumpAndWeaponActionIntegration(context);
    TestContentCardinality(context);
    TestSessionCommandsSnapshotsAndEvents(context);
    TestInvalidDeltaDoesNotAdvance(context);
    TestFocusLossSurvivesTransitionInputLock(context);
}

} // namespace fps::tests
