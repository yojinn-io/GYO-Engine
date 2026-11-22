#pragma once

// Game time in a World (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, sections 2.2, 3.3;
// Design Invariants in docs/architecture/target/v2/world-time.zh-Hant.md). Plain data and pure arithmetic: no
// time base (this header does not include MonotonicClock.hpp: authority_scan ZS1). All durations are whole
// nanoseconds; the scale is the only floating-point value, multiplied once per frame.
//
// Each type the Module puts into a World has its hash function here, next to the type, field by field (the World
// summary hashes each value with it; authority-rollback declaration 3.3).

#include "engine/kernel/Hash.hpp"

#include <cstdint>

namespace GYO::Engine::WorldTime {

using Nanoseconds = std::int64_t;

// The fixed step length, stored as whole nanoseconds. Given as a rate (rounded to the nearest nanosecond once,
// v8-arch-gyo2-t1-closed-by-time-events) or as a duration. The default is 64 Hz (15,625,000 ns, exact).
struct StepLength final {
    Nanoseconds nanoseconds = 15'625'000;

    // A finite positive rate; 60.0 gives 16,666,667.
    [[nodiscard]] static StepLength FromHz(double hz);
    // A positive duration.
    [[nodiscard]] static StepLength FromNanoseconds(Nanoseconds nanoseconds);
};

// The two caps on one frame (time-plan-two (4), v8-arch-gyo2-t1-three-items): with a fixed step at most
// maxStepsPerFrame whole steps run and the excess whole steps are discarded; without one a frame advances at most
// maxFrameDelta. The two never apply together.
struct TimeLimits final {
    std::uint32_t maxStepsPerFrame = 5;
    Nanoseconds maxFrameDelta = 250'000'000;
};

// The accumulator (the old FixedTickRuntime, renamed and made integer): adds time, hands out whole steps, keeps
// the remainder. It reads no time base, counts no tick and discards no time.
class TickAccumulator final {
public:
    TickAccumulator() = default;
    explicit TickAccumulator(StepLength step);

