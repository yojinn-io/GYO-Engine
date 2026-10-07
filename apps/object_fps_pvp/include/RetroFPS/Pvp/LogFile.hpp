#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace fps::pvp {

// Diagnostics of the product executables (Client and Match): a log file whose
// every line starts with the local wall clock (milliseconds and UTC offset)
// and the steady clock in nanoseconds, the clock of the movement trace. Hosts
// are not synchronized: compare their logs by wall time only roughly.
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
    // One timestamped line in the file only.
    void Write(std::string_view line);
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    class TeeBuffer;
    std::filesystem::path path_;
    std::mutex mutex_;
    std::ofstream file_;
    std::vector<std::pair<std::ostream*, std::streambuf*>> restored_;
    std::vector<std::unique_ptr<TeeBuffer>> buffers_;
};

// The local wall clock and the steady clock, as LogFile writes them.
[[nodiscard]] std::string LogTimestamp();
// The running executable (throws when the platform cannot tell).
[[nodiscard]] std::filesystem::path RunningExecutablePath();
// SHA-256 (hex) of a file, such as the running executable; "unavailable" when unreadable.
[[nodiscard]] std::string FileSha256(const std::filesystem::path& path);

} // namespace fps::pvp
