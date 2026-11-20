#include "engine/logging/Logger.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

#include "LoggerState.hpp"
#include "TextFormat.hpp"
#include "engine/base/Assert.hpp"
#include "engine/logging/Write.hpp"
#include "engine/logging/WriteFatalRecord.hpp"
#include "engine/os/Process.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Logging::Detail {

namespace {

constexpr std::size_t kMaxSinks = 64;

bool HasPrefix(const std::vector<std::string>& prefixes, const std::string_view category) noexcept {
    for (const std::string& prefix : prefixes) {
        if (category.starts_with(prefix)) return true;
    }
    return false;
}

} // namespace

bool SinkEntry::TakesCategory(const std::string_view category) const noexcept {
    if (!include.empty() && !HasPrefix(include, category)) return false;
    return !HasPrefix(exclude, category);
}

Level SinkEntry::MinimumFor(const std::string_view category) const noexcept {
    std::size_t best = 0;
    std::size_t bestLength = 0;
    for (std::size_t i = 1; i < levelPrefixes.size(); ++i) {
        const std::string& prefix = levelPrefixes[i];
        if (prefix.size() > bestLength && category.starts_with(prefix)) {
            best = i;
            bestLength = prefix.size();
        }
    }
    return static_cast<Level>(levels[best].load(std::memory_order_relaxed));
}

bool SinkEntry::Takes(const Level level, const std::string_view category) const noexcept {
    return level >= MinimumFor(category) && TakesCategory(category);
}

std::uint64_t TakingSinks(const State& state, const Level level, const std::string_view category) noexcept {
    if (static_cast<std::uint8_t>(level) < state.floor.load(std::memory_order_relaxed)) return 0;
    std::uint64_t taking = 0;
    for (std::size_t i = 0; i < state.sinks.size(); ++i) {
        if (state.sinks[i].Takes(level, category)) taking |= std::uint64_t{1} << i;
    }
    return taking;
}

std::mutex& StandardErrorMutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

void WriteToStandardError(const std::string_view line) noexcept {
    std::scoped_lock lock(StandardErrorMutex());
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

namespace {

// Engine::Base::Log's lines, once the Logger took the handler.
void OnBaseLog(const ::Engine::Base::LogMessage& message) noexcept {
    Level level = Level::Info;
    switch (message.level) {
    case ::Engine::Base::LogLevel::Debug: level = Level::Debug; break;
    case ::Engine::Base::LogLevel::Info: level = Level::Info; break;
    case ::Engine::Base::LogLevel::Warning: level = Level::Warning; break;
    case ::Engine::Base::LogLevel::Error: level = Level::Error; break;
    }
    Write(Entry{.level = level, .category = message.category, .text = message.text});
}

} // namespace

std::int64_t DefaultMonoNs() noexcept {
    return ::Engine::Time::NowNs();
}

WallTime DefaultWall() noexcept {
    const auto now = std::chrono::system_clock::now();
    WallTime wall;
    wall.unixMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    try {
        if (const auto local = ::Engine::Os::Process::ToLocalTime(now)) {
            wall.utcOffsetSeconds = static_cast<std::int32_t>(local.value().utcOffset.count());
        }
    } catch (...) {
        // The offset stays 0: a record is never worth a failure.
    }
    return wall;
}

namespace {

// The handlers the Logger replaced, for its own handlers to go on to. Atomic,
// not in the state: a handler may run while the Logger is being uninstalled.
constinit std::atomic<::Engine::Base::AssertionHandler> previousAssertion{nullptr};
constinit std::atomic<std::terminate_handler> previousTerminate{nullptr};

// A failed GYO_ASSERT: the Fatal record first, then the handler installed
// before (a test's throws; the default prints). The abort stays Base's.
void OnAssertion(const ::Engine::Base::AssertionFailure& failure) {
    if (current.load(std::memory_order_acquire) != nullptr) {
        const Field fields[] = {{"file", failure.location.file_name()},
                                {"line", static_cast<std::uint64_t>(failure.location.line())},
                                {"function", failure.location.function_name()}};
        static_cast<void>(WriteFatalRecord(Record{.level = Level::Fatal,
                                                  .category = "assert",
                                                  .text = failure.expression != nullptr ? failure.expression : "",
                                                  .fields = fields}));
    }
    const ::Engine::Base::AssertionHandler previous = previousAssertion.load(std::memory_order_acquire);
    (previous != nullptr ? previous : &::Engine::Base::DefaultAssertionHandler)(failure);
}

// What std::terminate was called for: the exception in flight, if any.
std::string_view TerminateText(std::string& buffer) noexcept {
    const std::exception_ptr inFlight = std::current_exception();
    if (!inFlight) return "std::terminate was called with no exception in flight";
    try {
        std::rethrow_exception(inFlight);
    } catch (const std::exception& exception) {
        try {
            buffer = exception.what();
            return buffer;
        } catch (...) {
            return "a std::exception (no memory to copy what())";
        }
    } catch (...) {
        return "an exception that is not a std::exception";
    }
}

// std::terminate: one Fatal record of category "terminate" (the fatal path, which
// then flushes what the sinks queued within the fatal deadline), then the handler
// installed before (std::abort when there was none). The record says what
// happened; it handles nothing (user, 2026-11-21).
[[noreturn]] void OnTerminate() {
    if (current.load(std::memory_order_acquire) != nullptr) {
        std::string buffer;
        const std::string_view text = TerminateText(buffer);
        static_cast<void>(WriteFatalRecord(Record{.level = Level::Fatal, .category = "terminate", .text = text}));
    }
    if (const std::terminate_handler previous = previousTerminate.load(std::memory_order_acquire)) previous();
    std::abort();
}

} // namespace

} // namespace GYO::Engine::Logging::Detail

