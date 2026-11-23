// BoxSet (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 4.4; P3-3): Build's checks, the
// content digest and Collider's equality and hash (CCR-13), and the contract that the hierarchy never changes a
// result: on randomized corpora (fixed-seed splitmix64) every query on a BoxSet returns, bit for bit, what the same
// single-box query over boxes 0..n-1 in index order returns (the nearest, ties to the lowest index; or all, by
// increasing index). The single-box query is the same query on a BoxSet of that one box.
#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace P3 = GYO::Engine::Physics3d;

namespace {

std::uint32_t Bits(const float v) { return std::bit_cast<std::uint32_t>(v); }
bool SameBits(const M::Vec3 a, const M::Vec3 b) {
    return Bits(a.x) == Bits(b.x) && Bits(a.y) == Bits(b.y) && Bits(a.z) == Bits(b.z);
}

std::vector<std::byte> HashOf(const P3::Collider& collider) {
    K::Hasher h;
    HashValue(h, collider);
    return {h.Data().begin(), h.Data().end()};
}

// splitmix64 (Steele, Lea, Flood 2014): fixed seeds, the same sequence on every platform.
class Random final {
public:
    explicit Random(const std::uint64_t seed) : state_(seed) {}
    std::uint64_t Next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // In [lo, hi), through double (exact for 24 bits), rounded to float once.
    float Uniform(const float lo, const float hi) {
        const double unit = static_cast<double>(Next() >> 40) / 16777216.0;
        return static_cast<float>(static_cast<double>(lo) + (static_cast<double>(hi) - static_cast<double>(lo)) * unit);
    }
    int Below(const int n) { return static_cast<int>(Next() % static_cast<std::uint64_t>(n)); }
    bool Chance(const int percent) { return Below(100) < percent; }
    // A multiple of 0.5 in [-limit, limit]: grid values make exact ties (shared faces, equal distances).
    float Grid(const int limit) { return static_cast<float>(Below(4 * limit + 1) - 2 * limit) * 0.5F; }

private:
    std::uint64_t state_;
};

M::Vec3 Point(Random& r, const float limit, const bool grid) {
    if (grid) {
        const int l = static_cast<int>(limit);
        return {r.Grid(l), r.Grid(l), r.Grid(l)};
    }
    return {r.Uniform(-limit, limit), r.Uniform(-limit, limit), r.Uniform(-limit, limit)};
}

M::Quaternion Rotation(Random& r) {
    for (;;) {
        const M::Quaternion q{r.Uniform(-1.0F, 1.0F), r.Uniform(-1.0F, 1.0F), r.Uniform(-1.0F, 1.0F),
                              r.Uniform(-1.0F, 1.0F)};
        const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (n > 0.01F) return q;  // the queries normalize; any non-zero quaternion is a valid pose
    }
}

M::Quaternion YRotation(Random& r) {
    for (;;) {
        const M::Quaternion q{0.0F, r.Uniform(-1.0F, 1.0F), 0.0F, r.Uniform(-1.0F, 1.0F)};
        if (q.y * q.y + q.w * q.w > 0.01F) return q;
    }
}

std::vector<M::Aabb> RandomBoxes(Random& r, const int count) {
    std::vector<M::Aabb> boxes;
    const float halves[] = {0.0F, 0.5F, 1.0F, 1.5F, 2.5F};
    for (int i = 0; i < count; ++i) {
        if (i > 0 && r.Chance(10)) {
            boxes.push_back(boxes[static_cast<std::size_t>(r.Below(i))]);  // a duplicate: equal results, lower index wins
            continue;
        }
        const bool grid = r.Chance(70);
        const M::Vec3 c = Point(r, 20.0F, grid);
        M::Vec3 h;
        if (grid) {
            h = {halves[r.Below(5)], halves[r.Below(5)], halves[r.Below(5)]};
        } else {
            h = {r.Uniform(0.0F, 3.0F), r.Uniform(0.0F, 3.0F), r.Uniform(0.0F, 3.0F)};
        }
        boxes.push_back({c - h, c + h});
    }
    return boxes;
}

struct Corpus final {
    P3::BoxSet set;
    std::vector<P3::BoxSet> singles;
    P3::Pose pose;
    bool yOnly = false;  // the set's rotation is about Y only (capsule casts are supported)
};

Corpus MakeCorpus(const std::uint64_t seed, const int count, const int poseKind) {
    Random r(seed);
    Corpus corpus;
    const std::vector<M::Aabb> boxes = RandomBoxes(r, count);
    auto built = P3::BoxSet::Build(boxes);
    REQUIRE(static_cast<bool>(built));
    corpus.set = *built;
    for (const M::Aabb& box : boxes) {
        auto single = P3::BoxSet::Build(std::vector<M::Aabb>{box});
        REQUIRE(static_cast<bool>(single));
        corpus.singles.push_back(*single);
    }
    switch (poseKind) {
    case 0: corpus.yOnly = true; break;
    case 1:
        corpus.pose = {Point(r, 5.0F, false), YRotation(r)};
        corpus.yOnly = true;
        break;
    default: corpus.pose = {Point(r, 5.0F, false), Rotation(r)}; break;
    }
    return corpus;
}

void CheckSame(const std::optional<P3::RayHit>& a, const std::optional<P3::RayHit>& b, const std::string& where) {
    REQUIRE_MESSAGE(a.has_value() == b.has_value(), "[p3-boxset-equivalence-ray] " << where);
    if (a) {
        REQUIRE_MESSAGE((Bits(a->t) == Bits(b->t) && SameBits(a->normal, b->normal)),
                        "[p3-boxset-equivalence-ray] " << where << " t " << a->t << " vs " << b->t);
    }
}

bool SameContact(const P3::Contact& a, const P3::Contact& b) {
    return SameBits(a.point1, b.point1) && SameBits(a.point2, b.point2) && SameBits(a.normal1, b.normal1) &&
           SameBits(a.normal2, b.normal2) && Bits(a.distance) == Bits(b.distance);
}

bool SameHit(const P3::ShapeHit& a, const P3::ShapeHit& b) {
    return Bits(a.time) == Bits(b.time) && SameBits(a.witness1, b.witness1) && SameBits(a.witness2, b.witness2) &&
           SameBits(a.normal1, b.normal1) && SameBits(a.normal2, b.normal2) && a.status == b.status;
}

P3::Shape MovingShape(Random& r) {
    if (r.Chance(50)) return P3::Sphere{r.Uniform(0.2F, 2.0F)};
    return P3::Capsule{r.Chance(10) ? 0.0F : r.Uniform(0.0F, 2.0F), r.Uniform(0.2F, 1.5F)};
}

struct Counts final {
    int rays = 0, rayHits = 0, contacts = 0, contactHits = 0, casts = 0, castHits = 0, unsupported = 0;
};

void RunQueries(const Corpus& corpus, const std::uint64_t seed, const int perKind, Counts& counts) {
    Random r(seed);
    const float inf = std::numeric_limits<float>::infinity();
    for (int q = 0; q < perKind; ++q) {
        // A ray: grid origins and axis-aligned directions make ties.
        const bool grid = r.Chance(40);
        M::Ray ray{Point(r, 25.0F, grid), {}};
        const int kind = r.Below(100);
        if (kind < 25) {
            const int axis = r.Below(3);
            const float sign = r.Chance(50) ? 1.0F : -1.0F;
            ray.direction = {axis == 0 ? sign : 0.0F, axis == 1 ? sign : 0.0F, axis == 2 ? sign : 0.0F};
        } else if (kind < 55) {
            // Diagonal directions from a grid origin reach grid edges and corners exactly: boxes tie on t while
            // entered through different faces, so the tie rule shows in the normal.
            ray.origin = Point(r, 25.0F, true);
            do {
                ray.direction = {static_cast<float>(r.Below(3) - 1), static_cast<float>(r.Below(3) - 1),
                                 static_cast<float>(r.Below(3) - 1)};
            } while (!P3::IsValid(ray));
        } else {
            do {
                ray.direction = Point(r, 1.0F, false);
            } while (!P3::IsValid(ray));
        }
        const float maxT = r.Chance(30) ? inf : r.Uniform(0.0F, 60.0F);
        std::optional<P3::RayHit> expected;
        for (const P3::BoxSet& single : corpus.singles) {
            const auto hit = P3::CastRay(single, corpus.pose, ray, maxT);
            if (hit && (!expected || hit->t < expected->t)) expected = hit;
        }
        const auto actual = P3::CastRay(corpus.set, corpus.pose, ray, maxT);
        CheckSame(actual, expected, "ray " + std::to_string(q));
        ++counts.rays;
        if (actual) ++counts.rayHits;
    }
    for (int q = 0; q < perKind; ++q) {
        // Grid shapes at grid points without rotation: distances to the grid boxes land exactly on multiples of
        // 0.5, so contacts sit exactly at the prediction limit (where a too small candidate box loses them).
        const bool exact = r.Chance(40);
        const P3::Shape shape = exact ? P3::Shape(P3::Sphere{0.5F * static_cast<float>(1 + r.Below(3))}) : MovingShape(r);
        const P3::Pose pose{Point(r, 22.0F, exact || r.Chance(40)), exact ? M::Quaternion{} : Rotation(r)};
        const float prediction = exact ? 0.5F * static_cast<float>(r.Below(3)) : (r.Chance(40) ? 0.0F : r.Uniform(0.0F, 1.5F));
        std::optional<P3::BoxContact> deepest;
        std::vector<P3::BoxContact> all;
        for (std::uint32_t i = 0; i < corpus.singles.size(); ++i) {
            auto one = P3::FindDeepestBoxContact(shape, pose, corpus.singles[i], corpus.pose, prediction);
            REQUIRE(static_cast<bool>(one));
            if (*one) {
                all.push_back({(*one)->contact, i});
                if (!deepest || (*one)->contact.distance < deepest->contact.distance) deepest = P3::BoxContact{(*one)->contact, i};
            }
        }
        auto actual = P3::FindDeepestBoxContact(shape, pose, corpus.set, corpus.pose, prediction);
        REQUIRE(static_cast<bool>(actual));
        REQUIRE_MESSAGE(actual->has_value() == deepest.has_value(), "[p3-boxset-equivalence-contact] " << q);
        if (deepest) {
            REQUIRE_MESSAGE(((*actual)->box == deepest->box && SameContact((*actual)->contact, deepest->contact)),
                            "[p3-boxset-equivalence-contact] " << q << " box " << (*actual)->box << " vs " << deepest->box);
        }
        auto every = P3::FindBoxContacts(shape, pose, corpus.set, corpus.pose, prediction);
        REQUIRE(static_cast<bool>(every));
        REQUIRE_MESSAGE(every->size() == all.size(), "[p3-boxset-equivalence-contacts] " << q);
        for (std::size_t i = 0; i < all.size(); ++i) {
            REQUIRE_MESSAGE(((*every)[i].box == all[i].box && SameContact((*every)[i].contact, all[i].contact)),
                            "[p3-boxset-equivalence-contacts] " << q << " at " << i);
        }
        ++counts.contacts;
        if (deepest) ++counts.contactHits;
    }
    for (int q = 0; q < perKind; ++q) {
        const bool exact = r.Chance(30);
        const P3::Shape shape = exact ? P3::Shape(P3::Sphere{0.5F * static_cast<float>(1 + r.Below(3))}) : MovingShape(r);
        // Capsules turn about Y only (their axis in the set's frame stays exactly Y when the set turns about Y).
        const bool capsule = std::holds_alternative<P3::Capsule>(shape);
        const P3::Pose pose{Point(r, 22.0F, exact || r.Chance(40)),
                            exact ? M::Quaternion{} : (capsule && !r.Chance(10) ? YRotation(r) : Rotation(r))};
        M::Vec3 motion = Point(r, 15.0F, exact || r.Chance(40));
        if (r.Chance(5)) motion = {};
        P3::ShapeCastOptions options;
        options.maxTime = r.Chance(50) ? 1.0F : r.Uniform(0.0F, 1.5F);
        options.targetDistance = r.Chance(50) ? 0.0F : r.Uniform(0.0F, 0.5F);
        options.stopAtPenetration = r.Chance(50);
        std::optional<P3::ShapeHit> expected;
        bool unsupported = false;
        for (const P3::BoxSet& single : corpus.singles) {
            auto hit = P3::CastShape(shape, pose, motion, single, corpus.pose, options);
            if (!hit) {
                unsupported = true;
                CHECK(hit.error().code == P3::Physics3dErrorCode::Unsupported);
                break;
            }
            if (*hit && (!expected || (*hit)->time < expected->time)) expected = **hit;
        }
        auto actual = P3::CastShape(shape, pose, motion, corpus.set, corpus.pose, options);
        REQUIRE_MESSAGE(static_cast<bool>(actual) == !unsupported, "[p3-boxset-equivalence-cast] support " << q);
        ++counts.casts;
        if (unsupported) {
            ++counts.unsupported;
            continue;
        }
        REQUIRE_MESSAGE(actual->has_value() == expected.has_value(), "[p3-boxset-equivalence-cast] " << q);
        if (expected) {
            REQUIRE_MESSAGE(SameHit(**actual, *expected), "[p3-boxset-equivalence-cast] " << q << " time "
                                                                                         << (*actual)->time << " vs "
                                                                                         << expected->time);
            ++counts.castHits;
        }
    }
}

} // namespace

