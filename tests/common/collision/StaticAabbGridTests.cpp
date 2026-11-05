#include <doctest/doctest.h>

#include "engine/collision/StaticAabbGrid.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// The static broadphase's equivalence contract (docs/architecture/collision.md):
// every query equals a linear scan of all boxes in index order. Judgment J4 of
// the v8-arch-big-map-engine-grid declaration
// (docs/architecture/plans/big-map/engine-grid.zh-Hant.md): 200 random sets and
// the edge cases, each query compared bit for bit with the scans written here.

using namespace Engine::Collision;
using Engine::Math::Aabb;
using Engine::Math::Ray;
using Engine::Math::Vec3;

namespace {

// The linear scans of the contract, written out.
std::vector<std::uint32_t> ScanOverlapping(const std::vector<Aabb>& boxes, const Aabb& b) {
    std::vector<std::uint32_t> result;
    for (std::uint32_t i = 0; i < boxes.size(); ++i) {
        const auto& a = boxes[i];
        if (a.minimum.x <= b.maximum.x && b.minimum.x <= a.maximum.x && a.minimum.y <= b.maximum.y &&
            b.minimum.y <= a.maximum.y && a.minimum.z <= b.maximum.z && b.minimum.z <= a.maximum.z)
            result.push_back(i);
    }
    return result;
}

std::optional<RaycastHit> ScanRaycast(const std::vector<Aabb>& boxes, const Ray& ray, const float distance,
                                      const TieBreak tie) {
    std::optional<RaycastHit> best;
    for (std::uint32_t i = 0; i < boxes.size(); ++i) {
        const auto h = RaycastAabb(ray, distance, boxes[i]);
        if (h && (!best || (tie == TieBreak::LowestIndex ? *h < best->distance : *h <= best->distance)))
            best = RaycastHit{*h, i};
    }
    return best;
}

std::optional<ContactHit> ScanSweep(const std::vector<Aabb>& boxes, const VerticalCapsule& capsule, const Vec3 d,
                                    const TieBreak tie) {
    std::optional<ContactHit> best;
    for (std::uint32_t i = 0; i < boxes.size(); ++i) {
        const auto c = SweepVerticalCapsuleAgainstAabb(capsule, d, boxes[i]);
        if (c && (!best || (tie == TieBreak::LowestIndex ? c->fraction < best->contact.fraction
                                                         : c->fraction <= best->contact.fraction)))
            best = ContactHit{*c, i};
    }
    return best;
}

std::vector<ContactHit> ScanSweepAll(const std::vector<Aabb>& boxes, const VerticalCapsule& capsule, const Vec3 d) {
    std::vector<ContactHit> result;
    for (std::uint32_t i = 0; i < boxes.size(); ++i)
        if (const auto c = SweepVerticalCapsuleAgainstAabb(capsule, d, boxes[i])) result.push_back({*c, i});
    return result;
}

std::vector<ContactHit> ScanOverlapAll(const std::vector<Aabb>& boxes, const VerticalCapsule& capsule) {
    std::vector<ContactHit> result;
    for (std::uint32_t i = 0; i < boxes.size(); ++i)
        if (const auto c = OverlapVerticalCapsuleAabb(capsule, boxes[i])) result.push_back({*c, i});
    return result;
}

// Bit comparisons.
std::uint32_t Bits(const float v) { return std::bit_cast<std::uint32_t>(v); }
bool Same(const Vec3 a, const Vec3 b) { return Bits(a.x) == Bits(b.x) && Bits(a.y) == Bits(b.y) && Bits(a.z) == Bits(b.z); }
bool Same(const Contact& a, const Contact& b) {
    return Bits(a.fraction) == Bits(b.fraction) && Same(a.position, b.position) && Same(a.normal, b.normal) &&
           Bits(a.penetrationDepth) == Bits(b.penetrationDepth);
}
bool Same(const std::optional<RaycastHit>& a, const std::optional<RaycastHit>& b) {
    if (a.has_value() != b.has_value()) return false;
    return !a || (Bits(a->distance) == Bits(b->distance) && a->index == b->index);
}
bool Same(const std::optional<ContactHit>& a, const std::optional<ContactHit>& b) {
    if (a.has_value() != b.has_value()) return false;
    return !a || (Same(a->contact, b->contact) && a->index == b->index);
}
bool Same(const std::vector<ContactHit>& a, const std::vector<ContactHit>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].index != b[i].index || !Same(a[i].contact, b[i].contact)) return false;
    return true;
}

