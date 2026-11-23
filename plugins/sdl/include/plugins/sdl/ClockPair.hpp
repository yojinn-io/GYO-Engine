#pragma once

// Event times on the engine time base (SDL declaration section 8; Design Invariant SDL-3 in
// docs/architecture/target/v2/sdl.zh-Hant.md). SDL stamps its events with SDL_GetTicksNS, a clock of its own (macOS:
// mach_absolute_time, which stops during sleep; Linux: CLOCK_MONOTONIC_RAW), so an adapter converts at its edge:
// once per pump it reads the engine clock, SDL's clock and the engine clock again, and places each event's SDL time
// relative to that pair. The functions take the readers as arguments and hold no SDL: an adapter passes
// SDL_GetTicksNS, a test passes scripted readers (master's platform-sdl timestamp contract, D34).
//
// Promised: (1) never later than the delivery time given as the ceiling; (2) within one pump, events whose SDL times
// are non-zero and do not decrease keep that order after conversion; (3) SDL time 0 (an event added without a time)
// is the delivery time. Not promised: a total order, or the order between two pumps (off by about bracket / 2).

#include <cstdint>

#include "engine/time/MonotonicClock.hpp"

namespace GYO::Plugins::SDL {

struct ClockPair final {
    // The midpoint of the two engine reads.
    Engine::Time::TimePoint engine{};
    // The SDL read between them, in nanoseconds.
    std::uint64_t sdlNs = 0;
    // The span between the two engine reads (the pair's uncertainty).
    Engine::Time::Duration bracket{};
};

struct ClockReaders final {
    Engine::Time::TimePoint (*engine)() noexcept = nullptr;
    std::uint64_t (*sdl)() noexcept = nullptr;
};

[[nodiscard]] ClockPair MakeClockPair(Engine::Time::TimePoint engineBefore, std::uint64_t sdlNs,
                                      Engine::Time::TimePoint engineAfter) noexcept;

// Reads the engine clock, then SDL's, then the engine clock again, in that order.
[[nodiscard]] ClockPair SampleClockPair(const ClockReaders& readers) noexcept;

// sdlNs placed relative to pair, no later than ceiling (the delivery time); 0 is the ceiling.
[[nodiscard]] Engine::Time::TimePoint ConvertSDLTime(std::uint64_t sdlNs, const ClockPair& pair,
                                                     Engine::Time::TimePoint ceiling) noexcept;

} // namespace GYO::Plugins::SDL
