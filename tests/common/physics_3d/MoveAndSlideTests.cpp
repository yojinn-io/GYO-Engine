// The kinematic character mechanism of the physics_3d Module (MoveAndSlide.hpp; avian's MoveAndSlide):
// ProjectVelocity, CastMove, Depenetrate and MoveAndSlide run from a System over the author's Query, the callback's
// responses, determinism (the same inputs give the same bits), and a golden run for the three platforms.
// Each CHECK carries a tag in brackets ("[p3-mas-...]") for mutants to expect.
#include <doctest/doctest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/MoveAndSlide.hpp"
#include "engine/physics_3d/Physics3dModule.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"

namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace P3 = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;

namespace {

using Colliders = K::Query<K::Read<P3::Collider>, K::Read<T::Transform>, K::Without<K::ChildOf>>;

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("entity_ids"));
    units.modules.push_back({"entity_ids", std::make_unique<GYO::Engine::EntityIds::EntityIdsModule>()});
    units.rows.push_back(ModuleRow("transform"));
    units.modules.push_back({"transform", std::make_unique<T::TransformModule>()});
    units.rows.push_back(ModuleRow("physics_3d", {"transform"}));
    units.modules.push_back({"physics_3d", std::make_unique<P3::Physics3dModule>()});
    return units;
}

// The job a frame runs over the colliders.
std::function<void(const Colliders&)>& Job() {
    static std::function<void(const Colliders&)> job;
    return job;
}

K::SystemResult RunJob(Colliders colliders) {
    if (Job()) Job()(colliders);
    return {};
}

std::unique_ptr<O::Product> Make() {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& s) {
        K::SystemOptions o;
        o.name = "RunJob";
        K::Mount m;
        m.schedule = "Main";
        m.stage = "Update";
        o.mounts.push_back(std::move(m));
        s.Registrar().AddSystem<&RunJob>(std::move(o));
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

void Run(O::Product& p, std::function<void(const Colliders&)> job) {
    Job() = std::move(job);
    auto ran = p.RunFrame();
    Job() = {};
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

void Put(O::Product& p, const K::EntityId id, P3::Shape shape, const float x, const float y, const float z,
         const M::Quaternion rotation = {}) {
    WorldOf(p).Insert(id, P3::Collider{std::move(shape)});
    T::Transform t;
    t.translation = {x, y, z};
    t.rotation = rotation;
    WorldOf(p).Insert(id, t);
}

P3::Pose At(const float x, const float y, const float z) { return P3::Pose{{x, y, z}, {}}; }

bool Near(const M::Vec3 a, const M::Vec3 b, const float eps = 1.0e-5F) {
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
}
bool SameBits(const M::Vec3 a, const M::Vec3 b) {
    return std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
           std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y) &&
           std::bit_cast<std::uint32_t>(a.z) == std::bit_cast<std::uint32_t>(b.z);
}

// A wall whose -X face is at x = 4 (entity 1); with corner, a second whose -Z face is at z = 4 (entity 2).
std::unique_ptr<O::Product> WallScene(const bool corner = false) {
    auto p = Make();
    Put(*p, 1, P3::Cuboid{{1.0F, 10.0F, 10.0F}}, 5.0F, 0.0F, 0.0F);
    if (corner) Put(*p, 2, P3::Cuboid{{10.0F, 10.0F, 1.0F}}, 0.0F, 0.0F, 5.0F);
    return p;
}

const P3::Shape kBall = P3::Ball{0.5F};

// The brute force written out independently (subsets of one and two planes), for the comparison.
M::Vec3 ReferenceProjection(const M::Vec3 v, const std::vector<M::Vec3>& normals) {
    const auto valid = [&normals](const M::Vec3 p) {
        for (const M::Vec3& n : normals) {
            if (M::Dot(p, n) < -P3::kDotEpsilon) return false;
        }
        return true;
    };
    if (valid(v)) return v;
    std::vector<M::Vec3> candidates;
    for (const M::Vec3& n : normals) {
        if (M::Dot(n, v) < -P3::kDotEpsilon) candidates.push_back(v - n * M::Dot(n, v));
    }
    for (std::size_t i = 0; i < normals.size(); ++i) {
        for (std::size_t j = i + 1; j < normals.size(); ++j) {
            const M::Vec3 e = M::Cross(normals[i], normals[j]);
            if (M::Dot(e, e) >= P3::kDotEpsilon) candidates.push_back(e * (M::Dot(v, e) / M::Dot(e, e)));
        }
    }
    std::optional<M::Vec3> best;
    for (const M::Vec3& c : candidates) {
        if (!valid(c)) continue;
        if (!best || M::LengthSquared(v - c) < M::LengthSquared(v - *best)) best = c;
    }
    return best.value_or(M::Vec3{});
}

} // namespace

