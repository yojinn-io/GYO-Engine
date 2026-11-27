// The arena files and their maps (big-map declaration section 15): the shipped maps read through Engine's voxel_map
// Module, the walls of pvp_arena occupying the same cells as the v1 file they were migrated from (the alignment test
// the game owns), what the reader refuses, the regions' pictures, and the arena file's rules.
#include <doctest/doctest.h>

#include "object_fps_pvp/data/ArenaMap.hpp"
#include "object_fps_pvp/sim/Arena.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>

namespace {
using namespace object_fps_pvp;
namespace Math = GYO::Engine::Math;

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const std::string kMaps = std::string(PVP_CONTENT_ROOT) + "/maps/";

std::optional<data::ArenaMap> Shipped(const std::string& name, std::string& error) {
    const std::string dir = kMaps + name + "/";
    return data::ReadArenaMap(
        ReadFile(dir + "map.json"),
        [&dir](const std::string& file, std::string&) -> std::optional<std::string> { return ReadFile(dir + file); }, error);
}

// The whole cells the boxes cover (every box here is in whole cells).
std::set<std::tuple<int, int, int>> Cells(const std::vector<Math::Aabb>& boxes) {
    std::set<std::tuple<int, int, int>> cells;
    for (const Math::Aabb& b : boxes) {
        for (int x = static_cast<int>(std::lround(b.minimum.x)); x < std::lround(b.maximum.x); ++x)
            for (int y = static_cast<int>(std::lround(b.minimum.y)); y < std::lround(b.maximum.y); ++y)
                for (int z = static_cast<int>(std::lround(b.minimum.z)); z < std::lround(b.maximum.z); ++z) cells.emplace(x, y, z);
    }
    return cells;
}
} // namespace

TEST_CASE("PvP map: pvp_arena's walls occupy the cells of the five v1 boxes it was migrated from") {
    std::string error;
    const auto map = Shipped("pvp_arena", error);
    REQUIRE_MESSAGE(map.has_value(), error);
    const std::vector<Math::Aabb> walls = data::MapColliders(*map);
    // The v1 file's five colliders in world units (origin (-1, 0, -1), cell 1), before the migration.
    const std::vector<Math::Aabb> v1{{{-1, 0, -1}, {0, 3, 21}},
                                     {{0, 0, -1}, {20, 3, 0}},
                                     {{0, 0, 20}, {20, 3, 21}},
                                     {{9, 0, 9}, {11, 2, 11}},
                                     {{20, 0, -1}, {21, 3, 21}}};
    CHECK(Cells(walls) == Cells(v1));
    // The tool merges boxes inside each 16-cell chunk: more boxes, the same space.
    CHECK(walls.size() > v1.size());
}

TEST_CASE("PvP map: every shipped arena's map reads, with pictures for its regions") {
    for (const std::string name : {"pvp_arena", "pvp_corners", "biome", "kiriyu_extraction_250m", "kiriyu_town_graybox",
                                   "kiriyu_incident_250m"}) {
        CAPTURE(name);
        std::string error;
        const auto map = Shipped(name, error);
        REQUIRE_MESSAGE(map.has_value(), error);
        CHECK(!data::MapColliders(*map).empty());
        const auto pictures = data::RegionPictures(*map);
        REQUIRE(pictures.size() == map->regions.size());
        for (const data::RegionPicture& picture : pictures) {
            REQUIRE(picture.model != nullptr);
            REQUIRE(picture.model->meshes.size() == 1);
            const auto& mesh = picture.model->meshes[0];
            CHECK(mesh.positions.size() == mesh.uvs.size());
            CHECK(mesh.indices.size() == mesh.positions.size() / 4 * 6);
            // Every vertex lies in the region's box.
            for (const Math::Vec3& p : mesh.positions) {
                CHECK(p.x >= picture.bounds.minimum.x);
                CHECK(p.x <= picture.bounds.maximum.x);
                CHECK(p.z >= picture.bounds.minimum.z);
                CHECK(p.z <= picture.bounds.maximum.z);
            }
        }
    }
}

TEST_CASE("PvP map: each region's picture made on its own is the one the whole map gives, in region order") {
    std::string error;
    const auto map = Shipped("kiriyu_town_graybox", error);
    REQUIRE_MESSAGE(map.has_value(), error);
    const auto whole = data::RegionPictures(*map);
    std::vector<data::RegionPicture> alone;
    for (std::size_t region = 0; region < map->regions.size(); ++region) {
        if (auto picture = data::RegionPictureOf(*map, region)) alone.push_back(std::move(*picture));
    }
    REQUIRE(alone.size() == whole.size());
    REQUIRE(whole.size() > 1);
    for (std::size_t i = 0; i < whole.size(); ++i) {
        CAPTURE(i);
        const auto& a = alone[i].model->meshes.front();
        const auto& b = whole[i].model->meshes.front();
        const auto same = [](const auto& x, const auto& y) {
            return x.size() == y.size() && std::memcmp(x.data(), y.data(), x.size() * sizeof(x[0])) == 0;
        };
        CHECK_MESSAGE((same(a.positions, b.positions) && same(a.normals, b.normals) && same(a.uvs, b.uvs) && same(a.indices, b.indices)),
                      "[pvp-region-alone] the same bytes");
        CHECK(alone[i].bounds.minimum.x == whole[i].bounds.minimum.x);
        CHECK(alone[i].bounds.minimum.z == whole[i].bounds.minimum.z);
    }
}