TEST_CASE("box_set: Build checks every box and the empty set is valid") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto inverted = P3::BoxSet::Build(std::vector<M::Aabb>{{{0, 0, 0}, {1, 1, 1}}, {{0, 2, 0}, {1, 1, 1}}});
    REQUIRE_FALSE(static_cast<bool>(inverted));
    CHECK_MESSAGE(inverted.error().code == P3::Physics3dErrorCode::InvalidBox, "[p3-boxset-build] min > max");
    CHECK(inverted.error().detail == "box 1");
    auto notFinite = P3::BoxSet::Build(std::vector<M::Aabb>{{{0, 0, nan}, {1, 1, 1}}});
    REQUIRE_FALSE(static_cast<bool>(notFinite));
    CHECK_MESSAGE(notFinite.error().code == P3::Physics3dErrorCode::InvalidBox, "[p3-boxset-build] NaN");
    auto infinite = P3::BoxSet::Build(std::vector<M::Aabb>{{{0, 0, 0}, {1, 1, std::numeric_limits<float>::infinity()}}});
    REQUIRE_FALSE(static_cast<bool>(infinite));
    auto flat = P3::BoxSet::Build(std::vector<M::Aabb>{{{0, 0, 0}, {1, 0, 1}}});
    CHECK_MESSAGE(static_cast<bool>(flat), "[p3-boxset-build] a box of zero thickness is valid");
    auto empty = P3::BoxSet::Build(std::vector<M::Aabb>{});
    REQUIRE(static_cast<bool>(empty));
    CHECK(empty->Size() == 0);
    CHECK(*empty == P3::BoxSet{});
    CHECK(empty->Digest() == P3::BoxSet{}.Digest());
}