// ---- ProjectVelocity

TEST_CASE("move_and_slide: ProjectVelocity: inside the cone, one plane, an edge of two, a corner of three") {
    const M::Vec3 up{0.0F, 1.0F, 0.0F};
    const M::Vec3 east{1.0F, 0.0F, 0.0F};
    const M::Vec3 north{0.0F, 0.0F, 1.0F};
    const std::vector<M::Vec3> floor{up};
    CHECK_MESSAGE(SameBits(P3::ProjectVelocity({1.0F, 0.5F, 0.0F}, floor), {1.0F, 0.5F, 0.0F}), "[p3-mas-project] inside");
    CHECK_MESSAGE(SameBits(P3::ProjectVelocity({1.0F, 0.0F, 0.0F}, {}), {1.0F, 0.0F, 0.0F}), "[p3-mas-project] no plane");
    CHECK_MESSAGE(SameBits(P3::ProjectVelocity({1.0F, -1.0F, 0.0F}, floor), {1.0F, 0.0F, 0.0F}), "[p3-mas-project] plane");
    const std::vector<M::Vec3> edge{east, up};
    CHECK_MESSAGE(SameBits(P3::ProjectVelocity({-1.0F, -1.0F, 1.0F}, edge), {0.0F, 0.0F, 1.0F}), "[p3-mas-project] edge");
    const std::vector<M::Vec3> corner{east, up, north};
    CHECK_MESSAGE(SameBits(P3::ProjectVelocity({-1.0F, -1.0F, -1.0F}, corner), {}), "[p3-mas-project] corner");
    // Within kDotEpsilon of a plane counts as along it.
    CHECK(SameBits(P3::ProjectVelocity({1.0F, -0.004F, 0.0F}, floor), {1.0F, -0.004F, 0.0F}));
}

TEST_CASE("move_and_slide: ProjectVelocity agrees with an independent brute force on random planes") {
    std::uint64_t state = 0x5EED0101ULL;
    const auto next = [&state]() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return static_cast<float>(static_cast<double>(z >> 40) / 16777216.0 * 2.0 - 1.0);
    };
    int zero = 0;
    for (int q = 0; q < 2000; ++q) {
        std::vector<M::Vec3> normals;
        const int count = static_cast<int>((static_cast<std::uint32_t>(q) * 7U) % 6U);
        while (static_cast<int>(normals.size()) < count) {
            const M::Vec3 n{next(), next(), next()};
            if (M::LengthSquared(n) > 0.05F) normals.push_back(M::Normalize(n));
        }
        const M::Vec3 v{next() * 5.0F, next() * 5.0F, next() * 5.0F};
        const M::Vec3 actual = P3::ProjectVelocity(v, normals);
        const M::Vec3 expected = ReferenceProjection(v, normals);
        REQUIRE_MESSAGE(Near(actual, expected, 1.0e-4F), "[p3-mas-project-reference] case " << q);
        if (M::LengthSquared(actual) == 0.0F) ++zero;
    }
    CHECK(zero > 0);
}

// ---- CastMove

