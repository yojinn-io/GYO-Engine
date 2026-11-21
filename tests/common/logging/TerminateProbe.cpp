// Design Invariant LG-2 (target/v2/logging.zh-Hant.md): std::terminate with takeTerminate writes one Fatal
// record of category "terminate", then flushes and hands over to the handler installed before (user,
// 2026-11-21: the logging mechanism says what happened). The process ends in abort, which doctest cannot
// observe, so TerminateProbe.cmake runs this executable and reads the file it leaves.
//
//   gyo_logging_terminate_probe <file> std     an uncaught std::runtime_error
//   gyo_logging_terminate_probe <file> other   an uncaught int
//   gyo_logging_terminate_probe <file> none    std::terminate() with no exception in flight

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

#include "engine/logging/Logger.hpp"

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace Logging = GYO::Engine::Logging;

namespace {

// Installed before the Logger: the Logger's handler must go on to it (LG-2).
[[noreturn]] void EarlierHandler() {
    std::fputs("GYO probe: the earlier terminate handler ran\n", stderr);
    std::fflush(stderr);
    std::abort();
}

// An early return prints this marker: the exit status cannot tell it from abort() (3 on Windows).
int SetupFailed(const char* why) {
    std::fprintf(stderr, "GYO probe: setup failed: %s\n", why);
    std::fflush(stderr);
    return 1;
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc < 3) return SetupFailed("usage");
    std::set_terminate(&EarlierHandler);
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "file",
                             .sink = Logging::JsonLinesFileOptions{.path = argv[1], .truncate = true},
                             .filter = {.minimum = Logging::Level::Info}});
    options.takeAssertions = false;
    options.takeTerminate = true;
    auto installed = Logging::Install(std::move(options));
    if (!installed) return SetupFailed("Install");
    Logging::LoggerHandle handle = std::move(installed.value());
    const char* mode = argv[2];
    if (std::strcmp(mode, "std") == 0) throw std::runtime_error("terminate probe: a std::exception");
    if (std::strcmp(mode, "other") == 0) throw 42;
    if (std::strcmp(mode, "none") == 0) std::terminate();
    return SetupFailed("unknown mode");
}
