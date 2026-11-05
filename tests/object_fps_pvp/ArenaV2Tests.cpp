#include <doctest/doctest.h>

#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/Movement.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fps::pvp;

// v8-product-arena-v2 (docs/tools/plans/v8/voxel-map-editor.md): judgments P1
// (schema 2 loading), P2 and P3 (the v1 walls retire: same boxes, same
// movement) and P4 (one support rule for spawns and movement).

namespace {

std::filesystem::path ProductAssets() { return std::filesystem::path(PVP_PRODUCT_ARENA_PATH).parent_path(); }
std::filesystem::path FrozenV1() {
    return std::filesystem::path(PVP_DOMAIN_TEST_ARENA_PATH).parent_path() / "arena_v1";
}

Arena LoadOrFail(const std::filesystem::path& path) {
    std::string error;
    auto arena = Arena::Load(path, error);
    REQUIRE_MESSAGE(arena, error);
    return *arena;
}

std::array<std::uint32_t, 6> Bits(const Engine::Math::Aabb& box) {
    return {std::bit_cast<std::uint32_t>(box.minimum.x), std::bit_cast<std::uint32_t>(box.minimum.y),
            std::bit_cast<std::uint32_t>(box.minimum.z), std::bit_cast<std::uint32_t>(box.maximum.x),
            std::bit_cast<std::uint32_t>(box.maximum.y), std::bit_cast<std::uint32_t>(box.maximum.z)};
}

std::vector<std::array<std::uint32_t, 6>> WallSet(const Arena& arena) {
    std::vector<std::array<std::uint32_t, 6>> set;
    for (const auto& wall : arena.walls) set.push_back(Bits(wall));
    std::sort(set.begin(), set.end());
    return set;
}

struct Scratch final {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("gyo-arena-v2-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        std::filesystem::create_directories(directory);
        std::filesystem::copy_file(ProductAssets() / "pvp_arena.voxel_mesh.json", directory / "map.voxel_mesh.json");
    }
    ~Scratch() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string Refused(const std::string& name, const std::string& text) const {
        std::ofstream(directory / name, std::ios::binary) << text;
        std::string error;
        CHECK_FALSE(Arena::Load(directory / name, error));
        return error;
    }
};

std::string V2(const std::string& geometry) {
    return R"({"version": 2, "id": "v2", "width": 20, "depth": 20, )" + geometry +
           R"("spawns": [{"position": [5, 0, 5], "yaw": 0}, {"position": [5, 0, 12], "yaw": 0}]})";
}

} // namespace

TEST_CASE("Arena schema 2 takes its walls from the baked map next to it") {
    Scratch scratch;
    std::ofstream(scratch.directory / "good.json", std::ios::binary) << V2(R"("geometry": "map.voxel_mesh.json", )");
    std::string error;
    Engine::VoxelMesh::VoxelMeshGeometry geometry;
    const auto arena = Arena::Load(scratch.directory / "good.json", error, &geometry);
    REQUIRE_MESSAGE(arena, error);
    // The file schema is not Arena::version: members and digest stay as they were.
    CHECK(arena->version == 1);
    CHECK(arena->walls.size() == 5);
    CHECK(geometry.colliders.size() == arena->walls.size());
    // In the file's order, which the digest covers.
    for (std::size_t i = 0; i < arena->walls.size() && i < geometry.colliders.size(); ++i)
        CHECK(Bits(arena->walls[i]) == Bits(geometry.colliders[i]));
    CHECK_FALSE(geometry.vertices.empty());

    CHECK(scratch.Refused("no-geometry.json", V2("")).find("geometry") != std::string::npos);
    CHECK(scratch.Refused("path.json", V2(R"("geometry": "../map.voxel_mesh.json", )")).find("plain file name") !=
          std::string::npos);
    CHECK(scratch.Refused("missing.json", V2(R"("geometry": "nowhere.json", )")).find("Cannot open arena geometry") !=
          std::string::npos);
    std::ofstream(scratch.directory / "bad.voxel_mesh.json", std::ios::binary) << R"({"schema": "gyo.voxel_mesh"})";
    CHECK(scratch.Refused("bad.json", V2(R"("geometry": "bad.voxel_mesh.json", )")).find("InvalidVoxelMesh") !=
          std::string::npos);
    CHECK(scratch.Refused("walls.json", V2(R"("geometry": "map.voxel_mesh.json", "walls": [], )"))
              .find("not from walls") != std::string::npos);
    auto three = V2(R"("geometry": "map.voxel_mesh.json", )");
    three.replace(three.find("\"version\": 2"), 12, "\"version\": 3");
    CHECK(scratch.Refused("three.json", three).find("Unsupported arena version") != std::string::npos);
}

