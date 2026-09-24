#include <doctest/doctest.h>

#include "TestAssets.hpp"
#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <string_view>
#include <variant>

namespace {
using namespace fps;

constexpr std::string_view kOpenMap =
    "#########\n"
    "#...P...#\n"
    "#.......#\n"
    "#.......#\n"
    "#.......#\n"
    "#.......#\n"
    "#...M...#\n"
    "#.....D.#\n"
    "#########\n";
constexpr float kFov = std::numbers::pi_v<float> / 3;

Float3 Minus(const Float3 a, const Float3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
float Length(const Float3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
Float3 Center(const EnemyHurtbox& region) {
    const auto a=region.shape.segmentStart, b=region.shape.segmentEnd;
    return {(a.x+b.x)*0.5F, (a.y+b.y)*0.5F, (a.z+b.z)*0.5F};
}
const EnemyHurtbox& Region(const EnemySnapshot& enemy, std::string_view id) {
    const auto found=std::ranges::find(enemy.hurtboxes,id,&EnemyHurtbox::region);
    REQUIRE(found!=enemy.hurtboxes.end());
    return *found;
}

std::shared_ptr<const CampaignContent> Content(std::string_view mapText,
                                             const WeaponShotGeometry geometry) {
    // Keep ownership local: use this product's deployed skeletal definitions,
    // then compose a one-enemy room and an explicitly calibrated test weapon.
    auto data=GameDataLoader::Parse(
        "enemy_id,kind,damage,attack_interval_seconds,hp,defense,hitbox_radius,hitbox_height,presentation_asset_id\n"
        "melee_basic,melee,15,0.9,50,5,0.2,1.6,test.enemy.melee\n"
        "ranged_basic,ranged,10,1.25,40,0,0.2,1.6,test.enemy.ranged\n",
        "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,draw_seconds,hide_seconds,presentation_asset_id\n"
        "starter_pistol,25,12,48,0,false,0.3,1,0.001,0.001,test.weapon\n",
        "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
        "test_room,Test room,test.map,,0,1,1,1\n");
    REQUIRE_MESSAGE(data,data.error);
    data.catalog->enemies=tests::ProductionApplication().Content()->Data().enemies;
    auto map=GridMapLoader::Parse(mapText);
    REQUIRE_MESSAGE(map,map.error);
    auto content=CampaignContent::Build(std::move(*data.catalog),{std::move(*map.map)},
                                       {{"starter_pistol",geometry}});
    REQUIRE_MESSAGE(content,content.error);
    return std::make_shared<CampaignContent>(std::move(*content.content));
}

GameSession Start(const std::shared_ptr<const CampaignContent>& content) {
    GameSessionConfig config;
    config.fadeInSeconds=config.fadeOutSeconds=0.001F;
    config.enemies.meleeSpeed=0.0001F;
    config.enemies.rangedSpeed=0.0001F;
    GameSession session;
    std::string error;
    REQUIRE_MESSAGE(session.Initialize(content,config,error),error);
    const std::array<GameSessionCommand,1> start{StartCampaignCommand{}};
    REQUIRE_MESSAGE(session.Advance(0.01F,{},start,error),error);
    for(int i=0;i<5;++i) REQUIRE_MESSAGE(session.Advance(0.01F,{},{},error),error);
    REQUIRE(session.Snapshot().screen==GameScreen::Playing);
    REQUIRE(session.Snapshot().enemies.size()==1);
    REQUIRE(session.Snapshot().player);
    return session;
}

struct Aim final {
    Float3 origin, forward, right, up;
    float yaw{},pitch{};
};
Aim AimAt(const PlayerSnapshot& player, const Float3 target) {
    Aim aim;
    aim.origin={player.position.x,player.feetY+player.eyeHeight,player.position.z};
    const auto delta=Minus(target,aim.origin);
    aim.yaw=std::atan2(delta.x,delta.z);
    aim.pitch=-std::atan2(delta.y,std::hypot(delta.x,delta.z));
    const float sy=std::sin(aim.yaw), cy=std::cos(aim.yaw);
    const float sp=std::sin(aim.pitch), cp=std::cos(aim.pitch);
    aim.forward={sy*cp,-sp,cy*cp};
    aim.right={cy,0,-sy};
    aim.up={sy*sp,cp,cy*sp};
    return aim;
}
WeaponShotGeometry MuzzleAt(const Aim& aim, const Float3 worldPoint) {
    const auto offset=Minus(worldPoint,aim.origin);
    const auto dot=[&](Float3 basis) {
        return offset.x*basis.x+offset.y*basis.y+offset.z*basis.z;
    };
    return {{dot(aim.right),dot(aim.up),dot(aim.forward)},kFov};
}
std::vector<CombatTarget> Targets(const EnemySnapshot& enemy) {
    std::vector<CombatTarget> targets;
    for(const auto& region:enemy.hurtboxes) targets.push_back({enemy.id,region.shape,region.region});
    return targets;
}
void Fire(GameSession& session, const Aim& aim) {
    const auto before=session.Snapshot();
    GameFrameInput input;
    input.lookEnabled=true;
    input.lookDeltaX=(aim.yaw-before.player->yawRadians)/PlayerSettings{}.mouseSensitivity;
    input.lookDeltaY=(aim.pitch-before.player->pitchRadians)/PlayerSettings{}.mouseSensitivity;
    input.fireHeld=input.firePressed=true;
    std::string error;
    // Zero delta preserves the sampled bones while exercising the real input,
    // camera aim, muzzle occlusion, weapon event, and damage pipeline.
    REQUIRE_MESSAGE(session.Advance(0,input,{},error),error);
    CHECK(std::count_if(session.Events().begin(),session.Events().end(),[](const auto& event) {
        return std::holds_alternative<ShotEvent>(event.payload);
    })==1);
    CHECK(session.Snapshot().weapon.magazineAmmo+1==before.weapon.magazineAmmo);
}
} // namespace

TEST_CASE("v2 session applies final muzzle region rather than the camera head region once") {
    auto probe=Start(Content(kOpenMap,{{0,0,0.35F},kFov}));
    const auto& initial=probe.Snapshot();
    const auto& enemy=initial.enemies.front();
    const auto targets=Targets(enemy);
    const auto aim=AimAt(*initial.player,Center(Region(enemy,"head")));
    const auto parsed=GridMapLoader::Parse(kOpenMap);
    REQUIRE(parsed);
    const auto cameraHit=CombatCollision::Raycast(*parsed.map,{},aim.origin,aim.forward,50,targets);
    REQUIRE(cameraHit);
    REQUIRE(cameraHit->kind==CombatHitKind::Target);
    REQUIRE(cameraHit->region=="head");

    // Deliberately exaggerate authored muzzle parallax to put its origin at
    // the shared torso/pelvis joint. Both volumes overlap here, while the
    // camera still sees the head. This makes a second deduction observable.
    const auto torsoStart=Region(enemy,"torso").shape.segmentStart;
    const Float3 muzzle{torsoStart.x,torsoStart.y,torsoStart.z};
    const auto direction=Minus(cameraHit->position,muzzle);
    const auto resolved=CombatCollision::Raycast(*parsed.map,{},muzzle,direction,Length(direction),targets);
    REQUIRE(resolved);
    REQUIRE(resolved->kind==CombatHitKind::Target);
    REQUIRE(resolved->region!="head");
    REQUIRE(resolved->region=="torso");
    // More than one region lies on this shot: only the nearest may deduct HP.
    CHECK(std::count_if(targets.begin(),targets.end(),[&](const auto& target) {
        return Engine::Collision::RaycastCapsule({muzzle.x,muzzle.y,muzzle.z},
            {direction.x,direction.y,direction.z},Length(direction),target.capsule).has_value();
    })>=2);
    auto session=Start(Content(kOpenMap,MuzzleAt(aim,muzzle)));
    const float before=session.Snapshot().enemies.front().health;
    Fire(session,aim);
    REQUIRE(session.Snapshot().enemies.size()==1);
    CHECK(session.Snapshot().enemies.front().health==doctest::Approx(before-20));
    CHECK(session.Snapshot().enemies.front().hitFlashRemainingSeconds>0);
}

TEST_CASE("v2 session head hit uses its multiplier once across the full weapon input path") {
    auto session=Start(Content(kOpenMap,{{0,0,0.35F},kFov}));
    const auto before=session.Snapshot();
    const auto aim=AimAt(*before.player,Center(Region(before.enemies.front(),"head")));
    Fire(session,aim);
    REQUIRE(session.Snapshot().enemies.size()==1);
    CHECK(session.Snapshot().enemies.front().health==doctest::Approx(before.enemies.front().health-45));
}

TEST_CASE("v2 session world obstruction prevents damage even when the crosshair sees a head") {
    std::string map{kOpenMap};
    // A side wall blocks the offset barrel path; the camera's central ray stays clear.
    map[4*10+5]='#';
    auto probe=Start(Content(map,{{0,0,0.35F},kFov}));
    const auto before=probe.Snapshot();
    const auto aim=AimAt(*before.player,Center(Region(before.enemies.front(),"head")));
    const auto parsed=GridMapLoader::Parse(map);
    REQUIRE(parsed);
    const auto targets=Targets(before.enemies.front());
    const auto cameraHit=CombatCollision::Raycast(*parsed.map,{},aim.origin,aim.forward,50,targets);
    REQUIRE(cameraHit);
    REQUIRE(cameraHit->kind==CombatHitKind::Target);
    REQUIRE(cameraHit->region=="head");
    const Float3 muzzle{6.5F,1.2F,2.5F};
    const auto direction=Minus(cameraHit->position,muzzle);
    const auto resolved=CombatCollision::Raycast(*parsed.map,{},muzzle,direction,Length(direction),targets);
    REQUIRE(resolved);
    REQUIRE(resolved->kind==CombatHitKind::Wall);
    auto session=Start(Content(map,MuzzleAt(aim,muzzle)));
    Fire(session,aim);
    REQUIRE(session.Snapshot().enemies.size()==1);
    CHECK(session.Snapshot().enemies.front().health==before.enemies.front().health);
    CHECK(session.Snapshot().enemies.front().hitFlashRemainingSeconds==0);
}
