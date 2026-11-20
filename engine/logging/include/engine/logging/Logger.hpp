#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/logging/Level.hpp"
#include "engine/logging/LoggingError.hpp"
#include "engine/logging/Record.hpp"
#include "engine/logging/Sink.hpp"

// The one Logger of the process (section 5.2 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md, Design
// Invariants LG-7 and LG-8). main installs it before the first Product and
// before starting other threads, and uninstalls it after the last Product and
// after every other thread that could still write has stopped. While none is
// installed, Write prints one line to standard error, as Engine::Base::Log's
// default handler does.

namespace GYO::Engine::Logging {

// One per-prefix minimum level of a sink.
struct LevelRule final {
    std::string prefix;
    Level minimum{Level::Info};
};

// Which records a sink takes (section 5.8). The categories it takes are fixed
// at Install; the levels may change at run time with SetLevel.
struct SinkFilter final {
    // Category prefixes the sink takes; empty takes every category.
    std::vector<std::string> include;
    // Category prefixes the sink never takes, checked after include.
    std::vector<std::string> exclude;
    // The minimum level of a category no rule in levels matches.
    Level minimum{Level::Info};
    // Per-prefix minimum levels; the longest matching prefix wins. Prefixes are distinct.
    std::vector<LevelRule> levels;
};

struct SinkSetup final {
    // Distinct among the sinks of one Logger; names the sink to SetLevel.
    std::string name;
    std::shared_ptr<Sink> sink;
    SinkFilter filter;
};

// The clocks a record is stamped with; tests inject their own (section 9.1).
struct LoggerClock final {
    // nullptr: Engine::Time::NowNs.
    std::int64_t (*monoNs)() noexcept = nullptr;
    // nullptr: the system clock, with the C runtime's local offset from UTC.
    WallTime (*wall)() noexcept = nullptr;
};

struct LoggerOptions final {
    std::vector<SinkSetup> sinks;
    // Engine::Base::Log's lines become records (the category and text as
    // given; Debug, Info, Warning and Error as the same levels).
    bool takeBaseLog{true};
    LoggerClock clock;
};

class LoggerHandle;

namespace Detail {
struct LoggerState;
} // namespace Detail

// Installs the Logger. A second Logger while one is installed is a Programmer
// Error, as are an empty or repeated sink name, a missing sink and a repeated
// level prefix.
[[nodiscard]] LoggingResult<LoggerHandle> Install(LoggerOptions options);

// Uninstalls the Logger when destroyed or closed: Close is called on every
// sink, in the order they were given, and the handlers the Logger took are
// put back. Move-only.
class LoggerHandle final {
public:
    LoggerHandle(LoggerHandle&& other) noexcept;
    LoggerHandle& operator=(LoggerHandle&& other) noexcept;
    LoggerHandle(const LoggerHandle&) = delete;
    LoggerHandle& operator=(const LoggerHandle&) = delete;
    ~LoggerHandle();

    // Idempotent.
    void Close() noexcept;

private:
    friend LoggingResult<LoggerHandle> Install(LoggerOptions options);
    explicit LoggerHandle(Detail::LoggerState* state) noexcept : state_(state) {}
    Detail::LoggerState* state_ = nullptr;
};

// Changes the minimum level of one of a sink's rules from the next record on;
// prefix "" is the sink's default minimum. Any thread. The sink and the
// prefix must have been given at Install (a Programmer Error otherwise, as is
// calling it with no Logger installed).
void SetLevel(std::string_view sinkName, std::string_view prefix, Level minimum);

// Waits until every record handed to a sink before the call has been handed
// on, at most timeout for all sinks together. True with no Logger installed.
[[nodiscard]] bool Flush(std::chrono::nanoseconds timeout) noexcept;

} // namespace GYO::Engine::Logging
