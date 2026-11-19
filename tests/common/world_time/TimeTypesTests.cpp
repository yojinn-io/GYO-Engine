// Game time types and arithmetic (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 6.4;
// T1 judgment 1). No World is built: the arithmetic is fed numbers.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/world_time/TimeTypes.hpp"

namespace W = GYO::Engine::WorldTime;
using W::Nanoseconds;

namespace {

constexpr Nanoseconds kStep = 15'625'000;  // 64 Hz
constexpr Nanoseconds kMs = 1'000'000;

W::TickAccumulator Accumulator(Nanoseconds step = kStep) { return W::TickAccumulator{W::StepLength{step}}; }

// The old FixedTickRuntime (master engine/runtime/include/engine/runtime/FixedTickRuntime.hpp), copied word for
// word except Math::Max -> std::max and without the callback: the reference of the point-by-point comparison.
struct OldAdvance {
    std::uint32_t steps{};
    double droppedSeconds{};
    double secondsUntilNextTick{};
};
class OldFixedTickRuntime {
public:
    OldFixedTickRuntime(double tickRate, std::uint32_t maximumCatchUpSteps)
        : stepSeconds_(1.0 / tickRate), maximumCatchUpSteps_(maximumCatchUpSteps) {}
    OldAdvance Advance(double elapsedSeconds) {
        accumulatedSeconds_ += elapsedSeconds;
        OldAdvance result;
        const double tolerance = stepSeconds_ * 1.0e-9;
        while (accumulatedSeconds_ + tolerance >= stepSeconds_ && result.steps < maximumCatchUpSteps_) {
            accumulatedSeconds_ = std::max(0.0, accumulatedSeconds_ - stepSeconds_);
            ++result.steps;
        }
        if (accumulatedSeconds_ + tolerance >= stepSeconds_) {
            double remainder = std::fmod(accumulatedSeconds_, stepSeconds_);
            if (remainder + tolerance >= stepSeconds_) remainder = 0.0;
            result.droppedSeconds = accumulatedSeconds_ - remainder;
            accumulatedSeconds_ = remainder;
        }
        result.secondsUntilNextTick = stepSeconds_ - accumulatedSeconds_;
        return result;
    }

private:
    double stepSeconds_{};
    std::uint32_t maximumCatchUpSteps_{};
    double accumulatedSeconds_{};
};

// One frame through the new combination: AdvanceVirtual cuts, then the accumulator takes the steps.
struct Frame {
    std::uint32_t steps{};
    Nanoseconds discarded{};
};
Frame RunFrame(W::VirtualTime& time, W::TickAccumulator& accumulator, Nanoseconds real,
               const W::TimeControl& control = {}, const W::TimeLimits& limits = {}) {
    const W::VirtualAdvance advance = W::AdvanceVirtual(time, real, control, &accumulator, limits);
    return Frame{accumulator.Add(advance.delta, limits.maxStepsPerFrame), advance.discarded};
}

} // namespace

TEST_CASE("types: accumulate 2.3 steps") {
    W::TickAccumulator a = Accumulator();
    CHECK(a.Add(23 * kStep / 10, 5) == 2);
    CHECK(a.Overstep() == 3 * kStep / 10);
    CHECK(a.UntilNextStep() == 7 * kStep / 10);
}

TEST_CASE("types: the remainder carries across frames") {
    W::TickAccumulator a = Accumulator();
    std::vector<std::uint32_t> perFrame;
    for (int frame = 0; frame < 10; ++frame) perFrame.push_back(a.Add(4 * kStep / 10, 5));
    CHECK(perFrame == std::vector<std::uint32_t>{0, 0, 1, 0, 1, 0, 0, 1, 0, 1});
}

