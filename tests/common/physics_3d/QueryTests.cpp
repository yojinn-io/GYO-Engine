// The pure queries of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, sections
// 4.2 and 4.3): the geometric cases of each supported pair (hits, misses, touching counts as a hit, starting inside,
// tangential motion with stopAtPenetration false), the unsupported pairs and the programmer errors.
// Each CHECK carries a tag in brackets ("[p3-...]") for mutants to expect.
#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace M = GYO::Engine::Math;
namespace P3 = GYO::Engine::Physics3d;

namespace {

P3::Pose At(const float x, const float y, const float z, const M::Quaternion rotation = {}) {
    return P3::Pose{{x, y, z}, rotation};
}

bool Near(const M::Vec3 a, const M::Vec3 b, const float eps = 1.0e-5F) {
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
}

bool Same(const M::Vec3 a, const M::Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// sin and cos of 45 degrees and 22.5 degrees as float literals (the tests call no transcendental function either).
constexpr float kS45 = 0.70710678F;
constexpr float kS22 = 0.38268343F;
constexpr float kC22 = 0.92387953F;
constexpr float kInf = std::numeric_limits<float>::infinity();

P3::BoxSet MakeBoxes(const std::vector<M::Aabb>& boxes) {
    auto built = P3::BoxSet::Build(boxes);
    REQUIRE(static_cast<bool>(built));
    return *built;
}

P3::Cuboid Floor() { return P3::Cuboid{{10.0F, 1.0F, 10.0F}}; }
P3::Pose FloorPose() { return At(0.0F, -1.0F, 0.0F); }  // top face at y = 0

} // namespace

// ---- CastRay

TEST_CASE("queries: a ray hits a sphere at the first crossing with the outward normal") {
    const P3::Shape ball = P3::Ball{1.0F};
    const M::Ray ray{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};
    const auto hit = P3::CastRay(ball, At(0.0F, 0.0F, 5.0F), ray, 100.0F);
    REQUIRE_MESSAGE(hit.has_value(), "[p3-ray-sphere] no hit");
    CHECK_MESSAGE(hit->t == 4.0F, "[p3-ray-sphere] t " << hit->t);
    CHECK_MESSAGE(Same(hit->normal, {0.0F, 0.0F, -1.0F}), "[p3-ray-sphere] normal");
    CHECK_MESSAGE(!P3::CastRay(ball, At(0.0F, 0.0F, 5.0F), ray, 3.0F).has_value(), "[p3-ray-maxt] beyond maxT");
    CHECK_MESSAGE(P3::CastRay(ball, At(0.0F, 0.0F, 5.0F), ray, 4.0F).has_value(), "[p3-ray-maxt] at maxT");
    CHECK_MESSAGE(!P3::CastRay(ball, At(0.0F, 0.0F, -5.0F), ray, kInf).has_value(), "[p3-ray-sphere] behind");
    CHECK_MESSAGE(!P3::CastRay(ball, At(0.0F, 3.0F, 5.0F), ray, kInf).has_value(), "[p3-ray-sphere] beside");
}

TEST_CASE("queries: a ray grazing a sphere hits it; t is in units of |direction|") {
    const P3::Shape ball = P3::Ball{1.0F};
    const auto graze = P3::CastRay(ball, At(1.0F, 0.0F, 5.0F), M::Ray{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}}, kInf);
    REQUIRE_MESSAGE(graze.has_value(), "[p3-ray-graze] a tangent ray missed");
    CHECK(graze->t == 5.0F);
    CHECK(Same(graze->normal, {-1.0F, 0.0F, 0.0F}));
    const auto scaled = P3::CastRay(ball, At(0.0F, 0.0F, 5.0F), M::Ray{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 2.0F}}, kInf);
    REQUIRE(scaled.has_value());
    CHECK_MESSAGE(scaled->t == 2.0F, "[p3-ray-units] " << scaled->t);
}

TEST_CASE("queries: a ray starting inside a solid shape hits at t = 0 with a zero normal") {
    const M::Ray ray{{0.0F, 0.0F, 5.5F}, {0.0F, 0.0F, 1.0F}};
    const auto ball = P3::CastRay(P3::Ball{1.0F}, At(0.0F, 0.0F, 5.0F), ray, kInf);
    REQUIRE(ball.has_value());
    CHECK_MESSAGE((ball->t == 0.0F && Same(ball->normal, {})), "[p3-ray-inside] sphere");
    const auto capsule = P3::CastRay(P3::CapsuleY(1.0F, 0.5F), At(0.0F, 0.0F, 5.0F), ray, kInf);
    REQUIRE(capsule.has_value());
    CHECK_MESSAGE((capsule->t == 0.0F && Same(capsule->normal, {})), "[p3-ray-inside] capsule");
    const auto box = P3::CastRay(P3::Cuboid{{1.0F, 1.0F, 1.0F}}, At(0.0F, 0.0F, 5.0F), ray, kInf);
    REQUIRE(box.has_value());
    CHECK_MESSAGE((box->t == 0.0F && Same(box->normal, {})), "[p3-ray-inside] cuboid");
    const auto set = P3::CastRay(MakeBoxes({{{-1.0F, -1.0F, 4.0F}, {1.0F, 1.0F, 6.0F}}}), At(0.0F, 0.0F, 0.0F), ray, kInf);
    REQUIRE(set.has_value());
    CHECK_MESSAGE((set->t == 0.0F && Same(set->normal, {})), "[p3-ray-inside] box set");
}