TEST_CASE("box_set: the digest and == follow the boxes' bits; Collider's == and hash agree (CCR-13)") {
    const std::vector<M::Aabb> boxes{{{0, 0, 0}, {1, 1, 1}}, {{2, 0, 0}, {3, 1, 1}}};
    std::vector<M::Aabb> negativeZero = boxes;
    negativeZero[0].minimum.x = -0.0F;
    std::vector<M::Aabb> swapped{boxes[1], boxes[0]};
    const P3::BoxSet a = *P3::BoxSet::Build(boxes);
    const P3::BoxSet b = *P3::BoxSet::Build(boxes);
    const P3::BoxSet z = *P3::BoxSet::Build(negativeZero);
    const P3::BoxSet s = *P3::BoxSet::Build(swapped);
    CHECK_MESSAGE(a.Digest() == b.Digest(), "[p3-boxset-digest] same boxes");
    CHECK_MESSAGE(a == b, "[p3-boxset-eq] separate bodies with the same boxes");
    CHECK_MESSAGE(a.Digest() != z.Digest(), "[p3-boxset-digest] -0 and +0 differ");
    CHECK_MESSAGE(!(a == z), "[p3-boxset-eq] -0 and +0 differ");
    CHECK_MESSAGE(a.Digest() != s.Digest(), "[p3-boxset-digest] the order counts");
    CHECK(!(a == s));
    const P3::Collider ca{a};
    const P3::Collider cb{b};
    const P3::Collider cz{z};
    CHECK(ca == cb);
    CHECK_MESSAGE(HashOf(ca) == HashOf(cb), "[p3-collider-hash] equal colliders hash equal");
    CHECK(!(ca == cz));
    CHECK_MESSAGE(HashOf(ca) != HashOf(cz), "[p3-collider-hash] the BoxSet digest is hashed");
    const P3::Collider sphere{P3::Sphere{0.5F}};
    const P3::Collider negative{P3::Sphere{-0.0F}};
    const P3::Collider zero{P3::Sphere{0.0F}};
    CHECK_MESSAGE(!(negative == zero), "[p3-collider-eq-bits] -0 and +0 differ");
    CHECK(HashOf(negative) != HashOf(zero));
    const P3::Collider capsule{P3::Capsule{0.5F, 0.5F}};
    CHECK_MESSAGE(!(sphere == capsule), "[p3-collider-eq] different shapes");
    CHECK_MESSAGE(HashOf(sphere) != HashOf(P3::Collider{P3::Cuboid{{0.5F, 0.5F, 0.5F}}}), "[p3-collider-hash] the kind");
    CHECK(HashOf(P3::Collider{P3::Cuboid{{0.5F, 0.5F, 0.5F}}}) != HashOf(P3::Collider{P3::Cuboid{{0.5F, 0.5F, 0.25F}}}));
}

