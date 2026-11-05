#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerHitbox.hpp"
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/ShotQuery.hpp"
#include "hit_aim.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "gyo/AppConfig.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// v8-product-hitbox-shot S2: the posed hit model on the product content.
namespace {
using namespace fps::pvp;
using Engine::Math::Vec3;

std::filesystem::path ContentRoot() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP test product directory");
    return base.value() / Gyo::AppConfig::Assets;
}

struct Product final {
    std::shared_ptr<const PlayerPresentationDefinition> definition;
    PlayerHitbox hitbox;
    std::shared_ptr<const PlayerHitModel> model;
};

const Product& LoadedProduct() {
    static const Product product = [] {
        std::string error;
        auto definition = LoadPlayerPoseDefinition(ContentRoot(), 1.8F, error);
        if (!definition) throw std::runtime_error(error);
        auto hitbox = LoadPlayerHitbox(ContentRoot(), error);
        if (!hitbox) throw std::runtime_error(error);
        return Product{definition, *hitbox, MakePosedPlayerHitModel(definition, *hitbox)};
    }();
    return product;
}

struct Target final {
    PlayerState state;
    CombatState combat;
    PlayerAnimation animation;
    std::uint64_t tick{600};
    Target(Vec3 position, float yaw, bool crouched = false) {
        state.playerId = 2;
        state.position = position;
        state.yaw = yaw;
        state.grounded = true;
        state.crouched = crouched;
        state.lifeGeneration = 1;
        state.lifeState = LifeState::Alive;
        state.lifeStateTick = 100;
        combat.playerId = 2;
        animation.playerId = 2;
        animation.lifeGeneration = 1;
        animation.jumpPhase = JumpPhase::Grounded;
        animation.jumpPhaseTick = 100;
    }
    [[nodiscard]] std::vector<PlayerWorldHitShape> Shapes() const {
        static const Arena arena{};
        std::vector<PlayerWorldHitShape> shapes;
        LoadedProduct().model->Shapes({arena, DefaultStanceRules, state, combat, animation, tick}, shapes);
        return shapes;
    }
    // The Client's pose for these authority values, by the Client's functions.
    [[nodiscard]] Engine::Model::Pose ClientPose() const {
        const auto& definition = *LoadedProduct().definition;
        const auto frame = PlayerFrameFromAuthority(state, combat, InterpolatePlayerAnimation(animation, animation, 0),
                                                    static_cast<double>(tick));
        std::string error;
        PlayerLocomotionState locomotion;
        REQUIRE(AdvancePlayerLocomotion(locomotion, frame, definition, error));
        PlayerActionPose actions;
        REQUIRE(ResolvePlayerActions(definition, locomotion, frame, actions, error));
        Engine::Model::Pose pose;
        REQUIRE(SamplePlayerBodyPose(definition, locomotion, actions, pose, error));
        return pose;
    }
    [[nodiscard]] Vec3 World(const Engine::Model::Pose& pose, std::size_t node, Vec3 local = {}) const {
        return PlayerBodyPointToWorld(*LoadedProduct().definition, state.position, state.yaw,
            Engine::Math::TransformPoint(pose.globalTransforms.at(node), local));
    }
    // A bone origin of the same pose.
    [[nodiscard]] Vec3 Bone(const std::string& name) const {
        const auto node = LoadedProduct().definition->character->model->FindNode(name);
        REQUIRE(node);
        return World(ClientPose(), *node);
    }
};

const Engine::Math::Capsule& Volume(const PlayerWorldHitShape& shape) {
    return std::get<Engine::Math::Capsule>(shape.volume);
}
} // namespace

