#include "engine/world_time/Sampling.hpp"

#include "engine/base/Assert.hpp"

namespace GYO::Engine::WorldTime {

void SampleRealTime(RealTime& real, const TimeSampling& sampling, Nanoseconds (*readTimeBase)() noexcept) {
    switch (sampling.mode) {
    case SamplingMode::TimeBase:
        GYO_ASSERT(readTimeBase != nullptr);
        AdvanceRealTo(real, readTimeBase());
        return;
    case SamplingMode::ManualDelta:
        AdvanceRealBy(real, sampling.value);
        return;
    case SamplingMode::ManualInstant:
        AdvanceRealTo(real, sampling.value);
        return;
    }
    GYO_ASSERT(false && "an unknown SamplingMode");
}

} // namespace GYO::Engine::WorldTime