// A platform-independent generator (splitmix64), so a failing set is the same
// set everywhere.
struct Random final {
    std::uint64_t state;
    std::uint64_t Next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
    // An integer in [0, n).
    std::uint32_t Below(const std::uint32_t n) { return static_cast<std::uint32_t>(Next() % n); }
    // A float on a quarter grid in [-limit, limit] (so faces coincide and
    // distances tie), or now and then an arbitrary float in that range.
    float Coordinate(const int limit) {
        if (Below(8) == 0) {
            const double unit = static_cast<double>(Next() >> 11U) / 9007199254740992.0;
            return static_cast<float>((unit * 2.0 - 1.0) * limit);
        }
        return static_cast<float>(static_cast<int>(Below(static_cast<std::uint32_t>(limit) * 8 + 1)) - limit * 4) *
               0.25F;
    }
};

Aabb RandomBox(Random& random, const int limit) {
    if (random.Below(40) == 0) return {{-100, -2, -100}, {100, 0, 100}}; // a floor across every cell
    const Vec3 corner{random.Coordinate(limit), random.Coordinate(limit), random.Coordinate(limit)};
    Vec3 size{static_cast<float>(random.Below(17)) * 0.25F, static_cast<float>(random.Below(17)) * 0.25F,
              static_cast<float>(random.Below(17)) * 0.25F};
    if (random.Below(10) != 0) {
        // Mostly solid boxes; the rest may be flat on an axis (zero thickness).
        if (size.x == 0) size.x = 0.5F;
        if (size.y == 0) size.y = 0.5F;
        if (size.z == 0) size.z = 0.5F;
    }
    return {corner, {corner.x + size.x, corner.y + size.y, corner.z + size.z}};
}

Vec3 RandomDirection(Random& random) {
    while (true) {
        Vec3 d{};
        switch (random.Below(4)) {
        case 0: { // along an axis: the other components exactly zero
            const float sign = random.Below(2) == 0 ? 1.0F : -1.0F;
            const auto axis = random.Below(3);
            d = {axis == 0 ? sign : 0.0F, axis == 1 ? sign : 0.0F, axis == 2 ? sign : 0.0F};
            break;
        }
        case 1: // in a plane of two axes
            d = {static_cast<float>(static_cast<int>(random.Below(5)) - 2), 0.0F,
                 static_cast<float>(static_cast<int>(random.Below(5)) - 2)};
            break;
        default:
            d = {random.Coordinate(3), random.Coordinate(3), random.Coordinate(3)};
            break;
        }
        if (d.x != 0 || d.y != 0 || d.z != 0) return d;
    }
}

VerticalCapsule RandomCapsule(Random& random, const int limit) {
    const float radii[] = {0.25F, 0.3F, 0.5F};
    const float radius = radii[random.Below(3)];
    const float height = 2.0F * radius + static_cast<float>(random.Below(9)) * 0.25F;
    return {{random.Coordinate(limit), random.Coordinate(limit), random.Coordinate(limit)}, height, radius};
}

Vec3 RandomDisplacement(Random& random) {
    switch (random.Below(5)) {
    case 0: return {};
    case 1: return {0.0F, -0.002F, 0.0F}; // the support probe of a character
    case 2: {
        const auto d = RandomDirection(random);
        const float scale = static_cast<float>(random.Below(9)) * 0.5F;
        return {d.x * scale, d.y * scale, d.z * scale};
    }
    default: return {random.Coordinate(4), random.Coordinate(4), random.Coordinate(4)};
    }
}

