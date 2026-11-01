#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerHitbox.hpp"
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "RetroFPS/Pvp/ShotQuery.hpp"
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
