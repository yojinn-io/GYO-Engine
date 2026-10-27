// Process facts on Windows.
#include "engine/os/Process.hpp"

#include "engine/os/Files.hpp"

#include "../common/Calendar.hpp"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Engine::Os::Process {

namespace {

// GetModuleFileNameW gives up beyond the longest path Windows has.
constexpr std::size_t kMaximumModulePath = 32768;

OsError FromWin32(const OsErrorCode code, const DWORD error, std::string message) {
    return MakeOsError(code, std::move(message), {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)});
}

// FILETIME counts 100 ns intervals.
std::chrono::nanoseconds FromFileTime(const FILETIME& time) noexcept {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return std::chrono::nanoseconds{static_cast<std::int64_t>(value.QuadPart) * 100};
}

// A long-path form is never returned (Design Invariant 5): \\?\C:\x becomes
// C:\x and \\?\UNC\server\share becomes \\server\share.
std::wstring WithoutLongPathPrefix(std::wstring path) {
    constexpr std::wstring_view unc = L"\\\\?\\UNC\\";
    constexpr std::wstring_view local = L"\\\\?\\";
    if (std::wstring_view(path).starts_with(unc)) return L"\\\\" + path.substr(unc.size());
    if (std::wstring_view(path).starts_with(local)) return path.substr(local.size());
    return path;
}

} // namespace

Base::Result<std::string, OsError> ExecutablePath() {
    std::wstring buffer(256, L'\0');
    for (;;) {
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(OsErrorCode::IoFailed, error, "cannot locate the executable"));
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return Files::PathToUtf8(std::filesystem::path(WithoutLongPathPrefix(std::move(buffer))));
        }
        if (buffer.size() >= kMaximumModulePath) {
            return Base::Err(OsError::Make(OsErrorCode::TooLarge, "executable path is too long"));
        }
        buffer.resize(buffer.size() * 2);
    }
}

Base::Result<std::chrono::nanoseconds, OsError> ProcessCpuTime() {
    FILETIME creation{}, exited{}, kernel{}, user{};
    if (!::GetProcessTimes(::GetCurrentProcess(), &creation, &exited, &kernel, &user)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(OsErrorCode::Unsupported, error, "cannot read the process CPU time"));
    }
    return FromFileTime(kernel) + FromFileTime(user);
}

Base::Result<std::chrono::nanoseconds, OsError> CurrentThreadCpuTime() {
    FILETIME creation{}, exited{}, kernel{}, user{};
    if (!::GetThreadTimes(::GetCurrentThread(), &creation, &exited, &kernel, &user)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(OsErrorCode::Unsupported, error, "cannot read the thread CPU time"));
    }
    return FromFileTime(kernel) + FromFileTime(user);
}

Base::Result<std::uint32_t, OsError> AnsiCodePage() {
    return static_cast<std::uint32_t>(::GetACP());
}

Base::Result<LocalTime, OsError> ToLocalTime(const std::chrono::system_clock::time_point time) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    if (const errno_t error = ::localtime_s(&local, &seconds); error != 0) {
        return Base::Err(MakeOsError(OsErrorCode::Unsupported, "cannot convert the time point to local time",
                                     {NativeErrorDomain::Errno, error}));
    }
    return Detail::LocalTimeFromCalendar(local, seconds);
}

} // namespace Engine::Os::Process