namespace GYO::Engine::Logging {

void Detail::LoggerState::RecomputeFloor() noexcept {
    std::uint8_t lowest = static_cast<std::uint8_t>(Level::Fatal);
    for (const Detail::SinkEntry& entry : sinks) {
        for (std::size_t i = 0; i < entry.levelPrefixes.size(); ++i) {
            const std::uint8_t level = entry.levels[i].load(std::memory_order_relaxed);
            if (level < lowest) lowest = level;
        }
    }
    // No sink: nothing is taken.
    if (sinks.empty()) lowest = static_cast<std::uint8_t>(Level::Fatal) + 1;
    floor.store(lowest, std::memory_order_relaxed);
}

std::int64_t Detail::LoggerState::MonoNs() const noexcept {
    return clock.monoNs ? clock.monoNs() : Detail::DefaultMonoNs();
}

WallTime Detail::LoggerState::Wall() const noexcept {
    return clock.wall ? clock.wall() : Detail::DefaultWall();
}

LoggingResult<LoggerHandle> Install(LoggerOptions options) {
    GYO_ASSERT(Detail::current.load(std::memory_order_acquire) == nullptr);
    GYO_ASSERT(options.sinks.size() <= Detail::kMaxSinks);
    auto state = std::make_unique<Detail::LoggerState>();
    state->clock = options.clock;
    state->sinks.reserve(options.sinks.size());
    for (SinkSetup& setup : options.sinks) {
        GYO_ASSERT(!setup.name.empty());
        std::shared_ptr<Sink> sink;
        if (auto* given = std::get_if<std::shared_ptr<Sink>>(&setup.sink)) {
            sink = std::move(*given);
        } else {
            auto file = std::get<JsonLinesFileOptions>(std::move(setup.sink));
            if (file.monoNs == nullptr) file.monoNs = options.clock.monoNs;
            auto opened = JsonLinesFileSink::Open(std::move(file));
            if (!opened) {
                for (Detail::SinkEntry& earlier : state->sinks) earlier.sink->Close();
                return ::Engine::Base::Err(std::move(opened).error());
            }
            sink = std::move(opened.value());
        }
        GYO_ASSERT(sink != nullptr);
        for (const Detail::SinkEntry& earlier : state->sinks) GYO_ASSERT(earlier.name != setup.name);
        Detail::SinkEntry entry;
        entry.name = std::move(setup.name);
        entry.sink = std::move(sink);
        entry.include = std::move(setup.filter.include);
        entry.exclude = std::move(setup.filter.exclude);
        entry.levelPrefixes.push_back({});
        for (LevelRule& rule : setup.filter.levels) {
            for (const std::string& earlier : entry.levelPrefixes) GYO_ASSERT(earlier != rule.prefix);
            entry.levelPrefixes.push_back(std::move(rule.prefix));
        }
        entry.levels = std::make_unique<std::atomic<std::uint8_t>[]>(entry.levelPrefixes.size());
        entry.levels[0].store(static_cast<std::uint8_t>(setup.filter.minimum), std::memory_order_relaxed);
        for (std::size_t i = 0; i < setup.filter.levels.size(); ++i) {
            entry.levels[i + 1].store(static_cast<std::uint8_t>(setup.filter.levels[i].minimum),
                                      std::memory_order_relaxed);
        }
        state->sinks.push_back(std::move(entry));
    }
    state->RecomputeFloor();
    state->fatalFlushTimeout = options.fatalFlushTimeout;
    state->fatalText.reserve(options.fatalLineCapacity);
    state->fatalJson.reserve(options.fatalLineCapacity);
    Detail::LoggerState* raw = state.release();
    Detail::current.store(raw, std::memory_order_release);
    if (options.takeBaseLog) {
        raw->tookBaseLog = true;
        raw->previousBaseLog = ::Engine::Base::SetLogHandler(&Detail::OnBaseLog);
    }
    if (options.takeAssertions) {
        raw->tookAssertions = true;
        raw->previousAssertion = ::Engine::Base::SetAssertionHandler(nullptr);
        Detail::previousAssertion.store(raw->previousAssertion, std::memory_order_release);
        ::Engine::Base::SetAssertionHandler(&Detail::OnAssertion);
    }
    if (options.takeTerminate) {
        raw->tookTerminate = true;
        raw->previousTerminate = std::get_terminate();
        Detail::previousTerminate.store(raw->previousTerminate, std::memory_order_release);
        std::set_terminate(&Detail::OnTerminate);
    }
    return LoggerHandle(raw);
}

LoggerHandle::LoggerHandle(LoggerHandle&& other) noexcept : state_(std::exchange(other.state_, nullptr)) {}

LoggerHandle& LoggerHandle::operator=(LoggerHandle&& other) noexcept {
    if (this != &other) {
        Close();
        state_ = std::exchange(other.state_, nullptr);
    }
    return *this;
}

LoggerHandle::~LoggerHandle() {
    Close();
}

void LoggerHandle::Close() noexcept {
    Detail::LoggerState* state = std::exchange(state_, nullptr);
    if (state == nullptr) return;
    if (state->tookBaseLog) ::Engine::Base::SetLogHandler(state->previousBaseLog);
    if (state->tookAssertions) ::Engine::Base::SetAssertionHandler(state->previousAssertion);
    if (state->tookTerminate) std::set_terminate(state->previousTerminate);
    Detail::current.store(nullptr, std::memory_order_release);
    for (Detail::SinkEntry& entry : state->sinks) entry.sink->Close();
    delete state;
}

Sink* LoggerHandle::FindSink(const std::string_view name) const noexcept {
    if (state_ == nullptr) return nullptr;
    for (const Detail::SinkEntry& entry : state_->sinks) {
        if (entry.name == name) return entry.sink.get();
    }
    return nullptr;
}

void SetLevel(const std::string_view sinkName, const std::string_view prefix, const Level minimum) {
    Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    GYO_ASSERT(state != nullptr);
    for (Detail::SinkEntry& entry : state->sinks) {
        if (entry.name != sinkName) continue;
        for (std::size_t i = 0; i < entry.levelPrefixes.size(); ++i) {
            if (entry.levelPrefixes[i] == prefix) {
                entry.levels[i].store(static_cast<std::uint8_t>(minimum), std::memory_order_relaxed);
                state->RecomputeFloor();
                return;
            }
        }
        GYO_ASSERT(false && "SetLevel: the prefix was not given to this sink at Install");
    }
    GYO_ASSERT(false && "SetLevel: no sink of this name");
}

bool Flush(const std::chrono::nanoseconds timeout) noexcept {
    Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    if (state == nullptr) return true;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool all = true;
    for (Detail::SinkEntry& entry : state->sinks) {
        if (!entry.sink->Flush(deadline)) all = false;
    }
    return all;
}

bool Enabled(const Level level, const std::string_view category) noexcept {
    const Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    if (state == nullptr) return true;
    return Detail::TakingSinks(*state, level, category) != 0;
}

namespace {

// The key written twice in fields, or empty when every key is distinct.
std::string_view RepeatedKey(const std::span<const Field> fields) noexcept {
    for (std::size_t i = 1; i < fields.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (fields[i].key == fields[j].key) return fields[i].key;
        }
    }
    return {};
}

void Deliver(const Detail::LoggerState& state, const std::uint64_t taking, const Record& record) noexcept {
    for (std::size_t i = 0; i < state.sinks.size(); ++i) {
        if ((taking >> i) & 1u) state.sinks[i].sink->Write(record);
    }
}

} // namespace

