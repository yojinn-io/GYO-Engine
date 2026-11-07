#include <doctest/doctest.h>

// v8-product-pa-broadphase B2: the wall queries on the arena's StaticAabbGrid
// return, bit for bit, what the former linear scans returned. The reference
// functions below are the scans as they were before the batch (commit 35d5deea),
// kept here only as the oracle.

#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/ShotQuery.hpp"
#include "engine/collision/Collision.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace reference {
namespace {
// A ground-bound enemy cannot use a vertical separation or slide component.
// Resolve rare initial penetration along a horizontal direction using the same
// exact Engine query, rather than approximating the raised actor as a 2D disk.
template <class Overlap>
Engine::Collision::Contact PlanarPenetration(Engine::Collision::Contact contact,
                                             Engine::Collision::VerticalCapsule body,
                                             Overlap overlap) {
    std::array<Engine::Math::Vec3, 4> directions{{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
    const float horizontal =
        Engine::Math::Length(Engine::Math::Vec3{contact.normal.x, 0, contact.normal.z});
    std::size_t count = directions.size();
    if (horizontal > 0.000001F) {
        directions[0] = {contact.normal.x / horizontal, 0, contact.normal.z / horizontal};
        count = 1;
    }
    std::optional<Engine::Collision::Contact> best;
    for (std::size_t index = 0; index < count; ++index) {
        const auto direction = directions[index];
        const auto penetrates = [&](const float distance) {
            auto candidate = body;
            candidate.feet.x += direction.x * distance;
            candidate.feet.z += direction.z * distance;
            const auto hit = overlap(candidate);
            return hit && hit->penetrationDepth > 0.0000001F;
        };
        float low = 0, high = Engine::Math::Max(body.radius, contact.penetrationDepth);
        int attempts = 0;
        while (penetrates(high) && attempts++ < 64)
            high *= 2;
        if (!std::isfinite(high) || attempts >= 64)
            throw std::runtime_error(
                "Cannot resolve planar character penetration within finite bounds");
        for (int iteration = 0; iteration < 24; ++iteration) {
            const float middle = (low + high) * 0.5F;
            if (penetrates(middle))
                low = middle;
            else
                high = middle;
        }
        if (!best || high < best->penetrationDepth) {
            contact.normal = direction;
            contact.penetrationDepth = high;
            best = contact;
        }
    }
    return *best;
}
} // namespace

bool CanPlace(const Engine::Collision::VerticalCapsule &body, std::span<const Engine::Math::Aabb> walls) {
    for (const auto &wall : walls)
        if (auto c = Engine::Collision::OverlapVerticalCapsuleAabb(body, wall);
            c && c->penetrationDepth > 0.00001F)
            return false;
    return true;
}

bool Supported(const Engine::Collision::VerticalCapsule &body, std::span<const Engine::Math::Aabb> walls) {
    if (body.feet.y <= 0.0001F) return true;
    for (const auto &wall : walls) {
        const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstAabb(body, {0, -0.002F, 0}, wall);
        if (contact && contact->normal.y > 0.5F) return true;
    }
    return false;
}

Engine::Math::Vec3 Move(Engine::Collision::VerticalCapsule body, Engine::Math::Vec3 displacement,
                        std::span<const Engine::Math::Aabb> walls, const bool constrainToFloor) {
    if (constrainToFloor)
        displacement.y = 0;
    for (int iteration = 0; iteration < 8; ++iteration) {
        std::optional<Engine::Collision::Contact> deepest;
        const auto consider = [&](auto c) {
            if (c && c->penetrationDepth > 0.00001F &&
                (!deepest || c->penetrationDepth > deepest->penetrationDepth))
                deepest = c;
        };
        for (const auto &wall : walls) {
            auto contact = Engine::Collision::OverlapVerticalCapsuleAabb(body, wall);
            if (constrainToFloor && contact && contact->penetrationDepth > 0.00001F)
                contact = PlanarPenetration(*contact, body, [&](const auto &candidate) {
                    return Engine::Collision::OverlapVerticalCapsuleAabb(candidate, wall);
                });
            consider(contact);
        }
        if (!deepest)
            break;
        body.feet = body.feet + deepest->normal * (deepest->penetrationDepth + 0.0001F);
    }
    for (int iteration = 0; iteration < 5; ++iteration) {
        std::optional<Engine::Collision::Contact> nearest;
        const auto consider = [&](auto c) {
            if (!c)
                return;
            if (constrainToFloor) {
                const float horizontal =
                    Engine::Math::Length(Engine::Math::Vec3{c->normal.x, 0, c->normal.z});
                if (horizontal <= 0.000001F)
                    return;
                c->normal = {c->normal.x / horizontal, 0, c->normal.z / horizontal};
            }
            if (!nearest || c->fraction < nearest->fraction)
                nearest = c;
        };
        const Engine::Math::Vec3 d = displacement;
        for (const auto &wall : walls)
            consider(Engine::Collision::SweepVerticalCapsuleAgainstAabb(body, d, wall));
        const float length = Engine::Math::Length(d);
        if (length < 0.000001F)
            break;
        const float fraction =
            nearest ? Engine::Math::Max(0.0F, nearest->fraction - 0.0001F / length) : 1.0F;
        body.feet = body.feet + d * fraction;
        if (!nearest)
            break;
        displacement = d * (1 - fraction);
        const auto n = nearest->normal;
        const float into = Engine::Math::Min(0.0F, Engine::Math::Dot(displacement, n));
        displacement = displacement - n * into;
    }
    return {body.feet.x, Engine::Math::Max(0.0F, body.feet.y), body.feet.z};
}

// QueryShot with no players: the floor, then every wall replacing on "<=".
fps::pvp::ShotHit Shot(const fps::pvp::Arena &arena, const fps::pvp::PlayerState &shooter, float yaw, float pitch,
                       float range) {
    using namespace fps::pvp;
    const Engine::Math::Vec3 origin{shooter.position.x,
                                    shooter.position.y + StanceEyeHeight(arena, DefaultStanceRules, shooter.crouched),
                                    shooter.position.z};
    yaw = Engine::Math::WrapRadians(yaw);
    pitch = Engine::Math::Clamp(pitch, -MovementMaximumPitch, MovementMaximumPitch);
    const float cosinePitch = std::cos(pitch);
    const Engine::Math::Vec3 direction = Engine::Math::Normalize(Engine::Math::Vec3{
        std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch});
    ShotHit closest{ShotHitKind::Miss, 0, range};
    if (origin.y <= 0) {
        closest = {ShotHitKind::World, 0, 0};
    } else if (const auto floor =
                   Engine::Math::Intersect(Engine::Math::Ray{origin, direction}, Engine::Math::Plane{});
               floor && *floor <= range) {
        closest = {ShotHitKind::World, 0, *floor};
    }
    for (const auto &wall : arena.walls) {
        const auto distance = Engine::Collision::RaycastAabb({origin, direction}, range, wall);
        if (distance && *distance <= closest.distance)
            closest = {ShotHitKind::World, 0, *distance};
    }
    return closest;
}
} // namespace reference

namespace {
// A fixed generator (not <random>: its distributions differ between standard
// libraries) so every platform draws the same sets.
struct Draw final {
    std::uint64_t state;
    std::uint32_t Next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    }
    int Int(int low, int high) { return low + static_cast<int>(Next() % static_cast<std::uint32_t>(high - low + 1)); }
    // Mostly on a 0.25 lattice (touching faces, equal distances), sometimes off it.
    float Coordinate(int low, int high) {
        const float lattice = static_cast<float>(Int(low * 4, high * 4)) * 0.25F;
        return Int(0, 3) == 0 ? lattice + static_cast<float>(Int(-99, 99)) * 0.001F : lattice;
    }
};

std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
bool SameVec(Engine::Math::Vec3 a, Engine::Math::Vec3 b) {
    return Bits(a.x) == Bits(b.x) && Bits(a.y) == Bits(b.y) && Bits(a.z) == Bits(b.z);
}

fps::pvp::Arena RandomArena(Draw &draw) {
    fps::pvp::Arena arena;
    arena.id = "pa_broadphase_cross";
    arena.width = arena.depth = 16;
    // Cell lengths below, at and above the box sizes; 0.01 makes the grid double it.
    constexpr std::array cells{0.01F, 0.25F, 0.5F, 1.0F, 3.0F};
    arena.cellSize = cells[static_cast<std::size_t>(draw.Int(0, 4))];
    const int count = draw.Int(0, 60);
    for (int i = 0; i < count; ++i) {
        const Engine::Math::Vec3 minimum{draw.Coordinate(0, 15), draw.Coordinate(0, 3), draw.Coordinate(0, 15)};
        Engine::Math::Vec3 size{draw.Coordinate(0, 3), draw.Coordinate(0, 2), draw.Coordinate(0, 3)};
        // Some flat boxes (zero thickness is valid for the grid) and some duplicates.
        if (draw.Int(0, 9) == 0 || size.y < 0) size.y = 0;
        if (size.x <= 0) size.x = 0.25F;
        if (size.z <= 0) size.z = 0.25F;
        arena.walls.push_back({minimum, minimum + size});
        if (draw.Int(0, 19) == 0) arena.walls.push_back(arena.walls.back());
    }
    arena.BuildWallGrid();
    return arena;
}

Engine::Math::Vec3 RandomDisplacement(Draw &draw) {
    switch (draw.Int(0, 4)) {
    case 0: return {};
    case 1: return {draw.Coordinate(-2, 2), 0, 0};
    case 2: return {0, draw.Coordinate(-2, 2), 0};
    default: return {draw.Coordinate(-2, 2), draw.Coordinate(-1, 1), draw.Coordinate(-2, 2)};
    }
}
} // namespace