TEST_CASE("queries: a ray hits an upright capsule on its side and on its caps") {
    const P3::Shape capsule = P3::CapsuleY(1.0F, 0.5F);
    const auto side = P3::CastRay(capsule, At(0.0F, 0.0F, 0.0F), M::Ray{{-5.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(side.has_value(), "[p3-ray-capsule] side");
    CHECK(side->t == 4.5F);
    CHECK(Same(side->normal, {-1.0F, 0.0F, 0.0F}));
    const auto top = P3::CastRay(capsule, At(0.0F, 0.0F, 0.0F), M::Ray{{0.0F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(top.has_value(), "[p3-ray-capsule] cap");
    CHECK(top->t == 3.5F);
    CHECK(Same(top->normal, {0.0F, 1.0F, 0.0F}));
    // Into the upper hemisphere, 0.25 above its centre: x = -sqrt(0.25 - 0.0625).
    const auto upper = P3::CastRay(capsule, At(0.0F, 0.0F, 0.0F), M::Ray{{-5.0F, 1.25F, 0.0F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(upper.has_value(), "[p3-ray-capsule] hemisphere");
    const float x = -std::sqrt(0.1875F);
    CHECK(upper->t == doctest::Approx(5.0F + x));
    CHECK(Near(upper->normal, {x / 0.5F, 0.5F, 0.0F}));
    CHECK_MESSAGE(!P3::CastRay(capsule, At(0.0F, 0.0F, 0.0F), M::Ray{{-5.0F, 1.6F, 0.0F}, {1.0F, 0.0F, 0.0F}}, kInf),
                  "[p3-ray-capsule] above the cap");
    // Lying along X (90 degrees about Z): the side faces up.
    const auto lying = P3::CastRay(capsule, At(0.0F, 0.0F, 0.0F, {0.0F, 0.0F, kS45, kS45}),
                                   M::Ray{{0.9F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(lying.has_value(), "[p3-ray-capsule] rotated");
    CHECK(lying->t == doctest::Approx(4.5F));
    CHECK(Near(lying->normal, {0.0F, 1.0F, 0.0F}));
}

TEST_CASE("queries: a capsule is any segment: a slanted one, its end cap, coincident ends (a ball), at the identity pose") {
    // A product's hit shapes (bones' capsules in world space) are Capsule{a, b, radius} at the identity Pose.
    const P3::Shape slantedCapsule = P3::Capsule{{-1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, 0.5F};
    const auto slanted = P3::CastRay(slantedCapsule, P3::Pose{}, M::Ray{{0.0F, 0.0F, 5.0F}, {0.0F, 0.0F, -1.0F}}, kInf);
    REQUIRE_MESSAGE(slanted.has_value(), "[p3-ray-segment-capsule] slanted");
    CHECK(slanted->t == 4.5F);
    CHECK(Same(slanted->normal, {0.0F, 0.0F, 1.0F}));
    const auto end = P3::CastRay(slantedCapsule, P3::Pose{}, M::Ray{{3.0F, 3.0F, 0.0F}, {-1.0F, -1.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(end.has_value(), "[p3-ray-segment-capsule] end cap");
    CHECK(end->t == doctest::Approx(2.0F - 0.5F / std::sqrt(2.0F)));
    const auto point = P3::CastRay(P3::Capsule{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 1.0F}, P3::Pose{},
                                   M::Ray{{-3.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(point.has_value(), "[p3-ray-segment-capsule] coincident ends");
    CHECK(point->t == 2.0F);
    CHECK(Same(point->normal, {-1.0F, 0.0F, 0.0F}));
    const auto ball = P3::CastRay(P3::Ball{1.0F}, At(1.0F, 0.0F, 0.0F), M::Ray{{-3.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, 10.0F);
    REQUIRE_MESSAGE(ball.has_value(), "[p3-ray-ball]");
    CHECK(ball->t == 3.0F);
    // CapsuleY at a pose and the same segment written out at the identity pose give the same bits.
    const auto posed = P3::CastRay(P3::CapsuleY(1.0F, 0.5F), At(0.25F, 0.0F, 0.0F),
                                   M::Ray{{-3.0F, 0.75F, 0.1F}, {1.0F, 0.0F, 0.0F}}, kInf);
    const auto written = P3::CastRay(P3::Capsule{{0.25F, -1.0F, 0.0F}, {0.25F, 1.0F, 0.0F}, 0.5F}, P3::Pose{},
                                     M::Ray{{-3.0F, 0.75F, 0.1F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE((posed.has_value() && written.has_value()));
    CHECK_MESSAGE((posed->t == written->t && Same(posed->normal, written->normal)), "[p3-ray-segment-same]");
}

TEST_CASE("queries: a ray hits a cuboid on the entered face, also rotated") {
    const P3::Shape box = P3::Cuboid{{1.0F, 2.0F, 3.0F}};
    const M::Ray ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}};
    const auto hit = P3::CastRay(box, At(10.0F, 0.0F, 0.0F), ray, kInf);
    REQUIRE_MESSAGE(hit.has_value(), "[p3-ray-cuboid]");
    CHECK(hit->t == 9.0F);
    CHECK(Same(hit->normal, {-1.0F, 0.0F, 0.0F}));
    // 90 degrees about Y: the local Z extent (3) faces the ray.
    const auto turned = P3::CastRay(box, At(10.0F, 0.0F, 0.0F, {0.0F, kS45, 0.0F, kS45}), ray, kInf);
    REQUIRE_MESSAGE(turned.has_value(), "[p3-ray-cuboid] rotated");
    CHECK(turned->t == doctest::Approx(7.0F));
    CHECK(Near(turned->normal, {-1.0F, 0.0F, 0.0F}));
    CHECK_MESSAGE(!P3::CastRay(box, At(10.0F, 2.5F, 0.0F), ray, kInf), "[p3-ray-cuboid] above");
    // Along the top face (y = 2): closed faces, a grazing ray hits.
    const auto graze = P3::CastRay(box, At(10.0F, -2.0F, 0.0F), ray, kInf);
    REQUIRE_MESSAGE(graze.has_value(), "[p3-ray-cuboid] graze");
    CHECK(graze->t == 9.0F);
}

TEST_CASE("queries: a ray on a BoxSet returns the nearest box's face") {
    const P3::BoxSet set = MakeBoxes({{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}}, {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}});
    const auto hit = P3::CastRay(set, At(0.0F, 0.0F, 0.0F), M::Ray{{-1.0F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(hit.has_value(), "[p3-ray-boxset]");
    CHECK(hit->t == 1.0F);
    CHECK(Same(hit->normal, {-1.0F, 0.0F, 0.0F}));
    const auto between = P3::CastRay(set, At(0.0F, 0.0F, 0.0F), M::Ray{{1.5F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE(between.has_value());
    CHECK(between->t == 0.5F);
    const auto moved = P3::CastRay(set, At(0.0F, 10.0F, 0.0F), M::Ray{{-1.0F, 10.5F, 0.5F}, {1.0F, 0.0F, 0.0F}}, kInf);
    REQUIRE_MESSAGE(moved.has_value(), "[p3-ray-boxset] posed");
    CHECK(moved->t == 1.0F);
    CHECK_MESSAGE(!P3::CastRay(P3::BoxSet{}, At(0.0F, 0.0F, 0.0F), M::Ray{{-1.0F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}}, kInf),
                  "[p3-ray-boxset] empty");
}

TEST_CASE("queries: a ray reaching two boxes at the same t through different faces takes the lower index's face") {
    // Box x is entered through its -X face at t = 1, box y through its -Y face at t = 1.
    const M::Aabb x{{0.0F, -5.0F, 0.0F}, {1.0F, 5.0F, 1.0F}};
    const M::Aabb y{{-5.0F, 0.0F, 0.0F}, {5.0F, 1.0F, 1.0F}};
    const M::Ray ray{{-1.0F, -1.0F, 0.5F}, {1.0F, 1.0F, 0.0F}};
    const auto first = P3::CastRay(MakeBoxes({x, y}), At(0.0F, 0.0F, 0.0F), ray, kInf);
    REQUIRE(first.has_value());
    CHECK(first->t == 1.0F);
    CHECK_MESSAGE(Same(first->normal, {-1.0F, 0.0F, 0.0F}), "[p3-boxset-ray-tie] box 0 is x");
    const auto second = P3::CastRay(MakeBoxes({y, x}), At(0.0F, 0.0F, 0.0F), ray, kInf);
    REQUIRE(second.has_value());
    CHECK_MESSAGE(Same(second->normal, {0.0F, -1.0F, 0.0F}), "[p3-boxset-ray-tie] box 0 is y");
}

// ---- FindContact

TEST_CASE("queries: sphere-sphere contact: separated within prediction, penetrating, coincident") {
    const P3::Shape ball = P3::Ball{1.0F};
    auto none = P3::FindContact(ball, At(0.0F, 0.0F, 0.0F), ball, At(3.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE(static_cast<bool>(none));
    CHECK_MESSAGE(!none->has_value(), "[p3-contact-prediction] beyond prediction");
    auto near = P3::FindContact(ball, At(0.0F, 0.0F, 0.0F), ball, At(3.0F, 0.0F, 0.0F), 1.0F);
    REQUIRE((near && near->has_value()));
    const P3::Contact& c = **near;
    CHECK_MESSAGE(c.distance == 1.0F, "[p3-contact-sphere] distance");
    CHECK(Same(c.point1, {1.0F, 0.0F, 0.0F}));
    CHECK(Same(c.point2, {2.0F, 0.0F, 0.0F}));
    CHECK(Same(c.normal1, {1.0F, 0.0F, 0.0F}));
    CHECK(Same(c.normal2, {-1.0F, 0.0F, 0.0F}));
    auto deep = P3::FindContact(ball, At(0.0F, 0.0F, 0.0F), ball, At(1.5F, 0.0F, 0.0F), 0.0F);
    REQUIRE((deep && deep->has_value()));
    CHECK_MESSAGE((*deep)->distance == -0.5F, "[p3-contact-sphere] penetration");
    auto same = P3::FindContact(ball, At(0.0F, 0.0F, 0.0F), ball, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((same && same->has_value()));
    CHECK((*same)->distance == -2.0F);
    CHECK_MESSAGE(Same((*same)->normal1, {1.0F, 0.0F, 0.0F}), "[p3-contact-coincident] +X");
    auto touch = P3::FindContact(ball, At(0.0F, 0.0F, 0.0F), ball, At(2.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE(touch);
    CHECK_MESSAGE(touch->has_value(), "[p3-contact-touch] touching is a contact");
}

TEST_CASE("queries: capsule-capsule contact: parallel and crossing") {
    const P3::Shape capsule = P3::CapsuleY(1.0F, 0.5F);
    auto parallel = P3::FindContact(capsule, At(0.0F, 0.0F, 0.0F), capsule, At(0.75F, 0.0F, 0.0F), 0.0F);
    REQUIRE((parallel && parallel->has_value()));
    CHECK_MESSAGE((*parallel)->distance == -0.25F, "[p3-contact-capsule] parallel");
    CHECK(Same((*parallel)->normal1, {1.0F, 0.0F, 0.0F}));
    CHECK(Same((*parallel)->point1, {0.5F, -1.0F, 0.0F}));
    CHECK(Same((*parallel)->point2, {0.25F, -1.0F, 0.0F}));
    // Lying along X, one above the upright one's top: touching.
    auto crossing = P3::FindContact(capsule, At(0.0F, 0.0F, 0.0F), capsule,
                                    At(0.0F, 2.0F, 0.0F, {0.0F, 0.0F, kS45, kS45}), 1.0e-3F);
    REQUIRE((crossing && crossing->has_value()));
    CHECK(std::abs((*crossing)->distance) <= 1.0e-6F);
    CHECK(Near((*crossing)->normal1, {0.0F, 1.0F, 0.0F}));
    auto sphere = P3::FindContact(P3::Ball{0.5F}, At(1.0F, 0.0F, 0.0F), capsule, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((sphere && sphere->has_value()));
    CHECK_MESSAGE((*sphere)->distance == 0.0F, "[p3-contact-sphere-capsule]");
    CHECK(Same((*sphere)->normal1, {-1.0F, 0.0F, 0.0F}));
}

TEST_CASE("queries: contact with a cuboid: separated, core outside, core inside (face exit, tie order)") {
    const P3::Shape cube = P3::Cuboid{{1.0F, 1.0F, 1.0F}};
    auto near = P3::FindContact(P3::Ball{0.5F}, At(2.0F, 0.0F, 0.0F), cube, At(0.0F, 0.0F, 0.0F), 0.5F);
    REQUIRE((near && near->has_value()));
    CHECK_MESSAGE((*near)->distance == 0.5F, "[p3-contact-cuboid] separated");
    CHECK(Same((*near)->point1, {1.5F, 0.0F, 0.0F}));
    CHECK(Same((*near)->point2, {1.0F, 0.0F, 0.0F}));
    CHECK(Same((*near)->normal1, {-1.0F, 0.0F, 0.0F}));
    auto far = P3::FindContact(P3::Ball{0.5F}, At(2.0F, 0.0F, 0.0F), cube, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE(far);
    CHECK(!far->has_value());
    // Centre inside: the nearest face is +X (push 0.25).
    auto inside = P3::FindContact(P3::Ball{0.5F}, At(0.75F, 0.0F, 0.0F), cube, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((inside && inside->has_value()));
    CHECK_MESSAGE((*inside)->distance == -0.75F, "[p3-contact-face-exit] depth");
    CHECK(Same((*inside)->normal1, {-1.0F, 0.0F, 0.0F}));
    CHECK(Same((*inside)->point2, {1.0F, 0.0F, 0.0F}));
    CHECK(Same((*inside)->point1, {0.25F, 0.0F, 0.0F}));
    // Centre at the centre: every push is 1; the first in the order -X, +X, -Y, +Y, -Z, +Z is -X.
    auto centre = P3::FindContact(P3::Ball{0.5F}, At(0.0F, 0.0F, 0.0F), cube, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((centre && centre->has_value()));
    CHECK_MESSAGE(Same((*centre)->normal1, {1.0F, 0.0F, 0.0F}), "[p3-contact-face-tie] exit through -X");
    CHECK(Same((*centre)->point2, {-1.0F, 0.0F, 0.0F}));
    CHECK((*centre)->distance == -1.5F);
}

TEST_CASE("queries: an upright capsule on a floor: touching, sinking, core entering the floor") {
    const P3::Shape capsule = P3::CapsuleY(1.0F, 0.5F);
    auto standing = P3::FindContact(capsule, At(0.0F, 1.5F, 0.0F), Floor(), FloorPose(), 0.0F);
    REQUIRE((standing && standing->has_value()));
    CHECK_MESSAGE((*standing)->distance == 0.0F, "[p3-contact-floor] touching");
    CHECK(Same((*standing)->normal1, {0.0F, -1.0F, 0.0F}));
    CHECK(Same((*standing)->point1, {0.0F, 0.0F, 0.0F}));
    CHECK(Same((*standing)->point2, {0.0F, 0.0F, 0.0F}));
    auto sinking = P3::FindContact(capsule, At(0.0F, 1.25F, 0.0F), Floor(), FloorPose(), 0.0F);
    REQUIRE((sinking && sinking->has_value()));
    CHECK((*sinking)->distance == -0.25F);
    auto entered = P3::FindContact(capsule, At(0.0F, 0.75F, 0.0F), Floor(), FloorPose(), 0.0F);
    REQUIRE((entered && entered->has_value()));
    CHECK_MESSAGE((*entered)->distance == -0.75F, "[p3-contact-core-inside] depth");
    CHECK(Same((*entered)->normal1, {0.0F, -1.0F, 0.0F}));
    CHECK(Same((*entered)->point2, {0.0F, 0.0F, 0.0F}));
    CHECK(Same((*entered)->point1, {0.0F, -0.75F, 0.0F}));
    auto above = P3::FindContact(capsule, At(0.0F, 1.75F, 0.0F), Floor(), FloorPose(), 0.25F);
    REQUIRE((above && above->has_value()));
    CHECK_MESSAGE((*above)->distance == 0.25F, "[p3-contact-floor] prediction (standing check)");
    // A capsule lying on the floor, crossing its edge.
    auto lying = P3::FindContact(capsule, At(10.0F, 0.25F, 0.0F, {0.0F, 0.0F, kS45, kS45}), Floor(), FloorPose(), 0.0F);
    REQUIRE((lying && lying->has_value()));
    CHECK((*lying)->distance == doctest::Approx(-0.25F));
    CHECK(Near((*lying)->normal1, {0.0F, -1.0F, 0.0F}));
}

TEST_CASE("queries: contact with a BoxSet: the deepest (ties to the lowest index) and all by index") {
    const P3::BoxSet set = MakeBoxes({{{-2.0F, -1.0F, -1.0F}, {0.0F, 0.0F, 1.0F}}, {{0.0F, -1.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}});
    const P3::Shape ball = P3::Ball{0.5F};
    auto seam = P3::FindDeepestBoxContact(ball, At(0.0F, 0.25F, 0.0F), set, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((seam && seam->has_value()));
    CHECK_MESSAGE((*seam)->box == 0, "[p3-boxset-contact-tie] " << (*seam)->box);
    CHECK((*seam)->contact.distance == -0.25F);
    auto right = P3::FindDeepestBoxContact(ball, At(0.5F, 0.25F, 0.0F), set, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((right && right->has_value()));
    CHECK((*right)->box == 1);
    auto one = P3::FindBoxContacts(ball, At(0.5F, 0.25F, 0.0F), set, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE(one);
    REQUIRE(one->size() == 1);
    CHECK((*one)[0].box == 1);
    auto both = P3::FindBoxContacts(ball, At(0.5F, 0.25F, 0.0F), set, At(0.0F, 0.0F, 0.0F), 0.1F);
    REQUIRE(both);
    REQUIRE_MESSAGE(both->size() == 2, "[p3-boxset-contacts-all]");
    CHECK((*both)[0].box == 0);
    CHECK((*both)[1].box == 1);
    auto generic = P3::FindContact(ball, At(0.5F, 0.25F, 0.0F), set, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE((generic && generic->has_value()));
    CHECK((*generic)->distance == (*right)->contact.distance);
}

TEST_CASE("queries: contact pairs outside the table return Unsupported") {
    auto box = P3::FindContact(P3::Cuboid{}, At(0.0F, 0.0F, 0.0F), P3::Ball{}, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE_FALSE(static_cast<bool>(box));
    CHECK_MESSAGE(box.error().code == P3::Physics3dErrorCode::Unsupported, "[p3-unsupported-contact]");
    auto set = P3::FindBoxContacts(P3::Cuboid{}, At(0.0F, 0.0F, 0.0F), P3::BoxSet{}, At(0.0F, 0.0F, 0.0F), 0.0F);
    REQUIRE_FALSE(static_cast<bool>(set));
    CHECK(set.error().code == P3::Physics3dErrorCode::Unsupported);
}

// ---- CastShape

TEST_CASE("queries: a sphere cast onto a floor: time, witnesses, normal, maxTime, touching at the end") {
    const P3::Shape ball = P3::Ball{0.5F};
    auto hit = P3::CastShape(ball, At(0.0F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE((hit && hit->has_value()));
    const P3::ShapeHit& h = **hit;
    CHECK_MESSAGE(h.time == 0.45F, "[p3-cast-sphere-floor] time " << h.time);
    CHECK(h.status == P3::ShapeHitStatus::Converged);
    CHECK(Same(h.normal1, {0.0F, -1.0F, 0.0F}));
    CHECK(Same(h.normal2, {0.0F, 1.0F, 0.0F}));
    CHECK(Same(h.witness1, {0.0F, 0.0F, 0.0F}));
    CHECK(Same(h.witness2, {0.0F, 0.0F, 0.0F}));
    auto skin = P3::CastShape(ball, At(0.0F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, Floor(), FloorPose(),
                              P3::ShapeCastOptions{1.0F, 0.5F, true});
    REQUIRE((skin && skin->has_value()));
    CHECK_MESSAGE((*skin)->time == 0.4F, "[p3-cast-target-distance] " << (*skin)->time);
    CHECK(Same((*skin)->witness1, {0.0F, 0.5F, 0.0F}));
    auto miss = P3::CastShape(ball, At(0.0F, 5.0F, 0.0F), {10.0F, 0.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE(miss);
    CHECK(!miss->has_value());
    auto shortCast = P3::CastShape(ball, At(0.0F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, Floor(), FloorPose(),
                                   P3::ShapeCastOptions{0.4F, 0.0F, true});
    REQUIRE(shortCast);
    CHECK_MESSAGE(!shortCast->has_value(), "[p3-cast-maxtime]");
    auto end = P3::CastShape(ball, At(0.0F, 5.0F, 0.0F), {0.0F, -4.5F, 0.0F}, Floor(), FloorPose());
    REQUIRE(end);
    REQUIRE_MESSAGE(end->has_value(), "[p3-cast-touch-end] touching at time 1 is a hit");
    CHECK((*end)->time == 1.0F);
}

TEST_CASE("queries: a cast that starts overlapping: stopAtPenetration, separating and tangential motion, zero motion") {
    const P3::Shape ball = P3::Ball{0.5F};
    const P3::Pose start = At(0.0F, 0.25F, 0.0F);
    auto stop = P3::CastShape(ball, start, {0.0F, 1.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE((stop && stop->has_value()));
    CHECK_MESSAGE((*stop)->status == P3::ShapeHitStatus::PenetratingOrWithinTargetDistance, "[p3-cast-penetrating]");
    CHECK((*stop)->time == 0.0F);
    CHECK(Same((*stop)->normal1, {0.0F, -1.0F, 0.0F}));
    const P3::ShapeCastOptions goOn{1.0F, 0.0F, false};
    auto away = P3::CastShape(ball, start, {0.0F, 1.0F, 0.0F}, Floor(), FloorPose(), goOn);
    REQUIRE(away);
    CHECK_MESSAGE(!away->has_value(), "[p3-cast-separating] moving away from an overlap is not a hit");
    auto along = P3::CastShape(ball, start, {1.0F, 0.0F, 0.0F}, Floor(), FloorPose(), goOn);
    REQUIRE(along);
    CHECK_MESSAGE(!along->has_value(), "[p3-cast-tangential] tangential motion is not a hit");
    auto into = P3::CastShape(ball, start, {0.0F, -1.0F, 0.0F}, Floor(), FloorPose(), goOn);
    REQUIRE((into && into->has_value()));
    CHECK_MESSAGE((*into)->status == P3::ShapeHitStatus::PenetratingOrWithinTargetDistance, "[p3-cast-into]");
    auto still = P3::CastShape(ball, start, {0.0F, 0.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE((still && still->has_value()));
    CHECK_MESSAGE((*still)->time == 0.0F, "[p3-cast-zero-motion] overlap");
    auto clear = P3::CastShape(ball, At(0.0F, 2.0F, 0.0F), {0.0F, 0.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE(clear);
    CHECK_MESSAGE(!clear->has_value(), "[p3-cast-zero-motion] clear");
    auto within = P3::CastShape(ball, At(0.0F, 0.75F, 0.0F), {0.0F, 0.0F, 0.0F}, Floor(), FloorPose(),
                                P3::ShapeCastOptions{1.0F, 0.5F, true});
    REQUIRE((within && within->has_value()));
    CHECK_MESSAGE((*within)->status == P3::ShapeHitStatus::PenetratingOrWithinTargetDistance, "[p3-cast-within-target]");
}

TEST_CASE("queries: a sphere cast onto a cuboid's edge and corner") {
    const P3::Shape cube = P3::Cuboid{{1.0F, 1.0F, 1.0F}};
    const P3::Shape ball = P3::Ball{0.5F};
    auto edge = P3::CastShape(ball, At(3.0F, 3.0F, 0.0F), {-2.0F, -2.0F, 0.0F}, cube, At(0.0F, 0.0F, 0.0F));
    REQUIRE((edge && edge->has_value()));
    const float r2 = 0.5F / std::sqrt(2.0F);
    CHECK_MESSAGE((*edge)->time == doctest::Approx((2.0F - r2) / 2.0F), "[p3-cast-edge] " << (*edge)->time);
    CHECK(Near((*edge)->normal1, {-1.0F / std::sqrt(2.0F), -1.0F / std::sqrt(2.0F), 0.0F}));
    CHECK(Near((*edge)->witness2, {1.0F, 1.0F, 0.0F}));
    auto corner = P3::CastShape(ball, At(3.0F, 3.0F, 3.0F), {-2.0F, -2.0F, -2.0F}, cube, At(0.0F, 0.0F, 0.0F));
    REQUIRE((corner && corner->has_value()));
    const float r3 = 0.5F / std::sqrt(3.0F);
    CHECK_MESSAGE((*corner)->time == doctest::Approx((2.0F - r3) / 2.0F), "[p3-cast-corner] " << (*corner)->time);
    CHECK(Near((*corner)->witness2, {1.0F, 1.0F, 1.0F}));
    // Passing the corner diagonally without touching it: the expanded box is entered, the rounded one is not.
    auto pass = P3::CastShape(ball, At(1.45F, 1.45F, -3.0F), {0.0F, 0.0F, 6.0F}, cube, At(0.0F, 0.0F, 0.0F));
    REQUIRE(pass);
    CHECK_MESSAGE(!pass->has_value(), "[p3-cast-edge-miss]");
    // A rotated cuboid (45 degrees about Y): its edge faces +X at x = sqrt(2).
    auto turned = P3::CastShape(ball, At(5.0F, 0.0F, 0.0F), {-5.0F, 0.0F, 0.0F}, cube,
                                At(0.0F, 0.0F, 0.0F, {0.0F, kS22, 0.0F, kC22}));
    REQUIRE((turned && turned->has_value()));
    CHECK((*turned)->time == doctest::Approx((5.0F - std::sqrt(2.0F) - 0.5F) / 5.0F));
}

TEST_CASE("queries: an upright capsule cast onto a floor and into a wall, also with the boxes turned about Y") {
    const P3::Shape capsule = P3::CapsuleY(1.0F, 0.5F);
    auto down = P3::CastShape(capsule, At(0.0F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE((down && down->has_value()));
    CHECK_MESSAGE((*down)->time == 0.35F, "[p3-cast-capsule-floor] " << (*down)->time);
    // 0.35 is not exact in binary: the witness lands within rounding of the floor (-4.4e-16 here).
    CHECK(Near((*down)->witness1, {0.0F, 0.0F, 0.0F}, 1.0e-6F));
    CHECK(Same((*down)->witness2, {0.0F, 0.0F, 0.0F}));
    CHECK(Same((*down)->normal1, {0.0F, -1.0F, 0.0F}));
    const P3::Shape wall = P3::Cuboid{{1.0F, 10.0F, 10.0F}};
    auto side = P3::CastShape(capsule, At(0.0F, 0.0F, 0.0F), {10.0F, 0.0F, 0.0F}, wall, At(5.0F, 0.0F, 0.0F));
    REQUIRE((side && side->has_value()));
    CHECK_MESSAGE((*side)->time == 0.35F, "[p3-cast-capsule-wall] " << (*side)->time);
    CHECK(Same((*side)->normal1, {1.0F, 0.0F, 0.0F}));
    CHECK(Same((*side)->witness1, {4.0F, 0.0F, 0.0F}));
    CHECK(Same((*side)->witness2, {4.0F, 0.0F, 0.0F}));
    // A low wall whose top edge is 0.25 below the capsule's lower cap centre: the cap hits the edge (the core
    // extends the box along Y; the edge of the extended box is a rounded edge).
    const P3::Shape low = P3::Cuboid{{1.0F, 0.25F, 10.0F}};
    auto ledge = P3::CastShape(capsule, At(0.0F, 1.0F, 0.0F), {10.0F, 0.0F, 0.0F}, low, At(5.0F, -0.5F, 0.0F));
    REQUIRE((ledge && ledge->has_value()));
    const float reach = std::sqrt(0.25F - 0.0625F);
    CHECK_MESSAGE((*ledge)->time == doctest::Approx((4.0F - reach) / 10.0F), "[p3-cast-capsule-ledge] " << (*ledge)->time);
    CHECK(Near((*ledge)->witness2, {4.0F, -0.25F, 0.0F}));
    CHECK(Near((*ledge)->witness1, {4.0F, -0.25F, 0.0F}));
    CHECK(Near((*ledge)->normal1, {reach / 0.5F, -0.5F, 0.0F}));
    // Any axis-parallel segment, not only along Y and not only centred: lying along X, and one from 0 to 2 in Y.
    auto lying = P3::CastShape(P3::Capsule{{-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 0.5F}, At(0.0F, 5.0F, 0.0F),
                               {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE_MESSAGE((lying && lying->has_value()), "[p3-cast-capsule-segment] lying along X");
    CHECK((*lying)->time == 0.45F);
    auto offset = P3::CastShape(P3::Capsule{{0.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}, 0.5F}, At(0.0F, 5.0F, 0.0F),
                                {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE_MESSAGE((offset && offset->has_value()), "[p3-cast-capsule-segment] not centred");
    CHECK((*offset)->time == 0.45F);
    CHECK(Near((*offset)->witness1, {0.0F, 0.0F, 0.0F}, 1.0e-6F));
    // Both turned about Y (by different angles): the capsule's axis in the box's frame stays exactly Y.
    auto turned = P3::CastShape(capsule, At(0.0F, 0.0F, 0.0F, {0.0F, kS22, 0.0F, kC22}), {10.0F, 0.0F, 0.0F}, wall,
                                At(5.0F, 0.0F, 0.0F, {0.0F, 1.0F, 0.0F, 0.0F}));
    REQUIRE_MESSAGE(static_cast<bool>(turned), "[p3-cast-capsule-axis] turned about Y is supported");
    REQUIRE(turned->has_value());
    CHECK((*turned)->time == doctest::Approx(0.35F));
}

TEST_CASE("queries: cast pairs outside the table return Unsupported") {
    const auto code = [](const auto& result) { return result.error().code; };
    auto tilted = P3::CastShape(P3::CapsuleY(1.0F, 0.5F), At(0.0F, 0.0F, 0.0F, {0.0F, 0.0F, kS22, kC22}),
                                {1.0F, 0.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE_FALSE(static_cast<bool>(tilted));
    CHECK_MESSAGE(code(tilted) == P3::Physics3dErrorCode::Unsupported, "[p3-unsupported-cast] tilted capsule");
    auto spheres = P3::CastShape(P3::Ball{}, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, P3::Ball{}, At(3.0F, 0.0F, 0.0F));
    REQUIRE_FALSE(static_cast<bool>(spheres));
    CHECK_MESSAGE(code(spheres) == P3::Physics3dErrorCode::Unsupported, "[p3-unsupported-cast] sphere-sphere");
    auto capsules = P3::CastShape(P3::Capsule{}, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, P3::Capsule{},
                                  At(3.0F, 0.0F, 0.0F));
    REQUIRE_FALSE(static_cast<bool>(capsules));
    CHECK(code(capsules) == P3::Physics3dErrorCode::Unsupported);
    auto box = P3::CastShape(P3::Cuboid{}, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE_FALSE(static_cast<bool>(box));
    CHECK(code(box) == P3::Physics3dErrorCode::Unsupported);
    auto diagonal = P3::CastShape(P3::Capsule{{-1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, 0.5F}, At(0.0F, 5.0F, 0.0F),
                                  {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE_FALSE(static_cast<bool>(diagonal));
    CHECK_MESSAGE(code(diagonal) == P3::Physics3dErrorCode::Unsupported, "[p3-unsupported-cast] diagonal segment");
    // A capsule of with coincident ends is a ball: any orientation.
    auto round = P3::CastShape(P3::CapsuleY(0.0F, 0.5F), At(0.0F, 5.0F, 0.0F, {0.0F, 0.0F, kS22, kC22}),
                               {0.0F, -10.0F, 0.0F}, Floor(), FloorPose());
    REQUIRE(static_cast<bool>(round));
    REQUIRE(round->has_value());
    CHECK((*round)->time == 0.45F);
}

TEST_CASE("queries: a cast onto a BoxSet returns the earliest box, ties to the lowest index") {
    const P3::BoxSet set = MakeBoxes({{{0.0F, -1.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}, {{-2.0F, -1.0F, -1.0F}, {0.0F, 0.0F, 1.0F}},
                                      {{-2.0F, -1.0F, -1.0F}, {-1.0F, 2.0F, 1.0F}}});
    auto seam = P3::CastShape(P3::Ball{0.5F}, At(0.0F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, set, At(0.0F, 0.0F, 0.0F));
    REQUIRE((seam && seam->has_value()));
    CHECK_MESSAGE((*seam)->time == 0.45F, "[p3-cast-boxset]");
    auto pillar = P3::CastShape(P3::Ball{0.5F}, At(-1.5F, 5.0F, 0.0F), {0.0F, -10.0F, 0.0F}, set, At(0.0F, 0.0F, 0.0F));
    REQUIRE((pillar && pillar->has_value()));
    CHECK_MESSAGE((*pillar)->time == 0.25F, "[p3-cast-boxset] the nearer box wins");
}

// ---- Programmer errors (section 4.2)

TEST_CASE("queries: invalid query arguments are programmer errors; IsValid tells them") {
    const M::Ray ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}};
    CHECK(!P3::IsValid(P3::Ball{0.0F}));
    CHECK(!P3::IsValid(P3::Capsule{{0.0F, std::numeric_limits<float>::infinity(), 0.0F}, {}, 0.5F}));
    CHECK(P3::IsValid(P3::CapsuleY(0.0F, 0.5F)));
    CHECK(!P3::IsValid(P3::CapsuleY(1.0F, 0.0F)));
    CHECK(!P3::IsValid(P3::Cuboid{{1.0F, std::numeric_limits<float>::quiet_NaN(), 1.0F}}));
    CHECK(!P3::IsValid(P3::Pose{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 0.0F}}));
    CHECK(!P3::IsValid(M::Ray{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}}));
    CHECK(!P3::IsValid(P3::ShapeCastOptions{-1.0F, 0.0F, true}));
    GYO_CHECK_ASSERTS(P3::CastRay(P3::Ball{-1.0F}, At(0.0F, 0.0F, 0.0F), ray, 1.0F));
    GYO_CHECK_ASSERTS(P3::CastRay(P3::Ball{}, At(0.0F, 0.0F, 0.0F), M::Ray{}, 1.0F));
    GYO_CHECK_ASSERTS(P3::CastRay(P3::Ball{}, At(0.0F, 0.0F, 0.0F), ray, std::numeric_limits<float>::quiet_NaN()));
    GYO_CHECK_ASSERTS(P3::FindContact(P3::Ball{}, At(0.0F, 0.0F, 0.0F), P3::Ball{}, At(0.0F, 0.0F, 0.0F), -1.0F));
    GYO_CHECK_ASSERTS(P3::CastShape(P3::Ball{}, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, Floor(), FloorPose(),
                                    P3::ShapeCastOptions{1.0F, -0.5F, true}));
}