namespace Detail {

void DeliverRecord(const LoggerState& state, const std::uint64_t taking, const Record& record) noexcept {
    if (const std::string_view repeated = RepeatedKey(record.fields); !repeated.empty()) {
        // One line that does not read back would break the file's contract
        // (Design Invariant LG-5): the record becomes an Error record instead.
        try {
            std::string text = "a record was dropped: field key \"";
            text.append(repeated).append("\" is written twice (category ").append(record.category).append(")");
            Record error = record;
            error.level = Level::Error;
            error.category = "logging";
            error.text = text;
            error.fields = {};
            Deliver(state, TakingSinks(state, error.level, error.category), error);
        } catch (...) {
        }
        return;
    }
    Deliver(state, taking, record);
}

void WriteWithoutLogger(const Record& record) noexcept {
    try {
        std::string line;
        AppendTextLine(line, record);
        line += '\n';
        WriteToStandardError(line);
    } catch (...) {
        // A record is never worth a failure.
    }
}

} // namespace Detail

void Write(const Entry& entry) noexcept {
    const Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    if (state == nullptr) {
        Detail::WriteWithoutLogger(Record{.level = entry.level,
                                          .category = entry.category,
                                          .text = entry.text,
                                          .world = entry.world,
                                          .worldInterrupted = entry.worldInterrupted,
                                          .fields = entry.fields});
        return;
    }
    const std::uint64_t taking = Detail::TakingSinks(*state, entry.level, entry.category);
    if (taking == 0) return;
    Detail::DeliverRecord(*state, taking,
                          Record{                                 .level = entry.level,
                                 .category = entry.category,
                                 .text = entry.text,
                                 .wall = state->Wall(),
                                 .monoNs = state->MonoNs(),
                                 .world = entry.world,
                                 .worldInterrupted = entry.worldInterrupted,
                                 .fields = entry.fields});
}

} // namespace GYO::Engine::Logging