TEST_CASE("PvP wall queries on the grid equal the linear scans bit for bit (pa-broadphase B2)") {
    constexpr int Sets = 200;
    constexpr int Queries = 200;
    Draw draw{0x9a17b0adULL};
    int generated = 0, compared = 0, mismatches = 0, throws = 0;
    for (int set = 0; set < Sets; ++set) {
        const auto arena = RandomArena(draw);
        const auto &grid = arena.WallGrid();
        for (int query = 0; query < Queries; ++query) {
            ++generated;
            const Engine::Collision::VerticalCapsule body{
                {draw.Coordinate(0, 15), draw.Int(0, 2) == 0 ? 0.0F : draw.Coordinate(0, 4), draw.Coordinate(0, 15)},
                draw.Int(0, 1) == 0 ? 1.8F : 1.2F, 0.25F};
            const auto displacement = RandomDisplacement(draw);
            const bool floor = draw.Int(0, 1) == 0;
            bool same = fps::CanPlaceCharacterBody(body, grid, {}) == reference::CanPlace(body, arena.walls) &&
                        fps::IsCharacterSupported(body, grid) == reference::Supported(body, arena.walls);
            // MoveCharacterBody may throw (an unresolved planar penetration): both must.
            std::optional<Engine::Math::Vec3> moved, expected;
            bool movedThrew = false, expectedThrew = false;
            try { moved = fps::MoveCharacterBody(body, displacement, grid, {}, floor); }
            catch (const std::runtime_error &) { movedThrew = true; }
            try { expected = reference::Move(body, displacement, arena.walls, floor); }
            catch (const std::runtime_error &) { expectedThrew = true; }
            throws += expectedThrew ? 1 : 0;
            same = same && movedThrew == expectedThrew && (movedThrew || SameVec(*moved, *expected));
            fps::pvp::PlayerState shooter;
            shooter.playerId = 1;
            shooter.position = body.feet;
            shooter.crouched = draw.Int(0, 3) == 0;
            const float yaw = static_cast<float>(draw.Int(-180, 180)) * 0.0174532925F;
            const float pitch = static_cast<float>(draw.Int(-89, 89)) * 0.0174532925F;
            const float range = draw.Int(0, 3) == 0 ? 3.0F : 100.0F;
            const auto shot = fps::pvp::QueryShot(arena, shooter, yaw, pitch, std::span<const fps::pvp::PlayerState>{}, range);
            const auto oracle = reference::Shot(arena, shooter, yaw, pitch, range);
            same = same && shot.kind == oracle.kind && shot.targetId == oracle.targetId &&
                   Bits(shot.distance) == Bits(oracle.distance) && shot.part == oracle.part;
            ++compared;
            if (!same) {
                ++mismatches;
                CAPTURE(set);
                CAPTURE(query);
                CHECK(same);
            }
        }
    }
    MESSAGE("B2 generated " << generated << ", compared " << compared << ", mismatches " << mismatches
                            << ", reference throws " << throws);
    CHECK(generated == Sets * Queries);
    CHECK(compared == generated);
    CHECK(mismatches == 0);
}

