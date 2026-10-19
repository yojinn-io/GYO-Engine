#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Engine::Trace {

enum class TraceErrorCode : std::uint16_t {
    // The file could not be opened for writing.
    OpenFailed = 1,
    // The writer thread or its wait primitive could not start.
    StartFailed = 2,
};

[[nodiscard]] constexpr const char* ToString(const TraceErrorCode code) noexcept {
    switch (code) {
    case TraceErrorCode::OpenFailed: return "OpenFailed";
    case TraceErrorCode::StartFailed: return "StartFailed";
    }
    return "Unknown";
}

using TraceError = Base::Error<TraceErrorCode>;
static_assert(Base::CodedError<TraceError>);

struct TraceSinkOptions final {
    std::filesystem::path path;
    // Appends to an existing file instead of replacing it.
    bool append{false};
    // Lines waiting for the writer; a Push that finds this many is dropped. Positive.
    std::size_t capacity{8192};
    // The writer thread's name (Threads::RoleThreadOptions); non-empty.
    std::string threadName{"gyo-trace"};
};

// Every Push is counted exactly once: pushed = written + droppedFull +
// droppedClosed once the sink is closed and no Push is still running.
struct TraceCounts final {
    // Lines handed to the file, the end line not included.
    std::uint64_t written{};
    // Refused because capacity lines were waiting.
    std::uint64_t droppedFull{};
    // Refused because Close had started.
    std::uint64_t droppedClosed{};
    [[nodiscard]] std::uint64_t Dropped() const noexcept { return droppedFull + droppedClosed; }
};

// One file of pre-serialized lines from any number of producer threads.
// Producers only append to a bounded queue under a short lock and never do
// file I/O or wait for the writer; one role thread (Threads::RoleThread)
// writes and flushes, woken by the Push that finds the queue empty, never by
// a timer. The content of a line is the consumer's: the sink adds the newline
// and nothing else, and never parses or rewrites it.
//
// Lines of one producer are written in its Push order; lines of different
// producers in the order they entered the queue. Close writes the consumer's
// end line after every accepted line, so it is the last line of the file.
//
// Push, FlushNow, Good and Counts may be called from any thread. Close, the
// move and the destructor belong to the owner, one thread at a time, never the
// writer thread itself.
class TraceSink final {
public:
    [[nodiscard]] static Base::Result<TraceSink, TraceError> Open(TraceSinkOptions options);

    TraceSink(TraceSink&& other) noexcept;
    TraceSink& operator=(TraceSink&&) = delete;
    TraceSink(const TraceSink&) = delete;
    TraceSink& operator=(const TraceSink&) = delete;
    // Closes without an end line.
    ~TraceSink();

    // line without its newline. False when the line was dropped (and counted).
    bool Push(std::string line);

    // Blocks until every line accepted before the call is written and flushed,
    // for paths about to end the process (an assertion or terminate handler).
    // False when that did not happen within timeout, when called on the writer
    // thread, or after Close.
    bool FlushNow(std::chrono::nanoseconds timeout = std::chrono::seconds{1});

    // Stops accepting lines, writes every accepted line, then the line
    // endLine(counts) returns, if any, flushes and closes the file. The counts handed to endLine are
    // the ones returned: written and droppedFull are final; droppedClosed counts
    // the refusals so far (a producer still pushing after Close is the owner's
    // stop-order error and is counted by Counts() only). Idempotent: a later
    // call returns the counts of the first.
    using EndLine = std::function<std::optional<std::string>(const TraceCounts&)>;
    TraceCounts Close(const EndLine& endLine = {});

    // False once a write or flush failed. Dropped lines do not make it false.
    [[nodiscard]] bool Good() const;
    [[nodiscard]] TraceCounts Counts() const;

private:
    struct Impl;
    explicit TraceSink(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Trace
