#include "../TestSupport.hpp"

#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <stdexcept>
#include <cmath>
#include <string>

namespace fps::tests {
namespace {

[[nodiscard]] GridMap ParseCombatMap(TestContext& context, const std::string& text) {
    MapLoadResult result = GridMapLoader::Parse(text);
    context.Expect(result.Succeeded(), "combat collision fixture parses");
    if (!result.map.has_value()) {
        throw std::runtime_error(result.error);
    }
    return std::move(*result.map);
}

void TestNearestWallAndTarget(TestContext& context) {
    const GridMap blocked = ParseCombatMap(context, "#####\n#P#D#\n#####");
    const CombatTarget target{42, {{3.5f, 1.5f}, 1.8f, 0.25f}};
    const std::optional<CombatHit> wallHit = CombatCollision::Raycast(
        blocked, {}, {1.5f, 1.0f, 1.5f}, {1.0f, 0.0f, 0.0f}, 10.0f, {&target, 1});
    context.Expect(
        wallHit.has_value() && wallHit->kind == CombatHitKind::Wall &&
            NearlyEqual(wallHit->distance, 0.5f),
        "wall occludes a capsule behind it");

    const GridMap open = ParseCombatMap(context, "P..D");
    const CombatTarget openTarget{7, {{2.5f, 0.5f}, 1.8f, 0.25f}};
    const std::optional<CombatHit> targetHit = CombatCollision::Raycast(
        open, {}, {0.5f, 1.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, 10.0f, {&openTarget, 1});
    context.Expect(
        targetHit.has_value() && targetHit->kind == CombatHitKind::Target &&
            targetHit->targetId == 7 && NearlyEqual(targetHit->distance, 1.75f),
        "open ray returns the nearest capsule target");
}

void TestFloorAndCapsuleSweep(TestContext& context) {
    const GridMap map = ParseCombatMap(context, "P.D");
    const std::optional<CombatHit> floorHit = CombatCollision::Raycast(
        map, {}, {0.5f, 1.0f, 0.5f}, {0.0f, -1.0f, 0.0f}, 5.0f);
    context.Expect(
        floorHit.has_value() && floorHit->kind == CombatHitKind::Floor &&
            NearlyEqual(floorHit->distance, 1.0f),
        "downward combat ray hits the floor");

    const VerticalCapsule capsule{{2.5f, 0.5f}, 1.8f, 0.25f};
    const std::optional<float> fraction = CombatCollision::SweepSegmentAgainstCapsule(
        {0.5f, 1.0f, 0.5f}, {4.5f, 1.0f, 0.5f}, 0.05f, capsule);
    context.Expect(
        fraction.has_value() && NearlyEqual(*fraction, 0.425f),
        "swept sphere expands the target capsule and reports normalized time");

    const std::optional<float> startingInside =
        CombatCollision::SweepSegmentAgainstCapsule(
            {2.5f, 1.0f, 0.5f}, {2.6f, 1.0f, 0.5f}, 0.05f, capsule);
    context.Expect(
        startingInside.has_value() && NearlyEqual(*startingInside, 0.0f),
        "capsule sweep reports an immediate hit when starting overlapped");
    context.ExpectThrows<std::invalid_argument>(
        [&capsule]() {
            static_cast<void>(CombatCollision::SweepSegmentAgainstCapsule(
                {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, -0.1f, capsule));
        },
        "zero-length capsule sweep rejects a negative radius");
}

void TestElevatedCapsuleQueries(TestContext& context) {
    const VerticalCapsule player{{2.5f, 0.5f}, 1.8f, 0.25f, 0.6f};
    context.Expect(!CombatCollision::SweepSegmentAgainstCapsule(
                       {0.5f, 0.25f, 0.5f}, {4.5f, 0.25f, 0.5f}, 0.05f, player),
                   "a low projectile passes beneath the airborne capsule");
    context.Expect(CombatCollision::SweepSegmentAgainstCapsule(
                       {0.5f, 1.2f, 0.5f}, {4.5f, 1.2f, 0.5f}, 0.05f, player).has_value(),
                   "a projectile at elevated body height still hits the jumping player");
    const auto top = CombatCollision::RaycastCapsule(
        {2.5f, 4.0f, 0.5f}, {0.0f, -1.0f, 0.0f}, 5.0f, player);
    context.Expect(top && NearlyEqual(*top, 1.6f),
                   "capsule top is feet Y plus full body height");
}

void TestMuzzleRetractionAndRealObstructions(TestContext& context) {
    const auto towards = [](const Float3 a, const Float3 b) {
        return Float3{b.x - a.x, b.y - a.y, b.z - a.z};
    };
    const auto distance = [](const Float3 a, const Float3 b) {
        return std::sqrt((b.x-a.x)*(b.x-a.x) + (b.y-a.y)*(b.y-a.y) + (b.z-a.z)*(b.z-a.z));
    };
    const GridMap corridor = ParseCombatMap(context,
        "####\n#P##\n#.##\n#D##\n####");
    const Float3 eye{1.75f, 1.6f, 1.5f};
    const Float3 desired{2.03f, 1.38f, 1.85f};
    const CombatTarget enemy{1, {{1.75f, 3.5f}, 1.8f, 0.2f}};
    const auto aim = CombatCollision::Raycast(corridor, {}, eye, {0, 0, 1}, 10, {&enemy, 1});
    context.Expect(aim && aim->kind == CombatHitKind::Target,
                   "reticle ray sees enemy while player legally hugs a side wall");
    if (!aim) return;
    const auto legacyHit = CombatCollision::Raycast(corridor, {}, desired,
        towards(desired, aim->position), distance(desired, aim->position), {&enemy, 1});
    context.Expect(legacyHit && legacyHit->kind == CombatHitKind::Wall && legacyHit->distance == 0.0f,
                   "legacy muzzle offset reproduces immediate absorption inside side wall");
    const Float3 muzzle = CombatCollision::ClampSegmentToWorld(corridor, {}, eye, desired);
    const auto corrected = CombatCollision::Raycast(corridor, {}, muzzle,
        towards(muzzle, aim->position), distance(muzzle, aim->position), {&enemy, 1});
    context.Expect(muzzle.x < 2.0f && corrected && corrected->kind == CombatHitKind::Target,
                   "retracted muzzle remains free and the clear shot reaches the enemy");

    const GridMap frontWall = ParseCombatMap(context, "P#D");
    const Float3 front = CombatCollision::ClampSegmentToWorld(
        frontWall, {}, {0.75f, 1.6f, 0.5f}, {1.10f, 1.38f, 0.5f});
    const CombatTarget behindWall{2, {{2.5f, 0.5f}, 1.8f, 0.2f}};
    const auto blocked = CombatCollision::Raycast(frontWall, {}, front, {1, 0, 0}, 5, {&behindWall, 1});
    context.Expect(front.x < 1.0f && blocked && blocked->kind == CombatHitKind::Wall && blocked->distance > 0,
                   "front wall still blocks the shot after muzzle retraction");

    const WorldSettings thinWorld{0.1f, 2.5f};
    const GridMap thinWall = ParseCombatMap(context, "P#...D");
    const Float3 beyond = CombatCollision::ClampSegmentToWorld(
        thinWall, thinWorld, {0.05f, 1.0f, 0.05f}, {0.35f, 0.9f, 0.05f});
    const auto thinHit = CombatCollision::Raycast(thinWall, thinWorld, beyond, {1, 0, 0}, 1);
    context.Expect(beyond.x < 0.1f && thinHit && thinHit->kind == CombatHitKind::Wall,
                   "camera-to-muzzle trace prevents spawning beyond an entire thin obstacle");

    const GridMap corner = ParseCombatMap(context, "P..D\n..#.\n....\n....");
    const Float3 cornerEye{1.8f, 1.6f, 0.5f};
    const Float3 cornerDesired{2.08f, 1.38f, 0.85f};
    const CombatTarget aroundCorner{3, {{1.8f, 3.5f}, 1.8f, 0.1f}};
    const auto clearAim = CombatCollision::Raycast(corner, {}, cornerEye, {0, 0, 1}, 10, {&aroundCorner, 1});
    const Float3 cornerMuzzle = CombatCollision::ClampSegmentToWorld(corner, {}, cornerEye, cornerDesired);
    context.Expect(clearAim && clearAim->kind == CombatHitKind::Target &&
                       NearlyEqual(cornerMuzzle.x, cornerDesired.x),
                   "clear camera-to-muzzle segment does not change the original offset");
    if (clearAim) {
        const auto cornerHit = CombatCollision::Raycast(corner, {}, cornerMuzzle,
            towards(cornerMuzzle, clearAim->position), distance(cornerMuzzle, clearAim->position), {&aroundCorner, 1});
        context.Expect(cornerHit && cornerHit->kind == CombatHitKind::Wall,
                       "second ray still blocks genuine muzzle-to-target corner obstruction");
    }

    const GridMap floor = ParseCombatMap(context, "P.D");
    const Float3 low = CombatCollision::ClampSegmentToWorld(
        floor, {}, {0.5f, 0.1f, 0.5f}, {0.6f, -0.1f, 0.7f});
    const auto floorHit = CombatCollision::Raycast(floor, {}, low, {0, -1, 0}, 1);
    context.Expect(low.y > 0 && floorHit && floorHit->kind == CombatHitKind::Floor,
                   "downward-pitched low muzzle retracts above floor while downward shots remain blocked");
    const Float3 airborneDesired{0.78f, 1.85f, 0.85f};
    const Float3 airborne = CombatCollision::ClampSegmentToWorld(
        floor, {}, {0.5f, 2.2f, 0.5f}, airborneDesired);
    context.Expect(NearlyEqual(airborne.x, airborneDesired.x) &&
                       NearlyEqual(airborne.y, airborneDesired.y) && NearlyEqual(airborne.z, airborneDesired.z),
                   "unobstructed jumping and pitched muzzle retains its original world-space position");
}

} // namespace

void RunCombatCollisionTests(TestContext& context) {
    TestMuzzleRetractionAndRealObstructions(context);
    TestElevatedCapsuleQueries(context);
    TestNearestWallAndTarget(context);
    TestFloorAndCapsuleSweep(context);
}

} // namespace fps::tests