namespace {
Vec3 Forward(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
Vec3 Right(float yaw) { return {std::cos(yaw), 0, -std::sin(yaw)}; }

// A shot from a shooter standing at feet, aimed at a world point.
ShotHit ShootAt(const Target& target, const std::vector<PlayerWorldHitShape>& shapes, Vec3 feet, Vec3 point) {
    static const Arena arena{};
    PlayerState shooter;
    shooter.playerId = 1;
    shooter.position = feet;
    const Vec3 eye{feet.x, feet.y + StanceEyeHeight(arena, DefaultStanceRules, false), feet.z};
    const auto direction = Engine::Math::Normalize(point - eye);
    const float yaw = std::atan2(direction.x, direction.z);
    const float pitch = -std::asin(direction.y);
    const std::array targets{ShotTarget{target.state.playerId, shapes}};
    return QueryShot(arena, DefaultStanceRules, shooter, yaw, pitch, targets, 100);
}

const Engine::Math::Capsule& PartVolume(const std::vector<PlayerWorldHitShape>& shapes, HitPart part,
                                        std::size_t index = 0) {
    for (const auto& shape : shapes)
        if (shape.part == part && index-- == 0) return Volume(shape);
    throw std::runtime_error("no such shape");
}
Vec3 Middle(const Engine::Math::Capsule& capsule) { return (capsule.segmentStart + capsule.segmentEnd) * 0.5F; }

float SegmentDistance(Vec3 point, const Engine::Math::Capsule& capsule) {
    const auto axis = capsule.segmentEnd - capsule.segmentStart;
    const float length = Engine::Math::Dot(axis, axis);
    const float t = length > 0 ? Engine::Math::Clamp(Engine::Math::Dot(point - capsule.segmentStart, axis) / length, 0.0F, 1.0F) : 0.0F;
    return Engine::Math::Length(point - (capsule.segmentStart + axis * t));
}

struct Pose final {
    const char* name;
    Target target;
};

std::vector<Pose> Poses() {
    std::vector<Pose> poses{{"idle yaw 0", Target({0, 0, 0}, 0)}, {"idle yaw 1.3", Target({3, 0, -2}, 1.3F)},
                            {"crouched", Target({0, 0, 0}, 0.4F, true)}, {"running", Target({1, 0, 1}, 1.3F)}};
    auto& running = poses.back().target;
    running.animation.gaitPhaseQ16 = 0x4000; // a quarter cycle in
    running.animation.moveWeightPermille = 1000;
    running.animation.jogWeightPermille = 1000;
    return poses;
}
} // namespace

TEST_CASE("PvP posed shapes judge the head, the body and the legs") {
    for (const auto& [name, target] : Poses()) {
        CAPTURE(std::string(name));
        const auto shapes = target.Shapes();
        REQUIRE(shapes.size() == 7);
        const auto front = target.state.position + Forward(target.state.yaw) * 5.0F;
        // The aim points come from the same pose the judgement uses.
        const auto& torso = PartVolume(shapes, HitPart::Body, 0); // spine_01 to spine_03
        const struct {
            HitPart part;
            Vec3 point;
            std::uint32_t damage;
        } aims[] = {{HitPart::Head, PartVolume(shapes, HitPart::Head).segmentStart, 50000},
                    {HitPart::Body, Middle(torso), 25000},
                    {HitPart::Leg, Middle(PartVolume(shapes, HitPart::Leg, 0)), 18750}};
        for (const auto& aim : aims) {
            CAPTURE(static_cast<int>(aim.part));
            const auto hit = ShootAt(target, shapes, front, aim.point);
            CHECK(hit.kind == ShotHitKind::Player);
            CHECK(hit.part == aim.part);
            CHECK(ShotDamage(PvpCombatRules, hit.part, PvpCombatRules.maximumHp) == aim.damage);
        }
    }
}

TEST_CASE("PvP posed shapes let a shot through the arm and between the legs") {
    for (const auto& [name, target] : Poses()) {
        if (target.state.crouched || target.animation.moveWeightPermille) continue; // standing idle only
        CAPTURE(std::string(name));
        const auto shapes = target.Shapes();
        // The right hand holds the weapon in front of the chest: no shape
        // covers it, and a shot from the right side through it passes.
        const auto hand = target.Bone("hand_r");
        for (const auto& shape : shapes) CHECK(SegmentDistance(hand, Volume(shape)) > Volume(shape).radius);
        const auto side = Vec3{hand.x, 0, hand.z} + Right(target.state.yaw) * 3.0F;
        CHECK(ShootAt(target, shapes, side, hand).kind != ShotHitKind::Player);
        // From the front between the knees.
        const auto knees = (target.Bone("calf_l") + target.Bone("calf_r")) * 0.5F;
        const auto front = target.state.position + Forward(target.state.yaw) * 5.0F;
        CHECK(ShootAt(target, shapes, front, knees).kind != ShotHitKind::Player);
    }
}

TEST_CASE("PvP posed shapes follow the target's place and facing") {
    const Target here({0, 0, 0}, 0), there({4, 0.5F, -3}, 1.3F);
    const auto a = here.Shapes(), b = there.Shapes();
    REQUIRE(a.size() == b.size());
    // The same pose turned by the yaw and moved by the position.
    for (std::size_t i = 0; i < a.size(); ++i) {
        CAPTURE(i);
        const auto local = Volume(a[i]).segmentStart;
        const Vec3 expected = there.state.position + Right(1.3F) * local.x + Vec3{0, local.y, 0} + Forward(1.3F) * local.z;
        const auto actual = Volume(b[i]).segmentStart;
        CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-5));
        CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-5));
        CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-5));
        CHECK(Volume(b[i]).radius == Volume(a[i]).radius);
        CHECK(b[i].part == a[i].part);
    }
}