TEST_CASE("box_set: every query equals the linear scan over the boxes in index order, bit for bit") {
    Counts counts;
    const struct {
        std::uint64_t seed;
        int boxes;
        int poseKind;
        int queries;
    } corpora[] = {
        {0x5EED0001ULL, 300, 0, 800},
        {0x5EED0002ULL, 300, 1, 800},
        {0x5EED0003ULL, 300, 2, 800},
        {0x5EED0004ULL, 3000, 1, 120},
    };
    for (const auto& c : corpora) {
        const Corpus corpus = MakeCorpus(c.seed, c.boxes, c.poseKind);
        RunQueries(corpus, c.seed ^ 0xA5A5A5A5ULL, c.queries, counts);
    }
    MESSAGE("rays " << counts.rays << " (" << counts.rayHits << " hit), contacts " << counts.contacts << " ("
                    << counts.contactHits << " hit), casts " << counts.casts << " (" << counts.castHits << " hit, "
                    << counts.unsupported << " unsupported)");
    // The corpora exercise both outcomes of every query (a corpus where nothing hits proves nothing).
    CHECK(counts.rayHits > counts.rays / 10);
    CHECK(counts.rayHits < counts.rays);
    CHECK(counts.contactHits > counts.contacts / 10);
    CHECK(counts.castHits > counts.casts / 10);
    CHECK(counts.unsupported > 0);
}

