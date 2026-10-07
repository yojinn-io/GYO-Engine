#include <doctest/doctest.h>

#include "RetroFPS/Pvp/Arena.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
using namespace fps::pvp;

// Synthetic arena with decimals that are not exact binary fractions, so the
// golden value also pins strtod on every CI platform.
constexpr const char* Fixture = R"({"version": 1, "id": "digest_fixture_arena", "width": 20.1, "depth": 12.3, "cell_size": 0.7,
 "movement_speed": 3.3, "body_height": 1.85, "radius": 0.27, "eye_height": 1.62, "jump_height": 0.65, "gravity": 17.9,
 "walls": [{"min": [-1, 0, -1], "max": [0, 3.3, 13.3]}, {"min": [10.1, 0, 4.4], "max": [11.2, 2.5, 5.6]}],
 "spawns": [{"position": [2.2, 0, 2.3], "yaw": 0.3}, {"position": [17.7, 0, 9.9], "yaw": 3.14159265}]})";
// Computed independently of the product code (Python struct and FNV-1a).
constexpr std::uint64_t FixtureDigest = 0x60d800e09a796f00ULL;

Arena LoadText(const std::string& text) {
    static std::atomic<unsigned> counter{};
    const auto path = std::filesystem::temp_directory_path() /
        ("gyo_pvp_arena_digest_" + std::to_string(++counter) + ".json");
    { std::ofstream(path, std::ios::binary) << text; }
    std::string error;
    auto arena = Arena::Load(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(arena, error);
    return *arena;
}

std::uint64_t Digest(const std::string& text) { return ArenaContentDigest(LoadText(text)); }

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}
}

TEST_CASE("PvP arena content digest matches the independent golden value") {
    CHECK(Digest(Fixture) == FixtureDigest);
}

TEST_CASE("PvP arena content digest ignores the JSON spelling of the same content") {
    const auto json = nlohmann::json::parse(Fixture);
    std::string crlf = json.dump(2);
    for (std::size_t at = 0; (at = crlf.find('\n', at)) != std::string::npos; at += 2) crlf.replace(at, 1, "\r\n");
    CHECK(Digest(crlf) == FixtureDigest);
    CHECK(Digest(json.dump()) == FixtureDigest);                       // whitespace and (sorted) key order
    CHECK(Digest(Replace(Fixture, "\"width\": 20.1", "\"width\": 2.01e1")) == FixtureDigest);
    CHECK(Digest(Replace(Fixture, "\"gravity\": 17.9", "\"gravity\": 17.900")) == FixtureDigest);
    CHECK(Digest(Replace(Fixture, "\"version\": 1,", "\"version\": 1, \"unknown\": [1, 2],")) == FixtureDigest);
    auto defaults = json;
    defaults["cell_size"] = 1.0;
    auto omitted = defaults;
    omitted.erase("cell_size");
    CHECK(Digest(defaults.dump()) == Digest(omitted.dump()));
}

TEST_CASE("PvP arena content digest changes with every member, order and signed zero") {
    const auto json = nlohmann::json::parse(Fixture);
    const auto changed = [&](auto edit) {
        auto copy = json;
        edit(copy);
        return Digest(copy.dump()) != FixtureDigest;
    };
    CHECK(changed([](auto& j) { j["id"] = "digest_fixture_arenb"; }));
    for (const char* key : {"width", "depth", "cell_size", "movement_speed", "body_height", "radius", "eye_height",
                            "jump_height", "gravity"}) {
        CAPTURE(key);
        CHECK(changed([&](auto& j) { j[key] = j[key].template get<double>() * 1.01; }));
    }
    CHECK(changed([](auto& j) { j["walls"][1]["max"][1] = 2.6; }));
    CHECK(changed([](auto& j) { j["spawns"][0]["yaw"] = 0.31; }));
    CHECK(changed([](auto& j) { std::swap(j["walls"][0], j["walls"][1]); }));
    CHECK(changed([](auto& j) { std::swap(j["spawns"][0], j["spawns"][1]); }));
    CHECK(changed([](auto& j) { j["walls"][0]["min"][1] = -0.0; }));  // JSON -0.0, not -0 (an integer +0)
    CHECK(changed([](auto& j) { j["walls"].erase(1); }));
    // Version 1 is the only loadable format, so its member is checked directly.
    auto arena = LoadText(Fixture);
    arena.version = 2;
    CHECK(ArenaContentDigest(arena) != FixtureDigest);
}

#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace {
Arena WithSpawns(std::size_t count) {
    Arena arena = LoadText(Fixture);
    arena.spawns.clear();
    // An 8 x 8 grid two metres apart, clear of the fixture walls.
    for (std::size_t i = 0; i < count; ++i)
        arena.spawns.push_back({{1.5F + 2.0F * static_cast<float>(i % 8), 0, 0.6F + 1.4F * static_cast<float>(i / 8)}, 0});
    return arena;
}