// v8-product-hitbox-tick-pose (a): the shapes of a tick are placed before its
// shots, so an action that starts in that tick is not in them. This measures
// how far each action's first instant moves the shapes from no action.
TEST_CASE("PvP an action at its first instant barely moves the shapes") {
    const Target plain({0, 0, 0}, 0);
    const auto base = plain.Shapes();
    const auto offset = [&](const Target& target) {
        const auto moved = target.Shapes();
        float worst = 0;
        for (std::size_t i = 0; i < base.size(); ++i) {
            worst = std::max(worst, Engine::Math::Length(Volume(moved[i]).segmentStart - Volume(base[i]).segmentStart));
            worst = std::max(worst, Engine::Math::Length(Volume(moved[i]).segmentEnd - Volume(base[i]).segmentEnd));
        }
        return worst;
    };
    Target shot = plain, reload = plain, hit = plain;
    shot.combat.lastShotActionId = 1;
    shot.combat.lastShotTick = shot.tick;
    reload.combat.reloadActionId = 2;
    reload.combat.reloadStartTick = reload.tick;
    reload.combat.reloadEndTick = reload.tick + 90;
    hit.combat.damageCount = 1;
    hit.combat.lastDamageTick = hit.tick;
    MESSAGE("first instant offsets (m): shot " << offset(shot) << " reload " << offset(reload) << " hit " << offset(hit));
    // Shot and reload move relative to their first frame: nothing at first.
    // The hit clip's first frame differs by float rounding only (6e-9 m on the
    // Intel Mac, 2026-11-01).
    CHECK(offset(shot) == 0);
    CHECK(offset(reload) == 0);
    CHECK(offset(hit) < 1e-6F);
}

