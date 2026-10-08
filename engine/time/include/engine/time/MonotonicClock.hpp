#pragma once

#include <chrono>
#include <cstdint>

namespace Engine::Time {

// The engine time base: every engine module measures elapsed time, waits and
// stamps events on this clock. It is monotonic and never adjusted.
//
// Sources: macOS libc++ reads CLOCK_MONOTONIC_RAW, which also counts system
// sleep; Windows reads QueryPerformanceCounter; Linux libstdc++ reads
// CLOCK_MONOTONIC. SDL_GetTicksNS is a different domain (macOS:
// mach_absolute_time, which stops during sleep; Linux: CLOCK_MONOTONIC_RAW), so
// SDL timestamps are converted at the platform boundary by sampling both clocks
// together, never by a fixed offset. File times (std::filesystem) are wall-clock
// values and stay off this base.
using MonotonicClock = std::chrono::steady_clock;
using TimePoint = MonotonicClock::time_point;
using Duration = MonotonicClock::duration;

[[nodiscard]] inline TimePoint Now() noexcept { return MonotonicClock::now(); }

// Nanoseconds since the clock's (unspecified) epoch; comparable only within one process.
[[nodiscard]] inline std::int64_t NowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Now().time_since_epoch()).count();
}

} // namespace Engine::Time