namespace {
// Boxes 0.5 on a side on a 1 m lattice from x, z = 4, clear of the two spawns.
fps::pvp::Arena LatticeArena(std::size_t colliders) {
    fps::pvp::Arena arena;
    arena.id = "pa_broadphase_budget";
    arena.width = arena.depth = 1100;
    arena.walls.reserve(colliders);
    for (std::size_t i = 0; i < colliders; ++i) {
        const Engine::Math::Vec3 minimum{4.0F + static_cast<float>(i % 1024), 0,
                                         4.0F + static_cast<float>(i / 1024)};
        arena.walls.push_back({minimum, minimum + Engine::Math::Vec3{0.5F, 0.5F, 0.5F}});
    }
    arena.spawns = {{{1, 0, 1}, 0}, {{2, 0, 2}, 0}};
    return arena;
}
} // namespace

TEST_CASE("PvP arena collider budget is 2^20 and its error names the colliders (pa-broadphase B3)") {
    std::string error;
    CHECK(LatticeArena(1025).Validate(error));
    CHECK(error.empty());
    CHECK(LatticeArena(fps::pvp::MaximumArenaColliders).Validate(error));
    CHECK(error.empty());
    CHECK_FALSE(LatticeArena(fps::pvp::MaximumArenaColliders + 1).Validate(error));
    CHECK(error == "Arena has 1048577 colliders; at most 1048576");
}

