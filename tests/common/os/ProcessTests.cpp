#include <doctest/doctest.h>

#include "engine/os/Files.hpp"
#include "engine/os/Process.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// The one common test directory allowed to look at the OS: the old per-OS
// lookups are written here as the reference the moved code must match
// (Design Invariant 9).
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <time.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

using namespace Engine::Os;
using namespace std::chrono_literals;

namespace {

std::filesystem::path Native(const std::string& utf8) {
    auto path = Files::PathFromUtf8(utf8);
    REQUIRE_MESSAGE(path, "PathFromUtf8 failed for " << utf8);
    return path.value();
}

// The lookup the product made before engine/os had one (LogFile.cpp).
std::filesystem::path ReferenceExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(256, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        REQUIRE(length != 0);
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__linux__)
    return std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    auto size = static_cast<std::uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.resize(size);
        REQUIRE(_NSGetExecutablePath(buffer.data(), &size) == 0);
    }
    return std::filesystem::canonical(buffer.data());
#else
    return {};
#endif
}

// The CPU clocks the product read before engine/os had them
// (NetworkStatistics.cpp), in nanoseconds.
#if defined(_WIN32)
std::chrono::nanoseconds FromFileTime(const FILETIME& time) {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return std::chrono::nanoseconds{static_cast<std::int64_t>(value.QuadPart) * 100};
}
#endif

std::chrono::nanoseconds ReferenceProcessCpu() {
#if defined(_WIN32)
    FILETIME creation{}, exited{}, kernel{}, user{};
    REQUIRE(GetProcessTimes(GetCurrentProcess(), &creation, &exited, &kernel, &user));
    return FromFileTime(kernel) + FromFileTime(user);
#else
    rusage usage{};
    REQUIRE(getrusage(RUSAGE_SELF, &usage) == 0);
    return std::chrono::seconds{usage.ru_utime.tv_sec + usage.ru_stime.tv_sec} +
           std::chrono::microseconds{usage.ru_utime.tv_usec + usage.ru_stime.tv_usec};
#endif
}

std::chrono::nanoseconds ReferenceThreadCpu() {
#if defined(_WIN32)
    FILETIME creation{}, exited{}, kernel{}, user{};
    REQUIRE(GetThreadTimes(GetCurrentThread(), &creation, &exited, &kernel, &user));
    return FromFileTime(kernel) + FromFileTime(user);
#else
    timespec time{};
    REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0);
    return std::chrono::seconds{time.tv_sec} + std::chrono::nanoseconds{time.tv_nsec};
#endif
}

// Sets TZ for the C runtime and restores the previous value afterwards.
class ScopedTimeZone final {
public:
    explicit ScopedTimeZone(const char* zone) {
        if (const char* previous = std::getenv("TZ")) previous_ = previous;
        Set(zone);
    }
    ScopedTimeZone(const ScopedTimeZone&) = delete;
    ScopedTimeZone& operator=(const ScopedTimeZone&) = delete;
    ~ScopedTimeZone() { Set(previous_ ? previous_->c_str() : nullptr); }

private:
    static void Set(const char* zone) {
#if defined(_WIN32)
        _putenv_s("TZ", zone ? zone : "");
        _tzset();
#else
        if (zone) setenv("TZ", zone, 1);
        else unsetenv("TZ");
        tzset();
#endif
    }

    std::optional<std::string> previous_;
};

std::tm ReferenceLocal(const std::time_t time) {
    std::tm local{};
#if defined(_WIN32)
    REQUIRE(localtime_s(&local, &time) == 0);
#else
    REQUIRE(localtime_r(&time, &local) != nullptr);
#endif
    return local;
}

std::string Strftime(const char* format, const std::tm& local) {
    char text[64]{};
    std::strftime(text, sizeof text, format, &local);
    return text;
}

