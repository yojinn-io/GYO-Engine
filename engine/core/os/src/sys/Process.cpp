// Process facts on macOS and Linux; the steps that differ between them are
// the platform's (Platform.hpp).
#include "engine/os/Process.hpp"

#include "Platform.hpp"
#include "../common/Calendar.hpp"
#include "../common/Paths.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

namespace GYO::Engine::OS::Process {

namespace {

OSError FromErrno(const OSErrorCode code, const int error, std::string message) {
    return MakeOSError(code, std::move(message), {NativeErrorDomain::Errno, error});
}

std::chrono::nanoseconds FromTimeval(const timeval& time) noexcept {
    return std::chrono::seconds{time.tv_sec} + std::chrono::microseconds{time.tv_usec};
}

Base::Result<std::string, OSError> Utf8Only(std::string path) {
    if (!Detail::IsValidUtf8(path)) {
        return Base::Err(
            Detail::RefusedArgument(OSErrorCode::InvalidEncoding, "executable path is not valid UTF-8", path.size()));
    }
    return path;
}

} // namespace

Base::Result<std::string, OSError> ExecutablePath() {
    auto path = Detail::CurrentPlatform().NativeExecutablePath();
    if (!path) return Base::Err(std::move(path).error());
    return Utf8Only(std::move(path).value());
}

Base::Result<std::chrono::nanoseconds, OSError> ProcessCpuTime() {
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(OSErrorCode::Unsupported, error, "cannot read the process CPU time"));
    }
    return FromTimeval(usage.ru_utime) + FromTimeval(usage.ru_stime);
}

Base::Result<std::chrono::nanoseconds, OSError> CurrentThreadCpuTime() {
    timespec time{};
    if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(OSErrorCode::Unsupported, error, "cannot read the thread CPU time"));
    }
    return std::chrono::seconds{time.tv_sec} + std::chrono::nanoseconds{time.tv_nsec};
}

Base::Result<std::uint32_t, OSError> AnsiCodePage() {
    return Base::Err(OSError::Make(OSErrorCode::Unsupported, "this platform has no ANSI code page"));
}

Base::Result<LocalTime, OSError> ToLocalTime(const std::chrono::system_clock::time_point time) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    if (::localtime_r(&seconds, &local) == nullptr) {
        const int error = errno;
        return Base::Err(FromErrno(OSErrorCode::Unsupported, error, "cannot convert the time point to local time"));
    }
    return Detail::LocalTimeFromCalendar(local, seconds);
}

} // namespace GYO::Engine::OS::Process
