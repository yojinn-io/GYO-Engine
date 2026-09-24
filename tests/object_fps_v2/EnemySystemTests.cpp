#include <doctest/doctest.h>

#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemySystem.hpp"
#include "RetroFPS/Gameplay/Player/Player.hpp"
#include "RetroFPS/Gameplay/Player/PlayerController.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace fps;

GridMap Room() {
    auto result = GridMapLoader::Parse("############\n"
                                       "#..........#\n"
                                       "#..........#\n"
                                       "#..........#\n"
                                       "#..........#\n"
                                       "#.....P....#\n"
                                       "#..........#\n"
                                       "#..........#\n"
                                       "#........D.#\n"
                                       "############\n");
    if (!result)
        throw std::runtime_error(result.error);
    return std::move(*result.map);
}

EnemyDefinition Definition(EnemyKind kind = EnemyKind::Melee) {
    using namespace Engine::Model;
    auto model = std::make_shared<ModelAsset>();
    model->nodes = {{"body", std::nullopt, {}}, {"hand", 0, {{0, 1, 0.1f}, {}, {1, 1, 1}}}};
    NodeTrack punch;
    punch.nodeIndex = 1;
    punch.translations = {{0, {0, 1, 0.1f}}, {0.2, {0, 1, 0.5f}}, {0.4, {0, 1, 0.1f}}};
    NodeTrack run;
    run.nodeIndex = 1;
    run.translations = {{0, {0, 1, 0.1f}}, {0.25, {0, 1, 0.25f}}, {0.5, {0, 1, 0.1f}}};
    model->clips = {
        {"idle", 1.0, {}}, {"move", 0.5, {run}}, {"attack", 0.5, {punch}}, {"dead", 0.4, {}}};
    REQUIRE(ValidateModel(*model));
    auto rig = std::make_shared<EnemyRig>();
    rig->model = model;
    rig->clips = {0, 1, 2, 3};
    rig->transitionSeconds = 0.05f;
    rig->hurtRegions = {{"torso", {0, {0, 0.3f, 0}}, {0, {0, 1.3f, 0}}, 0.15f},
                        {"hand", {1, {}}, {1, {}}, 0.08f}};
    rig->attackPoint = {1, {}};
    rig->attackRadius = 0.08f;
    rig->attackBeginSeconds = 0.1;
    rig->attackEndSeconds = 0.3;
    rig->releaseSeconds = 0.2;
    return {"synthetic",
            kind,
            7,
            kind == EnemyKind::Melee ? 0.90f : 1.25f,
            10,
            3,
            0.2f,
            1.6f,
            Engine::Asset::AssetId::FromString("test.character"),
            rig};
}

EnemyDefinition DamageDefinition(EnemyKind kind = EnemyKind::Melee) {
    auto definition = Definition(kind);
    definition.maxHealth = 100;
    definition.defense = 5;
    auto rig = std::make_shared<EnemyRig>(*definition.rig);
    rig->hurtRegions = {{"head", {0, {0, 1.4f, 0}}, {0, {0, 1.5f, 0}}, 0.12f, 2.0f},
                        {"torso", {0, {0, 0.3f, 0}}, {0, {0, 1.3f, 0}}, 0.15f, 1.0f},
                        {"arm", {1, {}}, {1, {}}, 0.08f, 0.75f}};
    definition.rig = rig;
    return definition;
}

void Initialize(EnemySystem &enemies, const GridMap &map) {
    std::string error;
    REQUIRE_MESSAGE(enemies.Initialize(map, map.GetSpawnPosition(), 0.25f, 1, {}, error), error);
}
EnemyId Spawn(EnemySystem &enemies, const GridMap &map, Float2 position,
              const EnemyDefinition &definition) {
    std::string error;
    const auto spawned =
        enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, position, definition, error);
    REQUIRE_MESSAGE(spawned.Spawned(), error);
    return spawned.enemyId;
}
EnemyTarget PlayerTarget(const GridMap &map) { return {map.GetSpawnPosition(), 0.25f, 1.8f, 0}; }
} // namespace