TEST_CASE("types: no time is lost since the last Reset") {
    W::TickAccumulator a = Accumulator();
    std::mt19937_64 random(7);
    Nanoseconds added = 0;
    std::uint64_t steps = 0;
    for (int frame = 0; frame < 1000; ++frame) {
        const Nanoseconds delta = static_cast<Nanoseconds>(random() % static_cast<std::uint64_t>(3 * kStep));
        added += delta;
        steps += a.Add(delta, 5);
        CHECK(added == static_cast<Nanoseconds>(steps) * kStep + a.Overstep());
        CHECK(a.Overstep() < kStep);
    }
    const Nanoseconds remainder = a.Overstep();
    CHECK(a.Reset() == remainder);
    CHECK(a.Overstep() == 0);
}

TEST_CASE("types: no tick in the accumulator or FixedTime") {
    // Only the step length and the remainder (and, for FixedTime, its elapsed total and pending frame time).
    static_assert(sizeof(W::TickAccumulator) == 2 * sizeof(Nanoseconds));
    static_assert(sizeof(W::FixedTime) == 4 * sizeof(Nanoseconds));
    CHECK(true);
}

TEST_CASE("types: AdvanceVirtual cuts whole steps beyond the cap") {
    W::TickAccumulator a = Accumulator();
    a.Add(6 * kStep / 10, 5);
    W::VirtualTime time;
    time.discarded = 11;
    const Frame frame = RunFrame(time, a, 77 * kStep / 10);  // 0.6 + 7.7 = 8.3 steps
    CHECK(frame.discarded == 3 * kStep);
    CHECK(time.discarded == 11 + 3 * kStep);
    CHECK(frame.steps == 5);
    CHECK(a.Overstep() == 3 * kStep / 10);
}

TEST_CASE("types: steps never exceed the cap after AdvanceVirtual") {
    std::mt19937_64 random(11);
    W::TickAccumulator a = Accumulator();
    W::VirtualTime time;
    for (int frame = 0; frame < 2000; ++frame) {
        const bool stall = random() % 50 == 0;
        const Nanoseconds real = stall ? static_cast<Nanoseconds>(2000 * kMs)
                                       : static_cast<Nanoseconds>(random() % static_cast<std::uint64_t>(4 * kStep));
        CHECK(RunFrame(time, a, real).steps <= 5);
    }
}

TEST_CASE("types: Add asserts beyond its limit") {
    W::TickAccumulator a = Accumulator();
    GYO_CHECK_ASSERTS(a.Add(6 * kStep, 5));
}

TEST_CASE("types: the frame's virtual delta is the steps times the step plus the change of the remainder") {
    std::mt19937_64 random(13);
    W::TickAccumulator a = Accumulator();
    W::VirtualTime time;
    Nanoseconds expectedElapsed = 0;
    for (int frame = 0; frame < 2000; ++frame) {
        const Nanoseconds before = a.Overstep();
        const Nanoseconds real = random() % 40 == 0 ? static_cast<Nanoseconds>(900 * kMs)
                                                    : static_cast<Nanoseconds>(random() % (3 * kStep));
        const Frame f = RunFrame(time, a, real);
        CHECK(time.delta == static_cast<Nanoseconds>(f.steps) * kStep + a.Overstep() - before);
        expectedElapsed += time.delta;
        CHECK(time.elapsed == expectedElapsed);
    }
}

