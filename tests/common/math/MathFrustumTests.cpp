#include <doctest/doctest.h>

#include <cmath>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Frustum.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Angle.hpp"

using namespace Engine::Math;

namespace {

// 90 degree square view from the origin along +Z: inside is |x| <= z, |y| <= z, 1 <= z <= 100.
Frustum TestFrustum() {
    return MakeFrustum(MakePerspective(DegreesToRadians(90.0F), 1.0F, 1.0F, 100.0F));
}

Aabb Box(const Vec3 minimum, const Vec3 maximum) { return {minimum, maximum}; }

} // namespace

TEST_CASE("Frustum planes are unit length and face inward") {
    const Frustum frustum = TestFrustum();
    const Vec3 inside{0.0F, 0.0F, 10.0F};
    for (const Plane& plane : frustum.planes) {
        CHECK(Length(plane.normal) == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK(SignedDistance(plane, inside) > 0.0F);
    }
    // Order: left, right, bottom, top, near, far.
    CHECK(frustum.planes[0].normal.x > 0.0F);
    CHECK(frustum.planes[1].normal.x < 0.0F);
    CHECK(frustum.planes[2].normal.y > 0.0F);
    CHECK(frustum.planes[3].normal.y < 0.0F);
    CHECK(frustum.planes[4].normal.z > 0.0F);
    CHECK(frustum.planes[5].normal.z < 0.0F);
}

TEST_CASE("Frustum near plane follows the 0..1 depth convention") {
    const Frustum frustum = TestFrustum();
    // Exact for these clip values: the near row is (0, 0, a, -a) with a = 100 / 99.
    CHECK(frustum.planes[4].normal.x == 0.0F);
    CHECK(frustum.planes[4].normal.y == 0.0F);
    CHECK(frustum.planes[4].normal.z == 1.0F);
    CHECK(frustum.planes[4].distance == 1.0F);
    // In front of the camera but nearer than the near plane.
    CHECK_FALSE(Intersects(frustum, Box({-0.1F, -0.1F, 0.6F}, {0.1F, 0.1F, 0.9F})));
}

TEST_CASE("Frustum-Aabb overlap keeps boxes inside and across planes") {
    const Frustum frustum = TestFrustum();
    CHECK(Intersects(frustum, Box({-1.0F, -1.0F, 9.0F}, {1.0F, 1.0F, 11.0F})));
    // Straddling one plane at a time.
    CHECK(Intersects(frustum, Box({-15.0F, -1.0F, 9.0F}, {-5.0F, 1.0F, 11.0F})));
    CHECK(Intersects(frustum, Box({5.0F, -1.0F, 9.0F}, {15.0F, 1.0F, 11.0F})));
    CHECK(Intersects(frustum, Box({-1.0F, -15.0F, 9.0F}, {1.0F, -5.0F, 11.0F})));
    CHECK(Intersects(frustum, Box({-1.0F, 5.0F, 9.0F}, {1.0F, 15.0F, 11.0F})));
    CHECK(Intersects(frustum, Box({-1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, 2.0F})));
    CHECK(Intersects(frustum, Box({-1.0F, -1.0F, 90.0F}, {1.0F, 1.0F, 110.0F})));
    // A box enclosing the whole frustum.
    CHECK(Intersects(frustum, Box({-1000.0F, -1000.0F, -1000.0F}, {1000.0F, 1000.0F, 1000.0F})));
}

TEST_CASE("Frustum-Aabb overlap is closed: touching the near plane counts") {
    const Frustum frustum = TestFrustum();
    CHECK(Intersects(frustum, Box({-0.1F, -0.1F, 0.5F}, {0.1F, 0.1F, 1.0F})));
}

TEST_CASE("Frustum-Aabb overlap rejects boxes outside one plane") {
    const Frustum frustum = TestFrustum();
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, -1.0F, -11.0F}, {1.0F, 1.0F, -9.0F}))); // behind
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, -1.0F, 101.0F}, {1.0F, 1.0F, 120.0F}))); // beyond far
    CHECK_FALSE(Intersects(frustum, Box({-30.0F, -1.0F, 9.0F}, {-20.0F, 1.0F, 11.0F}))); // left
    CHECK_FALSE(Intersects(frustum, Box({20.0F, -1.0F, 9.0F}, {30.0F, 1.0F, 11.0F})));   // right
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, -30.0F, 9.0F}, {1.0F, -20.0F, 11.0F}))); // below
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, 20.0F, 9.0F}, {1.0F, 30.0F, 11.0F})));   // above
}

TEST_CASE("Frustum-Aabb overlap is conservative near an edge") {
    const Frustum frustum = TestFrustum();
    // Outside: no point has both x >= -z and z <= 100 when x <= -101. Yet the box
    // is not fully outside the left plane nor fully outside the far plane, so the
    // culling test keeps it.
    CHECK(Intersects(frustum, Box({-110.0F, -1.0F, 95.0F}, {-101.0F, 1.0F, 105.0F})));
}

TEST_CASE("Frustum from a view-projection product follows the camera") {
    // A camera at (0, 0, -50) looking along +Z: view = translation by +50 on z.
    const Matrix4 clip = Multiply(MakePerspective(DegreesToRadians(90.0F), 1.0F, 1.0F, 100.0F),
                                  MakeTranslation({0.0F, 0.0F, 50.0F}));
    const Frustum frustum = MakeFrustum(clip);
    CHECK(Intersects(frustum, Box({-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F})));
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, -1.0F, 60.0F}, {1.0F, 1.0F, 70.0F}))); // beyond far (z > 50)
    CHECK_FALSE(Intersects(frustum, Box({-1.0F, -1.0F, -60.0F}, {1.0F, 1.0F, -55.0F}))); // behind
}
