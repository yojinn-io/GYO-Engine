#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/Visibility.hpp"

#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/scalar/Angle.hpp"

using namespace Gyo::Tools::VoxelMapEditor;
namespace Math = Engine::Math;

// Judgment C2 of ve-cull (docs/tools/plans/v8/voxel-editor-cull.md): which chunks a frame draws.

namespace {

// A camera at the origin looking along +Z (90 degrees, far 1000) and a draw distance of 50 around center.
ChunkView ViewAround(const Math::Vec3 center) {
    return MakeChunkView(Math::MakePerspective(Math::DegreesToRadians(90.0F), 1.0F, 0.5F, 1000.0F), center, 50.0F);
}

Math::Aabb Box(const Math::Vec3 minimum, const Math::Vec3 maximum) { return {minimum, maximum}; }

} // namespace

TEST_CASE("Visibility: a chunk ahead and within the draw distance is drawn") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK(Draws(view, Box({-8.0F, -8.0F, 10.0F}, {8.0F, 8.0F, 26.0F})));
}

TEST_CASE("Visibility: a chunk past the draw distance is not drawn, touching it is") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK_FALSE(Draws(view, Box({-8.0F, -8.0F, 60.0F}, {8.0F, 8.0F, 76.0F})));
    CHECK(Draws(view, Box({-8.0F, -8.0F, 50.0F}, {8.0F, 8.0F, 66.0F})));
    // The distance is in x and z together: (40, 40) is about 56.6 away.
    CHECK_FALSE(Draws(view, Box({40.0F, -8.0F, 40.0F}, {56.0F, 8.0F, 56.0F})));
    CHECK(Draws(view, Box({30.0F, -8.0F, 30.0F}, {46.0F, 8.0F, 46.0F})));
}

TEST_CASE("Visibility: the draw distance ignores height") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK(Draws(view, Box({-8.0F, 100.0F, 200.0F}, {8.0F, 116.0F, 216.0F})) == false); // far in z
    const ChunkView high = ViewAround({0.0F, 400.0F, 300.0F});
    CHECK(Draws(high, Box({-8.0F, -8.0F, 290.0F}, {8.0F, 8.0F, 306.0F})));
}

TEST_CASE("Visibility: the distance is from the center, not from the camera") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 100.0F});
    CHECK(Draws(view, Box({-8.0F, -8.0F, 90.0F}, {8.0F, 8.0F, 106.0F})));
    CHECK_FALSE(Draws(view, Box({-8.0F, -8.0F, 10.0F}, {8.0F, 8.0F, 26.0F})));
}

TEST_CASE("Visibility: a chunk within the distance but outside the frustum is not drawn") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK_FALSE(Draws(view, Box({-8.0F, -8.0F, -26.0F}, {8.0F, 8.0F, -10.0F}))); // behind
    CHECK_FALSE(Draws(view, Box({-40.0F, -8.0F, 10.0F}, {-30.0F, 8.0F, 20.0F}))); // left of the 90 degree view
}

TEST_CASE("Visibility: the draw distance bounds") {
    CHECK(MinDrawDistanceCells <= DefaultDrawDistanceCells);
    CHECK(DefaultDrawDistanceCells <= MaxDrawDistanceCells);
    CHECK(DefaultDrawDistanceCells == 256.0F);
}