TEST_CASE("v2 melee hand window sweeps crossed poses once and death cancels queued attacks") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto id = Spawn(enemies, map, {6.5f, 4.95f}, Definition());
    enemies.Update(map, PlayerTarget(map), 0.05f);
    CHECK(enemies.GetAttackEvents().empty());
    CHECK(enemies.GetSnapshots()[0].state == EnemyState::Attacking);
    enemies.Update(map, PlayerTarget(map), 0.15f);
    REQUIRE(enemies.GetAttackEvents().size() == 1);
    CHECK(enemies.GetAttackEvents()[0].enemyId == id);
    CHECK(enemies.GetAttackEvents()[0].damage == 7);
    REQUIRE(enemies.GetSnapshots()[0].attackShape);
    CHECK(enemies.GetSnapshots()[0].attackShape->radius == doctest::Approx(0.08f));
    enemies.Update(map, PlayerTarget(map), 0.25f);
    CHECK(enemies.GetAttackEvents().empty());
    CHECK_FALSE(enemies.GetSnapshots()[0].attackShape);

    // A second instance demonstrates death between simulation and consumption.
    enemies.Reset();
    Initialize(enemies, map);
    auto lethalDefinition = DamageDefinition();
    lethalDefinition.maxHealth = 10;
    const auto second = Spawn(enemies, map, {6.5f, 4.95f}, lethalDefinition);
    enemies.Update(map, PlayerTarget(map), 0.4f); // crosses the entire active window
    REQUIRE(enemies.GetAttackEvents().size() == 1);
    const auto damage = enemies.ApplyDamage(second, 8, "head");
    REQUIRE(damage.killed);
    CHECK(enemies.GetAttackEvents().empty());
    CHECK(enemies.CollectAliveBodies().empty());
    REQUIRE(enemies.GetSnapshots().size() == 1);
    CHECK(enemies.GetSnapshots()[0].hurtboxes.empty());
    CHECK_FALSE(enemies.GetSnapshots()[0].attackShape);
    CHECK(enemies.GetSnapshots()[0].state == EnemyState::Dead);
    CHECK_FALSE(enemies.RetireDead(second));
    enemies.Update(map, PlayerTarget(map), 0.4f);
    CHECK(enemies.RetireDead(second));
    CHECK(enemies.GetSnapshots().empty());
}

TEST_CASE("v2 melee hit follows animated fist instead of the planar attack range") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    Spawn(enemies, map, {6.5f, 4.95f}, Definition());
    enemies.Update(map, PlayerTarget(map), 0.05f);
    auto airborne = PlayerTarget(map);
    airborne.feetY = 2.0f;
    enemies.Update(map, airborne, 0.4f);
    CHECK(enemies.GetAttackEvents().empty());

    enemies.Reset();
    Initialize(enemies, map);
    auto miss = Definition();
    auto rig = std::make_shared<EnemyRig>(*miss.rig);
    rig->attackPoint.offset.x = 2.0f;
    miss.rig = rig;
    Spawn(enemies, map, {6.5f, 4.95f}, miss);
    enemies.Update(map, PlayerTarget(map), 0.4f);
    CHECK(enemies.GetSnapshots()[0].state == EnemyState::Attacking);
    CHECK(enemies.GetAttackEvents().empty());
}

TEST_CASE("v2 attack cadence follows authored cooldown rather than animation length") {
    const auto map = Room();
    for (const auto kind : {EnemyKind::Melee, EnemyKind::Ranged}) {
        EnemySystem enemies;
        Initialize(enemies, map);
        const auto definition = Definition(kind);
        Spawn(enemies, map, kind == EnemyKind::Melee ? Float2{6.5f, 4.95f} : Float2{6.5f, 2},
              definition);
        std::vector<float> times;
        for (int frame = 0; frame < 360; ++frame) {
            enemies.Update(map, PlayerTarget(map), 1.0f / 120);
            if (!enemies.GetAttackEvents().empty())
                times.push_back((frame + 1) / 120.0f);
        }
        REQUIRE(times.size() >= 2);
        for (std::size_t index = 1; index < times.size(); ++index)
            CHECK(times[index] - times[index - 1] ==
                  doctest::Approx(definition.attackIntervalSeconds).epsilon(0.01));
    }
}