TEST_CASE("PvP arena refuses colliders too large for the grid instead of asserting") {
    auto arena = LatticeArena(2);
    // Each box covers (1000 / 0.01 + 2)^3 cells by the bound: far over 2^32.
    arena.cellSize = 0.01F;
    arena.walls.push_back({{4, 0, 600}, {1004, 1000, 1600}});
    std::string error;
    CHECK_FALSE(arena.Validate(error));
    CHECK(error.find("too large for the collision grid") != std::string::npos);
}

// ---------------------------------------------------------------------------
// pa-broadphase B4: the wall queries' cost per tick. Skipped in CI; run with
//   gyo_object_fps_pvp_tests -tc='*B4*' --no-skip
// PVP_B4_BIOME names the biome arena file (when unset, only the synthetic map
// runs). Thresholds are declared (v8-product-pa-broadphase-declaration.md §4).
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
struct Timings final {
    std::vector<double> step, shot, tick; // milliseconds
};

double P99(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto rank = static_cast<std::size_t>(std::ceil(0.99 * static_cast<double>(values.size())));
    return values[std::max<std::size_t>(rank, 1) - 1];
}

// Four players walk, turn, jump and shoot for `ticks` ticks; each step and each
// shot is timed, and a tick is the sum of its four steps and four shots.
Timings Run(const fps::pvp::Arena &arena, std::size_t ticks) {
    using namespace fps::pvp;
    using Clock = std::chrono::steady_clock;
    Draw draw{0xb4b4b4b4ULL};
    std::vector<PlayerState> players;
    for (std::size_t i = 0; players.size() < 4 && i < arena.spawns.size(); ++i) {
        PlayerState state;
        state.playerId = static_cast<PlayerId>(players.size() + 1);
        state.position = arena.spawns[i].position;
        players.push_back(state);
    }
    REQUIRE(players.size() == 4);
    Timings timings;
    std::array<float, 4> yaw{}, forward{}, right{};
    for (std::uint64_t tick = 1; tick <= ticks; ++tick) {
        double total = 0;
        for (std::size_t p = 0; p < players.size(); ++p) {
            if (tick % 60 == p) { // a new intent about once a second
                yaw[p] = static_cast<float>(draw.Int(-180, 180)) * 0.0174532925F;
                forward[p] = static_cast<float>(draw.Int(-1, 1));
                right[p] = static_cast<float>(draw.Int(-1, 1));
            }
            const MovementCommand command{tick, forward[p], right[p], yaw[p], 0, draw.Int(0, 29) == 0};
            auto start = Clock::now();
            players[p] = StepMovement(arena, players[p], command);
            const double step = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            const float pitch = static_cast<float>(draw.Int(-30, 30)) * 0.0174532925F;
            start = Clock::now();
            const auto hit = QueryShot(arena, players[p], yaw[p], pitch, std::span<const PlayerState>(players), 100.0F);
            const double shot = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            (void)hit;
            timings.step.push_back(step);
            timings.shot.push_back(shot);
            total += step + shot;
        }
        timings.tick.push_back(total);
    }
    return timings;
}

