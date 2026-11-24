// The arena files and the baked voxel geometry the game reads itself (port declaration PV8): the contract's unit
// conversion against the shipped pvp_arena (the alignment test the reader owns), the members it checks and the
// arena file's rules.
#include <doctest/doctest.h>

#include "object_fps_pvp/data/VoxelMeshFile.hpp"
#include "object_fps_pvp/sim/Arena.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace {
using namespace object_fps_pvp;

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

const std::string kMaps = std::string(PVP_CONTENT_ROOT) + "/maps/";

std::string Mesh(const std::string& quads, const std::string& colliders, const std::string& origin = "[0, 0, 0]") {
    return R"({"schema": "gyo.voxel_mesh", "version": 1, "source_sha256": ")" + std::string(64, 'a') +
           R"(", "build_version": "test 1", "origin": )" + origin +
           R"(, "cell_size": 0.5, "palette": [{"name": "a", "faces": [[1,2,3,255],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]}], "quads": )" +
           quads + R"(, "colliders": )" + colliders + "}";
}
} // namespace

TEST_CASE("PvP voxel mesh: pvp_arena's five colliders become the arena's walls in world units") {
    std::string error;
    const auto file = data::ParseVoxelMeshFile(ReadFile(kMaps + "pvp_arena.voxel_mesh.json"), error);
    REQUIRE_MESSAGE(file.has_value(), error);
    const auto boxes = data::ColliderBoxes(*file);
    REQUIRE(boxes.size() == 5);
    // origin (-1, 0, -1), cell 1: units / 16 + origin.
    const float expected[5][6] = {
        {-1, 0, -1, 0, 3, 21},  // the -X wall
        {0, 0, -1, 20, 3, 0},   // the -Z wall
        {0, 0, 20, 20, 3, 21},  // the +Z wall
        {9, 0, 9, 11, 2, 11},   // the pillar
        {20, 0, -1, 21, 3, 21}, // the +X wall
    };
    for (std::size_t i = 0; i < 5; ++i) {
        CAPTURE(i);
        CHECK(boxes[i].minimum.x == expected[i][0]);
        CHECK(boxes[i].minimum.y == expected[i][1]);
        CHECK(boxes[i].minimum.z == expected[i][2]);
        CHECK(boxes[i].maximum.x == expected[i][3]);
        CHECK(boxes[i].maximum.y == expected[i][4]);
        CHECK(boxes[i].maximum.z == expected[i][5]);
    }
    CHECK(file->palette.size() == 1);
    CHECK(!file->quads.empty());
}

TEST_CASE("PvP voxel mesh: units are 1/16 of a cell from origin, origin in cells") {
    std::string error;
    const auto file = data::ParseVoxelMeshFile(Mesh("[[0, 0, 0, 2, 16, 16, 0]]", R"([{"min": [8, 0, 0], "max": [24, 16, 32]}])", "[2, 0, -4]"),
                                               error);
    REQUIRE_MESSAGE(file.has_value(), error);
    const auto boxes = data::ColliderBoxes(*file);
    REQUIRE(boxes.size() == 1);
    // x: (8 + 32) x 0.5 / 16 = 1.25; z: (0 - 64) x 0.5 / 16 = -2.
    CHECK(boxes[0].minimum.x == 1.25F);
    CHECK(boxes[0].maximum.x == 1.75F);
    CHECK(boxes[0].minimum.z == -2.0F);
    CHECK(boxes[0].maximum.z == -1.0F);
    CHECK(boxes[0].maximum.y == 0.5F);
    CHECK(file->quads[0].direction == 2);
    CHECK(file->palette[0][2][0] == 7);
}

TEST_CASE("PvP voxel mesh: what the reader refuses") {
    std::string error;
    CHECK_FALSE(data::ParseVoxelMeshFile(Mesh("[[0, 0, 0, 6, 16, 16, 0]]", "[]"), error).has_value());
    CHECK(error.find("direction") != std::string::npos);
    CHECK_FALSE(data::ParseVoxelMeshFile(Mesh("[[0, 0, 0, 1, 16, 16, 1]]", "[]"), error).has_value());
    CHECK(error.find("palette") != std::string::npos);
    CHECK_FALSE(data::ParseVoxelMeshFile(Mesh("[[0, 0, 0, 1, 0, 16, 0]]", "[]"), error).has_value());
    CHECK_FALSE(data::ParseVoxelMeshFile(Mesh("[[0, 0, 0, 1, 16, 16, 0]]", R"([{"min": [0, 0, 0], "max": [0, 16, 16]}])"), error).has_value());
    CHECK(error.find("min") != std::string::npos);
    CHECK_FALSE(data::ParseVoxelMeshFile(R"({"schema": "gyo.voxel_map", "version": 1})", error).has_value());
    CHECK(error.find("schema") != std::string::npos);
    CHECK_FALSE(data::ParseVoxelMeshFile("not json", error).has_value());
}

