#include <doctest/doctest.h>

#include "render/PrimitiveMesh.hpp"

namespace {

using namespace Engine::Render;

TEST_CASE("Primitive quads and cube expose indexed reusable mesh data") {
    const MeshData xy = MakeUnitQuadXY();
    const MeshData xz = MakeUnitQuadXZ();
    const MeshData cube = MakeUnitCube();

    CHECK(xy.vertices.size() == 4);
    CHECK(xy.indices.size() == 6);
    CHECK(xz.vertices.size() == 4);
    CHECK(xz.indices.size() == 6);
    CHECK(cube.vertices.size() == 24);
    CHECK(cube.indices.size() == 36);

    for (std::uint32_t index : cube.indices) {
        CHECK(index < cube.vertices.size());
    }
}

TEST_CASE("UV sphere validates segment counts and creates a closed seam") {
    CHECK_FALSE(MakeUvSphere(2, 8));
    CHECK_FALSE(MakeUvSphere(8, 2));
    CHECK_FALSE(MakeUvSphere(513, 8));

    auto sphere = MakeUvSphere(8, 16);
    REQUIRE(sphere);
    CHECK(sphere.value().vertices.size() == 9 * 17);
    CHECK(sphere.value().indices.size() == 8 * 16 * 6);
    CHECK(sphere.value().vertices.front().uv.x == doctest::Approx(0.0F));
    CHECK(sphere.value().vertices[16].uv.x == doctest::Approx(1.0F));
}

} // namespace
