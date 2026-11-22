#pragma once

// The time Module's sampling (time-events declaration 3.4): the one header of engine/world_time that reads the time
// base. The type headers (TimeTypes.hpp, FixedStep.hpp) do not include it, so a source that may not read the clock
// (authority_scan ZS1: a System that writes decision data) can still name Tick and the time Resources.

#include "engine/time/MonotonicClock.hpp"
#include "engine/world_time/TimeTypes.hpp"

namespace GYO::Engine::WorldTime {

// The time base now, in nanoseconds (::Engine::Time::MonotonicClock).
[[nodiscard]] inline Nanoseconds ReadTimeBase() noexcept { return ::Engine::Time::NowNs(); }

// Takes the frame's sample as sampling says and advances real time: the time base (now given by readTimeBase),
// a manual duration every frame, or a manual next instant. The first sample of a World gives delta 0 in the
// time-base and next-instant modes; a manual duration applies from the first frame on.
void SampleRealTime(RealTime& real, const TimeSampling& sampling, Nanoseconds (*readTimeBase)() noexcept = &ReadTimeBase);

} // namespace GYO::Engine::WorldTime