// The offset as strftime's %z prints it: a sign ('+' for zero), hours and minutes.
std::string FormatOffset(const std::chrono::seconds offset) {
    const auto total = offset.count() < 0 ? -offset.count() : offset.count();
    const auto minutes = total / 60;
    char text[16]{};
    std::snprintf(text, sizeof text, "%c%02lld%02lld", offset.count() < 0 ? '-' : '+',
                  static_cast<long long>(minutes / 60), static_cast<long long>(minutes % 60));
    return text;
}

std::string FormatWall(const Process::LocalTime& local, const char* format) {
    char text[64]{};
    std::snprintf(text, sizeof text, format, local.year, local.month, local.day, local.hour, local.minute,
                  local.second);
    return text;
}

} // namespace

TEST_CASE("Os process executable path names this test executable, as the old lookup did") {
    const auto path = Process::ExecutablePath();
    REQUIRE_MESSAGE(path, (path ? std::string{} : Engine::Base::Describe(path.error())));
    CHECK(Files::IsAbsolutePath(*path));
    const auto exists = Files::Exists(*path);
    REQUIRE(exists);
    CHECK(*exists);
    CHECK(Native(*path).stem() == "gyo_os_tests");
    CHECK(Native(*path) == ReferenceExecutablePath());
    // Asked again, the OS gives the same answer.
    const auto again = Process::ExecutablePath();
    REQUIRE(again);
    CHECK(*again == *path);
}

TEST_CASE("Os process reports its ANSI code page where the platform has one") {
    const auto page = Process::AnsiCodePage();
#if defined(_WIN32)
    // The manifest every executable of the repository embeds (Design Invariant 11).
    REQUIRE_MESSAGE(page, (page ? std::string{} : Engine::Base::Describe(page.error())));
    CHECK(*page == 65001u);
    CHECK(*page == ::GetACP());
#else
    REQUIRE_FALSE(page);
    CHECK(page.error().code == OsErrorCode::Unsupported);
#endif
}

TEST_CASE("Os process CPU times never decrease and the calling thread's grows with work") {
    const auto process = Process::ProcessCpuTime();
    const auto thread = Process::CurrentThreadCpuTime();
    REQUIRE(process);
    REQUIRE(thread);
    CHECK(process->count() >= 0);
    CHECK(thread->count() >= 0);
    const auto until = std::chrono::steady_clock::now() + 100ms;
    volatile std::uint64_t spin = 0;
    while (std::chrono::steady_clock::now() < until) spin = spin + 1;
    const auto processAfter = Process::ProcessCpuTime();
    const auto threadAfter = Process::CurrentThreadCpuTime();
    REQUIRE(processAfter);
    REQUIRE(threadAfter);
    CHECK(*processAfter >= *process);
    CHECK(*threadAfter > *thread);
}

TEST_CASE("Os process CPU times read the clocks the old lookups read") {
    // Each clock is read the old way just before and just after engine/os
    // reads it on the same thread: a monotonic clock puts engine/os's answer
    // between the two, in the same unit. (Two different clocks, such as the
    // process's getrusage and the thread's clock on macOS, need not agree at
    // any one instant, so they are not compared with each other.)
    const auto until = std::chrono::steady_clock::now() + 20ms;
    volatile std::uint64_t spin = 0;
    while (std::chrono::steady_clock::now() < until) spin = spin + 1;
    const auto processBefore = ReferenceProcessCpu();
    const auto process = Process::ProcessCpuTime();
    const auto processAfter = ReferenceProcessCpu();
    const auto threadBefore = ReferenceThreadCpu();
    const auto thread = Process::CurrentThreadCpuTime();
    const auto threadAfter = ReferenceThreadCpu();
    REQUIRE(process);
    REQUIRE(thread);
    CHECK(processBefore <= *process);
    CHECK(*process <= processAfter);
    CHECK(threadBefore <= *thread);
    CHECK(*thread <= threadAfter);
}

