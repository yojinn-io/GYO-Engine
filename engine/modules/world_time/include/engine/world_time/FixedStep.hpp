#pragma once

// Running, rewinding and putting back the fixed step (time-events declaration 3.3, 3.5; WT-3). RunFixedSteps
// calls RunOneFixedStep k times a frame; a rollback strategy calls RewindFixedSteps and then RunOneFixedStep k times
// (rollback declaration 7.2), so Tick, FixedTime::elapsed and ActiveTime change in these functions only. Each takes
// any ExclusiveWorld<...> (ExclusiveWorld<Writes<...>> is a distinct final type). No time base is read here.

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/World.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace GYO::Engine::WorldTime {

namespace Detail {

// The value of a Read or Modify of a time Resource the time Module registered and put in (absent: a Programmer
// Error, the product removed it).
template <class R>
R Present(Base::Result<std::optional<R>, Kernel::KernelError> visited) {
    GYO_ASSERT(static_cast<bool>(visited) && visited->has_value());
    return **visited;
}
// The same for a closure that returns nothing (the result says whether it was called).
inline void Called(Base::Result<bool, Kernel::KernelError> visited) {
    GYO_ASSERT(static_cast<bool>(visited) && *visited);
}

} // namespace Detail

// One fixed step: Tick + 1, FixedTime::elapsed + one step, ActiveTime = (step, FixedTime::elapsed), then the
// fixed-step schedule runs nested. Ok: ActiveTime is the frame's value again (VirtualTime's delta and elapsed).
// Err or an exception: passed on as it is, ActiveTime not put back (the World is interrupted; D2 ends the Product).
template <class ExclusiveWorldType>
[[nodiscard]] Base::Result<void, Kernel::KernelError> RunOneFixedStep(ExclusiveWorldType& world,
                                                                               std::string_view schedule) {
    Detail::Called(world.template Modify<Tick>([](Tick& tick) { ++tick.value; }));
    const ActiveTime fixed = Detail::Present(world.template Modify<FixedTime>([](FixedTime& time) {
        time.elapsed += time.accumulator.Step();
        return ActiveTime{time.accumulator.Step(), time.elapsed};
    }));
    Detail::Called(world.template Modify<ActiveTime>([fixed](ActiveTime& active) { active = fixed; }));
    if (auto ran = world.RunSchedule(schedule); !ran) return ran;
    const ActiveTime frame = Detail::Present(
        world.template Read<VirtualTime>([](const VirtualTime& time) { return ActiveTime{time.delta, time.elapsed}; }));
    Detail::Called(world.template Modify<ActiveTime>([frame](ActiveTime& active) { active = frame; }));
    return {};
}

// Takes k fixed steps back: Tick - k and FixedTime::elapsed - k steps; the accumulator does not move. Then
// RunOneFixedStep k times runs them again with the same Tick and fixed time as the first time (the step length
// never changes while the World runs). k may not pass Tick (GYO_ASSERT).
template <class ExclusiveWorldType>
void RewindFixedSteps(ExclusiveWorldType& world, std::uint64_t k) {
    Detail::Called(world.template Modify<Tick>([k](Tick& tick) {
        GYO_ASSERT(k <= tick.value);
        tick.value -= k;
    }));
    Detail::Called(world.template Modify<FixedTime>([k](FixedTime& time) {
        time.elapsed -= static_cast<Nanoseconds>(k) * time.accumulator.Step();
    }));
}

namespace Detail {

// A saved fixed-step state under this World's step length (declaration 3.5, world-time section 4): the steps run so
// far, and the saved remainder modulo the step (a remainder saved under a longer step can pass this World's step).
// Returns the whole steps beyond it, which the caller adds to VirtualTime::discarded (no TimeDiscarded: this runs
// in no System). A negative remainder is a Programmer Error.
inline Nanoseconds PutBackFixedTime(FixedTime& time, StepLength step, Nanoseconds elapsed, Nanoseconds overstep) {
    GYO_ASSERT(overstep >= 0);
    time.accumulator = TickAccumulator{step};
    time.elapsed = elapsed;
    const Nanoseconds kept = overstep % step.nanoseconds;
    time.accumulator.RestoreOverstep(kept);
    return overstep - kept;
}

} // namespace Detail

// Puts a saved fixed-step state back into a new World (a restore, declaration 3.5): the steps run so far and the
// accumulator's remainder. The step length is this World's setting, not the saved one's: a remainder of a whole
// step or more (saved under a longer step) keeps what is left below one step, and the whole steps beyond it go to
// VirtualTime::discarded. From a prepare function (the Host, World Ready); not between AdvanceTime and
// RunFixedSteps of one frame (the steps AdvanceTime allowed were counted with the old remainder: WT-3).
inline void RestoreFixedTime(Kernel::World& world, Nanoseconds elapsed, Nanoseconds overstep) {
    FixedTime* time = world.ResourceMut<FixedTime>();
    GYO_ASSERT(time != nullptr);
    const Nanoseconds excess = Detail::PutBackFixedTime(*time, StepLength{time->accumulator.Step()}, elapsed, overstep);
    if (excess > 0) {
        VirtualTime* virt = world.ResourceMut<VirtualTime>();
        GYO_ASSERT(virt != nullptr);
        virt->discarded += excess;
    }
}
template <class ExclusiveWorldType>
void RestoreFixedTime(ExclusiveWorldType& world, Nanoseconds elapsed, Nanoseconds overstep) {
    const Nanoseconds excess = Detail::Present(world.template Modify<FixedTime>([elapsed, overstep](FixedTime& time) {
        return Detail::PutBackFixedTime(time, StepLength{time.accumulator.Step()}, elapsed, overstep);
    }));
    if (excess > 0) Detail::Called(world.template Modify<VirtualTime>([excess](VirtualTime& time) { time.discarded += excess; }));
}

} // namespace GYO::Engine::WorldTime
