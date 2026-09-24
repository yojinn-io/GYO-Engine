#include <doctest/doctest.h>

#include "engine/runtime/FixedTickRuntime.hpp"

#include <limits>
#include <vector>

using Engine::Runtime::FixedTickRuntime;
using Engine::Runtime::TickContext;

TEST_CASE("FixedTickRuntime accumulates elapsed time without owning a clock") {
    FixedTickRuntime runtime(60, 5);
    std::vector<TickContext> ticks;
    const auto callback = [&](const TickContext& tick) { ticks.push_back(tick); };
    CHECK(runtime.Advance(1.0 / 120, callback).steps == 0);
    const auto second = runtime.Advance(1.0 / 120, callback);
    CHECK(second.steps == 1);
    REQUIRE(ticks.size() == 1);
    CHECK(ticks[0].tickId == 1);
    CHECK(ticks[0].deltaSeconds == doctest::Approx(1.0 / 60));
    CHECK(second.secondsUntilNextTick == doctest::Approx(1.0 / 60));
    CHECK(runtime.Advance(2.0 / 60, callback).steps == 2);
    CHECK(runtime.TickId() == 3);
}

TEST_CASE("FixedTickRuntime bounds catch-up and preserves only fractional backlog") {
    FixedTickRuntime runtime(60, 5);
    const auto result = runtime.Advance(10.5 / 60, [](const TickContext&) {});
    CHECK(result.steps == 5);
    CHECK(result.droppedSeconds == doctest::Approx(5.0 / 60));
    CHECK(result.secondsUntilNextTick == doctest::Approx(0.5 / 60));
    CHECK(runtime.TickId() == 5);
    CHECK(runtime.Advance(0.5 / 60, [](const TickContext&) {}).steps == 1);
    CHECK(runtime.TickId() == 6);
    runtime.Reset();
    CHECK(runtime.TickId() == 0);
    CHECK(runtime.Advance(0, [](const TickContext&) {}).secondsUntilNextTick == doctest::Approx(1.0 / 60));
}

TEST_CASE("FixedTickRuntime rejects invalid configuration and elapsed time") {
    CHECK_THROWS_AS(FixedTickRuntime(0), std::invalid_argument);
    CHECK_THROWS_AS(FixedTickRuntime(60, 0), std::invalid_argument);
    FixedTickRuntime runtime;
    CHECK_THROWS_AS(runtime.Advance(-1, [](const TickContext&) {}), std::invalid_argument);
    CHECK_THROWS_AS(runtime.Advance(std::numeric_limits<double>::infinity(), [](const TickContext&) {}), std::invalid_argument);
    CHECK_THROWS_AS(runtime.Advance(std::numeric_limits<double>::quiet_NaN(), [](const TickContext&) {}), std::invalid_argument);
    CHECK(runtime.TickId() == 0);
}

TEST_CASE("FixedTickRuntime partitioned time agrees with one advance when no steps are dropped") {
    FixedTickRuntime single(Engine::Runtime::TickSettings{60, 64});
    FixedTickRuntime partitioned;
    const auto whole = single.Advance(0.25, [](const TickContext&) {});
    Engine::Runtime::FixedTickAdvance fraction;
    std::uint32_t steps{};
    for (int index = 0; index < 250; ++index) {
        fraction = partitioned.Advance(0.001, [](const TickContext&) {});
        steps += fraction.steps;
        CHECK(fraction.droppedSeconds == 0);
    }
    CHECK(whole.steps == 15);
    CHECK(steps == whole.steps);
    CHECK(partitioned.TickId() == single.TickId());
    CHECK(fraction.secondsUntilNextTick == doctest::Approx(whole.secondsUntilNextTick));
}

TEST_CASE("FixedTickRuntime enormous elapsed time stays bounded without integer backlog overflow") {
    FixedTickRuntime runtime;
    std::vector<std::uint64_t> ticks;
    const auto result = runtime.Advance(1.0e100, [&](const TickContext& tick) {
        ticks.push_back(tick.tickId);
    });
    CHECK(result.steps == 5);
    REQUIRE(ticks.size() == 5);
    CHECK(ticks.front() == 1);
    CHECK(ticks.back() == 5);
    CHECK(std::isfinite(result.droppedSeconds));
    CHECK(result.droppedSeconds == doctest::Approx(1.0e100));
    CHECK(result.secondsUntilNextTick > 0);
    CHECK(result.secondsUntilNextTick <= runtime.StepSeconds());
    CHECK(runtime.Advance(0, [](const TickContext&) {}).steps == 0);
    CHECK(runtime.TickId() == 5);
}
