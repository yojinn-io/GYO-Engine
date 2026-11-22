#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "engine/logging/JsonLines.hpp"
#include "engine/logging/LoggingError.hpp"
#include "engine/logging/Sink.hpp"

// The JSON lines file sink (section 5.7 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md, Design Invariant
// LG-6; the transport follows master's engine/trace TraceSink). The thread
// that writes a record formats its line and pushes it into a bounded queue
// under a short lock: it does no file I/O and never waits for the writer. A
// full queue drops the line and counts it. One role thread writes the queue
// out, woken only by a push into an empty queue, Flush or Close, never by a
// timer; it hands the bytes to the OS at the end of each batch or every
// 64 KiB, always at the end of a line. Close writes every accepted line, then
// the end line, so the end line is the last line of the file.
//
// The file is opened twice, both times to append: once for the writer thread,
// once for the fatal path (the fatal output), which writes its line at once,
// without the queue and without waiting for the writer (section 5.6). Each
// write of either output is whole lines, and appending puts each at the end
// of the file. "Written" means handed to the OS, not to the disk.

namespace GYO::Engine::Logging {

// Where the sink's bytes go. The file through engine/os by default; tests
// inject their own (one that never returns, one that fails). Write receives
// whole lines and returns whether every byte was handed on.
class LineOutput {
public:
    LineOutput() = default;
    LineOutput(const LineOutput&) = delete;
    LineOutput& operator=(const LineOutput&) = delete;
    virtual ~LineOutput() = default;
    virtual bool Write(std::string_view bytes) noexcept = 0;
    virtual bool Close() noexcept;
};

struct JsonLinesFileOptions final {
    // UTF-8. Ignored when both outputs are injected.
    std::string path;
    // Starts the file empty (it is truncated once, then opened to append).
    bool truncate{false};
    // Lines waiting for the writer; a line that finds this many is dropped. Positive.
    std::size_t capacity{65536};
    // The writer thread's name; non-empty.
    std::string threadName{"gyo-log"};
    // The time base of the end line; nullptr is GYO::Engine::Time::NowNs. Install
    // fills it with the Logger's clock when left nullptr.
    std::int64_t (*monoNs)() noexcept = nullptr;
    // Tests only: what the writer thread and the fatal path write to, instead
    // of the file at path. Both or neither.
    std::shared_ptr<LineOutput> output;
    std::shared_ptr<LineOutput> fatalOutput;
};

class JsonLinesFileSink final : public Sink {
public:
    // OpenFailed when the file cannot be opened (either time), StartFailed
    // when the writer thread cannot start.
    [[nodiscard]] static LoggingResult<std::shared_ptr<JsonLinesFileSink>> Open(JsonLinesFileOptions options);

    // Closes if Close was not called.
    ~JsonLinesFileSink() override;

    void Write(const Record& record) noexcept override;
    // Writes the line through the fatal output at once, under its own lock if
    // that is free and without it otherwise (a crash path never waits).
    bool WriteFatal(const Record& record, std::string_view jsonLine, std::string_view textLine) noexcept override;
    // False when the lines accepted before the call were not written by the
    // deadline (also while another thread holds the queue's lock), when called
    // on the writer thread, after Close, or once a write failed.
    bool Flush(std::chrono::steady_clock::time_point deadline) noexcept override;
    // Stops accepting lines, writes every accepted one, the end line, and
    // closes both outputs. Idempotent. Not on the writer thread.
    void Close() noexcept override;

    // False once a write of the writer thread failed. Dropped lines do not make it false.
    [[nodiscard]] bool Good() const noexcept;
    [[nodiscard]] JsonLinesCounts Counts() const noexcept;

    struct Impl;

private:
    explicit JsonLinesFileSink(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Engine::Logging