// xhigh review 3a: every input of the posed model reaches the pose. Each case
// is compared bit for bit with the Client's pose of the same values, and must
// move the shapes away from the plain idle pose.
TEST_CASE("PvP posed shapes are the Client's pose for every kind of input") {
    const Target plain({1, 0, 2}, 0.6F);
    const auto plainShapes = plain.Shapes();
    std::vector<std::pair<const char*, Target>> cases;
    const auto add = [&](const char* name, auto&& edit) {
        Target target = plain;
        edit(target);
        cases.emplace_back(name, target);
    };
    add("running", [](Target& t) {
        t.animation.gaitPhaseQ16 = 0x18000; // one and a half cycles in
        t.animation.moveWeightPermille = 600;
        t.animation.jogWeightPermille = 400;
    });
    add("airborne", [](Target& t) {
        t.state.grounded = false;
        t.state.verticalVelocity = 1.5F;
        t.animation.jumpPhase = JumpPhase::Airborne;
        t.animation.jumpPhaseTick = t.tick - 10;
    });
    add("landing", [](Target& t) {
        t.animation.jumpPhase = JumpPhase::Land;
        t.animation.jumpPhaseTick = t.tick - 2;
    });
    add("crouched moving", [](Target& t) {
        t.state.crouched = true;
        t.animation.gaitPhaseQ16 = 0x6000;
        t.animation.moveWeightPermille = 1000;
    });
    add("hit 5 ticks ago", [](Target& t) {
        t.combat.damageCount = 1;
        t.combat.lastDamageTick = t.tick - 5;
    });
    // v8-product-hit-clip-by-part G2: the head hit plays its own clip.
    add("head hit 5 ticks ago", [](Target& t) {
        t.combat.damageCount = 1;
        t.combat.lastDamageTick = t.tick - 5;
        t.combat.lastHitPart = HitPart::Head;
    });
    add("leg hit 5 ticks ago", [](Target& t) {
        t.combat.damageCount = 1;
        t.combat.lastDamageTick = t.tick - 5;
        t.combat.lastHitPart = HitPart::Leg;
    });
    add("reloading", [](Target& t) {
        t.combat.reloadActionId = 4;
        t.combat.reloadStartTick = t.tick - 30;
        t.combat.reloadEndTick = t.tick + 60;
    });
    add("shot 3 ticks ago", [](Target& t) {
        t.combat.lastShotActionId = 3;
        t.combat.lastShotTick = t.tick - 3;
    });
    add("looking up", [](Target& t) { t.state.pitch = -0.6F; });
    add("idle later", [](Target& t) { t.tick += 37; });
    const auto& hitbox = LoadedProduct().hitbox;
    for (const auto& [name, target] : cases) {
        CAPTURE(std::string(name));
        const auto shapes = target.Shapes();
        const auto pose = target.ClientPose();
        REQUIRE(shapes.size() == hitbox.shapes.size());
        float moved = 0;
        for (std::size_t i = 0; i < shapes.size(); ++i) {
            const auto& shape = hitbox.shapes[i];
            const auto& volume = Volume(shapes[i]);
            const bool sphere = shape.kind == PlayerHitShape::Kind::Sphere;
            const auto start = sphere ? target.World(pose, shape.node, shape.offset) : target.World(pose, shape.node);
            const auto end = sphere ? start : target.World(pose, shape.nodeTo);
            CAPTURE(i);
            CHECK(std::bit_cast<std::uint32_t>(volume.segmentStart.y) == std::bit_cast<std::uint32_t>(start.y));
            CHECK(std::bit_cast<std::uint32_t>(volume.segmentStart.x) == std::bit_cast<std::uint32_t>(start.x));
            CHECK(std::bit_cast<std::uint32_t>(volume.segmentStart.z) == std::bit_cast<std::uint32_t>(start.z));
            CHECK(std::bit_cast<std::uint32_t>(volume.segmentEnd.y) == std::bit_cast<std::uint32_t>(end.y));
            CHECK(volume.radius == shape.radius);
            CHECK(shapes[i].part == shape.part);
            moved = std::max(moved, Engine::Math::Length(volume.segmentStart - Volume(plainShapes[i]).segmentStart));
            moved = std::max(moved, Engine::Math::Length(volume.segmentEnd - Volume(plainShapes[i]).segmentEnd));
        }
        MESSAGE(std::string(name) << ": shapes move " << moved << " m from plain idle");
        CHECK(moved > 0.001F);
    }
}

