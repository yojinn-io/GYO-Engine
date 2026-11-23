#pragma once

// Observation for the OS tests (v8-arch-gyo2-os-test-timeout): a doctest
// listener in CaseTiming.cpp writes one line when each test case and subcase
// starts and ends, with the wall time, the process's CPU time, its context
// switches and the machine's load, to a log file. A run that CTest ends by its
// timeout leaves the case it was in as the file's last line; the file outlives
// the next ctest run. It never changes a check: it only reads clocks and
// counters and writes text.
//
// Standard error gets only the file's path (and "kept <path>" at the end), not
// the lines: the mutation runner matches a mutant's expect against the output,
// and some expects are a case name that doctest prints only when the case
// fails, so the names must not appear for a case that passed.
//
// Reading the fields: on macOS a thread that blocks shows up in ivcsw, vcsw
// stays near 0; load is the one-minute average and lags behind.
//
// This is a diagnostic for the local timeouts of these two executables. The
// progress notes inside "loses no signal" are removed once the user's
// decision on that case is in place; the listener goes when no local timeout
// of these tests is open in the batch table.
//
// The file goes to $GYO_OS_CASE_TIMING_DIR, or else to the build directory
// given at compile time. A run that passed within KeepAfter is removed at its
// end; a failed, slow or killed run keeps its file.

namespace GYO::Test::CaseTiming {

// Writes one line to the file, prefixed and timed like the listener's, for a
// case that reports its own progress. printf-style.
void Note(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

} // namespace GYO::Test::CaseTiming
