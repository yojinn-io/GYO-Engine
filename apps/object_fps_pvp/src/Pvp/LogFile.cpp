#include "RetroFPS/Pvp/LogFile.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fps::pvp {

std::string LogTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char wall[32]{};
    char zone[8]{};
    std::strftime(wall, sizeof wall, "%Y-%m-%dT%H:%M:%S", &local);
    std::strftime(zone, sizeof zone, "%z", &local);
    const auto steady = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    char text[96]{};
    std::snprintf(text, sizeof text, "%s.%03d%s mono_ns=%lld", wall, static_cast<int>(milliseconds), zone,
                  static_cast<long long>(steady));
    return text;
}

// Platform lookup of the running executable; no window backend involved.
std::filesystem::path RunningExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(256, L'\0');
    for (;;) {
        const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
        if(length==0) throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),
                                             "Cannot locate the executable");
        if(length<buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        if(buffer.size()>=32768) throw std::runtime_error("Executable path is too long");
        buffer.resize(buffer.size()*2);
    }
#elif defined(__linux__)
    return std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    std::uint32_t size=static_cast<std::uint32_t>(buffer.size());
    if(_NSGetExecutablePath(buffer.data(),&size)!=0) {
        buffer.resize(size);
        if(_NSGetExecutablePath(buffer.data(),&size)!=0)
            throw std::runtime_error("Cannot locate the executable");
    }
    return std::filesystem::canonical(buffer.data());
#else
    throw std::runtime_error("Cannot locate the executable on this platform");
#endif
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
