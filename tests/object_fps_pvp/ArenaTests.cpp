// The arena files and their maps (big-map declaration section 15): the shipped maps read through Engine's voxel_map
// Module, the walls of pvp_arena occupying the same cells as the v1 file they were migrated from (the alignment test
// the game owns), what the reader refuses, the regions' pictures, and what an arena file says (version 5: what the
// ground decides; the rules are the mode's, the body the character's).
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
    CHECK(arena->spawns[1].position.z == 12.0F);
    // The bounds: the training arena stands on a floor plane; the big maps have their own ground.
    REQUIRE(arena->floor.has_value());
    CHECK(*arena->floor == 0.0F);
    CHECK(arena->killHeight == -10.0F);
    // Furnished by its scene (a door).
    REQUIRE(arena->scene.has_value());
    CHECK(*arena->scene == "scenes/pvp_arena.json");
    const auto town = sim::ParseArena(ReadFile(kMaps + "kiriyu_town_graybox.json"), error);
    REQUIRE_MESSAGE(town.has_value(), error);
    CHECK_FALSE(town->floor.has_value());
    CHECK(town->killHeight == -13.0F);
}

TEST_CASE("PvP arena: what an arena file says and what it refuses") {
    const auto arena = [](const std::string& patch) {
        return R"({"version": 5, "id": "a", "width": 10, "depth": 10, "map": "a", "kill_height": -10, )" + patch + "}";
    };
    const std::string twoSpawns = R"("spawns": [{"position": [1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])";
    std::string error;
    CHECK(sim::ParseArena(arena(twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, 0, 1], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": 0, "spawns": [{"position": [1, -1, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(R"({"version": 5, "id": "a", "width": 10, "depth": 10, "map": "", "kill_height": -10, )" + twoSpawns + "}", error)
                    .has_value());
    CHECK_FALSE(sim::ParseArena(R"({"version": 5, "id": "a", "depth": 10, "map": "a", "kill_height": -10, )" + twoSpawns + "}", error)
                    .has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("width": 0, )" + twoSpawns), error).has_value());
    // The id: 1 to 64 characters, required.
    const auto withId = [&](const std::string& id) {
        return R"({"version": 5, )" + id + R"("width": 10, "depth": 10, "map": "a", "kill_height": -10, )" + twoSpawns + "}";
    };
    CHECK(sim::ParseArena(withId(R"("id": ")" + std::string(64, 'a') + R"(", )"), error).has_value());
    CHECK_FALSE_MESSAGE(sim::ParseArena(withId(R"("id": ")" + std::string(65, 'a') + R"(", )"), error).has_value(), "an id of 65 characters");
    CHECK_FALSE_MESSAGE(sim::ParseArena(withId(R"("id": "", )"), error).has_value(), "an empty id");
    CHECK_FALSE_MESSAGE(sim::ParseArena(withId(""), error).has_value(), "no id");
    // The spawns: an array of 2 to 64, each three finite numbers and a finite yaw.
    CHECK_FALSE_MESSAGE(sim::ParseArena(arena(R"("spawns": {"position": [1, 0, 1], "yaw": 0})"), error).has_value(), "spawns not an array");
    std::string many = R"("spawns": [)";
    for (int i = 0; i < 65; ++i) many += std::string(i == 0 ? "" : ", ") + R"({"position": [5, 0, 5], "yaw": 0})";
    CHECK_FALSE_MESSAGE(sim::ParseArena(arena(many + "]"), error).has_value(), "65 spawns");
    for (const std::string bad : {R"({"position": [1, 0], "yaw": 0})", R"({"position": [1, 0, 1, 1], "yaw": 0})",
                                  R"({"position": [1, 1e39, 1], "yaw": 0})", R"({"position": [1, 0, 1], "yaw": 1e39})",
                                  R"({"position": [1, 0, 1]})"}) {
        CAPTURE(bad);
        CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [)" + bad + R"(, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    }
    // The bounds without a body: a spawn may stand anywhere in [0, width] x [0, depth] (the room for the character's
    // radius is LoadGameContent's to check), not outside.
    CHECK(sim::ParseArena(arena(R"("spawns": [{"position": [0, 0, 0], "yaw": 0}, {"position": [10, 0, 10], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [-0.1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, 0, 1], "yaw": 0}, {"position": [9, 0, 10.1], "yaw": 0}])"), error).has_value());
    // Version 4 carried the rules and the body; it is not read as version 5, even with version 5's members only.
    CHECK_FALSE(sim::ParseArena(R"({"version": 4, "id": "a", "width": 10, "depth": 10, "map": "a", "kill_height": -10, )" + twoSpawns + "}", error)
                    .has_value());
    CHECK(error == "unsupported arena version");
    // The members version 4 had for the rules and the body are not read: they change nothing an arena says.
    const auto bare = sim::ParseArena(arena(twoSpawns), error);
    REQUIRE_MESSAGE(bare.has_value(), error);
    const auto old = sim::ParseArena(arena(R"("movement_speed": 9, "body_height": 3, "radius": 1, "eye_height": 2.5, "jump_height": 2, )"
                                           R"("gravity": 5, "fall_damage": {"safe_height": 1, "points_per_metre": 1}, )" + twoSpawns),
                                     error);
    REQUIRE_MESSAGE(old.has_value(), error);
    CHECK(old->width == bare->width);
    CHECK(old->depth == bare->depth);
    CHECK(old->map == bare->map);
    CHECK(old->floor == bare->floor);
    CHECK(old->killHeight == bare->killHeight);
    REQUIRE(old->spawns.size() == bare->spawns.size());
    for (std::size_t i = 0; i < old->spawns.size(); ++i) {
        CHECK(old->spawns[i].position.x == bare->spawns[i].position.x);
        CHECK(old->spawns[i].position.z == bare->spawns[i].position.z);
    }
    // The bounds: no floor (absent), a kill height (required, below the floor and every spawn).
    CHECK_FALSE(bare->floor.has_value());
    CHECK(bare->killHeight == -10.0F);
    CHECK_FALSE(sim::ParseArena(R"({"version": 5, "id": "a", "width": 10, "depth": 10, "map": "a", )" + twoSpawns + "}", error).has_value());
    CHECK(sim::ParseArena(arena(R"("floor": 0, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": -10, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("floor": 0.5, )" + twoSpawns), error).has_value());  // a spawn below the floor
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, -10, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error)
                    .has_value());  // a spawn not above the kill height
}

TEST_CASE("PvP arena: the scene an arena names is a path inside the content, written as a mode's rule files") {
    const auto arena = [](const std::string& scene) {
        return R"({"version": 5, "id": "a", "width": 10, "depth": 10, "map": "a", "kill_height": -10, "scene": )" + scene +
               R"(, "spawns": [{"position": [1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}]})";
    };
    std::string error;
    const auto furnished = sim::ParseArena(arena(R"("scenes/pvp_arena.json")"), error);
    REQUIRE_MESSAGE(furnished.has_value(), error);
    REQUIRE(furnished->scene.has_value());
    CHECK_MESSAGE(*furnished->scene == "scenes/pvp_arena.json", "[pvp-arena-scene] " << furnished->scene.value_or("(none)"));
    const auto plain = sim::ParseArena(arena(R"("pvp_arena.json")"), error);
    REQUIRE_MESSAGE(plain.has_value(), error);
    CHECK(plain->scene == std::optional<std::string>("pvp_arena.json"));
    for (const std::string bad : {R"("")", R"("../scenes/a.json")", R"("scenes/../a.json")", R"("/scenes/a.json")", R"("scenes//a.json")",
                                  R"("./a.json")", R"("scenes\\a.json")", R"("C:/a.json")", R"(7)"}) {
        CAPTURE(bad);
        CHECK_MESSAGE(!sim::ParseArena(arena(bad), error).has_value(), "[pvp-arena-scene-path] " << bad);
    }
}
