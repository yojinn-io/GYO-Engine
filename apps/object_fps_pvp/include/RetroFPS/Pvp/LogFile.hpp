#pragma once

#include "engine/trace/TraceSink.hpp"

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace fps::pvp {

// Diagnostics of the product executables (Client and Match): a log file whose
// every line starts with the local wall clock (milliseconds and UTC offset)
// and the steady clock in nanoseconds, the clock of the movement trace. Hosts
// are not synchronized: compare their logs by wall time only roughly.
//
// The timestamp is taken on the writing thread; the file I/O happens on the
// Engine trace sink's writer thread, so no caller waits for the disk. A line
// that finds the queue full is dropped and counted; if any was, the file ends
// with a "[log] dropped_lines=" line.
class LogFile final {
public:
    // Opens (appends to) the file; throws when it cannot.
    explicit LogFile(const std::filesystem::path& path);
    ~LogFile();
    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    // Copies everything written to the stream into the file, line by line,
    // until this object is destroyed; the stream keeps its own output.
    void Tee(std::ostream& stream);
    // One timestamped line in the file only. Any thread.
    void Write(std::string_view line);
    // Blocks until every line written so far is in the file (bounded wait).
    bool FlushNow();
    // Until this object is destroyed, a failed GYO_ASSERT writes its failure
    // here and a std::terminate flushes the file before the previous handlers
    // run (the lines still queued would be lost otherwise). One LogFile per
    // process; install before starting threads.
    void FlushOnCrash();
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

    // Lines waiting for the writer before a Write is dropped.
    static constexpr std::size_t Capacity = 65536;

private:
    class TeeBuffer;
    std::filesystem::path path_;
    Engine::Trace::TraceSink sink_;
    std::vector<std::pair<std::ostream*, std::streambuf*>> restored_;
    std::vector<std::unique_ptr<TeeBuffer>> buffers_;
    bool crashFlush_{};
};

// The local wall clock and the steady clock, as LogFile writes them.
[[nodiscard]] std::string LogTimestamp();
// The running executable (throws when the platform cannot tell).
[[nodiscard]] std::filesystem::path RunningExecutablePath();
// SHA-256 (hex) of a file, such as the running executable; "unavailable" when unreadable.
[[nodiscard]] std::string FileSha256(const std::filesystem::path& path);

} // namespace fps::pvp