TEST_CASE("v2 melee hand cannot emerge through a wall during an existing attack") {
    const auto map = Room();
    auto blockedResult = GridMapLoader::Parse("############\n"
                                              "#..........#\n"
                                              "#..........#\n"
                                              "#..........#\n"
                                              "#.....#....#\n"
                                              "#.....P....#\n"
                                              "#..........#\n"
                                              "#..........#\n"
                                              "#........D.#\n"
                                              "############\n");
    REQUIRE(blockedResult);
    EnemySystem enemies;
    EnemySettings settings;
    settings.meleeAttackSurfaceDistance = 2;
    std::string error;
    REQUIRE(enemies.Initialize(map, map.GetSpawnPosition(), 0.25f, 1, settings, error));
    auto definition = Definition();
    auto rig = std::make_shared<EnemyRig>(*definition.rig);
    rig->attackPoint.offset.z = 1.5f;
    definition.rig = rig;
    Spawn(enemies, map, {6.5f, 3.6f}, definition);
    enemies.Update(map, PlayerTarget(map), 0.05f);
    REQUIRE(enemies.GetSnapshots()[0].state == EnemyState::Attacking);
    enemies.Update(*blockedResult.map, PlayerTarget(map), 0.35f);
    CHECK(enemies.GetAttackEvents().empty());
}

TEST_CASE("v2 ranged release samples exact event pose while snapshot uses current pose") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto definition = Definition(EnemyKind::Ranged);
    Spawn(enemies, map, {6.5f, 2.0f}, definition);
    enemies.Update(map, PlayerTarget(map), 0.4f);
    REQUIRE(enemies.GetAttackEvents().size() == 1);
    const auto &event = enemies.GetAttackEvents()[0];
    CHECK(event.origin.x == doctest::Approx(6.5f));
    CHECK(event.origin.y == doctest::Approx(1.0f));
    CHECK(event.origin.z == doctest::Approx(2.5f));
    const auto &snapshot = enemies.GetSnapshots()[0];
    const auto hand =
        EnemyBoneWorldPoint(*definition.rig, snapshot.pose, definition.rig->attackPoint,
                            snapshot.position, snapshot.yawRadians);
    CHECK(hand.z == doctest::Approx(2.1f));
    enemies.Update(map, PlayerTarget(map), 0.1f);
    CHECK(enemies.GetAttackEvents().empty());
}

TEST_CASE("v2 snapshots own poses and bone hurtboxes share that pose and model transform") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto definition = Definition();
    Spawn(enemies, map, {6.5f, 4.95f}, definition);
    Spawn(enemies, map, {8.5f, 3.5f}, definition);
    enemies.Update(map, PlayerTarget(map), 0.2f);
    const auto frozen = enemies.GetSnapshots()[0];
    REQUIRE(frozen.hurtboxes.size() == 2);
    const auto hand = EnemyBoneWorldPoint(*definition.rig, frozen.pose, definition.rig->attackPoint,
                                          frozen.position, frozen.yawRadians);
    CHECK(frozen.hurtboxes[1].shape.segmentStart.x == doctest::Approx(hand.x));
    CHECK(frozen.hurtboxes[1].shape.segmentStart.y == doctest::Approx(hand.y));
    CHECK(frozen.hurtboxes[1].shape.segmentStart.z == doctest::Approx(hand.z));
    CHECK(enemies.GetSnapshots()[0].pose.globalTransforms.data() !=
          enemies.GetSnapshots()[1].pose.globalTransforms.data());
    enemies.Update(map, PlayerTarget(map), 0.2f);
    CHECK(frozen.pose.globalTransforms[1].values[14] == doctest::Approx(0.5f));
    CHECK(enemies.GetSnapshots()[0].pose.globalTransforms[1].values[14] == doctest::Approx(0.1f));
    CHECK(frozen.body.height == doctest::Approx(1.6f));
    CHECK(enemies.GetSnapshots()[0].body.height == doctest::Approx(frozen.body.height));
}

