#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>

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

TEST_CASE("Wire box outlines its twelve edges with valid triangles") {
    auto wire = MakeWireBox({-2.0F, -1.0F, 3.0F}, {4.0F, 5.0F, 6.0F}, 0.02F);
    REQUIRE(wire);
    CHECK(wire.value().indices.size() == 12U * 36U);
    for (const auto index : wire.value().indices) {
        CHECK(index < wire.value().vertices.size());
    }
    for (const auto& vertex : wire.value().vertices) {
        CHECK(vertex.position.x >= -2.011F);
        CHECK(vertex.position.x <= 4.011F);
        CHECK(vertex.position.y >= -1.011F);
        CHECK(vertex.position.y <= 5.011F);
        CHECK(vertex.position.z >= 2.989F);
        CHECK(vertex.position.z <= 6.011F);
    }
    CHECK_FALSE(MakeWireBox({1, 0, 0}, {0, 1, 1}));
    CHECK_FALSE(MakeWireBox({0, 0, 0}, {1, 1, 1}, 0.0F));
}

TEST_CASE("Wire capsules support arbitrary axes and coincident sphere endpoints") {
    for (const auto end : {Float3{0, 2, 0}, Float3{2, 0, 0},
                           Float3{1, 1, 1}, Float3{0, 0, 0}}) {
        auto wire = MakeWireCapsule({}, end, 0.4F, 0.01F);
        REQUIRE(wire);
        REQUIRE_FALSE(wire.value().vertices.empty());
        CHECK(wire.value().indices.size() % 3 == 0);
        const float lengthSquared = end.x * end.x + end.y * end.y + end.z * end.z;
        for (const auto& vertex : wire.value().vertices) {
            const auto point = vertex.position;
            REQUIRE(std::isfinite(point.x));
            REQUIRE(std::isfinite(point.y));
            REQUIRE(std::isfinite(point.z));
            const float parameter = lengthSquared > 0.0F
                ? std::clamp((point.x * end.x + point.y * end.y + point.z * end.z) /
                             lengthSquared, 0.0F, 1.0F) : 0.0F;
            const float distance = std::hypot(point.x - parameter * end.x,
                point.y - parameter * end.y, point.z - parameter * end.z);
            // Segment chords approximate the surface; tube thickness is allowed.
            CHECK(distance >= 0.38F);
            CHECK(distance <= 0.408F);
        }
        for (const auto index : wire.value().indices) {
            CHECK(index < wire.value().vertices.size());
        }
    }
    CHECK_FALSE(MakeWireCapsule({}, {}, -1.0F));
    CHECK_FALSE(MakeWireCapsule({}, {}, 1.0F, 0.01F, 4));
    CHECK_FALSE(MakeWireCapsule({}, {std::numeric_limits<float>::infinity(), 0, 0}, 1));
}

} // namespace
