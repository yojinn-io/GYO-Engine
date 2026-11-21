#pragma once

#include <cstdint>
#include <string_view>

// How detailed a record is (docs/architecture/target/v2/logging-declaration.zh-Hant.md, section 5.2):
// the five levels of Bevy and tracing plus Fatal, which only the fatal path
// writes (WriteFatalRecord). Trace is only the most detailed level; it has
// nothing to do with the "trace." category prefix that routes analysis
// records to their own file (section 5.13).

namespace GYO::Engine::Logging {

// Ordered: a sink takes a record when its level is at least the sink's minimum.
enum class Level : std::uint8_t { Trace = 1, Debug, Info, Warning, Error, Fatal };

[[nodiscard]] constexpr std::string_view ToString(const Level level) noexcept {
    switch (level) {
    case Level::Trace: return "trace";
    case Level::Debug: return "debug";
    case Level::Info: return "info";
    case Level::Warning: return "warning";
    case Level::Error: return "error";
    case Level::Fatal: return "fatal";
    }
    return "unknown";
}

} // namespace GYO::Engine::Logging
