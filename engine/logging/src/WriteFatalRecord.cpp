#include "engine/logging/WriteFatalRecord.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

#include "LoggerState.hpp"
#include "TextFormat.hpp"
#include "engine/logging/JsonLines.hpp"

namespace GYO::Engine::Logging::Detail {

namespace {

thread_local bool inFatalPath = false;

class FatalScope final {
public:
    FatalScope() noexcept { inFatalPath = true; }
    ~FatalScope() { inFatalPath = false; }
    FatalScope(const FatalScope&) = delete;
    FatalScope& operator=(const FatalScope&) = delete;
};

} // namespace

bool InFatalPath() noexcept {
    return inFatalPath;
}

} // namespace GYO::Engine::Logging::Detail

namespace GYO::Engine::Logging {

namespace {

// Standard error, its lock only tried: a line may interleave with another rather than wait.
bool WriteStandardErrorTried(const std::string_view line) noexcept {
    std::unique_lock lock(Detail::StandardErrorMutex(), std::try_to_lock);
    const bool written = std::fwrite(line.data(), 1, line.size(), stderr) == line.size();
    return std::fflush(stderr) == 0 && written;
}

// A re-entered fatal path, or one that could not format: no allocation, no lock, a fixed buffer.
bool WriteBareLine(const Record& record) noexcept {
    char buffer[512];
    const std::string_view level = ToString(record.level);
    const int written = std::snprintf(buffer, sizeof buffer, "[%.*s] %.*s: %.*s\n",
                                      static_cast<int>(record.category.size() < 128 ? record.category.size() : 128),
                                      record.category.data(), static_cast<int>(level.size()), level.data(),
                                      static_cast<int>(record.text.size() < 320 ? record.text.size() : 320),
                                      record.text.data());
    if (written <= 0) return false;
    const std::size_t size = static_cast<std::size_t>(written) < sizeof buffer ? static_cast<std::size_t>(written)
                                                                               : sizeof buffer - 1;
    const bool all = std::fwrite(buffer, 1, size, stderr) == size;
    return std::fflush(stderr) == 0 && all;
}

} // namespace

FatalWriteReport WriteFatalRecord(const Record& given) noexcept {
    FatalWriteReport report;
    if (Detail::InFatalPath()) {
        report.reentered = true;
        report.console = WriteBareLine(given);
        return report;
    }
    Detail::FatalScope scope;
    Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);

    Record record = given;
    record.monoNs = state != nullptr ? state->MonoNs() : Detail::DefaultMonoNs();
    record.wall = state != nullptr ? state->Wall() : Detail::DefaultWall();

    // 1. Format once, in the lines Install reserved; a second fatal path at the
    //    same moment (the lock is only tried) formats into lines of its own.
    std::unique_lock<std::mutex> reserved;
    std::string ownText;
    std::string ownJson;
    std::string* text = &ownText;
    std::string* json = &ownJson;
    if (state != nullptr) {
        reserved = std::unique_lock(state->fatalLinesMutex, std::try_to_lock);
        if (reserved.owns_lock()) {
            text = &state->fatalText;
            json = &state->fatalJson;
            text->clear();
            json->clear();
        }
    }
    bool haveText = true;
    bool haveJson = true;
    try {
        Detail::AppendTextLine(*text, record);
        *text += '\n';
    } catch (...) {
        haveText = false;
    }
    try {
        AppendJsonLine(*json, record);
    } catch (...) {
        haveJson = false;
    }

    // 2. Standard error, at once.
    report.console = haveText ? WriteStandardErrorTried(*text) : WriteBareLine(record);
    if (state == nullptr) {
        report.flushed = true;
        return report;
    }

    // 3. Every sink that takes the category: its fatal output, at once.
    for (const Detail::SinkEntry& entry : state->sinks) {
        if (!entry.TakesCategory(record.category)) continue;
        const bool written = haveJson && haveText && entry.sink->WriteFatal(record, *json, *text);
        if (written) {
            ++report.sinksWritten;
        } else {
            ++report.sinksFailed;
        }
    }

    // 4. Only then, at best: what every sink queued before, within one deadline for all.
    const auto deadline = std::chrono::steady_clock::now() + state->fatalFlushTimeout;
    bool all = true;
    for (const Detail::SinkEntry& entry : state->sinks) {
        if (!entry.sink->Flush(deadline)) all = false;
    }
    report.flushed = all;
    return report;
}

} // namespace GYO::Engine::Logging