TEST_CASE("PvP arena: the shipped arenas read with their spawns and geometry") {
    std::string error;
    const auto list = sim::ParseArenaList(ReadFile(kMaps + "arenas.json"), error);
    REQUIRE_MESSAGE(list.has_value(), error);
    REQUIRE(list->size() == 3);
    CHECK(list->front() == "pvp_arena.json");
    for (const std::string& name : *list) {
        CAPTURE(name);
        const auto arena = sim::ParseArena(ReadFile(kMaps + name), error);
        REQUIRE_MESSAGE(arena.has_value(), error);
        CHECK(arena->spawns.size() >= 2);
        CHECK(!arena->geometry.empty());
    }
    const auto arena = sim::ParseArena(ReadFile(kMaps + "pvp_arena.json"), error);
    REQUIRE(arena.has_value());
    CHECK(arena->id == "pvp_training_v1");
    CHECK(arena->width == 20.0F);
    CHECK(arena->movementSpeed == 3.0F);
    CHECK(arena->jumpHeight == 0.6F);  // not in the file: the default
    CHECK(arena->gravity == 18.0F);
    CHECK(arena->spawns[1].position.z == 12.0F);
}

TEST_CASE("PvP arena: the rules of an arena file") {
    const auto arena = [](const std::string& patch) {
        return R"({"version": 2, "id": "a", "width": 10, "depth": 10, "geometry": "a.voxel_mesh.json", )" + patch + "}";
    };
    const std::string twoSpawns = R"("spawns": [{"position": [1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])";
    std::string error;
    CHECK(sim::ParseArena(arena(twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, 0, 1], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [0.1, 0, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("spawns": [{"position": [1, -1, 1], "yaw": 0}, {"position": [9, 0, 9], "yaw": 0}])"), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("eye_height": 2, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("body_height": 0.4, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(arena(R"("movement_speed": -1, )" + twoSpawns), error).has_value());
    CHECK_FALSE(sim::ParseArena(R"({"version": 2, "id": "a", "width": 10, "depth": 10, "geometry": "../a.json", )" + twoSpawns + "}", error)
                    .has_value());
    CHECK_FALSE(sim::ParseArena(R"({"version": 1, "id": "a", "width": 10, "depth": 10, "walls": [], )" + twoSpawns + "}", error).has_value());
}

TEST_CASE("PvP voxel mesh: the faces expand to four vertices each with the colour's texel, the palette to an image") {
    std::string error;
    const auto file = data::ParseVoxelMeshFile(
        Mesh("[[0, 0, 0, 2, 16, 32, 0], [16, 0, 0, 1, 16, 16, 0]]", "[]"), error);
    REQUIRE_MESSAGE(file.has_value(), error);
    const data::VoxelSurface surface = data::ExpandQuads(*file);
    REQUIRE(surface.positions.size() == 8);
    REQUIRE(surface.indices.size() == 12);
    // The +Y face at y 0 runs 16 units along X (0.5) and 32 along Z (1.0).
    CHECK(surface.positions[2].x == 0.5F);
    CHECK(surface.positions[2].z == 1.0F);
    CHECK(surface.normals[0].y == 1.0F);
    // The -X face at x 16 units (0.5): along Z and Y.
    CHECK(surface.positions[4].x == 0.5F);
    CHECK(surface.positions[6].y == 0.5F);
    CHECK(surface.normals[4].x == -1.0F);
    // The texel centres: one column, row = direction.
    CHECK(surface.uvs[0][0] == 0.5F);
    CHECK(surface.uvs[0][1] == doctest::Approx(2.5 / 6.0));
    CHECK(surface.uvs[4][1] == doctest::Approx(1.5 / 6.0));
    CHECK(surface.indices[6] == 4);
    const data::VoxelPaletteImage image = data::PaletteImage(*file);
    CHECK(image.width == 1);
    CHECK(image.height == 6);
    REQUIRE(image.pixels.size() == 24);
    CHECK(image.pixels[2 * 4 + 0] == 7);  // the +Y row's red
    CHECK(image.pixels[5 * 4 + 2] == 18); // the -Z row's blue
}