TEST_CASE("v2 occupied corpses reserve spawn positions and reset discards old poses") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto definition = Definition();
    const auto id = Spawn(enemies, map, {3.5f, 3.5f}, definition);
    REQUIRE(enemies.Kill(id));
    std::string error;
    CHECK(
        enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, {3.5f, 3.5f}, definition, error).status ==
        EnemySpawnStatus::Blocked);
    enemies.Update(map, PlayerTarget(map), 0.4f);
    CHECK(enemies.RetireExpiredDead() == 1);
    CHECK(enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, {3.5f, 3.5f}, definition, error)
              .Spawned());
    enemies.Reset();
    Initialize(enemies, map);
    CHECK(Spawn(enemies, map, {3.5f, 3.5f}, definition) == 1);
    CHECK(enemies.GetSnapshots()[0].state == EnemyState::Idle);
    CHECK(enemies.GetSnapshots()[0].stateElapsedSeconds == 0);
    CHECK(enemies.GetAttackEvents().empty());
}

TEST_CASE("v2 enemies block each other and player under repeated locomotion") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto definition = Definition();
    Spawn(enemies, map, {6.5f, 3.0f}, definition);
    Spawn(enemies, map, {6.5f, 2.0f}, definition);
    for (int i = 0; i < 180; ++i) {
        enemies.Update(map, PlayerTarget(map), 1.0f / 60);
        const auto bodies = enemies.CollectAliveBodies();
        REQUIRE(bodies.size() == 2);
        const auto pair = Engine::Collision::OverlapVerticalCapsules(bodies[0], bodies[1]);
        CHECK_FALSE((pair && pair->penetrationDepth > 0.0001f));
        const Engine::Collision::VerticalCapsule player{{6.5f, 0, 5.5f}, 1.8f, 0.25f};
        for (const auto &body : bodies) {
            const auto overlap = Engine::Collision::OverlapVerticalCapsules(body, player);
            CHECK_FALSE((overlap && overlap->penetrationDepth > 0.0001f));
        }
    }
}

TEST_CASE("v2 invalid rig data fails at spawn with an actionable diagnostic") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    auto definition = Definition();
    auto rig = std::make_shared<EnemyRig>(*definition.rig);
    rig->attackPoint.node = 100;
    definition.rig = rig;
    std::string error;
    CHECK(
        enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, {3.5f, 3.5f}, definition, error).status ==
        EnemySpawnStatus::Invalid);
    CHECK_FALSE(error.empty());
    CHECK(enemies.GetSnapshots().empty());
}

TEST_CASE("v2 region damage multiplies weapon damage before defense and preserves direct damage") {
    const auto map = Room();
    struct DamageCase {
        std::string_view region;
        float expected;
    };
    for (const auto &test : {DamageCase{"head", 45}, DamageCase{"torso", 20},
                             DamageCase{"arm", 13.75f}, DamageCase{"", 20}}) {
        CAPTURE(test.region);
        EnemySystem enemies;
        Initialize(enemies, map);
        const auto id = Spawn(enemies, map, {3.5f, 3.5f}, DamageDefinition());
        const auto result = enemies.ApplyDamage(id, 25, test.region);
        REQUIRE(result.applied);
        CHECK_FALSE(result.killed);
        CHECK(result.rawDamage == 25);
        CHECK(result.appliedDamage == doctest::Approx(test.expected));
        CHECK(result.remainingHealth == doctest::Approx(100 - test.expected));
        CHECK(enemies.GetSnapshots()[0].health == doctest::Approx(result.remainingHealth));
        CHECK(enemies.GetSnapshots()[0].hitFlashRemainingSeconds == kEnemyHitFlashSeconds);
    }

    // Existing synthetic definitions omit the appended multiplier field.
    EnemySystem enemies;
    Initialize(enemies, map);
    auto legacy = Definition();
    legacy.maxHealth = 100;
    legacy.defense = 5;
    REQUIRE(legacy.rig->hurtRegions[1].damageMultiplier == 1);
    const auto id = Spawn(enemies, map, {3.5f, 3.5f}, legacy);
    CHECK(enemies.ApplyDamage(id, 25, "hand").appliedDamage == doctest::Approx(20));
    CHECK(enemies.ApplyDamage(id, 25).appliedDamage == doctest::Approx(20));
}