    // Adds delta (>= 0) and takes every whole step; afterwards the remainder is below one step. Asserts the count
    // stays within limit (AdvanceVirtual has already cut the excess; the assertion guards a caller that did not).
    std::uint32_t Add(Nanoseconds delta, std::uint32_t limit);
    // The time since the last whole step (for interpolation).
    [[nodiscard]] Nanoseconds Overstep() const noexcept { return overstep_; }
    // The time until the next whole step.
    [[nodiscard]] Nanoseconds UntilNextStep() const noexcept { return step_ - overstep_; }
    [[nodiscard]] Nanoseconds Step() const noexcept { return step_; }
    // Clears the remainder and returns what was cleared.
    Nanoseconds Reset() noexcept;
    // Puts back a remainder below one step (a restore, section 3.5).
    void RestoreOverstep(Nanoseconds overstep);

private:
    Nanoseconds step_ = StepLength{}.nanoseconds;
    Nanoseconds overstep_ = 0;
};

// Real time: the sample of this frame, its distance from the last one and the total. Not affected by pause,
// scale or the caps.
struct RealTime final {
    Nanoseconds now = 0;
    Nanoseconds delta = 0;
    Nanoseconds elapsed = 0;
    std::uint8_t sampled = 0;  // 1 once a first sample was taken
};

// Pausable, scalable time. scale and paused are what this frame applied; discarded is the running total cut by
// the caps or not taken by the fixed step; fraction is the part of a nanosecond the scale left over, carried to
// the next frame so the total never drifts a whole nanosecond from the sum of real time times scale.
struct VirtualTime final {
    Nanoseconds delta = 0;
    Nanoseconds elapsed = 0;
    Nanoseconds discarded = 0;
    double scale = 1.0;
    double fraction = 0.0;
    std::uint8_t paused = 0;
};

// The fixed step: its accumulator, the total of the steps run so far, and the frame's virtual time handed from
// AdvanceTime to RunFixedSteps (pending; taken and cleared by RunFixedSteps).
struct FixedTime final {
    TickAccumulator accumulator;
    Nanoseconds elapsed = 0;
    Nanoseconds pending = 0;
};

// The time a System reads: the frame's virtual time, or inside a fixed step (step length, FixedTime::elapsed).
struct ActiveTime final {
    Nanoseconds delta = 0;
    Nanoseconds elapsed = 0;
};

// The pause and scale the product wants; AdvanceTime applies them on its next run.
struct TimeControl final {
    double scale = 1.0;
    std::uint8_t paused = 0;
};

// The fixed steps started so far: during the n-th step and after it, n.
struct Tick final {
    std::uint64_t value = 0;
};

// "Resimulating": registered by the time Module in every World it is built into, always present, false unless a
// rollback strategy writes it (rollback declaration 5.7, 7.2).
struct Resimulation final {
    std::uint8_t active = 0;
};

// Why time was discarded.
enum class DiscardCause : std::uint8_t {
    None,
    StepLimit,     // more whole fixed steps than TimeLimits::maxStepsPerFrame
    FrameLimit,    // more than TimeLimits::maxFrameDelta without a fixed step
    NotDriven,     // the fixed step did not take the frame's time (RunFixedSteps did not run)
};

// How AdvanceTime takes the frame's sample (Bevy TimeUpdateStrategy; time-events declaration 3.4). Until the
// runner's exchange point exists, each World samples on its own.
enum class SamplingMode : std::uint8_t {
    TimeBase,       // reads Time::MonotonicClock (the default; Sampling.hpp)
    ManualDelta,    // every frame advances by value, the first included, until changed (Bevy ManualDuration)
    ManualInstant,  // value is the frame's sample, the next point in time (the first sample gives delta 0)
};

// The time Module's sampling, written by the product or a test.
struct TimeSampling final {
    Nanoseconds value = 0;
    SamplingMode mode = SamplingMode::TimeBase;
};

// A retained message (time-events declaration section 5): time was discarded this frame. amount is what the
// frame discarded for cause; realDelta is the frame's real time. VirtualTime::discarded keeps the running total.
struct TimeDiscarded final {
    Nanoseconds amount = 0;
    Nanoseconds realDelta = 0;
    DiscardCause cause = DiscardCause::None;
};

// What the time Module's Systems read at run time (they are captureless): the caps of a frame, the name of the
// fixed-step schedule RunFixedSteps runs (NUL-terminated; plain bytes, so the World summary stays deterministic)
// and whether the World has a fixed step (0: AdvanceTime caps a frame at TimeLimits::maxFrameDelta and hands the
// fixed step nothing). Written once when the World is built (WorldTimeModule); not meant to be changed while it runs.
struct TimeConfig final {
    TimeLimits limits;
    char fixedSchedule[64] = {};
    std::uint8_t fixedStep = 1;
};

// What one frame of virtual time came to.
struct VirtualAdvance final {
    Nanoseconds delta = 0;
    Nanoseconds discarded = 0;
    DiscardCause cause = DiscardCause::None;
};

// Advances virtual time by one frame of real time (>= 0) under control and the caps, and returns the frame's
// delta and the amount discarded. With fixed (the World has a fixed step) only whole steps beyond
// maxStepsPerFrame are cut, so the remainder is the same as without the cut; without it the frame is capped at
// maxFrameDelta. Paused: the delta is 0 and the fraction is kept. The one place excess time is cut (DI 2).
VirtualAdvance AdvanceVirtual(VirtualTime& time, Nanoseconds realDelta, const TimeControl& control,
                              const TickAccumulator* fixed, const TimeLimits& limits);

// Real time from a sample of the time base: the first sample gives delta 0.
void AdvanceRealTo(RealTime& time, Nanoseconds now);
// Real time by a given duration (manual sampling: the delta applies on every frame, the first included).
void AdvanceRealBy(RealTime& time, Nanoseconds delta);

// The hash functions (Hash.hpp), field by field.
inline void HashValue(Kernel::Hasher& h, const StepLength& v) { HashValue(h, v.nanoseconds); }
inline void HashValue(Kernel::Hasher& h, const TimeLimits& v) {
    HashValue(h, v.maxStepsPerFrame);
    HashValue(h, v.maxFrameDelta);
}
inline void HashValue(Kernel::Hasher& h, const TickAccumulator& v) {
    HashValue(h, v.Step());
    HashValue(h, v.Overstep());
}
inline void HashValue(Kernel::Hasher& h, const RealTime& v) {
    HashValue(h, v.now);
    HashValue(h, v.delta);
    HashValue(h, v.elapsed);
    HashValue(h, v.sampled);
}
inline void HashValue(Kernel::Hasher& h, const VirtualTime& v) {
    HashValue(h, v.delta);
    HashValue(h, v.elapsed);
    HashValue(h, v.discarded);
    HashValue(h, v.scale);
    HashValue(h, v.fraction);
    HashValue(h, v.paused);
}
inline void HashValue(Kernel::Hasher& h, const FixedTime& v) {
    HashValue(h, v.accumulator);
    HashValue(h, v.elapsed);
    HashValue(h, v.pending);
}
inline void HashValue(Kernel::Hasher& h, const ActiveTime& v) {
    HashValue(h, v.delta);
    HashValue(h, v.elapsed);
}
inline void HashValue(Kernel::Hasher& h, const TimeControl& v) {
    HashValue(h, v.scale);
    HashValue(h, v.paused);
}
inline void HashValue(Kernel::Hasher& h, const Tick& v) { HashValue(h, v.value); }
inline void HashValue(Kernel::Hasher& h, const Resimulation& v) { HashValue(h, v.active); }
inline void HashValue(Kernel::Hasher& h, const TimeSampling& v) {
    HashValue(h, v.value);
    HashValue(h, v.mode);
}
inline void HashValue(Kernel::Hasher& h, const TimeDiscarded& v) {
    HashValue(h, v.amount);
    HashValue(h, v.realDelta);
    HashValue(h, v.cause);
}
// The schedule name up to its NUL (the bytes after it are not part of the value).
inline void HashValue(Kernel::Hasher& h, const TimeConfig& v) {
    HashValue(h, v.limits);
    std::uint64_t length = 0;
    while (length < sizeof(v.fixedSchedule) && v.fixedSchedule[length] != '\0') ++length;
    h.U64(length);
    for (std::uint64_t i = 0; i < length; ++i) HashValue(h, v.fixedSchedule[i]);
    HashValue(h, v.fixedStep);
}

} // namespace GYO::Engine::WorldTime