struct Tally final {
    std::size_t queries = 0;
    std::size_t compared = 0;
    std::size_t mismatches = 0;
    std::string firstMismatch;

    void Record(const bool same, const std::string& what) {
        ++queries;
        ++compared;
        if (!same) {
            if (mismatches == 0) firstMismatch = what;
            ++mismatches;
        }
    }
};

// Every query kind, compared with its scan; queries per kind per set.
// offset moves every query (for boxes placed far from the origin).
void CompareAll(const std::vector<Aabb>& boxes, const StaticAabbGrid& grid, Random& random, const int limit,
                const int queries, Tally& tally, const std::string& label, const Vec3 offset = {}) {
    const auto shift = [&](const Vec3 v) { return Vec3{v.x + offset.x, v.y + offset.y, v.z + offset.z}; };
    const float distances[] = {0.0F, 0.5F, 3.0F, 20.0F, 1.0e6F};
    for (int q = 0; q < queries; ++q) {
        const auto tag = label + " query " + std::to_string(q);
        // Overlapping, sometimes away from every box or flat.
        Vec3 a = shift({random.Coordinate(limit + 4), random.Coordinate(limit + 4), random.Coordinate(limit + 4)});
        Vec3 extent{static_cast<float>(random.Below(13)) * 0.5F, static_cast<float>(random.Below(13)) * 0.5F,
                    static_cast<float>(random.Below(13)) * 0.5F};
        const Aabb bounds{a, {a.x + extent.x, a.y + extent.y, a.z + extent.z}};
        tally.Record(grid.Overlapping(bounds) == ScanOverlapping(boxes, bounds), tag + " overlapping");
        // Rays, from anywhere (inside boxes included), both tie rules.
        const Ray ray{shift({random.Coordinate(limit + 2), random.Coordinate(limit + 2), random.Coordinate(limit + 2)}),
                      RandomDirection(random)};
        const float distance = distances[random.Below(5)];
        for (const auto tie : {TieBreak::LowestIndex, TieBreak::HighestIndex})
            tally.Record(Same(grid.RaycastNearest(ray, distance, tie), ScanRaycast(boxes, ray, distance, tie)),
                         tag + " raycast");
        // Capsules.
        auto capsule = RandomCapsule(random, limit);
        capsule.feet = shift(capsule.feet);
        const auto displacement = RandomDisplacement(random);
        for (const auto tie : {TieBreak::LowestIndex, TieBreak::HighestIndex})
            tally.Record(Same(grid.SweepVerticalCapsuleNearest(capsule, displacement, tie),
                              ScanSweep(boxes, capsule, displacement, tie)),
                         tag + " sweep nearest");
        tally.Record(Same(grid.SweepVerticalCapsuleAll(capsule, displacement),
                          ScanSweepAll(boxes, capsule, displacement)),
                     tag + " sweep all");
        tally.Record(Same(grid.OverlapVerticalCapsuleAll(capsule), ScanOverlapAll(boxes, capsule)),
                     tag + " overlap all");
    }
}

void Report(const Tally& tally, const char* name) {
    std::printf("%s: queries %zu compared %zu mismatches %zu\n", name, tally.queries, tally.compared,
                tally.mismatches);
    CHECK_MESSAGE(tally.mismatches == 0, tally.firstMismatch);
    CHECK(tally.compared == tally.queries);
}

} // namespace

TEST_CASE("grid equivalence: 200 random sets") {
    Tally tally;
    const float cells[] = {0.5F, 1.0F, 3.0F, 8.0F, 0.25F};
    for (std::uint64_t set = 0; set < 200; ++set) {
        Random random{0x6B1D0000ULL + set};
        const int limit = 4 + static_cast<int>(random.Below(20));
        const auto count = 1 + random.Below(300);
        std::vector<Aabb> boxes;
        for (std::uint32_t i = 0; i < count; ++i) {
            // Repeat an earlier box now and then: equal distances on distinct indices.
            if (!boxes.empty() && random.Below(12) == 0)
                boxes.push_back(boxes[random.Below(static_cast<std::uint32_t>(boxes.size()))]);
            else
                boxes.push_back(RandomBox(random, limit));
        }
        const StaticAabbGrid grid(boxes, cells[random.Below(5)]);
        REQUIRE(grid.Size() == boxes.size());
        CompareAll(boxes, grid, random, limit, 50, tally, "set " + std::to_string(set));
    }
    Report(tally, "grid equivalence (200 random sets)");
    CHECK(tally.queries == 200 * 50 * 7);
}