TEST_CASE("v2 region damage floors after defense and reports only remaining health on overkill") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto id = Spawn(enemies, map, {3.5f, 3.5f}, DamageDefinition());
    const auto minimum = enemies.ApplyDamage(id, 1, "arm");
    CHECK(minimum.applied);
    CHECK(minimum.appliedDamage == 1);
    CHECK(minimum.remainingHealth == 99);
    const auto overkill = enemies.ApplyDamage(id, 1000, "head");
    CHECK(overkill.applied);
    CHECK(overkill.killed);
    CHECK(overkill.rawDamage == 1000);
    CHECK(overkill.appliedDamage == 99);
    CHECK(overkill.remainingHealth == 0);
    CHECK(enemies.GetSnapshots()[0].state == EnemyState::Dead);
    CHECK(enemies.GetSnapshots()[0].hurtboxes.empty());
    const auto dead = enemies.ApplyDamage(id, 25, "head");
    CHECK_FALSE(dead.applied);
    CHECK_FALSE(dead.killed);
    CHECK(dead.rawDamage == 25);
    CHECK(dead.appliedDamage == 0);
}

TEST_CASE("v2 finite region damage products are safely clamped to health") {
    const auto map = Room();
    for (const auto multiplier : {1.5f, (std::numeric_limits<float>::max)()}) {
        CAPTURE(multiplier);
        EnemySystem enemies;
        Initialize(enemies, map);
        auto definition = DamageDefinition();
        definition.maxHealth = (std::numeric_limits<float>::max)();
        definition.defense = (std::numeric_limits<float>::max)();
        auto rig = std::make_shared<EnemyRig>(*definition.rig);
        rig->hurtRegions[0].damageMultiplier = multiplier;
        definition.rig = rig;
        const auto id = Spawn(enemies, map, {3.5f, 3.5f}, definition);
        const auto result = enemies.ApplyDamage(id, (std::numeric_limits<float>::max)(), "head");
        REQUIRE(result.applied);
        CHECK(std::isfinite(result.rawDamage));
        CHECK(std::isfinite(result.appliedDamage));
        if (multiplier == 1.5f) {
            // The product exceeds float range, but defense brings it back into range.
            CHECK_FALSE(result.killed);
            CHECK(result.appliedDamage / definition.maxHealth == doctest::Approx(0.5f));
            CHECK(result.remainingHealth / definition.maxHealth == doctest::Approx(0.5f));
        } else {
            CHECK(result.killed);
            CHECK(result.appliedDamage == definition.maxHealth);
            CHECK(result.remainingHealth == 0);
        }
    }
}

TEST_CASE("v2 rejected damage does not change health pose flash or pending attacks") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    const auto id = Spawn(enemies, map, {6.5f, 2.0f}, DamageDefinition(EnemyKind::Ranged));
    enemies.Update(map, PlayerTarget(map), 0.4f);
    REQUIRE(enemies.GetAttackEvents().size() == 1);
    const auto before = enemies.GetSnapshots()[0];
    for (const auto raw : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(raw);
        const auto result = enemies.ApplyDamage(id, raw, "head");
        CHECK_FALSE(result.applied);
        CHECK_FALSE(result.killed);
        CHECK(result.appliedDamage == 0);
    }
    CHECK_FALSE(enemies.ApplyDamage(id, 25, "missing").applied);
    CHECK_FALSE(enemies.ApplyDamage(id + 1, 25, "head").applied);
    const auto &after = enemies.GetSnapshots()[0];
    CHECK(after.health == before.health);
    CHECK(after.hitFlashRemainingSeconds == before.hitFlashRemainingSeconds);
    CHECK(after.stateElapsedSeconds == before.stateElapsedSeconds);
    CHECK(after.pose.globalTransforms[1].values == before.pose.globalTransforms[1].values);
    CHECK(enemies.GetAttackEvents().size() == 1);
}

