// The console side of a process on POSIX systems (macOS, Linux).
#include "engine/os/Console.hpp"

#include "../common/Paths.hpp"
#include "../common/Shutdown.hpp"

#include <cerrno>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include <signal.h>

namespace Engine::Os::Console {

namespace {

// Async-signal-safe: one lock-free atomic write.
void OnShutdownSignal(const int signal) noexcept {
    Detail::RecordShutdown(signal == SIGTERM ? ShutdownReason::Terminate : ShutdownReason::Interrupt);
}

std::mutex installing;
bool installed = false;

} // namespace

Base::Result<std::vector<std::string>, OsError> CommandLineUtf8(const int argc, const char* const* const argv) {
    std::vector<std::string> arguments;
    if (argv == nullptr) return arguments;
    arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 0U);
    for (int index = 0; index < argc && argv[index] != nullptr; ++index) {
        std::string argument(argv[index]);
        if (!Detail::IsValidUtf8(argument)) {
            return Base::Err(Detail::RefusedArgument(OsErrorCode::InvalidEncoding,
                                                     "command line argument is not valid UTF-8", argument.size()));
        }
        arguments.push_back(std::move(argument));
    }
    return arguments;
}

// POSIX consoles take bytes: nothing to set or restore.
Utf8ConsoleOutput Utf8ConsoleOutput::Enable() noexcept { return Utf8ConsoleOutput(true, 0); }

Utf8ConsoleOutput::Utf8ConsoleOutput(Utf8ConsoleOutput&& other) noexcept
    : applied_(other.applied_), previous_(other.previous_) {
    other.previous_ = 0;
}

Utf8ConsoleOutput::~Utf8ConsoleOutput() = default;

Base::Result<void, OsError> InstallShutdownRequests() {
    // A second caller waits until the first one has finished installing.
    const std::scoped_lock lock(installing);
    if (installed) return {};
    // As std::signal installed them before: restarted system calls, the
    // handler kept for later signals, the signal blocked while it runs.
    struct sigaction action {};
    action.sa_handler = &OnShutdownSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    constexpr int signals[] = {SIGINT, SIGTERM};
    struct sigaction previous[std::size(signals)] {};
    for (std::size_t index = 0; index < std::size(signals); ++index) {
        if (::sigaction(signals[index], &action, &previous[index]) != 0) {
            const int error = errno;
            // All or nothing: the ones already replaced get their handlers back.
            for (std::size_t undo = 0; undo < index; ++undo) static_cast<void>(::sigaction(signals[undo], &previous[undo], nullptr));
            return Base::Err(MakeOsError(OsErrorCode::Unsupported, "cannot install the shutdown-request handler",
                                         {NativeErrorDomain::Errno, error}));
        }
    }
    installed = true;
    return {};
}

} // namespace Engine::Os::Console
