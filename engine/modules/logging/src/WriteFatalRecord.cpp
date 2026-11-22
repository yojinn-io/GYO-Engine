#include "engine/logging/WriteFatalRecord.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>

#include "LoggerState.hpp"
#include "TextFormat.hpp"
#include "engine/base/Trace.hpp"
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

// The trace_path field's buffer, and the most fields a fatal record may have for it to be added.
constexpr std::size_t kTracePathBytes = 1024;
constexpr std::size_t kMaxFatalFields = 32;

// A Field has no default value; the fatal path's array is filled with blanks it overwrites.
template <std::size_t... I>
std::array<Field, sizeof...(I)> BlankFields(std::index_sequence<I...>) noexcept {
    return {((void)I, Field{"", 0})...};
}

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
    // A key written twice would give a line that does not read back (LG-5). The
    // fatal record stays Fatal: its fields give way to one that names the key.
    const std::string_view repeated = Detail::RepeatedKey(record.fields);
    const Field droppedFields{"dropped_fields", repeated};
    if (!repeated.empty()) record.fields = std::span<const Field>(&droppedFields, 1);
    // The trace path at this moment (trace declaration D4 (a)): where the E0 happened, read from the Base
    // facade into a fixed buffer (no allocation, no lock). A record with more than kMaxFatalFields fields,
    // or one that names trace_path itself, keeps its fields and goes without it (the key once: LG-5).
    char pathBuffer[kTracePathBytes];
    const std::size_t pathSize = ::GYO::Engine::Base::TraceCurrentPath(pathBuffer);
    std::array<Field, kMaxFatalFields + 1> withPath = BlankFields(std::make_index_sequence<kMaxFatalFields + 1>{});
    const bool namesPath = std::any_of(record.fields.begin(), record.fields.end(),
                                       [](const Field& field) { return field.key == "trace_path"; });
    if (pathSize != 0 && !namesPath && record.fields.size() <= kMaxFatalFields) {
        std::copy(record.fields.begin(), record.fields.end(), withPath.begin());
        withPath[record.fields.size()] = Field{"trace_path", std::string_view(pathBuffer, pathSize)};
        record.fields = std::span<const Field>(withPath.data(), record.fields.size() + 1);
    }
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