bool Same(const Engine::Math::Vec3 a, const Engine::Math::Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

Arena ProductArena() {
    std::string error;
    auto arena = Arena::Load(PVP_PRODUCT_ARENA_PATH, error);
    REQUIRE_MESSAGE(arena, error);
    return *arena;
}
} // namespace

TEST_CASE("Arena v1 accepts 2 to 64 spawns and the Match hosts only arenas with a spawn per player") {
    for (const std::size_t count : {std::size_t{2}, std::size_t{4}, std::size_t{64}}) {
        CAPTURE(count);
        std::string error;
        CHECK_MESSAGE(WithSpawns(count).Validate(error), error);
    }
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{65}}) {
        CAPTURE(count);
        std::string error;
        CHECK_FALSE(WithSpawns(count).Validate(error));
        CHECK(error.find("2-64 spawns") != std::string::npos);
    }
    std::string error;
    CHECK(ArenaHostsRoom(WithSpawns(MaxPlayers), error));
    CHECK_FALSE(ArenaHostsRoom(WithSpawns(MaxPlayers - 1), error));
    CHECK(error.find("spawns") != std::string::npos);
    // The digest covers the spawn count: adding one changes it.
    CHECK(ArenaContentDigest(WithSpawns(2)) != ArenaContentDigest(WithSpawns(3)));
}

TEST_CASE("The product arena keeps its two spawns and adds two on the open segment between them") {
    const auto arena = ProductArena();
    std::string error;
    REQUIRE_MESSAGE(arena.Validate(error), error);
    CHECK(ArenaHostsRoom(arena, error));
    REQUIRE(arena.spawns.size() == 4);
    const auto& a = arena.spawns[0];
    const auto& b = arena.spawns[1];
    CHECK(Same(a.position, {5, 0, 5}));
    CHECK(a.yaw == 0);
    CHECK(Same(b.position, {5, 0, 12}));
    CHECK(b.yaw == doctest::Approx(std::numbers::pi).epsilon(1e-7));
    CHECK(Same(arena.spawns[2].position, {5, 0, 7.5F}));
    CHECK(arena.spawns[2].yaw == 0);
    CHECK(Same(arena.spawns[3].position, {5, 0, 9.5F}));
    CHECK(arena.spawns[3].yaw == b.yaw);
    for (std::size_t i = 2; i < 4; ++i) {
        const auto& p = arena.spawns[i].position;
        // Strictly inside the segment from a to b.
        CHECK(p.x == a.position.x);
        CHECK(p.z > a.position.z);
        CHECK(p.z < b.position.z);
    }
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i + 1; j < 4; ++j)
            CHECK(Engine::Math::Length(arena.spawns[i].position - arena.spawns[j].position) >= 4 * arena.radius);
}

TEST_CASE("Spawns added on the segment never change a two-player spawn choice") {
    // With one other living player anywhere in the arena (or none), the
    // product arena selects exactly what its first two spawns alone select:
    // a point inside the segment is never farther than the farther end.
    const auto four = ProductArena();
    auto two = four;
    two.spawns.resize(2);
    const auto same = [&](std::span<const Engine::Math::Vec3> living) {
        const auto* chosen = SelectSpawn(four, living);
        const auto* reference = SelectSpawn(two, living);
        REQUIRE(chosen);
        REQUIRE(reference);
        return Same(chosen->position, reference->position) && chosen->yaw == reference->yaw;
    };
    CHECK(same({}));
    int checked = 0;
    for (float x = four.radius; x <= four.width - four.radius; x += 0.25F) {
        for (float z = four.radius; z <= four.depth - four.radius; z += 0.25F) {
            const Engine::Math::Vec3 other{x, 0, z};
            CAPTURE(x);
            CAPTURE(z);
            CHECK(same(std::span<const Engine::Math::Vec3>(&other, 1)));
            ++checked;
        }
    }
    CHECK(checked > 6000);
}

TEST_CASE("Equal-distance spawn choices keep content order") {
    // pv6 contract §5: the farthest free spawn from the living players; a tie goes
    // to the spawn earlier in content order.
    auto arena = ProductArena();
    arena.spawns.resize(2);
    const auto middle = (arena.spawns[0].position + arena.spawns[1].position) * 0.5F;
    REQUIRE(Engine::Math::LengthSquared(Engine::Math::ToVec3d(arena.spawns[0].position - middle)) ==
            Engine::Math::LengthSquared(Engine::Math::ToVec3d(arena.spawns[1].position - middle)));
    const std::span<const Engine::Math::Vec3> living(&middle, 1);
    for (const bool reversed : {false, true}) {
        CAPTURE(reversed);
        auto ordered = arena;
        if (reversed) std::swap(ordered.spawns[0], ordered.spawns[1]);
        const auto* chosen = SelectSpawn(ordered, living);
        REQUIRE(chosen);
        CHECK(Same(chosen->position, ordered.spawns[0].position));
    }
}