TEST_CASE("v2 lethal region damage cancels ranged releases before and after the event") {
    const auto map = Room();
    for (const auto time : {0.05f, 0.4f}) {
        CAPTURE(time);
        EnemySystem enemies;
        Initialize(enemies, map);
        const auto id = Spawn(enemies, map, {6.5f, 2.0f}, DamageDefinition(EnemyKind::Ranged));
        enemies.Update(map, PlayerTarget(map), time);
        REQUIRE(enemies.GetSnapshots()[0].state == EnemyState::Attacking);
        CHECK(enemies.GetAttackEvents().size() == (time < 0.2f ? 0 : 1));
        REQUIRE(enemies.ApplyDamage(id, 60, "head").killed);
        CHECK(enemies.GetAttackEvents().empty());
        CHECK(enemies.GetSnapshots()[0].hurtboxes.empty());
        enemies.Update(map, PlayerTarget(map), 0.4f);
        CHECK(enemies.GetAttackEvents().empty());
    }
}

TEST_CASE("v2 invalid region multipliers and duplicate identifiers report enemy and region") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    for (const auto multiplier : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(multiplier);
        auto definition = DamageDefinition();
        auto rig = std::make_shared<EnemyRig>(*definition.rig);
        rig->hurtRegions[0].damageMultiplier = multiplier;
        definition.rig = rig;
        std::string error;
        const auto result =
            enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, {3.5f, 3.5f}, definition, error);
        CHECK(result.status == EnemySpawnStatus::Invalid);
        CHECK(error.find(definition.id) != std::string::npos);
        CHECK(error.find("head") != std::string::npos);
        CHECK(enemies.GetSnapshots().empty());
    }
    auto duplicate = DamageDefinition();
    auto rig = std::make_shared<EnemyRig>(*duplicate.rig);
    rig->hurtRegions.push_back(rig->hurtRegions.front());
    duplicate.rig = rig;
    std::string error;
    const auto result =
        enemies.Spawn(map, map.GetSpawnPosition(), 0.25f, {3.5f, 3.5f}, duplicate, error);
    CHECK(result.status == EnemySpawnStatus::Invalid);
    CHECK(error.find(duplicate.id) != std::string::npos);
    CHECK(error.find("head") != std::string::npos);
    CHECK(enemies.GetSnapshots().empty());
}

TEST_CASE("v2 character adapter sweeps and slides real bodies past rounded corners") {
    const std::vector<Engine::Collision::Aabb> walls{{{0, 0, 0}, {1, 3, 1}}};
    const auto moved = MoveCharacterBody({{-1, 0, 0.25f}, 1.8f, 0.25f}, {2, 0, 0.5f}, walls, {});
    CHECK(moved.x <= -0.249f);
    CHECK(moved.z > 0.70f);
    const auto corner =
        MoveCharacterBody({{-0.2f, 0, -0.2f}, 1.8f, 0.25f}, {0, 0, -0.1f}, walls, {});
    CHECK(corner.x == doctest::Approx(-0.2f));
    CHECK(corner.z == doctest::Approx(-0.3f));

    const std::vector<Engine::Collision::VerticalCapsule> actors{{{0, 0, 0}, 1.6f, 0.25f}};
    const auto above = MoveCharacterBody({{-1, 2, 0}, 1.8f, 0.25f}, {2, 0, 0}, {}, actors);
    CHECK(above.x == doctest::Approx(1.0f));
    const auto landed = MoveCharacterBody({{0, 2, 0}, 1.8f, 0.25f}, {0, -3, 0}, {}, actors);
    CHECK(landed.y == doctest::Approx(1.6f).epsilon(0.001));
    CHECK(CanPlaceCharacterBody({{landed.x, landed.y, landed.z}, 1.8f, 0.25f}, {}, actors));
}

