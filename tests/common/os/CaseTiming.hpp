#pragma once

// Observation for the OS tests (v8-arch-gyo2-os-test-timeout): a doctest
// listener in CaseTiming.cpp writes one line when each test case and subcase
// starts and ends, with the wall time, the process's CPU time, its context
// switches and the machine's load, to standard error and to a log file. A run
// that CTest ends by its timeout shows the case it was in; the file outlives
// the next ctest run. It never changes a check: it only reads clocks and
// counters and writes text.
//
// The file goes to $GYO_OS_CASE_TIMING_DIR, or else to the build directory
// given at compile time. A run that passed within KeepAfter is removed at its
// end; a failed, slow or killed run keeps its file.

namespace GYO::Test::CaseTiming {

// Writes one line, prefixed and timed like the listener's, for a case that
// reports its own progress. printf-style.
void Note(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

} // namespace GYO::Test::CaseTiming