void Report(const char *name, const fps::pvp::Arena &arena, const Timings &t) {
    const double step = P99(t.step), shot = P99(t.shot), tick = P99(t.tick);
    MESSAGE("B4 " << name << ": colliders " << arena.walls.size() << ", cell " << arena.WallGrid().CellLength()
                  << " (given " << arena.cellSize << "), steps " << t.step.size() << ", ticks " << t.tick.size()
                  << ", p99 step " << step << " ms, p99 shot " << shot << " ms, p99 tick " << tick << " ms");
    CHECK(step <= 0.4);
    CHECK(shot <= 0.2);
    CHECK(tick <= 2.0);
}

// 10^5 boxes of biome's size class (0.5 to 4 m) on a 0.5 m lattice over
// 600 x 600 m and up to 20 m high: walls, slabs and pillars, overlapping freely.
fps::pvp::Arena SyntheticCity() {
    fps::pvp::Arena arena;
    arena.id = "pa_broadphase_b4_synthetic";
    arena.width = arena.depth = 600;
    arena.cellSize = 1;
    Draw draw{0xc17c17ULL};
    arena.walls.reserve(100000);
    while (arena.walls.size() < 100000) {
        const Engine::Math::Vec3 minimum{static_cast<float>(draw.Int(0, 1199)) * 0.5F,
                                         static_cast<float>(draw.Int(0, 40)) * 0.5F,
                                         static_cast<float>(draw.Int(0, 1199)) * 0.5F};
        const Engine::Math::Vec3 size{static_cast<float>(draw.Int(1, 8)) * 0.5F,
                                      static_cast<float>(draw.Int(1, 8)) * 0.5F,
                                      static_cast<float>(draw.Int(1, 8)) * 0.5F};
        arena.walls.push_back({minimum, minimum + size});
    }
    // Four floor spawns where a standing body fits.
    arena.BuildWallGrid();
    for (int x = 2; arena.spawns.size() < 4 && x < 598; x += 7)
        for (int z = 2; arena.spawns.size() < 4 && z < 598; z += 11) {
            const Engine::Math::Vec3 feet{static_cast<float>(x) + 0.25F, 0, static_cast<float>(z) + 0.25F};
            if (fps::CanPlaceCharacterBody({feet, arena.bodyHeight, arena.radius}, arena.WallGrid(), {}))
                arena.spawns.push_back({feet, 0});
        }
    std::string error;
    REQUIRE(arena.Validate(error));
    return arena;
}
} // namespace

TEST_CASE("PvP wall query cost per tick on biome and a 10^5 box map (pa-broadphase B4)" * doctest::skip()) {
    constexpr std::size_t Ticks = 20000;
    if (const char *biome = std::getenv("PVP_B4_BIOME")) {
        std::string error;
        const auto arena = fps::pvp::Arena::Load(std::filesystem::path(biome), error);
        REQUIRE_MESSAGE(arena, error);
        Report("biome", *arena, Run(*arena, Ticks));
    }
    const auto city = SyntheticCity();
    Report("synthetic", city, Run(city, Ticks));
}
