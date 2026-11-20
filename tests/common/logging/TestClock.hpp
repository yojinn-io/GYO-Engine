#pragma once

#include <atomic>
#include <cstdint>

#include "engine/logging/Logger.hpp"

// An injected clock for the logging tests (declaration 9.1: tests never read
// the real clocks). Counts its reads, so a test can show a refused record read
// no clock (Design Invariant LG-7).

namespace GYO::Engine::Logging::Test {

inline std::atomic<std::int64_t> monoNow{1'000};
inline std::atomic<std::int64_t> wallMsNow{1'763'600'411'007}; // 2025-11-20T01:00:11.007Z
inline std::atomic<std::int32_t> utcOffsetNow{9 * 3600};
inline std::atomic<std::uint64_t> clockReads{0};

inline std::int64_t TestMonoNs() noexcept {
    clockReads.fetch_add(1, std::memory_order_relaxed);
    return monoNow.load(std::memory_order_relaxed);
}

inline WallTime TestWall() noexcept {
    clockReads.fetch_add(1, std::memory_order_relaxed);
    return WallTime{.unixMs = wallMsNow.load(std::memory_order_relaxed),
                    .utcOffsetSeconds = utcOffsetNow.load(std::memory_order_relaxed)};
}

inline LoggerClock TestClock() noexcept {
    return LoggerClock{.monoNs = &TestMonoNs, .wall = &TestWall};
}

inline void ResetClock() noexcept {
    monoNow = 1'000;
    wallMsNow = 1'763'600'411'007;
    utcOffsetNow = 9 * 3600;
    clockReads = 0;
}

} // namespace GYO::Engine::Logging::Test
