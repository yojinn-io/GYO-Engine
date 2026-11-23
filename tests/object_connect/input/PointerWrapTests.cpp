// The pointer on wrapEdges levels (object_connect/input/PointerWrap.hpp),
// moved out of the original InputSystem without the window.
#include "TestSupport.hpp"

#include "object_connect/input/PointerWrap.hpp"

namespace object_connect::tests {
namespace {

using input::PointerWrap;
using input::PointerWrapResult;

constexpr AxisAlignedBox kPlayfield{{0.0f, 0.0f}, {1280.0f, 720.0f}};
constexpr Vec2 kClient{1280.0f, 720.0f};

[[nodiscard]] PointerWrapResult At(PointerWrap& wrap, const Vec2 client, const Vec2 relative = {}) {
    return wrap.Sample({client, kClient, relative}, kPlayfield);
}

void TestMapping(TestContext& context) {
    const Vec2 mapped = input::ClientToPlayfield({320.0f, 180.0f}, kPlayfield, {640.0f, 360.0f});
    context.Expect(NearlyEqual(mapped.x, 640.0f) && NearlyEqual(mapped.y, 360.0f),
                   "a half-size client maps to the playfield's scale");
    context.Expect(!input::CanMapPointer(kPlayfield, {0.0f, 720.0f}), "a client without a width maps nothing");
}

void TestCrossingAnEdge(TestContext& context) {
    PointerWrap wrap;
    const PointerWrapResult first = At(wrap, {100.0f, 100.0f});
    context.Expect(first.unwrappedPosition && NearlyEqual(first.unwrappedPosition->x, 100.0f) && !first.warpTo,
                   "the first sample starts tracking where the cursor is");

    const PointerWrapResult crossed = At(wrap, {0.0f, 100.0f});
    context.Expect(crossed.warpTo && NearlyEqual(crossed.warpTo->x, 1279.0f) && NearlyEqual(crossed.warpTo->y, 100.0f),
                   "reaching the left edge while moving left warps to the right edge");
    context.Expect(NearlyEqual(crossed.position.x, 1279.0f), "the pointer is on the right edge");
    context.Expect(crossed.unwrappedPosition && NearlyEqual(crossed.unwrappedPosition->x, -1.0f),
                   "the unwrapped pointer goes on past the left edge");

    const PointerWrapResult after = At(wrap, {1279.0f, 100.0f}, {1279.0f, 0.0f});
    context.Expect(!after.warpTo && after.unwrappedPosition && NearlyEqual(after.unwrappedPosition->x, -1.0f),
                   "the warp itself is no movement, and its relative sample is ignored once");

    const PointerWrapResult further = At(wrap, {1270.0f, 100.0f});
    context.Expect(further.unwrappedPosition && NearlyEqual(further.unwrappedPosition->x, -10.0f) &&
                       NearlyEqual(further.position.x, 1270.0f),
                   "moving on keeps the unwrapped pointer continuous");

    wrap.Reset();
    const PointerWrapResult restarted = At(wrap, {640.0f, 360.0f});
    context.Expect(restarted.unwrappedPosition && NearlyEqual(restarted.unwrappedPosition->x, 640.0f),
                   "after Reset the next sample starts again where the cursor is");
}

void TestPushAgainstAStoppedEdge(TestContext& context) {
    PointerWrap wrap;
    const PointerWrapResult still = At(wrap, {0.0f, 300.0f});
    context.Expect(!still.warpTo, "a cursor resting on the edge does not cross");
    const PointerWrapResult pushed = At(wrap, {0.0f, 300.0f}, {-3.0f, 0.0f});
    context.Expect(pushed.warpTo && NearlyEqual(pushed.warpTo->x, 1279.0f),
                   "the device's motion against a stopped edge crosses it");
    const PointerWrapResult down = At(wrap, {1279.0f, 719.0f});
    context.Expect(down.warpTo && NearlyEqual(down.warpTo->y, 0.0f) && down.unwrappedPosition &&
                       NearlyEqual(down.unwrappedPosition->y, 720.0f),
                   "the bottom edge wraps to the top");
}

} // namespace

void RunPointerWrapTests(TestContext& context) {
    TestMapping(context);
    TestCrossingAnEdge(context);
    TestPushAgainstAStoppedEdge(context);
}

} // namespace object_connect::tests
