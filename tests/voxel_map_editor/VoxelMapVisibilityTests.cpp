#include <doctest/doctest.h>

#include "gyo/voxel_map_editor/Editing.hpp"
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

Math::Aabb AabbOf(const Math::Vec3 minimum, const Math::Vec3 maximum) { return {minimum, maximum}; }

} // namespace

TEST_CASE("Visibility: a chunk ahead and within the draw distance is drawn") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK(Draws(view, AabbOf({-8.0F, -8.0F, 10.0F}, {8.0F, 8.0F, 26.0F})));
}

TEST_CASE("Visibility: a chunk past the draw distance is not drawn, touching it is") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK_FALSE(Draws(view, AabbOf({-8.0F, -8.0F, 60.0F}, {8.0F, 8.0F, 76.0F})));
    CHECK(Draws(view, AabbOf({-8.0F, -8.0F, 50.0F}, {8.0F, 8.0F, 66.0F})));
    // The distance is in x and z together: (40, 40) is about 56.6 away.
    CHECK_FALSE(Draws(view, AabbOf({40.0F, -8.0F, 40.0F}, {56.0F, 8.0F, 56.0F})));
    CHECK(Draws(view, AabbOf({30.0F, -8.0F, 30.0F}, {46.0F, 8.0F, 46.0F})));
}

TEST_CASE("Visibility: the draw distance ignores height") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK(Draws(view, AabbOf({-8.0F, 100.0F, 200.0F}, {8.0F, 116.0F, 216.0F})) == false); // far in z
    const ChunkView high = ViewAround({0.0F, 400.0F, 300.0F});
    CHECK(Draws(high, AabbOf({-8.0F, -8.0F, 290.0F}, {8.0F, 8.0F, 306.0F})));
}

TEST_CASE("Visibility: the distance is from the center, not from the camera") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 100.0F});
    CHECK(Draws(view, AabbOf({-8.0F, -8.0F, 90.0F}, {8.0F, 8.0F, 106.0F})));
    CHECK_FALSE(Draws(view, AabbOf({-8.0F, -8.0F, 10.0F}, {8.0F, 8.0F, 26.0F})));
}

TEST_CASE("Visibility: a chunk within the distance but outside the frustum is not drawn") {
    const ChunkView view = ViewAround({0.0F, 0.0F, 0.0F});
    CHECK_FALSE(Draws(view, AabbOf({-8.0F, -8.0F, -26.0F}, {8.0F, 8.0F, -10.0F}))); // behind
    CHECK_FALSE(Draws(view, AabbOf({-40.0F, -8.0F, 10.0F}, {-30.0F, 8.0F, 20.0F}))); // left of the 90 degree view
}

TEST_CASE("Visibility: the draw distance bounds") {
    CHECK(MinDrawDistanceCells <= DefaultDrawDistanceCells);
    CHECK(DefaultDrawDistanceCells <= MaxDrawDistanceCells);
    CHECK(DefaultDrawDistanceCells == 256.0F);
}

// Judgments E1 and E2 of ve-feel-1 (docs/tools/plans/v8/voxel-editor-feel-1.md): zoom and how far is drawn.

