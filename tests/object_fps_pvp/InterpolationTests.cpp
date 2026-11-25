// The remote players' interpolation timeline and sampling (present/Interpolation.hpp; port declaration, segment 6).
#include <doctest/doctest.h>

#include "object_fps_pvp/present/Interpolation.hpp"

#include <cmath>
#include <numbers>
#include <vector>

namespace {
using namespace object_fps_pvp::present;
constexpr double kStepSeconds = 0.015625;
} // namespace

TEST_CASE("interpolation: the delay is max(send interval x 1.2, 5 ms) plus four jitters plus one tick") {
    const TimelineSettings settings;
    CHECK(InterpolationDelay(settings, 0.0, kStepSeconds) == doctest::Approx(1.2 + 1.0));
    CHECK(InterpolationDelay(settings, 0.5, kStepSeconds) == doctest::Approx(1.2 + 2.0 + 1.0));
    // A long send interval in ticks of a short step: the ratio wins; a tiny one: the 5 ms minimum wins.
    TimelineSettings rare = settings;
    rare.sendIntervalTicks = 0.01;
    CHECK(InterpolationDelay(rare, 0.0, kStepSeconds) == doctest::Approx(0.005 / kStepSeconds + 1.0));
}

TEST_CASE("interpolation: the timeline jumps to its objective first, then moves by the frame's time") {
    const TimelineSettings settings;
    InterpolationTimeline timeline;
    Advance(timeline, 0.25, 100.0, settings);
    CHECK(timeline.synced);
    CHECK(timeline.tick == 100.0);
    CHECK(timeline.jumps == 1);
    // On its objective: it moves by the frame's time at speed 1.
    Advance(timeline, 0.25, 100.25, settings);
    CHECK(timeline.tick == doctest::Approx(100.25));
    CHECK(timeline.speed == 1.0);
    // Past the snap margin: it jumps again.
    Advance(timeline, 0.25, 120.0, settings);
    CHECK(timeline.tick == 120.0);
    CHECK(timeline.jumps == 2);
}

TEST_CASE("interpolation: behind its objective three times in a row it speeds up; ahead it slows down; near it, speed 1") {
    const TimelineSettings settings;
    InterpolationTimeline timeline;
    Advance(timeline, 0.0, 50.0, settings);
    // 2 ticks behind, twice: not yet.
    Advance(timeline, 0.0, 52.0, settings);
    Advance(timeline, 0.0, 52.0, settings);
    CHECK(timeline.speed == 1.0);
    Advance(timeline, 0.0, 52.0, settings);
    CHECK(timeline.speed == doctest::Approx(1.05));
    // Moving at 1.05 it gains on the objective.
    const double before = timeline.tick;
    Advance(timeline, 1.0, 53.0, settings);
    CHECK(timeline.tick == doctest::Approx(before + 1.05));
    // Within a tick: back to 1.
    Advance(timeline, 0.0, timeline.tick + 0.5, settings);
    CHECK(timeline.speed == 1.0);
    // Ahead three times: slower.
    for (int i = 0; i < 3; ++i) Advance(timeline, 0.0, timeline.tick - 2.0, settings);
    CHECK(timeline.speed == doctest::Approx(1.0 / 1.05));
    CHECK(timeline.jumps == 1);
}

TEST_CASE("interpolation: a bracket blends between the entries around t and holds before the first and after the last") {
    const std::vector<std::uint64_t> ticks{10, 12, 13};
    const Bracket mid = BracketAt(ticks, 11.0);
    CHECK(mid.start == 0);
    REQUIRE(mid.end.has_value());
    CHECK(*mid.end == 1);
    CHECK(mid.fraction == doctest::Approx(0.5));
    const Bracket on = BracketAt(ticks, 12.0);
    CHECK(on.start == 1);
    CHECK(on.fraction == 0.0);
    const Bracket after = BracketAt(ticks, 20.0);
    CHECK(after.start == 2);
    CHECK_FALSE(after.end.has_value());
    const Bracket before = BracketAt(ticks, 5.0);
    CHECK(before.start == 0);
    CHECK_FALSE(before.end.has_value());
}

TEST_CASE("interpolation: the oldest entries go only while a pair to blend stays") {
    const std::vector<std::uint64_t> ticks{10, 12, 13, 15};
    CHECK(Drainable(ticks, 11.0) == 0);
    CHECK(Drainable(ticks, 12.0) == 1);
    CHECK(Drainable(ticks, 14.0) == 2);
    // Past the newest (the next state is late): two stay.
    CHECK(Drainable(ticks, 99.0) == 2);
    const std::vector<std::uint64_t> two{10, 12};
    CHECK(Drainable(two, 99.0) == 0);
}

TEST_CASE("interpolation: yaw goes the short way around") {
    const float pi = std::numbers::pi_v<float>;
    CHECK(LerpRadiansShortest(0.0F, 1.0F, 0.5) == doctest::Approx(0.5F));
    // From just below pi to just above -pi: through pi, not through 0.
    const float halfway = LerpRadiansShortest(pi - 0.1F, -pi + 0.1F, 0.5);
    CHECK(std::abs(std::abs(halfway) - pi) < 0.01F);
}

TEST_CASE("interpolation: a pose blends where it stands and looks, but not across a new life") {
    const RemotePose a{{0.0F, 0.0F, 0.0F}, 0.0F, 0.0F, 100};
    RemotePose b{{2.0F, 0.0F, 4.0F}, 1.0F, 0.5F, 100};
    const RemotePose mid = BlendPose(a, b, 0.25);
    CHECK(mid.feet.x == doctest::Approx(0.5F));
    CHECK(mid.feet.z == doctest::Approx(1.0F));
    CHECK(mid.yaw == doctest::Approx(0.25F));
    CHECK(mid.pitch == doctest::Approx(0.125F));
    // A respawn between the two: the old place until the new state's tick.
    b.life = 200;
    const RemotePose respawned = BlendPose(a, b, 0.9);
    CHECK_MESSAGE(respawned.feet.x == 0.0F, "[pvp-interp-life] no walk from the death to the spawn");
    CHECK(respawned.feet.z == 0.0F);
}
