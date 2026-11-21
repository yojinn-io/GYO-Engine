#include "engine/world_time/TimeTypes.hpp"

#include <cmath>
#include <limits>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::WorldTime {

namespace {

constexpr Nanoseconds kNanosecondsPerSecond = 1'000'000'000;
// The largest scaled delta a frame may come to: well inside the exact range of double (2^53) and of Nanoseconds.
constexpr double kLargestScaled = 9.0e15;

} // namespace

StepLength StepLength::FromHz(double hz) {
    // A finite positive rate whose step is at least one nanosecond.
    GYO_ASSERT(std::isfinite(hz) && hz > 0.0 && hz <= static_cast<double>(kNanosecondsPerSecond));
    return StepLength{static_cast<Nanoseconds>(std::llround(static_cast<double>(kNanosecondsPerSecond) / hz))};
}

StepLength StepLength::FromNanoseconds(Nanoseconds nanoseconds) {
    GYO_ASSERT(nanoseconds > 0);
    return StepLength{nanoseconds};
}

TickAccumulator::TickAccumulator(StepLength step) : step_(step.nanoseconds) {
    GYO_ASSERT(step_ > 0);
}

std::uint32_t TickAccumulator::Add(Nanoseconds delta, std::uint32_t limit) {
    // A non-negative delta that does not overflow the remainder, and a non-zero limit.
    GYO_ASSERT(delta >= 0 && limit > 0 && delta <= std::numeric_limits<Nanoseconds>::max() - overstep_);
    const Nanoseconds total = overstep_ + delta;
    const Nanoseconds steps = total / step_;
    // More steps than the limit means AdvanceVirtual did not cut the frame first.
    GYO_ASSERT(steps <= static_cast<Nanoseconds>(limit));
    overstep_ = total - steps * step_;
    return static_cast<std::uint32_t>(steps);
}

Nanoseconds TickAccumulator::Reset() noexcept {
    const Nanoseconds cleared = overstep_;
    overstep_ = 0;
    return cleared;
}

void TickAccumulator::RestoreOverstep(Nanoseconds overstep) {
    GYO_ASSERT(overstep >= 0 && overstep < step_);
    overstep_ = overstep;
}

VirtualAdvance AdvanceVirtual(VirtualTime& time, Nanoseconds realDelta, const TimeControl& control,
                              const TickAccumulator* fixed, const TimeLimits& limits) {
    // A non-negative delta, a finite non-negative scale, non-zero caps.
    GYO_ASSERT(realDelta >= 0 && std::isfinite(control.scale) && control.scale >= 0.0 &&
               limits.maxStepsPerFrame > 0 && limits.maxFrameDelta > 0);
    VirtualAdvance result;
    // -0.0 is written as +0.0: the World summary hashes the bytes (declaration 2.2).
    time.scale = control.scale == 0.0 ? 0.0 : control.scale;
    time.paused = control.paused;
    if (control.paused != 0) {
        time.delta = 0;
        return result;
    }
    const double scaled = static_cast<double>(realDelta) * control.scale + time.fraction;
    // A frame whose scaled time stays exactly representable.
    GYO_ASSERT(std::isfinite(scaled) && scaled < kLargestScaled);
    const double whole = std::floor(scaled);
    time.fraction = scaled - whole;
    Nanoseconds delta = static_cast<Nanoseconds>(whole);
    if (fixed != nullptr) {
        const Nanoseconds step = fixed->Step();
        const Nanoseconds steps = (fixed->Overstep() + delta) / step;
        const Nanoseconds limit = static_cast<Nanoseconds>(limits.maxStepsPerFrame);
        if (steps > limit) {
            result.discarded = (steps - limit) * step;
            result.cause = DiscardCause::StepLimit;
        }
    } else if (delta > limits.maxFrameDelta) {
        result.discarded = delta - limits.maxFrameDelta;
        result.cause = DiscardCause::FrameLimit;
    }
    delta -= result.discarded;
    result.delta = delta;
    time.delta = delta;
    time.elapsed += delta;
    time.discarded += result.discarded;
    return result;
}

void AdvanceRealTo(RealTime& time, Nanoseconds now) {
    if (time.sampled == 0) {
        time.sampled = 1;
        time.delta = 0;
    } else {
        // The time base never runs backwards.
        GYO_ASSERT(now >= time.now);
        time.delta = now - time.now;
    }
    time.now = now;
    time.elapsed += time.delta;
}

void AdvanceRealBy(RealTime& time, Nanoseconds delta) {
    GYO_ASSERT(delta >= 0);
    time.sampled = 1;
    time.delta = delta;
    time.now += delta;
    time.elapsed += delta;
}

} // namespace GYO::Engine::WorldTime
