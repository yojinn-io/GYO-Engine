// Design Invariant LG-2 (target/v2/logging.zh-Hant.md): std::terminate with takeTerminate writes one Fatal
// record of category "terminate", then flushes and hands over to the handler installed before (user,
// 2026-11-21: the logging mechanism says what happened). The process ends in abort, which doctest cannot
// observe, so TerminateProbe.cmake runs this executable and reads the file it leaves.
//
//   gyo_logging_terminate_probe <file> std     an uncaught std::runtime_error
//   gyo_logging_terminate_probe <file> other   an uncaught int
//   gyo_logging_terminate_probe <file> none    std::terminate() with no exception in flight

#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

#include "engine/logging/Logger.hpp"

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace Logging = GYO::Engine::Logging;

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc < 3) return 2;
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "file",
                             .sink = Logging::JsonLinesFileOptions{.path = argv[1], .truncate = true},
                             .filter = {.minimum = Logging::Level::Info}});
    options.takeAssertions = false;
    options.takeTerminate = true;
    auto installed = Logging::Install(std::move(options));
    if (!installed) return 3;
    Logging::LoggerHandle handle = std::move(installed.value());
    const char* mode = argv[2];
    if (std::strcmp(mode, "std") == 0) throw std::runtime_error("terminate probe: a std::exception");
    if (std::strcmp(mode, "other") == 0) throw 42;
    if (std::strcmp(mode, "none") == 0) std::terminate();
    return 4;
}