TEST_CASE("move_and_slide: CastMove stops skinWidth from the wall, measured along the normal") {
    auto p = WallScene();
    std::optional<P3::MoveHit> straight;
    std::optional<P3::MoveHit> oblique;
    Run(*p, [&](const Colliders& c) {
        auto s = P3::CastMove(c, kBall, At(0.0F, 0.0F, 0.0F), {10.0F, 0.0F, 0.0F}, 0.01F);
        REQUIRE(static_cast<bool>(s));
        straight = *s;
        auto o = P3::CastMove(c, kBall, At(0.0F, 0.0F, 0.0F), {10.0F, 0.0F, 10.0F}, 0.01F);
        REQUIRE(static_cast<bool>(o));
        oblique = *o;
    });
    REQUIRE_MESSAGE(straight.has_value(), "[p3-mas-cast-move] no hit");
    CHECK(straight->entity == 1);
    CHECK(straight->collisionDistance == doctest::Approx(3.5F));
    CHECK_MESSAGE(straight->distance == doctest::Approx(3.49F).epsilon(1e-6), "[p3-mas-pull-back] " << straight->distance);
    CHECK(Near(straight->normal, {-1.0F, 0.0F, 0.0F}));
    CHECK(Near(straight->point, {4.0F, 0.0F, 0.0F}));
    CHECK(!straight->Intersects());
    REQUIRE(oblique.has_value());
    // Moving the safe distance along the movement leaves the ball skinWidth from the wall along its normal.
    const float s = oblique->distance / std::sqrt(2.0F);
    CHECK_MESSAGE(4.0F - (s + 0.5F) == doctest::Approx(0.01F).epsilon(1e-4), "[p3-mas-pull-back] oblique " << s);
}