TEST_CASE("ve-feel-1 E1: the zoom range goes from two cells to framing the whole map") {
    // No cells: the default 16 x 4 x 16 box.
    const auto empty = ZoomLimits(std::nullopt, 0.5F);
    CHECK(empty.minimum == doctest::Approx(1.0F));
    CHECK(empty.maximum == doctest::Approx(Math::Length(Math::Vec3{16.0F, 4.0F, 16.0F}) * 0.5F * 0.9F));
    // One cell: its chunk, 16 cells a side.
    GridDocument one = NewGrid();
    one.Set({3, 2, 1}, {1, 0});
    const auto bounds = ChunkBounds(one);
    REQUIRE(bounds);
    CHECK(bounds->first == Vec3i{0, 0, 0});
    CHECK(bounds->second == Vec3i{16, 16, 16});
    const auto small = ZoomLimits(bounds, 1.0F);
    CHECK(small.minimum == doctest::Approx(2.0F));
    CHECK(small.maximum == doctest::Approx(Math::Length(Math::Vec3{16.0F, 16.0F, 16.0F}) * 0.9F));
    // A km^2 map: the maximum grows with the map, not a fixed number.
    const std::optional<std::pair<Vec3i, Vec3i>> km2{{{0, 0, 0}, {2000, 16, 2000}}};
    const auto large = ZoomLimits(km2, 1.0F);
    CHECK(large.maximum == doctest::Approx(Math::Length(Math::Vec3{2000.0F, 16.0F, 2000.0F}) * 0.9F));
    CHECK(large.maximum > 2500.0F);
    // A tiny map: the framing distance's floor of 4 cells.
    const std::optional<std::pair<Vec3i, Vec3i>> flat{{{0, 0, 0}, {1, 1, 1}}};
    const auto tiny = ZoomLimits(flat, 0.5F);
    CHECK(tiny.maximum == doctest::Approx(4.0F * 0.5F));
    CHECK(tiny.maximum >= tiny.minimum);
    // Bounds over several chunks, negative ones too: low inclusive, high exclusive, in cells.
    GridDocument two = NewGrid();
    two.Set({-20, 3, 5}, {1, 0});
    two.Set({40, -1, 33}, {1, 0});
    const auto spread = ChunkBounds(two);
    REQUIRE(spread);
    CHECK(spread->first == Vec3i{-32, -16, 0});
    CHECK(spread->second == Vec3i{48, 16, 48});
    CHECK_FALSE(ChunkBounds(NewGrid()));
    // The distance stays between the two.
    CHECK(ClampZoom(0.1F, small) == doctest::Approx(small.minimum));
    CHECK(ClampZoom(1e6F, small) == doctest::Approx(small.maximum));
    CHECK(ClampZoom(10.0F, small) == doctest::Approx(10.0F));
}

TEST_CASE("ve-feel-1 E2: zooming out draws farther than the panel's distance") {
    CHECK(DrawDistanceFor(128.0F, 10.0F) == doctest::Approx(128.0F));
    CHECK(DrawDistanceFor(128.0F, 400.0F) == doctest::Approx(600.0F));
}

TEST_CASE("ve-feel-1 E2: at most the nearest chunks are drawn, the rest reported as left out") {
    // 10 x 10 chunks of 16 cells on the ground, the camera far above looking down at the middle.
    std::vector<Math::Aabb> boxes;
    for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x)
            boxes.push_back(AabbOf({x * 16.0F, 0.0F, z * 16.0F}, {x * 16.0F + 16.0F, 16.0F, z * 16.0F + 16.0F}));
    const Math::Matrix4 clip = Math::Multiply(Math::MakePerspective(Math::DegreesToRadians(90.0F), 1.0F, 1.0F, 5000.0F),
                                              Math::Multiply(Math::MakeRotationX(-Math::DegreesToRadians(90.0F)),
                                                             Math::MakeTranslation({-80.0F, -1000.0F, -80.0F})));
    const ChunkView view = MakeChunkView(clip, {80.0F, 0.0F, 80.0F}, 1000.0F);
    // All in view and range: a building-sized map is drawn whole.
    const auto all = ChooseChunks(view, boxes, 100);
    CHECK_FALSE(all.capped);
    CHECK(all.indices.size() == 100);
    // Capped at 4: the four around the center (80, 80), in ascending order.
    const auto four = ChooseChunks(view, boxes, 4);
    CHECK(four.capped);
    CHECK(four.indices == std::vector<std::size_t>{44, 45, 54, 55});
    // The same choice every time.
    CHECK(ChooseChunks(view, boxes, 4).indices == four.indices);
    // Ties at the cut: the next ring of eight is equally far, the lowest index wins.
    CHECK(ChooseChunks(view, boxes, 5).indices == std::vector<std::size_t>{34, 44, 45, 54, 55});
    // The cap the declaration set: a building-sized map (768 chunks) is always drawn whole.
    CHECK(MaxDrawnChunks == 2000);
}