TEST_CASE("types: the old FixedTickRuntime hand cases") {
    // The old cases at 60 Hz (16,666,667 ns here): the step counts carry over; the old double times do not
    // (rounding to whole nanoseconds), so they are checked against the integer step.
    const Nanoseconds step = W::StepLength::FromHz(60.0).nanoseconds;
    W::TickAccumulator a = Accumulator(step);
    W::VirtualTime time;
    // "accumulates elapsed time without owning a clock": half a step, half a step, two steps.
    CHECK(RunFrame(time, a, step / 2).steps == 0);
    CHECK(RunFrame(time, a, step - step / 2).steps == 1);
    CHECK(a.UntilNextStep() == step);
    CHECK(RunFrame(time, a, 2 * step).steps == 2);
    // "bounds catch-up and preserves only fractional backlog": 10.5 steps run 5, discard 5, keep half a step.
    W::TickAccumulator b = Accumulator(step);
    W::VirtualTime bt;
    const Frame bounded = RunFrame(bt, b, 21 * step / 2);
    CHECK(bounded.steps == 5);
    CHECK(bounded.discarded == 5 * step);
    CHECK(b.Overstep() == 21 * step / 2 - 10 * step);
    CHECK(RunFrame(bt, b, step - b.Overstep()).steps == 1);
    // "partitioned time agrees with one advance when no steps are dropped": 0.25 s in one frame (cap 64) and in
    // 250 frames of 1 ms.
    W::TickAccumulator single = Accumulator(step);
    W::VirtualTime st;
    W::TimeLimits wide;
    wide.maxStepsPerFrame = 64;
    const Frame whole = RunFrame(st, single, 250 * kMs, {}, wide);
    W::TickAccumulator parts = Accumulator(step);
    W::VirtualTime pt;
    std::uint32_t steps = 0;
    for (int index = 0; index < 250; ++index) {
        const Frame f = RunFrame(pt, parts, kMs);
        CHECK(f.discarded == 0);
        steps += f.steps;
    }
    // Differs from the old case (15), listed per T1 judgment 1: 60 Hz rounds to 16,666,667 ns, and 15 such steps
    // need 250,000,005 ns, 5 ns more than 0.25 s; the old double step (1/60 s with a 1e-9 tolerance) reached 15.
    CHECK(whole.steps == 14);
    CHECK(steps == whole.steps);
    CHECK(parts.Overstep() == single.Overstep());
    // "enormous elapsed time stays bounded": a frame of about a hundred days runs 5 steps, discards the rest.
    W::TickAccumulator c = Accumulator(step);
    W::VirtualTime ct;
    const Frame huge = RunFrame(ct, c, static_cast<Nanoseconds>(8.6e15));
    CHECK(huge.steps == 5);
    CHECK(huge.discarded > 0);
    CHECK(c.UntilNextStep() > 0);
    CHECK(c.UntilNextStep() <= step);
}

TEST_CASE("types: point by point against the old double version at 64 Hz") {
    // 1/64 s is exact in double, so the old version's steps, remainders and drops are exact too.
    OldFixedTickRuntime old(64.0, 5);
    W::TickAccumulator a = Accumulator(kStep);
    W::VirtualTime time;
    std::vector<Nanoseconds> inputs = {2000 * kMs, kStep, kStep / 4, kStep / 4, kStep / 2, 3 * kStep, kStep / 8};
    std::mt19937_64 random(17);
    for (int index = 0; index < 500; ++index) {
        // Multiples of 1/512 s (kStep / 8 = 1,953,125 ns) keep the old version's double arithmetic exact.
        inputs.push_back(static_cast<Nanoseconds>(random() % 200) * (kStep / 8));
    }
    for (const Nanoseconds input : inputs) {
        const OldAdvance o = old.Advance(static_cast<double>(input) / 1e9);
        const Frame n = RunFrame(time, a, input);
        CHECK(n.steps == o.steps);
        CHECK(static_cast<double>(n.discarded) / 1e9 == o.droppedSeconds);
        CHECK(static_cast<double>(a.UntilNextStep()) / 1e9 == o.secondsUntilNextTick);
    }
}

TEST_CASE("types: pause") {
    W::TickAccumulator a = Accumulator();
    W::VirtualTime time;
    W::TimeControl paused;
    paused.paused = 1;
    for (int frame = 0; frame < 3; ++frame) {
        CHECK(RunFrame(time, a, 20 * kMs, paused).steps == 0);
        CHECK(time.delta == 0);
        CHECK(time.paused == 1);
    }
    CHECK(time.elapsed == 0);
    CHECK(a.Overstep() == 0);
    W::TimeControl resumed;
    resumed.scale = 2.0;
    RunFrame(time, a, 3 * kMs, resumed);
    CHECK(time.delta == 6 * kMs);  // the resumed frame's own real time times the scale, nothing from the pause
    CHECK(time.paused == 0);
}