// v8-product-hit-clip-by-part (a), v8-product-hit-leg-clip (a): the judged
// shapes follow the hit clip of the part, as the Client draws it.
TEST_CASE("PvP posed shapes take the head hit clip for a head hit and the chest hit otherwise") {
    const auto struck = [](HitPart part) {
        Target target({1, 0, 2}, 0.6F);
        target.combat.damageCount = 1;
        target.combat.lastDamageTick = target.tick - 5;
        target.combat.lastHitPart = part;
        return target.Shapes();
    };
    const auto head = struck(HitPart::Head), body = struck(HitPart::Body), leg = struck(HitPart::Leg);
    REQUIRE(head.size() == body.size());
    float moved = 0;
    for (std::size_t i = 0; i < head.size(); ++i) {
        moved = std::max(moved, Engine::Math::Length(Volume(head[i]).segmentStart - Volume(body[i]).segmentStart));
        CAPTURE(i);
        const auto same = [](Vec3 a, Vec3 b) {
            return std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
                   std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y) &&
                   std::bit_cast<std::uint32_t>(a.z) == std::bit_cast<std::uint32_t>(b.z);
        };
        CHECK(same(Volume(leg[i]).segmentStart, Volume(body[i]).segmentStart));
        CHECK(same(Volume(leg[i]).segmentEnd, Volume(body[i]).segmentEnd));
    }
    MESSAGE("head hit against chest hit: shapes move " << moved << " m");
    CHECK(moved > 0.001F);
}

// xhigh review 1 (note): the accepted cost of v8-product-hitbox-tick-pose (a)
// is an action restarted in the judged tick. The snapshot of that tick shows
// the action at 0 s; the shapes show the earlier run k ticks in. Measured, no
// threshold was declared.
TEST_CASE("PvP an action restarted in the judged tick against the run it replaces") {
    const Target plain({0, 0, 0}, 0);
    const auto worst = [&](auto&& at) {
        float result = 0;
        for (std::uint64_t k = 1; k <= 40; ++k) {
            Target earlier = plain, restarted = plain;
            at(earlier, plain.tick - k);
            at(restarted, plain.tick);
            const auto a = earlier.Shapes(), b = restarted.Shapes();
            for (std::size_t i = 0; i < a.size(); ++i) {
                result = std::max(result, Engine::Math::Length(Volume(a[i]).segmentStart - Volume(b[i]).segmentStart));
                result = std::max(result, Engine::Math::Length(Volume(a[i]).segmentEnd - Volume(b[i]).segmentEnd));
            }
        }
        return result;
    };
    const auto hit = worst([](Target& t, std::uint64_t tick) {
        t.combat.damageCount = 1;
        t.combat.lastDamageTick = tick;
    });
    const auto shot = worst([](Target& t, std::uint64_t tick) {
        t.combat.lastShotActionId = 1;
        t.combat.lastShotTick = tick;
    });
    // v8-product-hit-clip-by-part (xhigh review): the head hit restarted, and
    // a head hit replacing a chest hit k ticks in (another clip, not only time).
    const auto headHit = worst([](Target& t, std::uint64_t tick) {
        t.combat.damageCount = 1;
        t.combat.lastDamageTick = tick;
        t.combat.lastHitPart = HitPart::Head;
    });
    float chestToHead = 0;
    for (std::uint64_t k = 1; k <= 40; ++k) {
        Target earlier = plain, restarted = plain;
        earlier.combat.damageCount = 1;
        earlier.combat.lastDamageTick = plain.tick - k;
        earlier.combat.lastHitPart = HitPart::Body;
        restarted.combat.damageCount = 2;
        restarted.combat.lastDamageTick = plain.tick;
        restarted.combat.lastHitPart = HitPart::Head;
        const auto a = earlier.Shapes(), b = restarted.Shapes();
        for (std::size_t i = 0; i < a.size(); ++i) {
            chestToHead = std::max(chestToHead, Engine::Math::Length(Volume(a[i]).segmentStart - Volume(b[i]).segmentStart));
            chestToHead = std::max(chestToHead, Engine::Math::Length(Volume(a[i]).segmentEnd - Volume(b[i]).segmentEnd));
        }
    }
    MESSAGE("restarted action, worst shape offset over 1..40 ticks (m): hit " << hit << " shot " << shot
            << " head hit " << headHit << " chest then head " << chestToHead);
    CHECK(hit >= 0);
}

