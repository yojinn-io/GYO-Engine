#include "plugins/sdl/ClockPair.hpp"

#include <chrono>

namespace GYO::Plugins::Sdl {

ClockPair MakeClockPair(const Engine::Time::TimePoint engineBefore, const std::uint64_t sdlNs,
                        const Engine::Time::TimePoint engineAfter) noexcept {
    return {engineBefore + (engineAfter - engineBefore) / 2, sdlNs, engineAfter - engineBefore};
}

ClockPair SampleClockPair(const ClockReaders& readers) noexcept {
    // Separate statements: the three reads happen in this order.
    const Engine::Time::TimePoint before = readers.engine();
    const std::uint64_t sdl = readers.sdl();
    const Engine::Time::TimePoint after = readers.engine();
    return MakeClockPair(before, sdl, after);
}

Engine::Time::TimePoint ConvertSdlTime(const std::uint64_t sdlNs, const ClockPair& pair,
                                       const Engine::Time::TimePoint ceiling) noexcept {
    if (sdlNs == 0) return ceiling;
    // The unsigned difference converted modulo 2^64 (C++20): an event before the pair maps before it, without UB.
    const std::chrono::nanoseconds since{static_cast<std::int64_t>(sdlNs - pair.sdlNs)};
    const Engine::Time::TimePoint converted = pair.engine + std::chrono::duration_cast<Engine::Time::Duration>(since);
    return converted < ceiling ? converted : ceiling;
}

} // namespace GYO::Plugins::Sdl