TEST_CASE("The baked arenas retire v1: the same wall boxes, bit for bit") {
    for (const char* name : {"pvp_arena.json", "pvp_corners.json"}) {
        CAPTURE(name);
        const auto v1 = LoadOrFail(FrozenV1() / name);
        const auto v2 = LoadOrFail(ProductAssets() / name);
        CHECK(WallSet(v1) == WallSet(v2));
        CHECK(v1.spawns.size() == v2.spawns.size());
        CHECK(v1.id == v2.id);
    }
}

TEST_CASE("The baked arenas retire v1: the same movement, bit for bit") {
    // Walk, run into walls at angles, slide along them, jump and crouch, from
    // every spawn, into the corners and the pillar.
    for (const char* name : {"pvp_arena.json", "pvp_corners.json"}) {
        CAPTURE(name);
        const auto v1 = LoadOrFail(FrozenV1() / name);
        const auto v2 = LoadOrFail(ProductAssets() / name);
        for (const auto& spawn : v1.spawns) {
            PlayerState a{1, spawn.position, spawn.yaw};
            PlayerState b = a;
            for (std::uint64_t step = 1; step <= 1200; ++step) {
                const float yaw = spawn.yaw + 0.37F * static_cast<float>(step / 90) + (step % 7 == 0 ? 0.6F : 0.0F);
                const MovementCommand command{step, 1.0F, (step / 45) % 3 == 0 ? 0.6F : -0.2F, yaw, 0,
                                              step % 53 == 0, (step / 200) % 4 == 3};
                a = StepMovement(v1, a, command);
                b = StepMovement(v2, b, command);
                const bool same = std::bit_cast<std::uint32_t>(a.position.x) == std::bit_cast<std::uint32_t>(b.position.x) &&
                                  std::bit_cast<std::uint32_t>(a.position.y) == std::bit_cast<std::uint32_t>(b.position.y) &&
                                  std::bit_cast<std::uint32_t>(a.position.z) == std::bit_cast<std::uint32_t>(b.position.z) &&
                                  a.grounded == b.grounded && a.crouched == b.crouched;
                REQUIRE_MESSAGE(same, "step " << step);
            }
        }
    }
}

TEST_CASE("A spawn stands on the floor or on a wall top, as movement grounds it") {
    auto arena = LoadOrFail(FrozenV1() / "pvp_arena.json");
    // The pillar is [9, 11] x [0, 2] x [9, 11]: its top is a floor.
    const Engine::Math::Vec3 onPillar{10, 2, 10};
    arena.spawns[1] = {onPillar, 0};
    std::string error;
    CHECK_MESSAGE(arena.Validate(error), error);
    // Movement agrees: a body there stays where it is, grounded.
    const PlayerState standing = StepMovement(arena, PlayerState{1, onPillar, 0}, MovementCommand{1});
    CHECK(standing.grounded);
    CHECK(standing.position.y == 2);
    CHECK(fps::IsCharacterSupported({onPillar, arena.bodyHeight, arena.radius}, arena.walls));

    arena.spawns[1] = {{5, 1, 15}, 0}; // in the air
    CHECK_FALSE(arena.Validate(error));
    CHECK(error.find("wall top") != std::string::npos);
    CHECK_FALSE(fps::IsCharacterSupported({{5, 1, 15}, arena.bodyHeight, arena.radius}, arena.walls));
    arena.spawns[1] = {{5, -1, 15}, 0}; // below the floor
    CHECK_FALSE(arena.Validate(error));
    arena.spawns[1] = {{5, 0, 15}, 0}; // the floor, as before
    CHECK(arena.Validate(error));
}