TEST_CASE("types: a long gap while paused is held by the cap") {
    W::TickAccumulator a = Accumulator();
    W::VirtualTime time;
    const Frame f = RunFrame(time, a, 10'000 * kMs);  // the first frame after a 10 s pause
    CHECK(f.steps == 5);
    CHECK(f.discarded == 10'000 * kMs / kStep * kStep - 5 * kStep);
    CHECK(time.discarded == f.discarded);
}

TEST_CASE("types: scale") {
    for (const double scale : {0.5, 2.0, 0.0}) {
        W::TickAccumulator a = Accumulator();
        W::VirtualTime time;
        W::TimeControl control;
        control.scale = scale;
        RunFrame(time, a, 10 * kMs, control);
        CHECK(time.scale == scale);
        CHECK(time.delta == static_cast<Nanoseconds>(10 * kMs * scale));
    }
    W::TickAccumulator a = Accumulator();
    W::VirtualTime time;
    W::TimeControl fast;
    fast.scale = 2.0;
    const Frame f = RunFrame(time, a, 100 * kMs, fast);  // 200 ms = 12.8 steps: 7 whole steps cut
    CHECK(f.steps == 5);
    CHECK(f.discarded == 7 * kStep);
    CHECK(time.scale == 2.0);
}

TEST_CASE("types: the scale's fraction carries to the next frame") {
    W::VirtualTime time;
    W::TimeControl half;
    half.scale = 0.5;
    W::TimeLimits limits;
    W::AdvanceVirtual(time, 1, half, nullptr, limits);
    CHECK(time.delta == 0);
    W::AdvanceVirtual(time, 1, half, nullptr, limits);
    CHECK(time.delta == 1);
    CHECK(time.elapsed == 1);
    // A pause and a change of scale keep the fraction: half a nanosecond, a paused frame, half again.
    W::VirtualTime kept;
    W::AdvanceVirtual(kept, 1, half, nullptr, limits);
    W::TimeControl pausedHalf = half;
    pausedHalf.paused = 1;
    W::AdvanceVirtual(kept, 7, pausedHalf, nullptr, limits);
    CHECK(kept.fraction == 0.5);
    W::TimeControl quarter;
    quarter.scale = 0.25;
    W::AdvanceVirtual(kept, 2, quarter, nullptr, limits);
    CHECK(kept.delta == 1);
    // Random frames, the scale changing on the way and a pause: the total never drifts a whole nanosecond.
    // The scales are multiples of 1/1024, so real times scale is exact in double and the reference is an exact
    // integer sum in 1/1024 ns (a double or long double sum would round on its own; long double is double on
    // arm64).
    std::mt19937_64 random(19);
    W::VirtualTime t;
    W::TimeLimits wide;
    wide.maxFrameDelta = std::numeric_limits<Nanoseconds>::max();
    std::int64_t exactTimes1024 = 0;
    std::int64_t scaleTimes1024 = 379;
    for (int frame = 0; frame < 5000; ++frame) {
        if (frame % 500 == 0) scaleTimes1024 = 100 + static_cast<std::int64_t>(random() % 3000);
        W::TimeControl control;
        control.scale = static_cast<double>(scaleTimes1024) / 1024.0;
        control.paused = frame % 777 < 20 ? 1 : 0;
        const Nanoseconds real = static_cast<Nanoseconds>(random() % (40 * kMs));
        W::AdvanceVirtual(t, real, control, nullptr, wide);
        if (control.paused == 0) exactTimes1024 += real * scaleTimes1024;
        const std::int64_t gap = exactTimes1024 - t.elapsed * 1024;
        CHECK(gap >= 0);
        CHECK(gap < 1024);
    }
}

TEST_CASE("types: without a fixed step the frame is capped at 250 ms") {
    W::VirtualTime time;
    const W::VirtualAdvance capped = W::AdvanceVirtual(time, 400 * kMs, {}, nullptr, {});
    CHECK(capped.delta == 250 * kMs);
    CHECK(capped.discarded == 150 * kMs);
    CHECK(capped.cause == W::DiscardCause::FrameLimit);
    CHECK(time.discarded == 150 * kMs);
    W::TimeLimits tight;
    tight.maxFrameDelta = 100 * kMs;
    CHECK(W::AdvanceVirtual(time, 400 * kMs, {}, nullptr, tight).delta == 100 * kMs);
    // With a fixed step the duration cap does not apply.
    W::TickAccumulator a = Accumulator();
    W::TimeLimits manySteps;
    manySteps.maxStepsPerFrame = 100;
    W::VirtualTime fixedTime;
    const W::VirtualAdvance uncut = W::AdvanceVirtual(fixedTime, 400 * kMs, {}, &a, manySteps);
    CHECK(uncut.delta == 400 * kMs);
    CHECK(uncut.discarded == 0);
}

TEST_CASE("types: the step length") {
    CHECK(W::StepLength{}.nanoseconds == 15'625'000);
    CHECK(W::StepLength::FromHz(64.0).nanoseconds == 15'625'000);
    CHECK(W::StepLength::FromHz(60.0).nanoseconds == 16'666'667);
    CHECK(W::StepLength::FromNanoseconds(12'345).nanoseconds == 12'345);
    GYO_CHECK_ASSERTS(W::StepLength::FromHz(0.0));
    GYO_CHECK_ASSERTS(W::StepLength::FromHz(std::numeric_limits<double>::infinity()));
    GYO_CHECK_ASSERTS(W::StepLength::FromHz(std::numeric_limits<double>::quiet_NaN()));
    GYO_CHECK_ASSERTS(W::StepLength::FromNanoseconds(0));
}

TEST_CASE("types: RealTime") {
    W::RealTime real;
    W::AdvanceRealTo(real, 5'000);
    CHECK(real.delta == 0);
    CHECK(real.elapsed == 0);
    W::AdvanceRealTo(real, 7'500);
    CHECK(real.delta == 2'500);
    CHECK(real.elapsed == 2'500);
    GYO_CHECK_ASSERTS(W::AdvanceRealTo(real, 7'000));
    W::RealTime manual;
    W::AdvanceRealBy(manual, kStep);
    CHECK(manual.delta == kStep);  // manual sampling: the first frame counts
    CHECK(manual.elapsed == kStep);
}

TEST_CASE("types: preconditions") {
    W::TickAccumulator a = Accumulator();
    GYO_CHECK_ASSERTS(a.Add(-1, 5));
    GYO_CHECK_ASSERTS(a.Add(1, 0));
    a.Add(kStep - 1, 5);
    GYO_CHECK_ASSERTS(a.Add(std::numeric_limits<Nanoseconds>::max(), 5));
    W::VirtualTime time;
    W::TimeControl negative;
    negative.scale = -1.0;
    GYO_CHECK_ASSERTS(W::AdvanceVirtual(time, 1, negative, nullptr, {}));
    W::TimeControl notFinite;
    notFinite.scale = std::numeric_limits<double>::quiet_NaN();
    GYO_CHECK_ASSERTS(W::AdvanceVirtual(time, 1, notFinite, nullptr, {}));
    W::TimeControl huge;
    huge.scale = 1.0e10;
    GYO_CHECK_ASSERTS(W::AdvanceVirtual(time, 1'000'000'000, huge, nullptr, {}));
    GYO_CHECK_ASSERTS(W::AdvanceVirtual(time, -1, {}, nullptr, {}));
    W::TimeLimits zero;
    zero.maxStepsPerFrame = 0;
    GYO_CHECK_ASSERTS(W::AdvanceVirtual(time, 1, {}, &a, zero));
    GYO_CHECK_ASSERTS(a.RestoreOverstep(kStep));
}
