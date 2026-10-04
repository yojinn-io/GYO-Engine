#include "CollisionCorpus.hpp"

#include "engine/collision/Collision.hpp"

#include <bit>
#include <cstdio>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>

namespace Engine::Test::CollisionCorpus {
namespace {

using namespace Engine::Collision;
using Engine::Math::Aabb;
using Engine::Math::Capsule;
using Engine::Math::Ray;
using Engine::Math::Segment;
using Engine::Math::Vec3;

// Inputs come from integers so every platform builds the same float bits.
// SplitMix64 is fully specified by its integer arithmetic.
class Generator final {
public:
    explicit Generator(std::uint64_t seed) noexcept : state_(seed) {}
    std::uint64_t Next() noexcept {
        std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }
    // A uniform integer in [low, high].
    int Range(int low, int high) noexcept {
        return low + static_cast<int>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
    // A multiple of 1/128 in [-extent, extent] (exact in float).
    float Grid(float extent) noexcept {
        const int steps = static_cast<int>(extent * 128.0f);
        return static_cast<float>(Range(-steps, steps)) / 128.0f;
    }
    Vec3 GridVec(float extent) noexcept { return {Grid(extent), Grid(extent), Grid(extent)}; }

private:
    std::uint64_t state_;
};

constexpr float Ulp20 = 1.0f / 1048576.0f;  // 2^-20

std::string Hex(float value) {
    char text[9];
    std::snprintf(text, sizeof(text), "%08x", static_cast<unsigned>(std::bit_cast<std::uint32_t>(value)));
    return text;
}
std::string Hex(Vec3 value) { return Hex(value.x) + "," + Hex(value.y) + "," + Hex(value.z); }
std::string Describe(const VerticalCapsule& c) { return Hex(c.feet) + "/" + Hex(c.height) + "/" + Hex(c.radius); }
std::string Describe(const Aabb& b) { return Hex(b.minimum) + "/" + Hex(b.maximum); }
std::string Describe(const Capsule& c) { return Hex(c.segmentStart) + "/" + Hex(c.segmentEnd) + "/" + Hex(c.radius); }
std::string Result(const std::optional<float>& hit) { return hit ? "hit=" + Hex(*hit) : std::string("miss"); }
std::string Result(const std::optional<Contact>& hit) {
    if (!hit) return "miss";
    return "f=" + Hex(hit->fraction) + " p=" + Hex(hit->position) + " n=" + Hex(hit->normal) +
           " d=" + Hex(hit->penetrationDepth);
}

// Valid targets: boxes strictly ordered on every axis, capsules strictly
// taller than two radii (both the current checks and any planned stricter
// validity check accept them).
const std::array<Aabb, 5> Boxes{{
    {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {{-2.0f, 0.0f, -0.5f}, {2.0f, 3.0f, 0.5f}},
    {{0.0f, 0.0f, 0.0f}, {4.0f, 0.0625f, 4.0f}},
    {{1000.0f, 0.0f, 1000.0f}, {1001.0f, 1.0f, 1001.0f}},
    {{0.25f, 0.25f, 0.25f}, {0.3125f, 0.3125f, 0.3125f}},
}};
const std::array<VerticalCapsule, 5> Capsules{{
    {{0.0f, 0.0f, 0.0f}, 1.8f, 0.25f},
    {{1.0f, 0.4f, -2.0f}, 1.8f, 0.25f},
    {{0.0f, 1.0f, 0.0f}, 0.875f, 0.4f},
    {{-3.0f, -1.0f, 2.0f}, 4.0f, 1.0f},
    {{1000.0f, 0.0f, 1000.0f}, 1.8f, 0.25f},
}};
const std::array<Capsule, 3> GeneralCapsules{{
    {{-1.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, 0.25f},
    {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, 0.5f},
    {{0.5f, 0.0f, -1.0f}, {-0.5f, 2.0f, 1.0f}, 0.375f},
}};
constexpr std::array<float, 3> SweepRadii{0.0f, 0.05f, 0.25f};
constexpr std::array<float, 4> MaximumDistances{0.5f, 4.0f, 16.0f, 64.0f};
constexpr std::array<float, 4> ShallowDepths{5.0e-8f, 5.0e-7f, 5.0e-6f, 5.0e-5f};

Vec3 Centre(const Aabb& b) { return (b.minimum + b.maximum) * 0.5f; }
Vec3 Centre(const VerticalCapsule& c) { return {c.feet.x, c.feet.y + c.height * 0.5f, c.feet.z}; }

struct RayCase final {
    std::string tag;
    Ray ray;
    float maximumDistance{};
    float sweepRadius{};
};
struct SweepCase final {
    std::string tag;
    Segment path;
    float sweepRadius{};
};
struct MoveCase final {
    std::string tag;
    VerticalCapsule capsule;
    Vec3 displacement{};
};

std::vector<RayCase> BoxRays(const Aabb& b, Generator& random) {
    const Vec3 m = b.minimum, M = b.maximum, c = Centre(b);
    std::vector<RayCase> cases{
        {"face+x", {{m.x - 2.0f, c.y, c.z}, {1, 0, 0}}, 8.0f},
        {"face-x", {{M.x + 2.0f, c.y, c.z}, {-1, 0, 0}}, 8.0f},
        {"face+y", {{c.x, m.y - 2.0f, c.z}, {0, 1, 0}}, 8.0f},
        {"face-y", {{c.x, M.y + 2.0f, c.z}, {0, -1, 0}}, 8.0f},
        {"face+z", {{c.x, c.y, m.z - 2.0f}, {0, 0, 1}}, 8.0f},
        {"face-z", {{c.x, c.y, M.z + 2.0f}, {0, 0, -1}}, 8.0f},
        {"inside", {c, {1, 0, 0}}, 8.0f},
        {"on-top-plane-parallel", {{m.x - 2.0f, M.y, c.z}, {1, 0, 0}}, 8.0f},
        {"above-top-plane-parallel", {{m.x - 2.0f, M.y + Ulp20, c.z}, {1, 0, 0}}, 8.0f},
        {"on-edge-parallel", {{m.x - 2.0f, M.y, M.z}, {1, 0, 0}}, 8.0f},
        {"near-parallel-1e-6", {{m.x - 2.0f, M.y + 1.0e-6f, c.z}, {1, -1.0e-6f, 0}}, 8.0f},
        {"near-parallel-above-epsilon", {{m.x - 2.0f, M.y + 1.0e-6f, c.z}, {1, -1.0000002e-6f, 0}}, 8.0f},
        {"near-parallel-below-epsilon", {{m.x - 2.0f, M.y + 1.0e-6f, c.z}, {1, -9.999999e-7f, 0}}, 8.0f},
        {"max-equals-hit", {{m.x - 2.0f, c.y, c.z}, {1, 0, 0}}, 2.0f},
        {"max-short-of-hit", {{m.x - 2.0f, c.y, c.z}, {1, 0, 0}}, 2.0f - 2.0f * Ulp20},
        {"zero-max-on-face", {{m.x, c.y, c.z}, {1, 0, 0}}, 0.0f},
        {"zero-max-outside", {{m.x - Ulp20, c.y, c.z}, {1, 0, 0}}, 0.0f},
        {"long", {{m.x - 10000.0f, c.y, c.z}, {1, 0, 0}}, 20000.0f},
        {"long-diagonal", {{c.x - 6000.0f, c.y - 6000.0f, c.z + 3000.0f}, {2, 2, -1}}, 20000.0f},
        {"unnormalized", {{m.x - 2.0f, c.y, c.z}, {1000, 0, 0}}, 8.0f},
    };
    for (int index = 0; index < 150; ++index) {
        const Vec3 origin = c + random.GridVec(8.0f);
        Vec3 target = c + random.GridVec(1.5f);
        if (target.x == origin.x && target.y == origin.y && target.z == origin.z) target.x += 1.0f;
        cases.push_back({"random", {origin, target - origin},
            MaximumDistances[static_cast<std::size_t>(random.Range(0, 3))]});
    }
    return cases;
}

std::vector<RayCase> CapsuleRays(const VerticalCapsule& k, Generator& random) {
    const float r = k.radius, bottom = k.feet.y + r, top = k.feet.y + k.height - r;
    const float middle = (bottom + top) * 0.5f;
    const Vec3 f = k.feet;
    std::vector<RayCase> cases{
        {"side", {{f.x - 3.0f, middle, f.z}, {1, 0, 0}}, 8.0f},
        {"side-tangent", {{f.x - 3.0f, middle, f.z + r}, {1, 0, 0}}, 8.0f},
        {"side-tangent-outside", {{f.x - 3.0f, middle, f.z + r + Ulp20}, {1, 0, 0}}, 8.0f},
        {"side-tangent-inside", {{f.x - 3.0f, middle, f.z + r - Ulp20}, {1, 0, 0}}, 8.0f},
        {"top-down", {{f.x, f.y + k.height + 2.0f, f.z}, {0, -1, 0}}, 8.0f},
        {"bottom-up", {{f.x, f.y - 2.0f, f.z}, {0, 1, 0}}, 8.0f},
        {"top-cap-graze", {{f.x - 3.0f, f.y + k.height, f.z}, {1, 0, 0}}, 8.0f},
        {"axis-parallel-on-side", {{f.x + r, f.y - 2.0f, f.z}, {0, 1, 0}}, 8.0f},
        {"inside", {Centre(k), {1, 0, 0}}, 8.0f},
        {"max-equals-hit", {{f.x - 3.0f, middle, f.z}, {1, 0, 0}}, 3.0f - r},
        {"max-short-of-hit", {{f.x - 3.0f, middle, f.z}, {1, 0, 0}}, 3.0f - r - 2.0f * Ulp20},
        {"steep-1e-3", {{f.x - 0.5f, f.y + k.height + 4.0f, f.z}, {1.0e-3f, -1, 0}}, 16.0f},
        {"steep-above-epsilon", {{f.x - 0.5f, f.y + k.height + 4.0f, f.z}, {1.0001e-3f, -1, 0}}, 16.0f},
        {"steep-below-epsilon", {{f.x - 0.5f, f.y + k.height + 4.0f, f.z}, {0.9999e-3f, -1, 0}}, 16.0f},
        {"long", {{f.x - 10000.0f, middle, f.z}, {1, 0, 0}}, 20000.0f},
        {"unnormalized", {{f.x - 3.0f, middle, f.z}, {0.001f, 0, 0}}, 8.0f},
    };
    for (int index = 0; index < 150; ++index) {
        const Vec3 origin = Centre(k) + random.GridVec(8.0f);
        Vec3 target = Centre(k) + random.GridVec(k.height);
        if (target.x == origin.x && target.y == origin.y && target.z == origin.z) target.y += 1.0f;
        cases.push_back({"random", {origin, target - origin},
            MaximumDistances[static_cast<std::size_t>(random.Range(0, 3))]});
    }
    std::vector<RayCase> withRadii;
    for (const float sweep : SweepRadii)
        for (RayCase item : cases) {
            item.sweepRadius = sweep;
            withRadii.push_back(std::move(item));
        }
    return withRadii;
}

std::vector<SweepCase> CapsuleSweeps(const VerticalCapsule& k, Generator& random) {
    const float r = k.radius, middle = k.feet.y + k.height * 0.5f;
    const Vec3 f = k.feet;
    std::vector<SweepCase> cases{
        {"through", {{f.x - 3.0f, middle, f.z}, {f.x + 3.0f, middle, f.z}}},
        {"stationary-inside", {Centre(k), Centre(k)}},
        {"stationary-outside", {{f.x + 2.0f, middle, f.z}, {f.x + 2.0f, middle, f.z}}},
        {"short-5e-7-toward", {{f.x - r - 4.0e-7f, middle, f.z}, {f.x - r + 1.0e-7f, middle, f.z}}},
        {"short-2e-6-toward", {{f.x - r - 1.0e-6f, middle, f.z}, {f.x - r + 1.0e-6f, middle, f.z}}},
        {"graze", {{f.x - 3.0f, middle, f.z + r}, {f.x + 3.0f, middle, f.z + r}}},
        {"stops-at-surface", {{f.x - 3.0f, middle, f.z}, {f.x - r, middle, f.z}}},
        {"stops-short", {{f.x - 3.0f, middle, f.z}, {f.x - r - Ulp20 * 4.0f, middle, f.z}}},
        {"long", {{f.x - 5000.0f, middle, f.z}, {f.x + 5000.0f, middle, f.z}}},
    };
    for (int index = 0; index < 150; ++index) {
        const Vec3 start = Centre(k) + random.GridVec(6.0f);
        const Vec3 end = random.Range(0, 9) == 0 ? start : Centre(k) + random.GridVec(6.0f);
        cases.push_back({"random", {start, end}});
    }
    std::vector<SweepCase> withRadii;
    for (const float sweep : SweepRadii)
        for (SweepCase item : cases) {
            item.sweepRadius = sweep;
            withRadii.push_back(std::move(item));
        }
    return withRadii;
}

// A body capsule around a box: touching each side, just apart, penetrating,
// resting on top, hanging below, and random placements.
std::vector<MoveCase> BodiesAroundBox(const Aabb& b, Generator& random) {
    const Vec3 m = b.minimum, M = b.maximum, c = Centre(b);
    const float r = 0.25f, h = 1.75f;
    const float y = M.y - h * 0.5f > m.y ? m.y : c.y - h * 0.5f;
    std::vector<MoveCase> cases{
        {"touch-x", {{m.x - r, y, c.z}, h, r}, {1, 0, 0}},
        {"touch-x-away", {{m.x - r, y, c.z}, h, r}, {-1, 0, 0}},
        {"touch-x-tangent", {{m.x - r, y, c.z}, h, r}, {0, 0, 1}},
        {"touch-x-still", {{m.x - r, y, c.z}, h, r}, {}},
        {"apart-x", {{m.x - r - Ulp20 * 4.0f, y, c.z}, h, r}, {1, 0, 0}},
        {"inside", {{c.x, y, c.z}, h, r}, {1, 0, 0}},
        {"on-top", {{c.x, M.y, c.z}, h, r}, {0, 0, 0}},
        {"on-top-slide", {{c.x, M.y, c.z}, h, r}, {0.5f, 0, 0.25f}},
        {"on-top-fall", {{c.x, M.y, c.z}, h, r}, {0, -0.5f, 0}},
        {"above-land", {{c.x, M.y + 0.5f, c.z}, h, r}, {0, -1, 0}},
        {"below-touch", {{c.x, m.y - h, c.z}, h, r}, {0, 1, 0}},
        {"corner-diagonal", {{m.x - 1.0f, y, m.z - 1.0f}, h, r}, {2, 0, 2}},
        {"edge-graze", {{m.x - 2.0f, y, m.z - r}, h, r}, {4, 0, 0}},
        {"tiny-into", {{m.x - r - 5.0e-8f, y, c.z}, h, r}, {1.0e-7f, 0, 0}},
        {"high-speed", {{m.x - 2.0f, y, c.z}, h, r}, {1000, 0, 0}},
    };
    // Initial overlaps around the separating-contact tolerance: a shallow
    // overlap may move away or slide; a deeper one is reported as blocking.
    for (const float depth : ShallowDepths) {
        cases.push_back({"shallow-away", {{m.x - r + depth, y, c.z}, h, r}, {-1, 0, 0}});
        cases.push_back({"shallow-tangent", {{m.x - r + depth, y, c.z}, h, r}, {0, 0, 1}});
    }
    for (int index = 0; index < 150; ++index) {
        const Vec3 feet = c + random.GridVec(4.0f);
        const float radius = static_cast<float>(random.Range(16, 128)) / 128.0f;
        const float height = 2.0f * radius + static_cast<float>(random.Range(1, 256)) / 128.0f;
        const Vec3 move = random.Range(0, 9) == 0 ? Vec3{} : random.GridVec(4.0f);
        cases.push_back({"random", {feet, height, radius}, move});
    }
    return cases;
}

std::vector<MoveCase> BodiesAroundCapsule(const VerticalCapsule& t, Generator& random) {
    const float r = 0.25f, h = 1.75f, gap = t.radius + r;
    const Vec3 f = t.feet;
    std::vector<MoveCase> cases{
        {"touch-side", {{f.x - gap, f.y, f.z}, h, r}, {1, 0, 0}},
        {"touch-side-away", {{f.x - gap, f.y, f.z}, h, r}, {-1, 0, 0}},
        {"touch-side-tangent", {{f.x - gap, f.y, f.z}, h, r}, {0, 0, 1}},
        {"touch-side-still", {{f.x - gap, f.y, f.z}, h, r}, {}},
        {"apart-side", {{f.x - gap - Ulp20 * 4.0f, f.y, f.z}, h, r}, {1, 0, 0}},
        {"coincident", {f, h, r}, {1, 0, 0}},
        {"stacked", {{f.x, f.y + t.height, f.z}, h, r}, {0, -1, 0}},
        {"pass-by", {{f.x - 3.0f, f.y, f.z + gap}, h, r}, {6, 0, 0}},
        {"tiny-into", {{f.x - gap - 5.0e-8f, f.y, f.z}, h, r}, {1.0e-7f, 0, 0}},
        {"high-speed", {{f.x - 2.0f, f.y, f.z}, h, r}, {1000, 0, 0}},
    };
    for (const float depth : ShallowDepths) {
        cases.push_back({"shallow-away", {{f.x - gap + depth, f.y, f.z}, h, r}, {-1, 0, 0}});
        cases.push_back({"shallow-tangent", {{f.x - gap + depth, f.y, f.z}, h, r}, {0, 0, 1}});
    }
    for (int index = 0; index < 150; ++index) {
        const Vec3 feet = f + random.GridVec(3.0f);
        const float radius = static_cast<float>(random.Range(16, 128)) / 128.0f;
        const float height = 2.0f * radius + static_cast<float>(random.Range(1, 256)) / 128.0f;
        const Vec3 move = random.Range(0, 9) == 0 ? Vec3{} : random.GridVec(3.0f);
        cases.push_back({"random", {feet, height, radius}, move});
    }
    return cases;
}

std::string RayInputs(const RayCase& item) {
    return "o=" + Hex(item.ray.origin) + " d=" + Hex(item.ray.direction) + " max=" + Hex(item.maximumDistance) +
           " sr=" + Hex(item.sweepRadius);
}

void Emit(std::vector<std::string>& out, std::string_view query, std::size_t target, std::size_t index,
          const std::string& tag, const std::string& inputs, const std::string& result) {
    out.push_back(std::string(query) + " t" + std::to_string(target) + "#" + std::to_string(index) + " " + tag +
                  " | " + inputs + " | " + result);
}

std::uint32_t UlpDistance(float a, float b) {
    const auto ordered = [](float value) {
        const auto bits = std::bit_cast<std::int32_t>(value);
        return bits < 0 ? static_cast<std::int64_t>((std::numeric_limits<std::int32_t>::min)()) - bits
                        : static_cast<std::int64_t>(bits);
    };
    const auto distance = ordered(a) - ordered(b);
    return static_cast<std::uint32_t>(distance < 0 ? -distance : distance);
}

void Count(OverloadComparison& stats, const std::optional<float>& vertical, const std::optional<float>& general) {
    ++stats.cases;
    if (!vertical && !general) { ++stats.bothMiss; return; }
    if (vertical && !general) { ++stats.onlyFloatHits; return; }
    if (!vertical && general) { ++stats.onlyDoubleHits; return; }
    ++stats.bothHit;
    const auto ulp = UlpDistance(*vertical, *general);
    stats.maximumUlp = ulp > stats.maximumUlp ? ulp : stats.maximumUlp;
    const std::size_t bucket = ulp == 0 ? 0 : ulp == 1 ? 1 : ulp <= 4 ? 2 : ulp <= 16 ? 3 : ulp <= 256 ? 4 : 5;
    ++stats.ulpBuckets[bucket];
}

} // namespace

std::vector<std::string> Records() {
    std::vector<std::string> out;
    Generator random(0x4646312d636f6c6cULL);  // "FF1-coll"
    for (std::size_t target = 0; target < Boxes.size(); ++target) {
        const auto rays = BoxRays(Boxes[target], random);
        for (std::size_t index = 0; index < rays.size(); ++index)
            Emit(out, "RaycastAabb", target, index, rays[index].tag, Describe(Boxes[target]) + " " + RayInputs(rays[index]),
                Result(RaycastAabb(rays[index].ray, rays[index].maximumDistance, Boxes[target])));
    }
    for (std::size_t target = 0; target < Capsules.size(); ++target) {
        const auto& k = Capsules[target];
        const Capsule general = ToCapsule(k);
        Emit(out, "ToCapsule", target, 0, "convert", Describe(k), Describe(general));
        const auto rays = CapsuleRays(k, random);
        for (std::size_t index = 0; index < rays.size(); ++index) {
            const auto& item = rays[index];
            Emit(out, "RaycastCapsule/vertical", target, index, item.tag, Describe(k) + " " + RayInputs(item),
                Result(RaycastCapsule(item.ray, item.maximumDistance, k, item.sweepRadius)));
            Emit(out, "RaycastCapsule/general", target, index, item.tag, Describe(general) + " " + RayInputs(item),
                Result(RaycastCapsule(item.ray, item.maximumDistance, general, item.sweepRadius)));
        }
        const auto sweeps = CapsuleSweeps(k, random);
        for (std::size_t index = 0; index < sweeps.size(); ++index) {
            const auto& item = sweeps[index];
            const std::string inputs = "s=" + Hex(item.path.start) + " e=" + Hex(item.path.end) + " sr=" + Hex(item.sweepRadius);
            Emit(out, "SweepSphereAgainstCapsule/vertical", target, index, item.tag, Describe(k) + " " + inputs,
                Result(SweepSphereAgainstCapsule(item.path, item.sweepRadius, k)));
            Emit(out, "SweepSphereAgainstCapsule/general", target, index, item.tag, Describe(general) + " " + inputs,
                Result(SweepSphereAgainstCapsule(item.path, item.sweepRadius, general)));
        }
    }
    for (std::size_t target = 0; target < GeneralCapsules.size(); ++target) {
        const auto& g = GeneralCapsules[target];
        const VerticalCapsule around{{(g.segmentStart.x + g.segmentEnd.x) * 0.5f, -1.0f, (g.segmentStart.z + g.segmentEnd.z) * 0.5f}, 6.0f, 1.0f};
        const auto rays = CapsuleRays(around, random);
        for (std::size_t index = 0; index < rays.size(); ++index)
            Emit(out, "RaycastCapsule/arbitrary", target, index, rays[index].tag, Describe(g) + " " + RayInputs(rays[index]),
                Result(RaycastCapsule(rays[index].ray, rays[index].maximumDistance, g, rays[index].sweepRadius)));
        const auto sweeps = CapsuleSweeps(around, random);
        for (std::size_t index = 0; index < sweeps.size(); ++index) {
            const auto& item = sweeps[index];
            Emit(out, "SweepSphereAgainstCapsule/arbitrary", target, index, item.tag,
                Describe(g) + " s=" + Hex(item.path.start) + " e=" + Hex(item.path.end) + " sr=" + Hex(item.sweepRadius),
                Result(SweepSphereAgainstCapsule(item.path, item.sweepRadius, g)));
        }
    }
    for (std::size_t target = 0; target < Boxes.size(); ++target) {
        const auto bodies = BodiesAroundBox(Boxes[target], random);
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            const auto& item = bodies[index];
            const std::string inputs = Describe(Boxes[target]) + " c=" + Describe(item.capsule);
            Emit(out, "OverlapVerticalCapsuleAabb", target, index, item.tag, inputs,
                Result(OverlapVerticalCapsuleAabb(item.capsule, Boxes[target])));
            Emit(out, "SweepVerticalCapsuleAgainstAabb", target, index, item.tag, inputs + " m=" + Hex(item.displacement),
                Result(SweepVerticalCapsuleAgainstAabb(item.capsule, item.displacement, Boxes[target])));
        }
    }
    for (std::size_t target = 0; target < Capsules.size(); ++target) {
        const auto bodies = BodiesAroundCapsule(Capsules[target], random);
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            const auto& item = bodies[index];
            const std::string inputs = Describe(Capsules[target]) + " c=" + Describe(item.capsule);
            Emit(out, "OverlapVerticalCapsules", target, index, item.tag, inputs,
                Result(OverlapVerticalCapsules(item.capsule, Capsules[target])));
            Emit(out, "SweepVerticalCapsuleAgainstCapsule", target, index, item.tag, inputs + " m=" + Hex(item.displacement),
                Result(SweepVerticalCapsuleAgainstCapsule(item.capsule, item.displacement, Capsules[target])));
        }
    }
    return out;
}

std::map<std::string, OverloadComparison> CompareCapsuleOverloads() {
    std::map<std::string, OverloadComparison> result;
    // The same generator sequence as Records, so the compared inputs are the
    // recorded ones (box rays are drawn first and skipped here).
    Generator random(0x4646312d636f6c6cULL);
    for (const auto& box : Boxes) static_cast<void>(BoxRays(box, random));
    for (const auto& k : Capsules) {
        const Capsule general = ToCapsule(k);
        for (const auto& item : CapsuleRays(k, random))
            Count(result["RaycastCapsule"], RaycastCapsule(item.ray, item.maximumDistance, k, item.sweepRadius),
                RaycastCapsule(item.ray, item.maximumDistance, general, item.sweepRadius));
        for (const auto& item : CapsuleSweeps(k, random))
            Count(result["SweepSphereAgainstCapsule"], SweepSphereAgainstCapsule(item.path, item.sweepRadius, k),
                SweepSphereAgainstCapsule(item.path, item.sweepRadius, general));
    }
    return result;
}

} // namespace Engine::Test::CollisionCorpus
