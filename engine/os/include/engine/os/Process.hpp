#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <chrono>
#include <cstdint>
#include <string>

// Facts of the running process (docs/architecture/os.md). Every query asks the
// OS again; nothing is cached. How the answers are printed is the caller's.

namespace Engine::Os::Process {

// The running executable image as an absolute UTF-8 path: the module path on
// Windows, /proc/self/exe on Linux, the image path with links resolved on
// macOS. Not the same fact as an application's resource directory (a macOS
// app bundle keeps its resources apart from the executable). Unsupported on
// other platforms; InvalidEncoding when the path has no UTF-8 form.
[[nodiscard]] Base::Result<std::string, OsError> ExecutablePath();

// User plus kernel CPU time used so far by the whole process, and by the
// calling thread only. Windows counts in scheduler ticks (about 15.6 ms).
// Unsupported when the platform refuses the query.
[[nodiscard]] Base::Result<std::chrono::nanoseconds, OsError> ProcessCpuTime();
[[nodiscard]] Base::Result<std::chrono::nanoseconds, OsError> CurrentThreadCpuTime();

// The ANSI code page the process runs with (GetACP): UTF-8 (65001) where the
// executable declares it in its manifest (Design Invariant 11), the system's
// setting otherwise (Windows before 10 1903, or an executable built elsewhere).
// For diagnostics, such as a start line; engine/os never converts through it.
// Unsupported on platforms that have none (POSIX narrow text is bytes).
[[nodiscard]] Base::Result<std::uint32_t, OsError> AnsiCodePage();

// A time point in the C runtime's local time zone: the calendar fields and the
// offset from UTC at that instant (local minus UTC: +09:00 is 32400 s).
struct LocalTime final {
    int year;   // 2026
    int month;  // 1-12
    int day;    // 1-31
    int hour;   // 0-23
    int minute; // 0-59
    int second; // 0-60
    std::chrono::seconds utcOffset;
};

// Converts the whole seconds of time (system_clock::to_time_t) as
// localtime_r / localtime_s do, with the time zone the C runtime last read
// (TZ or the system setting). Unsupported, with the native error, for a time
// point the platform cannot convert (on Windows, after the year 3000).
[[nodiscard]] Base::Result<LocalTime, OsError> ToLocalTime(std::chrono::system_clock::time_point time);

} // namespace Engine::Os::Process