TEST_CASE("v2 floor constrained enemy never discards vertical slide into a raised player") {
    const std::vector<Engine::Collision::VerticalCapsule> actors{{{0, 1.5f, 0}, 1.8f, 0.25f}};
    const Engine::Collision::VerticalCapsule initial{{-1, 0, 0}, 1.6f, 0.25f};
    // The unconstrained solver may slide below the actor; discarding that Y
    // afterwards used to place the enemy's top cap back inside the actor.
    const auto moved = MoveCharacterBody(initial, {2, 0, 0}, {}, actors, true);
    CHECK(moved.y == 0);
    CHECK(moved.x <= -0.299f);
    CHECK(CanPlaceCharacterBody({{moved.x, moved.y, moved.z}, 1.6f, 0.25f}, {}, actors));
    const auto sideways = MoveCharacterBody(initial, {2, 0, 0.2f}, {}, actors, true);
    CHECK(sideways.y == 0);
    CHECK(CanPlaceCharacterBody({{sideways.x, sideways.y, sideways.z}, 1.6f, 0.25f}, {}, actors));
    // A pure vertical initial contact still needs horizontal depenetration.
    const auto overlap = MoveCharacterBody({{0, 0, 0}, 1.6f, 0.25f}, {}, {}, actors, true);
    CHECK(overlap.y == 0);
    CHECK(CanPlaceCharacterBody({{overlap.x, overlap.y, overlap.z}, 1.6f, 0.25f}, {}, actors));
}

TEST_CASE("v2 open-edge maps preserve solid world boundaries and expose their actual boxes") {
    auto map = GridMapLoader::Parse("P.D\n...\n");
    REQUIRE(map);
    const auto walls = BuildWorldCollisionBoxes(*map.map, {});
    CHECK(walls.size() == 4);
    const auto left = MoveCharacterBody({{0.5f, 0, 0.5f}, 1.8f, 0.25f}, {-20, 0, 0}, walls, {});
    CHECK(left.x == doctest::Approx(0.25f).epsilon(0.001));
    const auto right =
        MoveCharacterBody({{1.5f, 0, 0.5f}, 1.6f, 0.2f}, {20, 0, 0}, walls, {}, true);
    CHECK(right.x == doctest::Approx(2.8f).epsilon(0.001));
    const auto back =
        MoveCharacterBody({{1.5f, 0, 0.5f}, 1.6f, 0.2f}, {0, 0, -20}, walls, {}, true);
    CHECK(back.z == doctest::Approx(0.2f).epsilon(0.001));
    CHECK(CanPlaceCharacterBody({{right.x, right.y, right.z}, 1.6f, 0.2f}, walls, {}));
}

TEST_CASE("v2 combat chooses a single nearest bone region including limbs outside the body") {
    const auto map = Room();
    const std::vector<CombatTarget> targets{{7, {{5, 0.4f, 4}, {5, 1.4f, 4}, 0.2f}, "torso"},
                                            {7, {{4.1f, 1, 3}, {4.1f, 1, 4}, 0.1f}, "arm"},
                                            {8, {{4.1f, 0.4f, 5}, {4.1f, 1.4f, 5}, 0.2f}, "torso"}};
    const auto limb = CombatCollision::Raycast(map, {}, {4.1f, 1, 2}, {0, 0, 1}, 6, targets);
    REQUIRE(limb);
    CHECK(limb->kind == CombatHitKind::Target);
    CHECK(limb->targetId == 7);
    CHECK(limb->region == "arm");
    CHECK(limb->distance == doctest::Approx(0.9f));
    const std::vector<CombatTarget> overlap{{7, {{5, 0.4f, 4}, {5, 1.4f, 4}, 0.2f}, "torso"},
                                            {7, {{5, 1, 3.9f}, {5, 1, 4.2f}, 0.2f}, "arm"}};
    const auto one = CombatCollision::Raycast(map, {}, {5, 1, 2}, {0, 0, 1}, 6, overlap);
    REQUIRE(one);
    CHECK(one->targetId == 7);
    CHECK(one->region == "arm");
    const auto wall = CombatCollision::Raycast(map, {}, {5, 1, 0.5f}, {0, 0, 1}, 6, overlap);
    REQUIRE(wall);
    CHECK(wall->kind == CombatHitKind::Wall);
}