TEST_CASE("Os process thread CPU time belongs to the calling thread only") {
    // The main thread has spun; a new thread has not, so its own time is
    // below the process's time read before it started.
    const auto until = std::chrono::steady_clock::now() + 50ms;
    volatile std::uint64_t spin = 0;
    while (std::chrono::steady_clock::now() < until) spin = spin + 1;
    const auto process = Process::ProcessCpuTime();
    REQUIRE(process);
    std::optional<std::chrono::nanoseconds> fresh;
    std::thread([&] {
        if (auto time = Process::CurrentThreadCpuTime()) fresh = *time;
    }).join();
    REQUIRE(fresh);
    CHECK(*fresh < *process);
}

TEST_CASE("Os process local time matches the C runtime's fields and %z under fixed time zones") {
    // Whole seconds since the epoch: 2026-10-16T01:40:11Z, 2026-07-01T12:00:00Z,
    // 2026-01-15T12:00:00Z, 2000-02-29T23:59:59Z, 2026-03-08T10:30:00Z and
    // 2026-11-01T05:00:00Z (near the North American daylight saving changes).
    const std::vector<std::time_t> times{1792114811, 1782907200, 1768478400, 951868799, 1772965800, 1793509200};
    for (const char* zone : {"UTC0", "JST-9", "IST-5:30", "NST3:30NDT", "PST8PDT"}) {
        const ScopedTimeZone scoped(zone);
        for (const auto seconds : times) {
            CAPTURE(zone);
            CAPTURE(seconds);
            const auto point = std::chrono::system_clock::from_time_t(seconds) + 999ms;
            const auto local = Process::ToLocalTime(point);
            REQUIRE_MESSAGE(local, (local ? std::string{} : Engine::Base::Describe(local.error())));
            const auto reference = ReferenceLocal(seconds);
            CHECK(local->year == reference.tm_year + 1900);
            CHECK(local->month == reference.tm_mon + 1);
            CHECK(local->day == reference.tm_mday);
            CHECK(local->hour == reference.tm_hour);
            CHECK(local->minute == reference.tm_min);
            CHECK(local->second == reference.tm_sec);
            // Formatted by the caller, the fields and the offset give the
            // bytes strftime gave (%Y with four-digit years).
            CHECK(FormatWall(*local, "%04d-%02d-%02dT%02d:%02d:%02d") == Strftime("%Y-%m-%dT%H:%M:%S", reference));
            CHECK(FormatWall(*local, "%04d%02d%02d-%02d%02d%02d") == Strftime("%Y%m%d-%H%M%S", reference));
            CHECK(FormatOffset(local->utcOffset) == Strftime("%z", reference));
        }
    }
}

TEST_CASE("Os process local time offsets: zero, half hours and negative") {
    const auto at = std::chrono::system_clock::from_time_t(1768478400); // 2026-01-15T12:00:00Z
    {
        const ScopedTimeZone scoped("UTC0");
        const auto local = Process::ToLocalTime(at);
        REQUIRE(local);
        CHECK(local->utcOffset == 0s);
        CHECK(local->hour == 12);
    }
    {
        const ScopedTimeZone scoped("IST-5:30");
        const auto local = Process::ToLocalTime(at);
        REQUIRE(local);
        CHECK(local->utcOffset == 5h + 30min);
        CHECK(local->hour == 17);
        CHECK(local->minute == 30);
    }
    {
        const ScopedTimeZone scoped("NST3:30NDT");
        const auto local = Process::ToLocalTime(at);
        REQUIRE(local);
        CHECK(local->utcOffset == -(3h + 30min));
        CHECK(local->hour == 8);
        CHECK(local->minute == 30);
        CHECK(FormatOffset(local->utcOffset) == "-0330");
    }
}

#if defined(_WIN32)
TEST_CASE("Os process local time refuses a time point the C runtime cannot convert") {
    // The Windows C runtime converts up to the year 3000.
    const auto local = Process::ToLocalTime(std::chrono::system_clock::time_point{} + std::chrono::seconds{33'000'000'000});
    REQUIRE_FALSE(local);
    CHECK(local.error().code == OsErrorCode::Unsupported);
    CHECK(local.error().detail.rfind("errno=", 0) == 0);
}
#endif
