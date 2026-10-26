// Process facts on POSIX systems (macOS, Linux).
#include "engine/os/Process.hpp"

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
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace Engine::Os::Process {

namespace {

OsError FromErrno(const OsErrorCode code, const int error, std::string message) {
    return MakeOsError(code, std::move(message), {NativeErrorDomain::Errno, error});
}

std::chrono::nanoseconds FromTimeval(const timeval& time) noexcept {
    return std::chrono::seconds{time.tv_sec} + std::chrono::microseconds{time.tv_usec};
}

Base::Result<std::string, OsError> Utf8Only(std::string path) {
    if (!Detail::IsValidUtf8(path)) {
        return Base::Err(
            Detail::RefusedArgument(OsErrorCode::InvalidEncoding, "executable path is not valid UTF-8", path.size()));
    }
    return path;
}

} // namespace

Base::Result<std::string, OsError> ExecutablePath() {
#if defined(__linux__)
    std::vector<char> buffer(1024);
    for (;;) {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            const int error = errno;
            return Base::Err(FromErrno(OsErrorCode::IoFailed, error, "cannot locate the executable"));
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return Utf8Only(std::string(buffer.data(), static_cast<std::size_t>(length)));
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    auto size = static_cast<std::uint32_t>(buffer.size());
    if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
        // size now holds the length needed.
        buffer.resize(size);
        if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return Base::Err(OsError::Make(OsErrorCode::IoFailed, "cannot locate the executable"));
        }
    }
    const std::unique_ptr<char, decltype(&std::free)> resolved(::realpath(buffer.data(), nullptr), &std::free);
    if (!resolved) {
        const int error = errno;
        return Base::Err(FromErrno(OsErrorCode::IoFailed, error, "cannot resolve the executable path"));
    }
    return Utf8Only(std::string(resolved.get()));
#else
    return Base::Err(OsError::Make(OsErrorCode::Unsupported, "this platform cannot tell the executable path"));
#endif
}

Base::Result<std::chrono::nanoseconds, OsError> ProcessCpuTime() {
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(OsErrorCode::Unsupported, error, "cannot read the process CPU time"));
    }
    return FromTimeval(usage.ru_utime) + FromTimeval(usage.ru_stime);
}

Base::Result<std::chrono::nanoseconds, OsError> CurrentThreadCpuTime() {
    timespec time{};
    if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(OsErrorCode::Unsupported, error, "cannot read the thread CPU time"));
    }
    return std::chrono::seconds{time.tv_sec} + std::chrono::nanoseconds{time.tv_nsec};
}

Base::Result<LocalTime, OsError> ToLocalTime(const std::chrono::system_clock::time_point time) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    if (::localtime_r(&seconds, &local) == nullptr) {
        const int error = errno;
        return Base::Err(FromErrno(OsErrorCode::Unsupported, error, "cannot convert the time point to local time"));
    }
    return Detail::LocalTimeFromCalendar(local, seconds);
}

} // namespace Engine::Os::Process
