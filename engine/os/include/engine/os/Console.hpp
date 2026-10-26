#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The console side of a process (docs/architecture/os.md): its command line in
// UTF-8, UTF-8 output to its console and the requests to shut it down. What a
// program does on a shutdown request is the program's policy.

namespace Engine::Os::Console {

// The process's command line as UTF-8 arguments, the program name first.
// Windows: GetCommandLineW split by CommandLineToArgvW, whatever the ANSI code
// page is; argc and argv are not read. POSIX: argv as given.
// InvalidEncoding when an argument has no UTF-8 form (on POSIX, bytes that are
// not UTF-8).
[[nodiscard]] Base::Result<std::vector<std::string>, OsError> CommandLineUtf8(int argc, const char* const* argv);

// While it lives, UTF-8 written to the process's console shows as UTF-8: on
// Windows the console's output code page is UTF-8 and the previous one comes
// back when the object is destroyed; POSIX needs no change. Applied() is false
// when the request needs an OS call that failed, as when there is no console
// (a process started from a desktop). Output redirected to a file or a pipe is
// bytes either way.
class Utf8ConsoleOutput final {
public:
    [[nodiscard]] static Utf8ConsoleOutput Enable() noexcept;
    Utf8ConsoleOutput(Utf8ConsoleOutput&& other) noexcept;
    Utf8ConsoleOutput& operator=(Utf8ConsoleOutput&&) = delete;
    Utf8ConsoleOutput(const Utf8ConsoleOutput&) = delete;
    Utf8ConsoleOutput& operator=(const Utf8ConsoleOutput&) = delete;
    ~Utf8ConsoleOutput();

    [[nodiscard]] bool Applied() const noexcept { return applied_; }

private:
    Utf8ConsoleOutput(bool applied, std::uint32_t previous) noexcept : applied_(applied), previous_(previous) {}

    bool applied_;
    // The output code page to restore; 0 when there is nothing to restore.
    std::uint32_t previous_;
};

// What asked the process to shut down.
enum class ShutdownReason : std::uint8_t {
    Interrupt = 1, // SIGINT; Ctrl+C on Windows
    Terminate,     // SIGTERM (POSIX)
    Break,         // Ctrl+Break (Windows)
    ConsoleClose,  // the console window closes (Windows)
};

[[nodiscard]] constexpr const char* ToString(const ShutdownReason reason) noexcept {
    switch (reason) {
    case ShutdownReason::Interrupt:    return "interrupt";
    case ShutdownReason::Terminate:    return "terminate";
    case ShutdownReason::Break:        return "break";
    case ShutdownReason::ConsoleClose: return "console_close";
    }
    return "unknown";
}

// Installs the process's shutdown-request handlers, once per process; later
// calls do nothing; nothing is installed when it fails. The first request's
// reason is kept.
// POSIX: SIGINT and SIGTERM (sigaction, SA_RESTART, kept installed, as
// std::signal installed them): later signals only repeat the request.
// Windows: Ctrl+C, Ctrl+Break and closing the console (SetConsoleCtrlHandler).
// After a request, another Ctrl+C or Ctrl+Break ends the process at once, as a
// second Ctrl+C did before. On a close the OS ends the process when the
// handler returns, so the handler waits up to 4 s for the program to finish.
[[nodiscard]] Base::Result<void, OsError> InstallShutdownRequests();

// The first shutdown request since InstallShutdownRequests, if any. Any thread.
[[nodiscard]] std::optional<ShutdownReason> RequestedShutdown() noexcept;

} // namespace Engine::Os::Console
