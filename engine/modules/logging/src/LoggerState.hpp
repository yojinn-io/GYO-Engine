#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Assert.hpp"
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
    // SetLevel stores a level and recomputes floor under this lock, so that two
    // calls at once cannot leave floor at a value one of them computed earlier.
    std::mutex levelMutex;
    bool tookBaseLog{false};
    Base::LogHandler previousBaseLog{nullptr};
    bool tookAssertions{false};
    Base::AssertionHandler previousAssertion{nullptr};
    bool tookTerminate{false};
    std::terminate_handler previousTerminate{nullptr};
    std::chrono::nanoseconds fatalFlushTimeout{};
    // The fatal path's lines, reserved at Install; tried, never waited for (a
    // second fatal path at the same moment formats into lines of its own).
    std::mutex fatalLinesMutex;
    std::string fatalText;
    std::string fatalJson;

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

// The default clocks: GYO::Engine::Time::NowNs, and the system clock with the C
// runtime's local offset from UTC.
[[nodiscard]] std::int64_t DefaultMonoNs() noexcept;
[[nodiscard]] WallTime DefaultWall() noexcept;

// Whether a fatal path (WriteFatalRecord) is running on this thread.
[[nodiscard]] bool InFatalPath() noexcept;

// The lock that keeps lines on standard error whole: a ConsoleSink on standard
// error and the no-Logger line take it; the fatal path only tries it.
[[nodiscard]] std::mutex& StandardErrorMutex() noexcept;

// The key written twice in fields, or empty when every key is distinct (no allocation).
[[nodiscard]] std::string_view RepeatedKey(std::span<const Field> fields) noexcept;

// Hands record to the sinks in taking; a record that writes one key twice
// goes as the Error record that replaces it (category "logging") to those
// sinks and to every sink that takes Error records of category "logging".
void DeliverRecord(const LoggerState& state, std::uint64_t taking, const Record& record) noexcept;

// The text line of record on standard error: what a record becomes with no Logger installed.
void WriteWithoutLogger(const Record& record) noexcept;

// Writes one line to standard error in one call and flushes it, under
// StandardErrorMutex: the text form of a record when no Logger is installed.
void WriteToStandardError(std::string_view line) noexcept;

} // namespace GYO::Engine::Logging::Detail