TEST_CASE("move_and_slide: CastMove with no movement casts skinWidth; a start inside is ignored unless moving in") {
    auto p = WallScene();
    std::optional<P3::MoveHit> close;
    std::optional<P3::MoveHit> far;
    std::optional<P3::MoveHit> away;
    std::optional<P3::MoveHit> into;
    Run(*p, [&](const Colliders& c) {
        close = *P3::CastMove(c, kBall, At(3.495F, 0.0F, 0.0F), {}, 0.01F);
        far = *P3::CastMove(c, kBall, At(0.0F, 0.0F, 0.0F), {}, 0.01F);
        away = *P3::CastMove(c, kBall, At(4.2F, 0.0F, 0.0F), {-1.0F, 0.0F, 0.0F}, 0.01F);
        into = *P3::CastMove(c, kBall, At(4.2F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 0.01F);
    });
    REQUIRE_MESSAGE(close.has_value(), "[p3-mas-cast-zero] within skinWidth along +X");
    CHECK(close->distance == 0.0F);
    CHECK(close->collisionDistance == doctest::Approx(0.005F).epsilon(1e-3));
    CHECK_MESSAGE(!far.has_value(), "[p3-mas-cast-zero] nothing near");
    CHECK_MESSAGE(!away.has_value(), "[p3-mas-cast-inside] moving out of an overlap is not a hit");
    REQUIRE_MESSAGE(into.has_value(), "[p3-mas-cast-inside] moving into an overlap");
    CHECK(into->Intersects());
    CHECK(into->distance == 0.0F);
}

// ---- Depenetrate

TEST_CASE("move_and_slide: Depenetrate pushes out of a wall and out of a corner to skinWidth") {
    auto p = WallScene(true);
    M::Vec3 wall{};
    M::Vec3 corner{};
    M::Vec3 clear{};
    Run(*p, [&](const Colliders& c) {
        wall = *P3::Depenetrate(c, kBall, At(3.8F, 0.0F, 0.0F), P3::DepenetrationConfig{});
        corner = *P3::Depenetrate(c, kBall, At(3.8F, 0.0F, 3.8F), P3::DepenetrationConfig{});
        clear = *P3::Depenetrate(c, kBall, At(0.0F, 0.0F, 0.0F), P3::DepenetrationConfig{});
    });
    CHECK_MESSAGE(Near(wall, {-0.31F, 0.0F, 0.0F}), "[p3-mas-depenetrate] wall " << wall.x);
    CHECK_MESSAGE(Near(corner, {-0.31F, 0.0F, -0.31F}), "[p3-mas-depenetrate] corner " << corner.x << " " << corner.z);
    CHECK(SameBits(clear, {}));
}

TEST_CASE("move_and_slide: a contact deeper than penetrationRejectionThreshold is left alone; 0 iterations is off") {
    auto p = WallScene();
    M::Vec3 deep{1.0F, 1.0F, 1.0F};
    M::Vec3 off{1.0F, 1.0F, 1.0F};
    Run(*p, [&](const Colliders& c) {
        deep = *P3::Depenetrate(c, P3::Ball{1.0F}, At(4.5F, 0.0F, 0.0F), P3::DepenetrationConfig{});
        P3::DepenetrationConfig none;
        none.iterations = 0;
        off = *P3::Depenetrate(c, kBall, At(3.8F, 0.0F, 0.0F), none);
    });
    CHECK_MESSAGE(SameBits(deep, {}), "a 1.5 m deep contact was resolved: " << deep.x);
    CHECK(SameBits(off, {}));
}

// ---- MoveAndSlide

TEST_CASE("move_and_slide: a free move goes the whole way; a move into a wall slides along it") {
    auto p = WallScene();
    P3::MoveAndSlideOutput free{};
    P3::MoveAndSlideOutput slide{};
    Run(*p, [&](const Colliders& c) {
        free = *P3::MoveAndSlide(c, kBall, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 1.0F, P3::MoveAndSlideConfig{});
        slide = *P3::MoveAndSlide(c, kBall, At(3.0F, 0.0F, 0.0F), {2.0F, 0.0F, 2.0F}, 1.0F, P3::MoveAndSlideConfig{});
    });
    CHECK_MESSAGE(SameBits(free.position, {1.0F, 0.0F, 0.0F}), "[p3-mas-free]");
    CHECK(SameBits(free.projectedVelocity, {1.0F, 0.0F, 0.0F}));
    CHECK_MESSAGE(slide.position.x == doctest::Approx(3.49F).epsilon(1e-3), "[p3-mas-slide] x " << slide.position.x);
    CHECK_MESSAGE(slide.position.z > 1.5F, "[p3-mas-slide] z " << slide.position.z);
    CHECK(Near(slide.projectedVelocity, {0.0F, 0.0F, 2.0F}, 1.0e-4F));
}

TEST_CASE("move_and_slide: a move into a corner stops there") {
    auto p = WallScene(true);
    P3::MoveAndSlideOutput out{};
    std::vector<K::EntityId> seen;
    Run(*p, [&](const Colliders& c) {
        out = *P3::MoveAndSlide(c, kBall, At(3.0F, 0.0F, 3.0F), {5.0F, 0.0F, 5.0F}, 1.0F, P3::MoveAndSlideConfig{},
                                P3::AcceptAll{}, [&seen](const P3::MoveAndSlideHitData& hit) {
                                    seen.push_back(hit.entity);
                                    return P3::MoveAndSlideHitResponse::Accept;
                                });
    });
    CHECK_MESSAGE(Near(out.position, {3.49F, 0.0F, 3.49F}, 2.0e-3F), "[p3-mas-corner] " << out.position.x << " " << out.position.z);
    CHECK_MESSAGE(Near(out.projectedVelocity, {}, 1.0e-4F), "[p3-mas-corner] velocity");
    // The sweep hits wall 1 first (a tie goes to the smaller EntityId); wall 2 comes in as a contact plane, and
    // onHit names it (not the sweep hit's entity, as avian does).
    REQUIRE(!seen.empty());
    CHECK(seen.front() == 1);
    CHECK_MESSAGE(std::find(seen.begin(), seen.end(), K::EntityId{2}) != seen.end(), "[p3-mas-onhit-entity] wall 2");
}

TEST_CASE("move_and_slide: an unfiltered Ball collider is Unsupported; a scaled collider is UnsupportedScale") {
    auto p = WallScene();
    Put(*p, 3, P3::Ball{0.5F}, 2.0F, 0.0F, 3.0F);  // another character, not filtered out
    std::optional<P3::Physics3dError> slide;
    std::optional<P3::Physics3dError> cast;
    std::optional<P3::Physics3dError> scaled;
    bool filtered = false;
    Run(*p, [&](const Colliders& c) {
        auto s = P3::MoveAndSlide(c, kBall, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 1.0F, P3::MoveAndSlideConfig{});
        if (!s) slide = s.error();
        auto m = P3::CastMove(c, kBall, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 0.01F);
        if (!m) cast = m.error();
        auto f = P3::MoveAndSlide(c, kBall, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 1.0F, P3::MoveAndSlideConfig{},
                                  [](const K::EntityId e) { return e != 3; });
        filtered = static_cast<bool>(f);
    });
    REQUIRE_MESSAGE(slide.has_value(), "[p3-mas-unsupported] MoveAndSlide swept against a Ball");
    CHECK(slide->code == P3::Physics3dErrorCode::Unsupported);
    CHECK(slide->detail.find("entity 3") != std::string::npos);
    REQUIRE_MESSAGE(cast.has_value(), "[p3-mas-unsupported] CastMove swept against a Ball");
    CHECK(cast->code == P3::Physics3dErrorCode::Unsupported);
    CHECK(cast->detail.find("entity 3") != std::string::npos);
    CHECK_MESSAGE(filtered, "[p3-mas-unsupported] filtering the Ball out works");
    T::Transform big;
    big.translation = {0.0F, -5.0F, 0.0F};
    big.scale = {2.0F, 1.0F, 1.0F};
    WorldOf(*p).Insert(4, P3::Collider{P3::Cuboid{{1.0F, 1.0F, 1.0F}}});
    WorldOf(*p).Insert(4, big);
    Run(*p, [&](const Colliders& c) {
        auto s = P3::MoveAndSlide(c, kBall, At(0.0F, 0.0F, 0.0F), {1.0F, 0.0F, 0.0F}, 1.0F, P3::MoveAndSlideConfig{},
                                  [](const K::EntityId e) { return e != 3; });
        if (!s) scaled = s.error();
    });
    REQUIRE_MESSAGE(scaled.has_value(), "[p3-mas-scale] a scaled collider was used");
    CHECK(scaled->code == P3::Physics3dErrorCode::UnsupportedScale);
    CHECK(scaled->detail == "entity 4");
}

TEST_CASE("move_and_slide: walking on a floor with the ground plane handed in keeps the height") {
    auto p = Make();
    Put(*p, 1, P3::Cuboid{{50.0F, 1.0F, 50.0F}}, 0.0F, -1.0F, 0.0F);  // top at y = 0
    P3::MoveAndSlideConfig config;
    config.planes.push_back({0.0F, 1.0F, 0.0F});
    P3::MoveAndSlideOutput out{};
    Run(*p, [&](const Colliders& c) {
        out = *P3::MoveAndSlide(c, kBall, At(0.0F, 0.51F, 0.0F), {3.0F, -1.0F, 0.0F}, 1.0F, config);
    });
    CHECK_MESSAGE(out.position.x == doctest::Approx(3.0F).epsilon(1e-3), "[p3-mas-floor] x " << out.position.x);
    CHECK_MESSAGE((out.position.y >= 0.5F && out.position.y <= 0.521F), "[p3-mas-floor] y " << out.position.y);
    CHECK(Near(out.projectedVelocity, {3.0F, 0.0F, 0.0F}, 1.0e-4F));
}

TEST_CASE("move_and_slide: onHit Ignore leaves the velocity as it was; Abort stops at the first hit") {
    auto p = WallScene();
    P3::MoveAndSlideOutput ignored{};
    P3::MoveAndSlideOutput aborted{};
    int calls = 0;
    std::vector<K::EntityId> seen;
    Run(*p, [&](const Colliders& c) {
        ignored = *P3::MoveAndSlide(c, kBall, At(3.0F, 0.0F, 0.0F), {2.0F, 0.0F, 2.0F}, 1.0F, P3::MoveAndSlideConfig{},
                                    P3::AcceptAll{}, [&seen](const P3::MoveAndSlideHitData& hit) {
                                        seen.push_back(hit.entity);
                                        return P3::MoveAndSlideHitResponse::Ignore;
                                    });
        aborted = *P3::MoveAndSlide(c, kBall, At(3.0F, 0.0F, 0.0F), {2.0F, 0.0F, 2.0F}, 1.0F, P3::MoveAndSlideConfig{},
                                    P3::AcceptAll{}, [&calls](const P3::MoveAndSlideHitData&) {
                                        ++calls;
                                        return P3::MoveAndSlideHitResponse::Abort;
                                    });
    });
    CHECK_MESSAGE(SameBits(ignored.projectedVelocity, {2.0F, 0.0F, 2.0F}), "[p3-mas-onhit-ignore]");
    REQUIRE(!seen.empty());
    CHECK(seen.front() == 1);
    CHECK(ignored.position.x <= 3.5F);  // never through the wall: the final depenetration keeps it out
    CHECK_MESSAGE(calls == 1, "[p3-mas-onhit-abort] calls " << calls);
    CHECK(SameBits(aborted.projectedVelocity, {2.0F, 0.0F, 2.0F}));
    CHECK(aborted.position.x == doctest::Approx(3.49F).epsilon(1e-3));
}

TEST_CASE("move_and_slide: a nearly parallel contact plane replaces the less blocking one") {
    // The handed-in plane is 2.9 degrees off the wall's normal (similar under cos 5 degrees). The sweep hit is
    // ignored, so the wall's contact plane meets the handed-in one in the merge: the wall's blocks more and must
    // replace it, which takes the whole velocity out (the handed-in one alone would leave some of it).
    auto p = WallScene();
    P3::MoveAndSlideConfig config;
    config.planes.push_back(M::Normalize(M::Vec3{-1.0F, 0.05F, 0.0F}));
    P3::MoveAndSlideOutput out{};
    Run(*p, [&](const Colliders& c) {
        int calls = 0;
        out = *P3::MoveAndSlide(c, kBall, At(3.0F, 0.0F, 0.0F), {2.0F, 0.0F, 0.0F}, 1.0F, config, P3::AcceptAll{},
                                [&calls](const P3::MoveAndSlideHitData&) {
                                    return ++calls == 1 ? P3::MoveAndSlideHitResponse::Ignore
                                                        : P3::MoveAndSlideHitResponse::Accept;
                                });
    });
    CHECK_MESSAGE(SameBits(out.projectedVelocity, {}), "[p3-mas-similar-planes] " << out.projectedVelocity.x << " "
                                                                                 << out.projectedVelocity.y);
}

TEST_CASE("move_and_slide: a capsule on a turned BoxSet: the same inputs give the same bits (a resimulation repeats it)") {
    const auto once = []() {
        auto p = Make();
        auto boxes = P3::BoxSet::Build(std::vector<M::Aabb>{{{-20.0F, -1.0F, -20.0F}, {20.0F, 0.0F, 20.0F}},
                                                            {{3.0F, 0.0F, -20.0F}, {4.0F, 3.0F, 20.0F}},
                                                            {{-20.0F, 0.0F, 5.0F}, {20.0F, 3.0F, 6.0F}}});
        REQUIRE(static_cast<bool>(boxes));
        Put(*p, 7, *boxes, 0.0F, 0.0F, 0.0F, {0.0F, 0.25881905F, 0.0F, 0.96592583F});  // 30 degrees about Y
        P3::MoveAndSlideConfig config;
        config.planes.push_back({0.0F, 1.0F, 0.0F});
        P3::MoveAndSlideOutput out{};
        Run(*p, [&](const Colliders& c) {
            out = *P3::MoveAndSlide(c, P3::CapsuleY(0.6F, 0.35F), At(0.3F, 0.96F, 0.7F), {4.1F, -0.3F, 1.7F}, 1.3F, config);
        });
        return out;
    };
    const P3::MoveAndSlideOutput a = once();
    const P3::MoveAndSlideOutput b = once();
    CHECK_MESSAGE((SameBits(a.position, b.position) && SameBits(a.projectedVelocity, b.projectedVelocity)),
                  "[p3-mas-deterministic]");
    // Golden (P3-5): generated on macOS x64 (AppleClang), 2026-11-23. Do not regenerate to make a platform pass.
    const std::uint32_t golden[6] = {0x409366DEU, 0x3F75C290U, 0x402CB1E5U, 0x333B7F70U, 0x00000000U, 0x33B50449U};
    const float actual[6] = {a.position.x, a.position.y, a.position.z, a.projectedVelocity.x, a.projectedVelocity.y,
                             a.projectedVelocity.z};
    std::string listing;
    for (const float v : actual) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "0x%08XU, ", static_cast<unsigned>(std::bit_cast<std::uint32_t>(v)));
        listing += buffer;
    }
    for (int i = 0; i < 6; ++i) {
        CHECK_MESSAGE(std::bit_cast<std::uint32_t>(actual[i]) == golden[i], "[p3-mas-golden] value " << i << ": " << listing);
    }
}

