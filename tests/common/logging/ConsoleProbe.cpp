// What the logging Module writes on standard error (Design Invariants LG-2 and LG-8): the fatal path's own
// console line, also when the fatal path is re-entered; the line a record becomes with no Logger installed;
// and a ConsoleSink on standard error that must not write the fatal line a second time. doctest cannot read
// the process's standard error without platform code (the OS boundary guard keeps that in engine/os), so
// ConsoleProbe.cmake runs this executable and reads what it wrote (adversarial check, 2026-11-21).
//
//   gyo_logging_console_probe fatal      a fatal record "outer-line" whose sink re-enters with "inner-line"
//   gyo_logging_console_probe nologger   Write "no-logger-line" with no Logger installed
//   gyo_logging_console_probe twice      a ConsoleSink on standard error, then a fatal record "once-only"

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "engine/logging/ConsoleSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/Write.hpp"
#include "engine/logging/WriteFatalRecord.hpp"

namespace Logging = GYO::Engine::Logging;

namespace {

struct Reentering final : Logging::Sink {
    int depth = 0;
    void Write(const Logging::Record&) noexcept override {}
    bool WriteFatal(const Logging::Record&, std::string_view, std::string_view) noexcept override {
        if (depth++ == 0) {
            static_cast<void>(Logging::WriteFatalRecord(
                Logging::Record{.level = Logging::Level::Fatal, .category = "assert", .text = "inner-line"}));
        }
        return true;
    }
};

int Install(std::shared_ptr<Logging::Sink> sink, std::optional<Logging::LoggerHandle>& handle) {
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "probe", .sink = std::move(sink), .filter = {}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto installed = Logging::Install(std::move(options));
    if (!installed) return 3;
    handle.emplace(std::move(installed.value()));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string_view mode = argv[1];
    std::optional<Logging::LoggerHandle> handle;
    if (mode == "fatal") {
        if (const int failed = Install(std::make_shared<Reentering>(), handle)) return failed;
        static_cast<void>(Logging::WriteFatalRecord(
            Logging::Record{.level = Logging::Level::Fatal, .category = "assert", .text = "outer-line"}));
        return 0;
    }
    if (mode == "nologger") {
        Logging::Write(Logging::Level::Warning, "net", "no-logger-line");
        return 0;
    }
    if (mode == "twice") {
        if (const int failed = Install(std::make_shared<Logging::ConsoleSink>(), handle)) return failed;
        static_cast<void>(Logging::WriteFatalRecord(
            Logging::Record{.level = Logging::Level::Fatal, .category = "assert", .text = "once-only"}));
        return 0;
    }
    return 4;
}