TEST_CASE("box_set: a node whose entry equals the best t is still searched for a lower index") {
    // Ten boxes spread along X: the hierarchy splits them on X, the left half (visited first) holds box 9, the
    // right half box 0. Both are reached at t = 1 (box 9 through -X, box 0 through -Y); the right half's node is
    // also entered at t = 1. The linear scan's answer is box 0.
    std::vector<M::Aabb> boxes;
    boxes.push_back({{-1.0F, 0.0F, 0.0F}, {30.0F, 1.0F, 1.0F}});  // box 0, centre x 14.5
    for (const float x : {-30.0F, -25.0F, -20.0F, -15.0F, 40.0F, 45.0F, 50.0F, 55.0F}) {
        boxes.push_back({{x - 0.5F, 0.0F, 49.5F}, {x + 0.5F, 1.0F, 50.5F}});  // off the ray
    }
    boxes.push_back({{0.0F, -5.0F, 0.0F}, {1.0F, 5.0F, 1.0F}});  // box 9, centre x 0.5
    auto set = P3::BoxSet::Build(boxes);
    REQUIRE(static_cast<bool>(set));
    const M::Ray ray{{-1.0F, -1.0F, 0.5F}, {1.0F, 1.0F, 0.0F}};
    const auto hit = P3::CastRay(*set, P3::Pose{}, ray, std::numeric_limits<float>::infinity());
    REQUIRE(hit.has_value());
    CHECK(hit->t == 1.0F);
    CHECK_MESSAGE((hit->normal.x == 0.0F && hit->normal.y == -1.0F), "[p3-boxset-ray-prune] the node was skipped");
}
