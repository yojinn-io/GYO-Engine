#pragma once

#include <chrono>
#include <string_view>

#include "engine/logging/Record.hpp"

// Where records go (section 5.7 of docs/architecture/target/v2/logging-declaration.zh-Hant.md).
// The built-in sinks are ConsoleSink, JsonLinesFileSink and MemorySink; a
// product may write its own. Which records reach a sink is decided by the
// Logger with the sink's SinkFilter (Logger.hpp), not by the sink.
//
// Every function is noexcept (Design Invariant LG-11): a sink that fails counts
// the failure and reports it through its own means; it never throws into the
// code that wrote the record, which may be a System of a World.

namespace GYO::Engine::Logging {

class Sink {
public:
    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    virtual ~Sink() = default;

    // On the thread that wrote the record, possibly on several threads at once.
    virtual void Write(const Record& record) noexcept = 0;

    // The fatal path (WriteFatalRecord, section 5.6): the record already
    // formatted as one JSON line and one text line, both without the newline.
    // The line must be handed on without waiting for anything this sink queued
    // earlier; standard error has already been written by the fatal path
    // itself. Returns whether the line was handed on. Default: nothing to do.
    virtual bool WriteFatal(const Record& record, std::string_view jsonLine, std::string_view textLine) noexcept;

    // Waits until every record handed in before the call has been handed on
    // (to the OS, not to the disk), at most until deadline. Returns whether
    // that happened. Default: nothing is queued, true.
    virtual bool Flush(std::chrono::steady_clock::time_point deadline) noexcept;

    // Once, when the Logger is uninstalled, after the last Write and before
    // the sink is released. Default: nothing.
    virtual void Close() noexcept;
};

} // namespace GYO::Engine::Logging