// v8-product-hitbox-acceptance A2 (B5), measured only (v8-product-hitbox-b5-threshold (a)):
// how far the shapes on the body the Client draws between two snapshots lie
// from the shapes the Match judges at the earlier tick J. The authority runs
// come from the real PvpMatch with the product's animation rules; a hit is
// written into the recorded combat state, which both ends read the same way.
// The Client side samples as SnapshotTimeline does (position, yaw and pitch
// interpolated, combat from the earlier snapshot, the animation interpolated)
// and carries its locomotion state from sample to sample.
TEST_CASE("PvP B5: shapes of the Client's interpolated body against the Match's judged body") {
    const auto& product = LoadedProduct();
    const auto& definition = *product.definition;
    Arena arena;
    arena.id = "b5_arena";
    arena.width = arena.depth = 20;
    arena.spawns = {{{4, 0, 4}, 0}, {{4, 0, 14}, 0}};
    struct Run final {
        const char* name;
        MovementCommand (*command)(std::uint64_t);
        bool struck;
    };
    const Run runs[] = {
        {"idle", [](std::uint64_t) { return MovementCommand{}; }, false},
        {"running and turning", [](std::uint64_t k) {
             MovementCommand c;
             c.moveForward = 1;
             c.yaw = 0.02F * static_cast<float>(k);
             return c;
         }, false},
        {"jumping forward", [](std::uint64_t k) {
             MovementCommand c;
             c.moveForward = 1;
             c.jumpRequested = k % 40 == 1;
             return c;
         }, false},
        {"crouched strafing", [](std::uint64_t) {
             MovementCommand c;
             c.moveRight = 1;
             c.crouchHeld = true;
             return c;
         }, false},
        {"struck while running", [](std::uint64_t) {
             MovementCommand c;
             c.moveForward = 1;
             return c;
         }, true},
    };
    const auto& hitbox = product.hitbox;
    const auto endpoints = [&](const Engine::Model::Pose& pose, const PlayerState& state) {
        std::vector<Vec3> points;
        for (const auto& shape : hitbox.shapes) {
            const auto world = [&](std::size_t node, Vec3 local) {
                return PlayerBodyPointToWorld(definition, state.position, state.yaw,
                    Engine::Math::TransformPoint(pose.globalTransforms.at(node), local));
            };
            const bool sphere = shape.kind == PlayerHitShape::Kind::Sphere;
            points.push_back(sphere ? world(shape.node, shape.offset) : world(shape.node, {}));
            points.push_back(sphere ? points.back() : world(shape.nodeTo, {}));
        }
        return points;
    };
    for (const auto& run : runs) {
        CAPTURE(std::string(run.name));
        PvpMatch match(arena, PvpCombatRules, DefaultStanceRules, definition.animationRules);
        std::string error;
        REQUIRE(match.Join(1, error));
        REQUIRE(match.Join(2, error));
        std::vector<WorldSnapshot> history;
        for (std::uint64_t k = 1; k <= 240; ++k) {
            auto command = run.command(k);
            command.sequence = k;
            REQUIRE(match.SubmitInput({2, {command}}));
            match.Tick({match.TickCount() + 1, MovementTickSeconds});
            history.push_back(match.Snapshot());
            if (run.struck && k % 25 == 0) { // a Body or Head hit every 25 ticks, written into this tick's record
                for (auto& combat : history.back().combat)
                    if (combat.playerId == 2) {
                        ++combat.damageCount;
                        combat.lastDamageTick = history.back().tick;
                        combat.lastAttackerId = 1;
                        combat.lastHitPart = k % 50 == 0 ? HitPart::Head : HitPart::Body;
                    }
            }
            if (run.struck && k > 1) // the record carries over until the next hit
                for (auto& combat : history.back().combat)
                    if (combat.playerId == 2 && k % 25 != 0)
                        for (const auto& previous : history[history.size() - 2].combat)
                            if (previous.playerId == 2) {
                                combat.damageCount = previous.damageCount;
                                combat.lastDamageTick = previous.lastDamageTick;
                                combat.lastAttackerId = previous.lastAttackerId;
                                combat.lastHitPart = previous.lastHitPart;
                            }
        }
        const auto find = [](const auto& items) {
            const auto found = std::find_if(items.begin(), items.end(), [](const auto& item) { return item.playerId == 2; });
            REQUIRE(found != items.end());
            return *found;
        };
        PlayerLocomotionState locomotion;
        std::vector<float> offsets, interpolationOnly, historyOnly, matchStep;
        for (std::size_t j = 60; j + 1 < history.size(); ++j) {
            const auto& a = history[j];
            const auto& b = history[j + 1];
            const auto aPlayer = find(a.players), bPlayer = find(b.players);
            const auto aCombat = find(a.combat);
            const auto aAnimation = find(a.animation), bAnimation = find(b.animation);
            std::vector<PlayerWorldHitShape> judged;
            product.model->Shapes({arena, DefaultStanceRules, aPlayer, aCombat, aAnimation, a.tick}, judged);
            // The Match's own move from tick J to J+1: what one tick of motion is.
            std::vector<PlayerWorldHitShape> next;
            product.model->Shapes({arena, DefaultStanceRules, bPlayer, find(b.combat), bAnimation, b.tick}, next);
            float step = 0;
            for (std::size_t i = 0; i < judged.size(); ++i) {
                step = std::max(step, Engine::Math::Length(Volume(judged[i]).segmentStart - Volume(next[i]).segmentStart));
                step = std::max(step, Engine::Math::Length(Volume(judged[i]).segmentEnd - Volume(next[i]).segmentEnd));
            }
            matchStep.push_back(step);
            for (const double alpha : {0.25, 0.5, 0.75}) {
                auto player = aPlayer;
                const auto fraction = static_cast<float>(alpha);
                player.position = Engine::Math::Lerp(aPlayer.position, bPlayer.position, fraction);
                player.yaw = Engine::Math::LerpRadiansShortest(aPlayer.yaw, bPlayer.yaw, fraction);
                player.pitch = Engine::Math::Lerp(aPlayer.pitch, bPlayer.pitch, fraction);
                auto frame = PlayerFrameFromAuthority(player, aCombat,
                    InterpolatePlayerAnimation(aAnimation, bAnimation, alpha), static_cast<double>(a.tick) + alpha);
                frame.deltaSeconds = MovementTickSeconds / 4;
                REQUIRE(AdvancePlayerLocomotion(locomotion, frame, definition, error));
                PlayerActionPose actions;
                REQUIRE(ResolvePlayerActions(definition, locomotion, frame, actions, error));
                Engine::Model::Pose pose;
                REQUIRE(SamplePlayerBodyPose(definition, locomotion, actions, pose, error));
                const auto drawn = endpoints(pose, player);
                // The same sample from a fresh locomotion state, as the Match poses:
                // splits the offset into interpolation and the Client's own history.
                PlayerLocomotionState fresh;
                auto freshFrame = frame;
                REQUIRE(AdvancePlayerLocomotion(fresh, freshFrame, definition, error));
                PlayerActionPose freshActions;
                REQUIRE(ResolvePlayerActions(definition, fresh, freshFrame, freshActions, error));
                Engine::Model::Pose freshPose;
                REQUIRE(SamplePlayerBodyPose(definition, fresh, freshActions, freshPose, error));
                const auto freshDrawn = endpoints(freshPose, player);
                float worst = 0, interpolation = 0, own = 0;
                for (std::size_t i = 0; i < judged.size(); ++i) {
                    const Vec3 match[2] = {Volume(judged[i]).segmentStart, Volume(judged[i]).segmentEnd};
                    for (std::size_t e = 0; e < 2; ++e) {
                        worst = std::max(worst, Engine::Math::Length(match[e] - drawn[2 * i + e]));
                        interpolation = std::max(interpolation, Engine::Math::Length(match[e] - freshDrawn[2 * i + e]));
                        own = std::max(own, Engine::Math::Length(freshDrawn[2 * i + e] - drawn[2 * i + e]));
                    }
                }
                offsets.push_back(worst);
                interpolationOnly.push_back(interpolation);
                historyOnly.push_back(own);
            }
        }
        REQUIRE(!offsets.empty());
        const auto summary = [](std::vector<float> values) {
            std::sort(values.begin(), values.end());
            const auto at = [&](double q) { return values[static_cast<std::size_t>(q * static_cast<double>(values.size() - 1))]; };
            std::ostringstream text;
            text << "P50 " << at(0.5) << " P95 " << at(0.95) << " max " << values.back();
            return text.str();
        };
        MESSAGE("B5 " << std::string(run.name) << ": " << offsets.size() << " samples (m); drawn against judged "
                      << summary(offsets) << "; interpolation only " << summary(interpolationOnly)
                      << "; the Client's history only " << summary(historyOnly)
                      << "; the Match's own move per tick " << summary(matchStep));
        CHECK(offsets.back() >= 0);
    }
}

