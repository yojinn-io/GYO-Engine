#pragma once

#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

// Engine diagnostics: a level, a category and a message, handed to one
// replaceable handler for the whole program. The default handler writes one
// line to std::cerr, so a consumer that copies std::cerr into its own log file
// receives Engine lines without installing anything. Where the lines finally
// go (a file, a trace, a console) belongs to the consumer.
//
// Not for Programmer Errors (GYO_ASSERT) or Runtime Errors (Result); a log
// line never changes control flow.

namespace Engine::Base {

enum class LogLevel : std::uint8_t { Debug, Info, Warning, Error };

[[nodiscard]] constexpr std::string_view ToString(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error: return "error";
    }
    return "unknown";
}

// The views are valid only during the handler call.
struct LogMessage final {
    LogLevel level{LogLevel::Info};
    std::string_view category;
    std::string_view text;
};

// May run on any thread, concurrently; must not throw.
using LogHandler = void (*)(const LogMessage&) noexcept;

namespace Detail {
inline constinit std::atomic<LogHandler> logHandler{nullptr};
} // namespace Detail

// "[category] level: text", written as one string so concurrent lines rarely interleave.
inline void DefaultLogHandler(const LogMessage& message) noexcept {
    try {
        std::string line;
        line.reserve(message.category.size() + message.text.size() + 16);
        line.append("[").append(message.category).append("] ").append(ToString(message.level)).append(": ");
        line.append(message.text).append("\n");
        std::cerr << line << std::flush;
    } catch (...) {
        // A log line is never worth a failure.
    }
}

// Installs handler for the whole program and returns the previous one;
// nullptr selects DefaultLogHandler. Safe from any thread at any time: a line
// being logged uses either the old or the new handler. The caller keeps
// whatever the old handler writes to alive until no line can still reach it.
inline LogHandler SetLogHandler(const LogHandler handler) noexcept {
    return Detail::logHandler.exchange(handler, std::memory_order_acq_rel);
}

inline void Log(const LogLevel level, const std::string_view category, const std::string_view text) noexcept {
    const LogHandler handler = Detail::logHandler.load(std::memory_order_acquire);
    (handler ? handler : &DefaultLogHandler)(LogMessage{level, category, text});
}

} // namespace Engine::Base
