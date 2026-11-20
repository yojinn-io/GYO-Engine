#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Log.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/Record.hpp"

// The installed Logger, shared by the files of engine/logging. Not a public header.

namespace GYO::Engine::Logging::Detail {

// One sink and its filter. The levels are atomics so that SetLevel can change
// them while other threads write; everything else is fixed at Install.
struct SinkEntry final {
    std::string name;
    std::shared_ptr<Sink> sink;
    std::vector<std::string> include;
    std::vector<std::string> exclude;
    // levelPrefixes[0] is "" (the default minimum); levels[i] goes with levelPrefixes[i].
    std::vector<std::string> levelPrefixes;
    std::unique_ptr<std::atomic<std::uint8_t>[]> levels;

    [[nodiscard]] bool TakesCategory(std::string_view category) const noexcept;
    [[nodiscard]] Level MinimumFor(std::string_view category) const noexcept;
    [[nodiscard]] bool Takes(Level level, std::string_view category) const noexcept;
};

struct LoggerState final {
    std::vector<Detail::SinkEntry> sinks;
    LoggerClock clock;
    // The lowest minimum level over every rule of every sink: a record below
    // it is refused without looking at the sinks.
    std::atomic<std::uint8_t> floor{0};
    bool tookBaseLog{false};
    ::Engine::Base::LogHandler previousBaseLog{nullptr};

    void RecomputeFloor() noexcept;
    [[nodiscard]] std::int64_t MonoNs() const noexcept;
    [[nodiscard]] WallTime Wall() const noexcept;
};

using State = LoggerState;

// The installed Logger, or nullptr.
inline constinit std::atomic<State*> current{nullptr};

// The sinks of state that take a record of this level and category, as a bit
// set (Install allows at most 64 sinks).
[[nodiscard]] std::uint64_t TakingSinks(const State& state, Level level, std::string_view category) noexcept;

// Writes one line to standard error in one call and flushes it: the text form
// of a record when no Logger is installed.
void WriteToStandardError(std::string_view line) noexcept;

} // namespace GYO::Engine::Logging::Detail