TEST_CASE("grid edge: ties pick the lowest or the highest index") {
    // Three equal boxes and one behind them; a ray, a sweep and a support
    // probe each meet the three at one distance.
    const std::vector<Aabb> boxes{{{2, 0, -1}, {3, 1, 1}}, {{2, 0, -1}, {3, 1, 1}}, {{5, 0, -1}, {6, 1, 1}},
                                  {{2, 0, -1}, {3, 1, 1}}};
    const StaticAabbGrid grid(boxes, 1.0F);
    const Ray ray{{0, 0.5F, 0}, {1, 0, 0}};
    const auto lowest = grid.RaycastNearest(ray, 10.0F, TieBreak::LowestIndex);
    const auto highest = grid.RaycastNearest(ray, 10.0F, TieBreak::HighestIndex);
    REQUIRE(lowest);
    REQUIRE(highest);
    CHECK(lowest->index == 0);
    CHECK(highest->index == 3);
    CHECK(lowest->distance == 2.0F);
    CHECK(Same(lowest, ScanRaycast(boxes, ray, 10.0F, TieBreak::LowestIndex)));
    CHECK(Same(highest, ScanRaycast(boxes, ray, 10.0F, TieBreak::HighestIndex)));
    const VerticalCapsule capsule{{0, 0, 0}, 1.0F, 0.25F};
    const Vec3 move{4, 0, 0};
    const auto sweepLow = grid.SweepVerticalCapsuleNearest(capsule, move, TieBreak::LowestIndex);
    const auto sweepHigh = grid.SweepVerticalCapsuleNearest(capsule, move, TieBreak::HighestIndex);
    REQUIRE(sweepLow);
    CHECK(sweepLow->index == 0);
    REQUIRE(sweepHigh);
    CHECK(sweepHigh->index == 3);
    CHECK(Same(sweepLow, ScanSweep(boxes, capsule, move, TieBreak::LowestIndex)));
    CHECK(Same(sweepHigh, ScanSweep(boxes, capsule, move, TieBreak::HighestIndex)));
    const auto all = grid.SweepVerticalCapsuleAll(capsule, move);
    // The capsule reaches x = 4.25, short of the box behind (x = 5).
    REQUIRE(all.size() == 3);
    CHECK(all[0].index == 0);
    CHECK(all[1].index == 1);
    CHECK(all[2].index == 3);
    CHECK(Same(all, ScanSweepAll(boxes, capsule, move)));
}

