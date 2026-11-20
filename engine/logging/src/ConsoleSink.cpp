#include "engine/logging/ConsoleSink.hpp"

#include <string>

#include "LoggerState.hpp"
#include "TextFormat.hpp"

namespace GYO::Engine::Logging {

ConsoleSink::ConsoleSink(std::FILE* stream) noexcept : stream_(stream) {}

namespace {

bool WriteLine(std::FILE* stream, std::mutex& mutex, const std::string_view line) noexcept {
    std::scoped_lock lock(mutex);
    const bool written = std::fwrite(line.data(), 1, line.size(), stream) == line.size();
    return std::fflush(stream) == 0 && written;
}

} // namespace

void ConsoleSink::Write(const Record& record) noexcept {
    try {
        std::string line;
        line.reserve(record.category.size() + record.text.size() + 32 + 24 * record.fields.size());
        Detail::AppendTextLine(line, record);
        line += '\n';
        // Standard error shares one lock with the no-Logger line and the fatal
        // path, so that their lines stay whole (the fatal path only tries it).
        std::mutex& mutex = stream_ == stderr ? Detail::StandardErrorMutex() : ownMutex_;
        if (!WriteLine(stream_, mutex, line)) failed_.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
        failed_.fetch_add(1, std::memory_order_relaxed);
    }
}

bool ConsoleSink::WriteFatal(const Record&, std::string_view, const std::string_view textLine) noexcept {
    if (stream_ == stderr) return true;
    // Another stream: tried, never waited for (a crash path never waits).
    std::unique_lock lock(ownMutex_, std::try_to_lock);
    const bool written = std::fwrite(textLine.data(), 1, textLine.size(), stream_) == textLine.size();
    return std::fflush(stream_) == 0 && written;
}

} // namespace GYO::Engine::Logging