// v8-product-real-match-acceptance-repair (defect, 2026-11-03): the probes'
// torso aim (hit_aim.hpp) must be judged Body by the posed shapes. Every
// ordered pair of the product arena's spawns, both standing idle as spawned.
TEST_CASE("PvP the probes' torso aim is judged Body from every product spawn") {
    std::string error;
    const auto arena = Arena::Load(ContentRoot() / "maps" / "pvp_arena.json", error);
    REQUIRE_MESSAGE(arena, error);
    const HitAimModel aim(ContentRoot(), arena->bodyHeight);
    const auto& model = *LoadedProduct().model;
    for (std::size_t s = 0; s < arena->spawns.size(); ++s)
        for (std::size_t t = 0; t < arena->spawns.size(); ++t) {
            if (s == t) continue;
            CAPTURE(s);
            CAPTURE(t);
            const auto make = [&](PlayerId id, const SpawnPoint& spawn) {
                PlayerState state;
                state.playerId = id;
                state.position = spawn.position;
                state.yaw = spawn.yaw;
                state.grounded = true;
                state.lifeGeneration = 1;
                state.lifeState = LifeState::Alive;
                state.lifeStateTick = 100;
                return state;
            };
            WorldSnapshot snapshot;
            snapshot.tick = 600;
            snapshot.players = {make(1, arena->spawns[s]), make(2, arena->spawns[t])};
            for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
                CombatState combat;
                combat.playerId = id;
                snapshot.combat.push_back(combat);
                PlayerAnimation animation;
                animation.playerId = id;
                animation.lifeGeneration = 1;
                animation.jumpPhase = JumpPhase::Grounded;
                animation.jumpPhaseTick = 100;
                snapshot.animation.push_back(animation);
            }
            const auto aimed = aim.Aim(*arena, DefaultStanceRules, snapshot, 1, 2, HitPart::Body);
            std::vector<PlayerWorldHitShape> shapes;
            model.Shapes({*arena, DefaultStanceRules, snapshot.players[1], snapshot.combat[1], snapshot.animation[1],
                          snapshot.tick}, shapes);
            const std::array targets{ShotTarget{2, shapes}};
            const auto hit = QueryShot(*arena, DefaultStanceRules, snapshot.players[0], aimed.yaw, aimed.pitch, targets, 100);
            CHECK(hit.kind == ShotHitKind::Player);
            CHECK(hit.part == HitPart::Body);
            // The GUI runner aims once and fires several shots: the later ones meet
            // the target in its hit reaction (Body or Head), at every tick of it.
            for (const HitPart struck : {HitPart::Body, HitPart::Head})
                for (std::uint64_t k = 0; k <= 20; ++k) {
                    CAPTURE(static_cast<int>(struck));
                    CAPTURE(k);
                    auto combat = snapshot.combat[1];
                    combat.damageCount = 1;
                    combat.lastDamageTick = snapshot.tick - k;
                    combat.lastAttackerId = 1;
                    combat.lastHitPart = struck;
                    std::vector<PlayerWorldHitShape> reacting;
                    model.Shapes({*arena, DefaultStanceRules, snapshot.players[1], combat, snapshot.animation[1], snapshot.tick},
                                 reacting);
                    const std::array moved{ShotTarget{2, reacting}};
                    const auto later = QueryShot(*arena, DefaultStanceRules, snapshot.players[0], aimed.yaw, aimed.pitch, moved, 100);
                    CHECK(later.kind == ShotHitKind::Player);
                    CHECK(later.part == HitPart::Body);
                }
        }
}