TEST_CASE("v2 overlapping hurt regions apply only the nearest region damage once") {
    const auto map = Room();
    EnemySystem enemies;
    Initialize(enemies, map);
    auto definition = DamageDefinition();
    auto rig = std::make_shared<EnemyRig>(*definition.rig);
    rig->hurtRegions[0].start = rig->hurtRegions[1].start;
    rig->hurtRegions[0].end = rig->hurtRegions[1].end;
    rig->hurtRegions[0].radius = 0.25f;
    definition.rig = rig;
    const auto id = Spawn(enemies, map, {5, 4}, definition);
    std::vector<CombatTarget> targets;
    for (const auto &region : enemies.GetSnapshots()[0].hurtboxes)
        targets.push_back({id, region.shape, region.region});
    const auto hit = CombatCollision::Raycast(map, {}, {5, 1, 2}, {0, 0, 1}, 6, targets);
    REQUIRE(hit);
    REQUIRE(hit->kind == CombatHitKind::Target);
    CHECK(hit->region == "head");
    const auto damage = enemies.ApplyDamage(hit->targetId, 25, hit->region);
    CHECK(damage.appliedDamage == 45);
    CHECK(enemies.GetSnapshots()[0].health == 55);
}

TEST_CASE("v2 player lands on actors and resumes falling after walking off") {
    const auto map = Room();
    PlayerController controller;
    PlayerSettings settings;
    settings.jumpHeight = 2.5f;
    std::string error;
    REQUIRE(controller.Configure(settings, error));
    Player player;
    REQUIRE(controller.Initialize(player, map, {}, error));
    PlayerControlInput jump;
    jump.jumpPressed = true;
    controller.Update(player, jump, 0.3f, map, {});
    REQUIRE(player.GetFeetY() > 1.6f);
    const std::vector<Engine::Collision::VerticalCapsule> actors{{{6.5f, 0, 5.5f}, 1.6f, 0.25f}};
    for (int frame = 0; frame < 200 && !player.IsGrounded(); ++frame)
        controller.Update(player, {}, 0.01f, map, {}, actors);
    REQUIRE(player.IsGrounded());
    REQUIRE(player.GetFeetY() == doctest::Approx(1.6f).epsilon(0.001));
    controller.Update(player, {}, 0.1f, map, {}, actors);
    CHECK(player.IsGrounded());
    CHECK(player.GetFeetY() == doctest::Approx(1.6f).epsilon(0.001));

    PlayerControlInput walk;
    walk.moveRight = 1;
    controller.Update(player, walk, 0.3f, map, {}, actors);
    CHECK_FALSE(player.IsGrounded());
    controller.Update(player, {}, 0.2f, map, {}, actors);
    CHECK(player.GetFeetY() < 1.5f);
    CHECK(player.GetVerticalVelocity() < 0);
    for (int frame = 0; frame < 100 && !player.IsGrounded(); ++frame)
        controller.Update(player, {}, 0.01f, map, {}, actors);
    CHECK(player.IsGrounded());
    CHECK(player.GetFeetY() == doctest::Approx(0.0f));
}

TEST_CASE("v2 player ceiling contact stops ascent without establishing ground or another jump") {
    const auto map = Room();
    PlayerController controller;
    PlayerSettings settings;
    settings.jumpHeight = 2.5f;
    std::string error;
    REQUIRE(controller.Configure(settings, error));
    Player player;
    REQUIRE(controller.Initialize(player, map, {}, error));
    const std::vector<Engine::Collision::VerticalCapsule> ceiling{
        {{6.5f, 2.2f, 5.5f}, 1.6f, 0.25f}};
    PlayerControlInput jump;
    jump.jumpPressed = true;
    controller.Update(player, jump, 0.1f, map, {}, ceiling);
    CHECK(player.GetFeetY() == doctest::Approx(0.4f).epsilon(0.001));
    CHECK(player.GetVerticalVelocity() == doctest::Approx(0.0f));
    CHECK_FALSE(player.IsGrounded());
    controller.Update(player, jump, 0.1f, map, {}, ceiling);
    CHECK(player.GetFeetY() < 0.35f);
    CHECK(player.GetVerticalVelocity() < 0);
    CHECK_FALSE(player.IsGrounded());
}