TEST_CASE("grid edge: cell boundaries, coplanar and tangent contact") {
    // Boxes exactly on cell lines, negative coordinates, faces shared.
    std::vector<Aabb> boxes;
    for (int x = -3; x < 3; ++x)
        for (int z = -3; z < 3; ++z) boxes.push_back({{float(x), -1, float(z)}, {float(x + 1), 0, float(z + 1)}});
    boxes.push_back({{-3, 0, -3}, {-2, 2, 3}}); // a wall on the cell line x = -2
    boxes.push_back({{1, 0, 1}, {1, 2, 2}});    // zero thickness, on a cell line
    for (const float cell : {1.0F, 2.0F, 0.5F, 3.0F}) {
        const StaticAabbGrid grid(boxes, cell);
        Tally tally;
        const auto label = "cell " + std::to_string(cell);
        // A box touching the floor's top face only (coplanar), and one touching a corner.
        for (const Aabb& b : {Aabb{{-1, 0, -1}, {1, 1, 1}}, Aabb{{3, 0, 3}, {4, 1, 4}}, Aabb{{-2, 0.5F, 0}, {-2, 0.5F, 0}},
                              Aabb{{1, 0, 1}, {1, 0, 1}}, Aabb{{10, 10, 10}, {11, 11, 11}}})
            tally.Record(grid.Overlapping(b) == ScanOverlapping(boxes, b), label + " overlapping");
        // Rays along the floor's top plane (coplanar), parallel to axes, from inside a box, zero length.
        const Ray rays[] = {{{-4, 0, 0.5F}, {1, 0, 0}},  {{0.5F, 0, -4}, {0, 0, 1}},    {{0.5F, -0.5F, 0.5F}, {0, 1, 0}},
                            {{-5, 1, 1.5F}, {1, 0, 0}},  {{0.5F, 5, 0.5F}, {0, -1, 0}}, {{1, 1, 1.5F}, {1, 0, 0}},
                            {{-2, 1, -4}, {0, 0, 1}},    {{-4, 3, -4}, {1, -1, 1}},     {{9, 9, 9}, {1, 1, 1}}};
        for (const auto& ray : rays)
            for (const float distance : {0.0F, 1.0F, 8.0F, 1.0e6F})
                for (const auto tie : {TieBreak::LowestIndex, TieBreak::HighestIndex})
                    tally.Record(Same(grid.RaycastNearest(ray, distance, tie), ScanRaycast(boxes, ray, distance, tie)),
                                 label + " raycast");
        // Capsules standing on the floor (tangent), sliding along it, against
        // the wall's face, starting inside a box, and the support probe.
        const VerticalCapsule capsules[] = {{{0, 0, 0}, 1.8F, 0.25F},        {{-1.75F, 0, 0}, 1.8F, 0.25F},
                                            {{0.5F, -0.5F, 0.5F}, 1.0F, 0.5F}, {{1, 0.5F, 1.5F}, 1.0F, 0.25F},
                                            {{2.75F, 0, 2.75F}, 1.0F, 0.25F}};
        const Vec3 moves[] = {{0, 0, 0}, {0, -0.002F, 0}, {-3, 0, 0}, {1, 0, 1}, {0, -1, 0}, {2, 0, 0}};
        for (const auto& capsule : capsules) {
            tally.Record(Same(grid.OverlapVerticalCapsuleAll(capsule), ScanOverlapAll(boxes, capsule)),
                         label + " overlap all");
            for (const auto& move : moves) {
                tally.Record(Same(grid.SweepVerticalCapsuleAll(capsule, move), ScanSweepAll(boxes, capsule, move)),
                             label + " sweep all");
                for (const auto tie : {TieBreak::LowestIndex, TieBreak::HighestIndex})
                    tally.Record(Same(grid.SweepVerticalCapsuleNearest(capsule, move, tie),
                                      ScanSweep(boxes, capsule, move, tie)),
                                 label + " sweep nearest");
            }
        }
        Report(tally, ("grid edge " + label).c_str());
    }
}

TEST_CASE("grid edge: a tiny cell length is doubled, results unchanged") {
    std::vector<Aabb> boxes{{{-50, 0, -50}, {-49, 1, -49}}, {{49, 0, 49}, {50, 1, 50}}, {{0, 0, 0}, {1, 1, 1}}};
    const StaticAabbGrid grid(boxes, 0.001F);
    CHECK(grid.CellLength() > 0.001);
    const double c = grid.CellLength();
    const double cells = (std::floor(100.0 / c) + 1) * (std::floor(1.0 / c) + 1) * (std::floor(100.0 / c) + 1);
    CHECK(cells <= static_cast<double>(StaticAabbGrid::MaxCells));
    // The doubling stops at the first length that fits.
    const double half = c / 2;
    CHECK((std::floor(100.0 / half) + 1) * (std::floor(1.0 / half) + 1) * (std::floor(100.0 / half) + 1) >
          static_cast<double>(StaticAabbGrid::MaxCells));
    Random random{42};
    Tally tally;
    CompareAll(boxes, grid, random, 50, 200, tally, "tiny cell");
    Report(tally, "grid edge tiny cell");
}

