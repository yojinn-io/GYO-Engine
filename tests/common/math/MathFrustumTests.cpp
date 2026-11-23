#include <doctest/doctest.h>

#include <cmath>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Frustum.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Angle.hpp"

using namespace GYO::Engine::Math;

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

// MakeOrthographic (transform-camera declaration 4.4): the clip convention of MakePerspective. The bounds are
// powers of two apart, so every mapped corner is exact.
TEST_CASE("MakeOrthographic maps the box corners to the clip corners with depth 0 at near and 1 at far") {
    const Matrix4 m = MakeOrthographic(-2.0F, 6.0F, -1.0F, 3.0F, 1.0F, 5.0F);
    const Vec3 low = TransformPoint(m, {-2.0F, -1.0F, 1.0F});
    const Vec3 high = TransformPoint(m, {6.0F, 3.0F, 5.0F});
    CHECK_MESSAGE((low.x == -1.0F && low.y == -1.0F && low.z == 0.0F), "[math-ortho-low] " << low.x << " " << low.y << " " << low.z);
    CHECK_MESSAGE((high.x == 1.0F && high.y == 1.0F && high.z == 1.0F), "[math-ortho-high] " << high.x << " " << high.y << " " << high.z);
    CHECK(m.values[15] == 1.0F);
    CHECK(m.values[3] == 0.0F);
    CHECK(m.values[7] == 0.0F);
    CHECK(m.values[11] == 0.0F);
}

TEST_CASE("MakeOrthographic with nearClip above farClip puts a larger z nearer") {
    const Matrix4 m = MakeOrthographic(-640.0F, 640.0F, -360.0F, 360.0F, 1000.0F, -1000.0F);
    CHECK_MESSAGE(TransformPoint(m, {0.0F, 0.0F, 1000.0F}).z == 0.0F, "[math-ortho-swapped-near]");
    CHECK_MESSAGE(TransformPoint(m, {0.0F, 0.0F, -1000.0F}).z == 1.0F, "[math-ortho-swapped-far]");
    CHECK(TransformPoint(m, {0.0F, 0.0F, 2.0F}).z < TransformPoint(m, {0.0F, 0.0F, 1.0F}).z);
}

TEST_CASE("MakeFrustum of MakeOrthographic keeps the box and rejects what is outside") {
    const Frustum frustum = MakeFrustum(MakeOrthographic(-2.0F, 6.0F, -1.0F, 3.0F, 1.0F, 5.0F));
    CHECK(Intersects(frustum, Box({0.0F, 0.0F, 2.0F}, {1.0F, 1.0F, 3.0F})));
    CHECK_FALSE(Intersects(frustum, Box({7.0F, 0.0F, 2.0F}, {8.0F, 1.0F, 3.0F})));
    CHECK_FALSE(Intersects(frustum, Box({0.0F, 0.0F, 6.0F}, {1.0F, 1.0F, 7.0F})));
    CHECK_FALSE(Intersects(frustum, Box({0.0F, 0.0F, -1.0F}, {1.0F, 1.0F, 0.5F})));
}

TEST_CASE("Multiply of a Matrix4 and a Vec4 is the full homogeneous product") {
    const Matrix4 m = MakePerspective(DegreesToRadians(90.0F), 1.0F, 1.0F, 100.0F);
    const Vec4 clip = Multiply(m, MakePoint4({2.0F, 3.0F, 10.0F}));
    CHECK(clip.w == 10.0F);  // clip w = view z
    CHECK(clip.x == doctest::Approx(2.0F));
    CHECK(clip.y == doctest::Approx(3.0F));
    const Vec4 direction = Multiply(MakeTranslation({5.0F, 6.0F, 7.0F}), MakeDirection4({1.0F, 0.0F, 0.0F}));
    CHECK((direction.x == 1.0F && direction.y == 0.0F && direction.z == 0.0F && direction.w == 0.0F));
}
