// Observation for the concurrency tests (v7.1 platform_sdl_concurrency timeout on
// linux-x64, run 40510515336: which case hung, and where, was not visible).
//
// Every test case writes a start and an end line to stderr. A case still running
// after 10 s gets a report every 10 s: its last mark, how long ago that was, and
// the counters its own threads advance; then a separate line with the SDL event
// queue's size (missing if reading the queue blocks). The watchdog only writes:
// it fails nothing, stops nothing, and never logs through SDL or the Engine (the
// log bridge tests count every line logged there).
#pragma once

#include <atomic>

namespace Engine::Test::Watchdog {

// Where the test's main thread is: one atomic store each, no output.
// `phase` must be a string literal.
void Mark(const char* phase, int round = -1) noexcept;

// Names the two counters below in reports; string literals, for the current case.
void NameCounters(const char* first, const char* second) noexcept;

// Advanced by a test's own threads (relaxed); reset when a case starts.
extern std::atomic<long long> firstCount;
extern std::atomic<long long> secondCount;

} // namespace Engine::Test::Watchdog
