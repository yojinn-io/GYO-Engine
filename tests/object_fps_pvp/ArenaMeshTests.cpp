#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ArenaMesh.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace fps::pvp;

// v8-product-arena-v2, decision R2: the Client draws a baked map as one mesh
// and one palette texture, and a schema 1 arena (no geometry) as boxes.

namespace {
Engine::VoxelMesh::VoxelMeshGeometry TwoFaces() {
    const std::string text = std::string(R"({"schema": "gyo.voxel_mesh", "version": 1, "source_sha256": ")") +
        std::string(64, 'a') + R"(", "build_version": "test", "origin": [0, 0, 0], "cell_size": 1.0, )"
        R"("palette": [{"name": "a", "faces": [[1,2,3,255],[4,5,6,255],[7,8,9,255],[10,11,12,255],[13,14,15,255],[16,17,18,255]]}], )"
        R"("quads": [[0, 0, 0, 3, 16, 16, 0], [0, 16, 0, 2, 16, 16, 0]], "colliders": []})";
    const auto document = Engine::VoxelMesh::ParseVoxelMesh(text);
    REQUIRE(document);
    return Engine::VoxelMesh::ExpandVoxelMesh(*document);
}
} // namespace

TEST_CASE("A baked arena is drawn as its mesh and palette; a schema 1 arena as boxes") {
    const auto geometry = TwoFaces();
    CHECK(DrawsBakedMesh(geometry));
    CHECK_FALSE(DrawsBakedMesh(Engine::VoxelMesh::VoxelMeshGeometry{}));

    const auto mesh = ArenaRenderMesh(geometry);
    REQUIRE(mesh.vertices.size() == geometry.vertices.size());
    CHECK(mesh.indices == geometry.indices);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        CHECK(std::bit_cast<std::uint32_t>(mesh.vertices[i].position.x) ==
              std::bit_cast<std::uint32_t>(geometry.vertices[i].position.x));
        CHECK(std::bit_cast<std::uint32_t>(mesh.vertices[i].position.y) ==
              std::bit_cast<std::uint32_t>(geometry.vertices[i].position.y));
        CHECK(std::bit_cast<std::uint32_t>(mesh.vertices[i].position.z) ==
              std::bit_cast<std::uint32_t>(geometry.vertices[i].position.z));
        CHECK(std::bit_cast<std::uint32_t>(mesh.vertices[i].uv.x) == std::bit_cast<std::uint32_t>(geometry.vertices[i].uv.x));
        CHECK(std::bit_cast<std::uint32_t>(mesh.vertices[i].uv.y) == std::bit_cast<std::uint32_t>(geometry.vertices[i].uv.y));
    }
    const auto image = ArenaPaletteImage(geometry);
    CHECK(image.width == 1);
    CHECK(image.height == 6);
    CHECK(image.rowPitch == 0);
    CHECK(image.rgba8.size() == 24);
    CHECK(image.colorSpace == Engine::Render::TextureColorSpace::SRgb);
}

TEST_CASE("The baked map drawn is the one of the arena the Match chose") {
    std::vector<Arena> installed(3);
    installed[0].id = "a";
    installed[1].id = "b";
    installed[2].id = "c";
    CHECK(InstalledArenaIndex(installed, "a") == std::optional<std::size_t>{0});
    CHECK(InstalledArenaIndex(installed, "c") == std::optional<std::size_t>{2});
    CHECK_FALSE(InstalledArenaIndex(installed, "d"));
}
