// The console side of a process on Windows.
#include "engine/os/Console.hpp"

#include "../common/Shutdown.hpp"
#include "Wide.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

namespace Engine::Os::Console {

namespace {

// The OS ends the process about 5 s after a close; the handler gives the
// program less than that to finish (D49).
constexpr DWORD kCloseGraceMilliseconds = 4000;

// Runs on a thread the OS starts for each event.
BOOL WINAPI OnConsoleControl(const DWORD event) noexcept {
    switch (event) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        // After a request, another Ctrl+C or Ctrl+Break gets the default
        // handling and ends the process at once, as a second Ctrl+C did when
        // the C runtime's SIGINT handling reset itself: the way out of a
        // shutdown that hangs (Design Invariant 9).
        if (RequestedShutdown()) return FALSE;
        Detail::RecordShutdown(event == CTRL_C_EVENT ? ShutdownReason::Interrupt : ShutdownReason::Break);
        return TRUE;
    case CTRL_CLOSE_EVENT:
        Detail::RecordShutdown(ShutdownReason::ConsoleClose);
        // Returning lets the OS end the process; the program's own exit
        // ends it, and this wait, sooner.
        ::Sleep(kCloseGraceMilliseconds);
        return TRUE;
    default:
        // Log off and system shutdown keep the default handling.
        return FALSE;
    }
}

std::mutex installing;
bool installed = false;

struct LocalFreeDeleter final {
    void operator()(LPWSTR* arguments) const noexcept { ::LocalFree(arguments); }
};

} // namespace

Base::Result<std::vector<std::string>, OsError> CommandLineUtf8(int /*argc*/, const char* const* /*argv*/) {
    int count = 0;
    const std::unique_ptr<LPWSTR, LocalFreeDeleter> wide(::CommandLineToArgvW(::GetCommandLineW(), &count));
    if (!wide) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::IoFailed, "cannot read the command line",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        auto argument = Detail::WideToUtf8(wide.get()[index]);
        if (!argument) return Base::Err(std::move(argument.error()));
        arguments.push_back(std::move(*argument));
    }
    return arguments;
}

Utf8ConsoleOutput Utf8ConsoleOutput::Enable() noexcept {
    const UINT previous = ::GetConsoleOutputCP();
    if (previous == 0) return Utf8ConsoleOutput(false, 0);
    if (previous == CP_UTF8) return Utf8ConsoleOutput(true, 0);
    if (!::SetConsoleOutputCP(CP_UTF8)) return Utf8ConsoleOutput(false, 0);
    return Utf8ConsoleOutput(true, previous);
}

Utf8ConsoleOutput::Utf8ConsoleOutput(Utf8ConsoleOutput&& other) noexcept
    : applied_(other.applied_), previous_(other.previous_) {
    other.previous_ = 0;
}

Utf8ConsoleOutput::~Utf8ConsoleOutput() {
    if (previous_ != 0) static_cast<void>(::SetConsoleOutputCP(previous_));
}

Base::Result<void, OsError> InstallShutdownRequests() {
    // A second caller waits until the first one has finished installing.
    const std::scoped_lock lock(installing);
    if (installed) return {};
    if (!::SetConsoleCtrlHandler(&OnConsoleControl, TRUE)) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::Unsupported, "cannot install the shutdown-request handler",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    installed = true;
    return {};
}

} // namespace Engine::Os::Console