namespace {

struct SeamRun final {
    float maxY = -1.0e9F;
    float minSpeed = 1.0e9F;
    std::vector<M::Vec3> normals;  // every plane onHit saw, in order
};

// A capsule walking across the seam of two boxes with coplanar tops, from startX at speedX, 60 steps of 1/60 s.
SeamRun WalkSeam(const float startX, const float speedX) {
    auto p = Make();
    auto boxes = P3::BoxSet::Build(std::vector<M::Aabb>{{{-2.0F, -1.0F, -1.0F}, {0.0F, 0.0F, 1.0F}},
                                                        {{0.0F, -1.0F, -1.0F}, {2.0F, 0.0F, 1.0F}}});
    REQUIRE(static_cast<bool>(boxes));
    Put(*p, 1, *boxes, 0.0F, 0.0F, 0.0F);
    P3::MoveAndSlideConfig config;
    config.planes.push_back({0.0F, 1.0F, 0.0F});
    const P3::Shape capsule = P3::CapsuleY(0.5F, 0.5F);
    M::Vec3 position{startX, 1.0F + config.depenetration.skinWidth, 0.0F};  // resting on the tops at skin height
    SeamRun run;
    for (int step = 0; step < 60; ++step) {
        P3::MoveAndSlideOutput out{};
        Run(*p, [&](const Colliders& c) {
            auto moved = P3::MoveAndSlide(c, capsule, P3::Pose{position, {}}, {speedX, -1.0F, 0.0F}, 1.0F / 60.0F, config,
                                          P3::AcceptAll{}, [&run](const P3::MoveAndSlideHitData& hit) {
                                              run.normals.push_back(hit.normal);
                                              return P3::MoveAndSlideHitResponse::Accept;
                                          });
            REQUIRE(static_cast<bool>(moved));
            out = *moved;
        });
        position = out.position;
        run.maxY = std::max(run.maxY, position.y);
        run.minSpeed = std::min(run.minSpeed, std::abs(out.projectedVelocity.x));
    }
    return run;
}

std::string Describe(const SeamRun& run, const float rest) {
    std::string text = "max y " + std::to_string(run.maxY) + " (rest " + std::to_string(rest) + ", over by " +
                       std::to_string(run.maxY - rest) + "), min |vx| " + std::to_string(run.minSpeed) + ", planes:";
    for (const M::Vec3& n : run.normals) {
        if (n.x == 0.0F && n.y == 1.0F && n.z == 0.0F) continue;  // the ground plane: not listed
        text += " (" + std::to_string(n.x) + "," + std::to_string(n.y) + "," + std::to_string(n.z) + ")";
    }
    return text;
}

} // namespace

// S1 measured, decision pending: whether crossing a seam of coplanar boxes lifts the capsule or slows it (the
// edge of the next box can give a contact plane that is not the top face).
TEST_CASE("move_and_slide: crossing the seam of two coplanar boxes neither lifts nor slows the capsule" *
          doctest::skip(true)) {
    const float rest = 1.0F + P3::MoveAndSlideConfig{}.depenetration.skinWidth;
    for (const float direction : {-1.0F, 1.0F}) {
        const SeamRun run = WalkSeam(-1.5F * direction, 3.0F * direction);
        MESSAGE("from " << (direction < 0.0F ? "+X" : "-X") << ": " << Describe(run, rest));
        CHECK_MESSAGE(run.maxY <= rest + 1.0e-5F, "[p3-mas-seam] lifted: " << Describe(run, rest));
        CHECK_MESSAGE(run.minSpeed >= 3.0F - 1.0e-5F, "[p3-mas-seam] slowed: " << Describe(run, rest));
    }
}