TEST_CASE("grid edge: boxes a million metres from the origin") {
    // Far coordinates: float spacing there is 1/16 m, so faces still coincide
    // and the candidate margins are relative.
    Tally tally;
    for (const Vec3 offset : {Vec3{1.0e6F, 0, 0}, Vec3{-1.0e6F, 0, 1.0e6F}, Vec3{0, -1.0e6F, -1.0e6F}}) {
        for (std::uint64_t set = 0; set < 10; ++set) {
            Random random{0xFA400000ULL + set};
            std::vector<Aabb> boxes;
            const auto count = 1 + random.Below(200);
            for (std::uint32_t i = 0; i < count; ++i) {
                auto box = RandomBox(random, 12);
                box.minimum = {box.minimum.x + offset.x, box.minimum.y + offset.y, box.minimum.z + offset.z};
                box.maximum = {box.maximum.x + offset.x, box.maximum.y + offset.y, box.maximum.z + offset.z};
                boxes.push_back(box);
            }
            const StaticAabbGrid grid(boxes, 2.0F);
            CompareAll(boxes, grid, random, 12, 30, tally, "far set " + std::to_string(set), offset);
        }
    }
    Report(tally, "grid edge far coordinates");
    CHECK(tally.queries == 3 * 10 * 30 * 7);
}

TEST_CASE("grid edge: empty grid and queries outside it") {
    const std::vector<Aabb> none;
    const StaticAabbGrid empty(none, 1.0F);
    CHECK(empty.Size() == 0);
    CHECK(empty.Overlapping({{0, 0, 0}, {1, 1, 1}}).empty());
    CHECK_FALSE(empty.RaycastNearest({{0, 0, 0}, {1, 0, 0}}, 10.0F, TieBreak::LowestIndex));
    CHECK(empty.SweepVerticalCapsuleAll({{0, 0, 0}, 1.0F, 0.25F}, {1, 0, 0}).empty());
    CHECK(empty.OverlapVerticalCapsuleAll({{0, 0, 0}, 1.0F, 0.25F}).empty());
    const std::vector<Aabb> one{{{0, 0, 0}, {1, 1, 1}}};
    const StaticAabbGrid grid(one, 1.0F);
    CHECK(grid.Overlapping({{5, 5, 5}, {6, 6, 6}}).empty());
    CHECK(grid.Overlapping({{-6, -6, -6}, {-5, -5, -5}}).empty());
    // A ray from far away, and one that starts beyond the box going away.
    const Ray far{{-1000, 0.5F, 0.5F}, {1, 0, 0}};
    CHECK(Same(grid.RaycastNearest(far, 2000.0F, TieBreak::LowestIndex), ScanRaycast(one, far, 2000.0F, TieBreak::LowestIndex)));
    CHECK(grid.RaycastNearest(far, 2000.0F, TieBreak::LowestIndex)->distance == 1000.0F);
    CHECK_FALSE(grid.RaycastNearest(far, 999.0F, TieBreak::LowestIndex));
    CHECK_FALSE(grid.RaycastNearest({{5, 0.5F, 0.5F}, {1, 0, 0}}, 1.0e6F, TieBreak::LowestIndex));
    CHECK(grid.RaycastNearest({{0.5F, 0.5F, 0.5F}, {0, 1, 0}}, 0.0F, TieBreak::LowestIndex)->distance == 0.0F);
    // A sweep that ends at the box from far away.
    const VerticalCapsule capsule{{-100, 0, 0.5F}, 1.0F, 0.25F};
    CHECK(Same(grid.SweepVerticalCapsuleNearest(capsule, {200, 0, 0}, TieBreak::LowestIndex),
               ScanSweep(one, capsule, {200, 0, 0}, TieBreak::LowestIndex)));
    CHECK(grid.SweepVerticalCapsuleNearest(capsule, {200, 0, 0}, TieBreak::LowestIndex).has_value());
}