TEST_CASE("PvP map: a region that cannot be read or is not the index's is refused with its reason") {
    const std::string dir = kMaps + "pvp_arena/";
    const std::string index = ReadFile(dir + "map.json");
    std::string error;
    const auto missing = data::ReadArenaMap(
        index, [](const std::string& file, std::string& why) -> std::optional<std::string> { why = file + ": not there"; return std::nullopt; },
        error);
    CHECK_FALSE(missing.has_value());
    CHECK(error.find("not there") != std::string::npos);
    // The right file with one more space: its SHA-256 is not the index's.
    const auto changed = data::ReadArenaMap(
        index, [&dir](const std::string& file, std::string&) -> std::optional<std::string> { return ReadFile(dir + file) + " "; },
        error);
    CHECK_FALSE(changed.has_value());
    CHECK(error.find("R3") != std::string::npos);
    CHECK_FALSE(data::ReadArenaMap("not json", [](const std::string&, std::string&) { return std::optional<std::string>{}; }, error)
                    .has_value());
    CHECK(error.find("M1") != std::string::npos);
}

TEST_CASE("PvP arena: the shipped arenas read with their spawns and map") {
    std::string error;
    const auto list = sim::ParseArenaList(ReadFile(kMaps + "arenas.json"), error);
    REQUIRE_MESSAGE(list.has_value(), error);
    REQUIRE(list->size() == 6);
    CHECK(list->front() == "pvp_arena.json");
    CHECK(list->back() == "kiriyu_incident_250m.json");
    for (const std::string& name : *list) {
        CAPTURE(name);
        const auto arena = sim::ParseArena(ReadFile(kMaps + name), error);
        REQUIRE_MESSAGE(arena.has_value(), error);
        CHECK(arena->spawns.size() >= 2);
        CHECK(!arena->map.empty());
    }
    const auto arena = sim::ParseArena(ReadFile(kMaps + "pvp_arena.json"), error);
    REQUIRE(arena.has_value());
    CHECK(arena->id == "pvp_training_v1");
    CHECK(arena->map == "object_fps_pvp.map.pvp_arena");
    CHECK(arena->width == 20.0F);
    CHECK(arena->movementSpeed == 3.0F);
    CHECK(arena->jumpHeight == 0.6F);  // not in the file: the default
    CHECK(arena->gravity == 18.0F);
    CHECK(arena->spawns[1].position.z == 12.0F);
    // Version 4's bounds: the training arena stands on a floor plane; the big maps have their own ground.
    REQUIRE(arena->floor.has_value());
    CHECK(*arena->floor == 0.0F);
    CHECK(arena->killHeight == -10.0F);
    REQUIRE(arena->fallDamage.has_value());
    CHECK(arena->fallDamage->safeHeight == 4.0F);
    CHECK(arena->fallDamage->milliPointsPerMetre == 15'000U);
    const auto town = sim::ParseArena(ReadFile(kMaps + "kiriyu_town_graybox.json"), error);
    REQUIRE_MESSAGE(town.has_value(), error);
    CHECK_FALSE(town->floor.has_value());
    CHECK(town->killHeight == -13.0F);
}

TEST_CASE("PvP arena: the rules of an arena file") {
    const auto arena = [](const std::string& patch) {
        return R"({"version": 4, "id": "a", "width": 10, "depth": 10, "map": "a", "kill_height": -10, )" + patch + "}";
    };
    const std::string twoSpawns = R"("spawns": [{"position": [1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])";
    std::string error;
    CHECK(sim::ParseArena(arena(twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, 0, 1], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [0.1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": 0, "spawns": [{"position": [1, -1, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("eye_height": 2, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("body_height": 0.4, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("movement_speed": -1, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(R"({"version": 4, "id": "a", "width": 10, "depth": 10, "map": "", "kill_height": -10, )" + twoSpawns + "}", error)
                    .has_value());
    // Version 3 had no bounds (its floor was always y = 0); it is not read as version 4, even with version 4's members.
    CHECK_FALSE(sim::ParseArena(R"({"version": 3, "id": "a", "width": 10, "depth": 10, "map": "a", "kill_height": -10, )" + twoSpawns + "}", error)
                    .has_value());
    // The bounds: no floor (absent), a kill height (required, below the floor and every spawn), the fall damage.
    const auto bare = sim::ParseArena(arena(twoSpawns), error);
    REQUIRE_MESSAGE(bare.has_value(), error);
    CHECK_FALSE(bare->floor.has_value());
    CHECK_FALSE(bare->fallDamage.has_value());
    CHECK(bare->killHeight == -10.0F);
    CHECK_FALSE(sim::ParseArena(R"({"version": 4, "id": "a", "width": 10, "depth": 10, "map": "a", )" + twoSpawns + "}", error).has_value());
    CHECK(sim::ParseArena(arena(R"("floor": 0, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": -10, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": 0.5, )" + twoSpawns), error).has_value());  // a spawn below the floor
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, -10, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error)
                    .has_value());  // a spawn not above the kill height
    const auto hurt = sim::ParseArena(arena(R"("fall_damage": {"safe_height": 3.5, "points_per_metre": 12.5}, )" + twoSpawns), error);
    REQUIRE_MESSAGE(hurt.has_value(), error);
    REQUIRE(hurt->fallDamage.has_value());
    CHECK(hurt->fallDamage->safeHeight == 3.5F);
    CHECK(hurt->fallDamage->milliPointsPerMetre == 12'500U);
    for (const std::string bad : {R"("fall_damage": {"points_per_metre": 10}, )", R"("fall_damage": {"safe_height": 0, "points_per_metre": 10}, )",
                                  R"("fall_damage": {"safe_height": 4, "points_per_metre": -1}, )",
                                  R"("fall_damage": {"safe_height": 4, "points_per_metre": 5000000}, )",
                                  R"("fall_damage": {"safe_height": 4, "points_per_metre": 0.0001}, )"}) {
        CAPTURE(bad);
        CHECK_FALSE(sim::ParseArena(arena(bad + twoSpawns), error).has_value());
    }
}
