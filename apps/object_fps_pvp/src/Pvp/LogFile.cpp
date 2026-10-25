#include "RetroFPS/Pvp/LogFile.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/os/Files.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

namespace fps::pvp {

Engine::Os::Process::LocalTime LocalWallClock(const std::chrono::system_clock::time_point time) {
    if (auto local = Engine::Os::Process::ToLocalTime(time)) return *local;
    return {.year = 1900, .month = 1, .day = 0, .hour = 0, .minute = 0, .second = 0, .utcOffset = {}};
}

std::string FormatLogTimestamp(const Engine::Os::Process::LocalTime& local, const int milliseconds,
                               const long long monotonicNs) {
    const long long offset = local.utcOffset.count();
    const long long minutes = (offset < 0 ? -offset : offset) / 60;
    char text[96]{};
    std::snprintf(text, sizeof text, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02lld%02lld mono_ns=%lld", local.year,
                  local.month, local.day, local.hour, local.minute, local.second, milliseconds, offset < 0 ? '-' : '+',
                  minutes / 60, minutes % 60, monotonicNs);
    return text;
}

std::string LogFileName(const std::string_view prefix, const Engine::Os::Process::LocalTime& local) {
    char stamp[32]{};
    std::snprintf(stamp, sizeof stamp, "-%04d%02d%02d-%02d%02d%02d.log", local.year, local.month, local.day, local.hour,
                  local.minute, local.second);
    return std::string(prefix) + stamp;
}

std::string LogTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const auto local = LocalWallClock(now);
    const auto steady = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return FormatLogTimestamp(local, static_cast<int>(milliseconds), static_cast<long long>(steady));
}

// The running executable, from engine/os; no window backend involved. In the
// platform's preferred separators, as the native lookup gave it before.
std::filesystem::path RunningExecutablePath() {
    auto utf8 = Engine::Os::Process::ExecutablePath();
    if (!utf8) throw std::runtime_error("Cannot locate the executable (" + Engine::Base::Describe(utf8.error()) + ")");
    auto path = Engine::Os::Files::PathFromUtf8(*utf8);
    if (!path) throw std::runtime_error("Cannot locate the executable (" + Engine::Base::Describe(path.error()) + ")");
    return std::move(path->make_preferred());
}

std::string FileSha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "unavailable";
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return Engine::Base::Sha256(std::as_bytes(std::span(bytes)));
}

// Forwards every character to the stream's own buffer and, per completed line,
// a timestamped copy to the log file.
class LogFile::TeeBuffer final : public std::streambuf {
public:
    TeeBuffer(LogFile& owner, std::streambuf* original) : owner_(owner), original_(original) {}

protected:
    int_type overflow(int_type character) override {
        if (traits_type::eq_int_type(character, traits_type::eof())) return traits_type::not_eof(character);
        const char value = traits_type::to_char_type(character);
        return xsputn(&value, 1) == 1 ? character : traits_type::eof();
    }
    std::streamsize xsputn(const char* text, std::streamsize count) override {
        if (original_) original_->sputn(text, count);
        std::scoped_lock lock(mutex_);
        for (std::streamsize i = 0; i < count; ++i) {
            if (text[i] == '\n') {
                owner_.Write(pending_);
                pending_.clear();
            } else {
                pending_.push_back(text[i]);
            }
        }
        return count;
    }
    int sync() override { return original_ ? original_->pubsync() : 0; }

private:
    LogFile& owner_;
    std::streambuf* original_;
    std::mutex mutex_;
    std::string pending_;
};

namespace {
Engine::Trace::TraceSink OpenLogSink(const std::filesystem::path& path, std::size_t capacity) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    auto sink = Engine::Trace::TraceSink::Open(
        {.path = path, .append = true, .capacity = capacity, .threadName = "pvp-log", .flushEachLine = true});
    if (!sink) throw std::runtime_error("Cannot open log file: " + path.string() + " (" +
                                        Engine::Base::Describe(sink.error()) + ")");
    return std::move(*sink);
}

// The LogFile that FlushOnCrash installed, and the handlers it replaced.
std::atomic<LogFile*> crashLog{nullptr};
Engine::Base::AssertionHandler previousAssertionHandler{};
std::terminate_handler previousTerminateHandler{};

void FlushingAssertionHandler(const Engine::Base::AssertionFailure& failure) {
    if (auto* log = crashLog.load(std::memory_order_acquire)) {
        log->Write(std::string("GYO_ASSERT failed: ") + failure.expression + " at " + failure.location.file_name() +
                   ":" + std::to_string(failure.location.line()) + " in " + failure.location.function_name());
        static_cast<void>(log->FlushNow());
    }
    (previousAssertionHandler ? previousAssertionHandler : &Engine::Base::DefaultAssertionHandler)(failure);
}

[[noreturn]] void FlushingTerminateHandler() {
    if (auto* log = crashLog.load(std::memory_order_acquire)) static_cast<void>(log->FlushNow());
    if (previousTerminateHandler) previousTerminateHandler();
    std::abort();
}
} // namespace

LogFile::LogFile(const std::filesystem::path& path, std::size_t capacity)
    : path_(path), sink_(OpenLogSink(path, capacity)) {}

LogFile::~LogFile() {
    for (auto it = restored_.rbegin(); it != restored_.rend(); ++it) it->first->rdbuf(it->second);
    if (crashFlush_) {
        std::set_terminate(previousTerminateHandler);
        Engine::Base::SetAssertionHandler(previousAssertionHandler);
        crashLog.store(nullptr, std::memory_order_release);
    }
    // Dropped lines (a full queue, or a Write racing this destructor) are
    // reported once, as the last line.
    static_cast<void>(sink_.Close([](const Engine::Trace::TraceCounts& counts) -> std::optional<std::string> {
        if (counts.Dropped() == 0) return std::nullopt;
        return LogTimestamp() + " [log] dropped_lines=" + std::to_string(counts.Dropped());
    }));
}

void LogFile::Tee(std::ostream& stream) {
    buffers_.push_back(std::make_unique<TeeBuffer>(*this, stream.rdbuf()));
    restored_.emplace_back(&stream, stream.rdbuf(buffers_.back().get()));
}

void LogFile::Write(std::string_view line) {
    std::string text = LogTimestamp();
    text.reserve(text.size() + 1 + line.size());
    text.push_back(' ');
    text.append(line);
    static_cast<void>(sink_.Push(std::move(text)));
}

bool LogFile::FlushNow() { return sink_.FlushNow(); }

void LogFile::FlushOnCrash() {
    if (crashFlush_) return;
    LogFile* expected = nullptr;
    if (!crashLog.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
        throw std::logic_error("Another log file already flushes on crash");
    previousAssertionHandler = Engine::Base::SetAssertionHandler(&FlushingAssertionHandler);
    previousTerminateHandler = std::set_terminate(&FlushingTerminateHandler);
    crashFlush_ = true;
}

} // namespace fps::pvp
