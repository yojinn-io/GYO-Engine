// The fatal path's last write (logging declaration 5.6; Design Invariant LG-2): standard error and every
// sink's fatal output first, without waiting for anything; only then a flush within one deadline. The
// assertion and terminate handlers.
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "AssertTestSupport.hpp"
#include "TestClock.hpp"
#include "TestOutputs.hpp"
#include "engine/base/Trace.hpp"
#include "engine/json/Document.hpp"
#include "engine/logging/JsonLines.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/WriteFatalRecord.hpp"

namespace Logging = GYO::Engine::Logging;
using Logging::Level;
using Logging::Record;
using GYO::Test::Logging::RecordingOutput;

namespace {

using namespace std::chrono_literals;

Record Broken() {
    static const Logging::Field fields[] = {{"file", "Simulation.cpp"}, {"line", 3u}};
    return Record{.level = Level::Fatal,
                  .category = "assert",
                  .text = "broken",
                  .world = "Simulation",
                  .fields = fields};
}

Logging::LoggerOptions WithMemory(std::shared_ptr<Logging::MemorySink> memory, Logging::SinkFilter filter = {}) {
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "memory", .sink = std::move(memory), .filter = std::move(filter)});
    options.clock = GYO::Test::Logging::TestClock();
    options.takeAssertions = false;
    options.takeTerminate = false;
    return options;
}

} // namespace

TEST_CASE("fatal: with no Logger only standard error is written") {
    const auto report = Logging::WriteFatalRecord(Broken());
    CHECK(report.console);
    CHECK(report.sinksWritten == 0);
    CHECK(report.sinksFailed == 0);
    CHECK(report.flushed);
    CHECK_FALSE(report.reentered);
}

TEST_CASE("fatal: every sink's WriteFatal gets the record's JSON line and text line, stamped with the Logger's clock") {
    GYO::Test::Logging::ResetClock();
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    GYO::Test::Logging::monoNow = 777;
    const auto report = Logging::WriteFatalRecord(Broken());
    CHECK(report.console);
    CHECK(report.sinksWritten == 1);
    CHECK(report.flushed);
    const auto records = memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].fatal);
    CHECK(records[0].level == Level::Fatal);
    CHECK(records[0].monoNs == 777);
    CHECK(records[0].world == "Simulation");
    Record expected = Broken();
    expected.monoNs = 777;
    expected.wall = GYO::Test::Logging::TestWall();
    std::string json;
    Logging::AppendJsonLine(json, expected);
    REQUIRE(memory->FatalJsonLines().size() == 1);
    CHECK(memory->FatalJsonLines()[0] == json);
    CHECK(memory->FatalTextLines()[0] == "[assert] fatal: broken world=Simulation file=Simulation.cpp line=3\n");
}

TEST_CASE("fatal: a sink that does not take the category gets nothing; the level is not consulted") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto other = std::make_shared<Logging::MemorySink>();
    auto options = WithMemory(memory, {.exclude = {"assert"}});
    options.sinks.push_back({.name = "other", .sink = other, .filter = {.minimum = Level::Fatal}});
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    const auto report = Logging::WriteFatalRecord(Broken());
    CHECK(report.sinksWritten == 1);
    CHECK(memory->Records().empty());
    CHECK(other->Records().size() == 1);
}

TEST_CASE("fatal: with the writer held, the fatal line is in the fatal output when it returns; the flush times out") {
    auto output = std::make_shared<RecordingOutput>();
    auto fatal = std::make_shared<RecordingOutput>();
    Logging::LoggerOptions options;
    options.sinks.push_back(
        {.name = "file",
         .sink = Logging::JsonLinesFileOptions{.output = output, .fatalOutput = fatal},
         .filter = {.minimum = Level::Trace}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    options.fatalFlushTimeout = 50ms;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    auto* sink = dynamic_cast<Logging::JsonLinesFileSink*>(handle.value().FindSink("file"));
    REQUIRE(sink != nullptr);
    output->Hold();
    sink->Write(Record{.level = Level::Info, .category = "c", .text = "queued before"});
    REQUIRE(output->WaitForWriteStarted(1, 5s));

    const auto start = std::chrono::steady_clock::now();
    const auto report = Logging::WriteFatalRecord(Broken());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(report.console);
    CHECK(report.sinksWritten == 1);
    CHECK_FALSE(report.flushed);
    CHECK(elapsed >= 50ms);
    CHECK(fatal->Lines().size() == 1);
    CHECK(fatal->Bytes().find("\"level\":\"fatal\"") != std::string::npos);
    CHECK(output->Bytes().empty());
    output->Release();
}

TEST_CASE("fatal: on a sink's own writer thread the flush is skipped at once; the fatal line is still written") {
    // An output whose first write starts a fatal path on the writer thread itself.
    struct FatalInWrite final : Logging::LineOutput {
        std::atomic<bool> done{false};
        std::optional<Logging::FatalWriteReport> report;
        std::chrono::steady_clock::duration elapsed{};
        bool Write(std::string_view) noexcept override {
            if (!done.exchange(true)) {
                const auto start = std::chrono::steady_clock::now();
                report = Logging::WriteFatalRecord(Broken());
                elapsed = std::chrono::steady_clock::now() - start;
            }
            return true;
        }
    };
    auto output = std::make_shared<FatalInWrite>();
    auto fatal = std::make_shared<RecordingOutput>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "file",
                             .sink = Logging::JsonLinesFileOptions{.output = output, .fatalOutput = fatal},
                             .filter = {.minimum = Level::Trace}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    options.fatalFlushTimeout = 5s;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    auto* sink = dynamic_cast<Logging::JsonLinesFileSink*>(handle.value().FindSink("file"));
    sink->Write(Record{.level = Level::Info, .category = "c", .text = "wakes the writer"});
    REQUIRE(sink->Flush(std::chrono::steady_clock::now() + 10s));
    REQUIRE(output->report.has_value());
    CHECK_FALSE(output->report->flushed);
    CHECK(output->elapsed < 4s);
    CHECK(fatal->Lines().size() == 1);
}

TEST_CASE("fatal: a fatal path started again on the same thread writes only standard error") {
    struct Reentering final : Logging::Sink {
        std::optional<Logging::FatalWriteReport> inner;
        int depth = 0;
        void Write(const Record&) noexcept override {}
        bool WriteFatal(const Record& record, std::string_view, std::string_view) noexcept override {
            // Only once, so that a missing re-entry flag shows as a wrong report, not as endless recursion.
            if (depth++ == 0) inner = Logging::WriteFatalRecord(record);
            return true;
        }
    };
    auto reentering = std::make_shared<Reentering>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "re", .sink = reentering, .filter = {}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    const auto outer = Logging::WriteFatalRecord(Broken());
    CHECK_FALSE(outer.reentered);
    CHECK(outer.sinksWritten == 1);
    REQUIRE(reentering->inner.has_value());
    CHECK(reentering->inner->reentered);
    CHECK(reentering->inner->console);
    CHECK(reentering->inner->sinksWritten == 0);
    // The flag is down again once the outer path returned.
    reentering->depth = 1;
    CHECK_FALSE(Logging::WriteFatalRecord(Broken()).reentered);
}

TEST_CASE("fatal: a sink whose WriteFatal fails is counted in sinksFailed") {
    struct Failing final : Logging::Sink {
        void Write(const Record&) noexcept override {}
        bool WriteFatal(const Record&, std::string_view, std::string_view) noexcept override { return false; }
    };
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "f", .sink = std::make_shared<Failing>(), .filter = {}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    const auto report = Logging::WriteFatalRecord(Broken());
    CHECK(report.sinksWritten == 0);
    CHECK(report.sinksFailed == 1);
}

TEST_CASE("fatal: a failed GYO_ASSERT writes a Fatal record of category assert, then goes on to the earlier handler") {
    auto memory = std::make_shared<Logging::MemorySink>();
    bool thrown = false;
    {
        GYO::Test::ScopedAssertionHandler throwing;
        auto options = WithMemory(memory);
        options.takeAssertions = true;
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
        try {
            GYO_ASSERT(1 + 1 == 3);
        } catch (const GYO::Engine::Base::AssertionFailure& failure) {
            thrown = true;
            CHECK(std::string(failure.expression) == "1 + 1 == 3");
        }
        handle.value().Close();
        // The earlier (throwing) handler is back.
        const auto now = GYO::Engine::Base::SetAssertionHandler(nullptr);
        GYO::Engine::Base::SetAssertionHandler(now);
        CHECK(now == &GYO::Test::ThrowAssertionFailure);
    }
    CHECK(thrown);
    const auto records = memory->Records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].fatal);
    CHECK(records[0].level == Level::Fatal);
    CHECK(records[0].category == "assert");
    CHECK(records[0].text == "1 + 1 == 3");
    REQUIRE(records[0].Find("file") != nullptr);
    CHECK(records[0].Find("file")->text.find("FatalTests.cpp") != std::string::npos);
    REQUIRE(records[0].Find("line") != nullptr);
    CHECK(records[0].Find("line")->uintValue > 0);
}

TEST_CASE("fatal: Install takes std::terminate when asked and puts the earlier handler back") {
    const std::terminate_handler before = std::get_terminate();
    {
        auto memory = std::make_shared<Logging::MemorySink>();
        auto options = WithMemory(memory);
        options.takeTerminate = true;
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
        CHECK(std::get_terminate() != before);
    }
    CHECK(std::get_terminate() == before);
    {
        auto memory = std::make_shared<Logging::MemorySink>();
        auto handle = Logging::Install(WithMemory(memory));
        REQUIRE(handle);
        CHECK(std::get_terminate() == before);
    }
}

TEST_CASE("fatal: a fatal record with a repeated key still writes a line that reads back, and stays Fatal (review S2)") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    const Logging::Field fields[] = {{"k", 1}, {"k", 2}};
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "x", .fields = fields}));
    REQUIRE(memory->FatalJsonLines().size() == 1);
    const std::string line = memory->FatalJsonLines()[0];
    auto document = GYO::Engine::Json::Parse(line);
    REQUIRE_MESSAGE(static_cast<bool>(document), "S2: the fatal line reads back (LG-5): " << line);
    const auto root = document.value().Root().AsObject().value();
    CHECK(root.Find("level").value().AsString().value() == "fatal");
    CHECK(root.Find("fields").value().AsObject().value().Find("dropped_fields").value().AsString().value() == "k");
}

namespace {

// Records, in one shared sequence, when its fatal output was written and when it was flushed.
struct OrderSink final : Logging::Sink {
    std::vector<std::string>* order = nullptr;
    std::string name;
    std::vector<std::chrono::steady_clock::time_point> deadlines;
    int fatal = 0;
    void Write(const Record&) noexcept override {}
    bool WriteFatal(const Record&, std::string_view, std::string_view) noexcept override {
        ++fatal;
        order->push_back(name + ":fatal");
        return true;
    }
    bool Flush(const std::chrono::steady_clock::time_point deadline) noexcept override {
        order->push_back(name + ":flush");
        deadlines.push_back(deadline);
        return true;
    }
};

} // namespace

TEST_CASE("fatal: every fatal output is written before any sink is flushed; all sinks share one deadline (LG-2)") {
    std::vector<std::string> order;
    auto a = std::make_shared<OrderSink>();
    auto b = std::make_shared<OrderSink>();
    a->order = b->order = &order;
    a->name = "a";
    b->name = "b";
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "a", .sink = a, .filter = {}});
    options.sinks.push_back({.name = "b", .sink = b, .filter = {}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    static_cast<void>(Logging::WriteFatalRecord(Broken()));
    CHECK_MESSAGE((order == std::vector<std::string>{"a:fatal", "b:fatal", "a:flush", "b:flush"}),
                  "LG-2 order: the fatal outputs first, then the flush");
    REQUIRE(a->deadlines.size() == 1);
    REQUIRE(b->deadlines.size() == 1);
    CHECK_MESSAGE(a->deadlines[0] == b->deadlines[0], "LG-2 step 4: one deadline for all sinks");
}

TEST_CASE("fatal: a sink that does not take the category is still flushed; its level is not consulted for the fatal output") {
    std::vector<std::string> order;
    auto excluded = std::make_shared<OrderSink>();
    auto strict = std::make_shared<OrderSink>();
    excluded->order = strict->order = &order;
    excluded->name = "excluded";
    strict->name = "strict";
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "excluded", .sink = excluded, .filter = {.exclude = {"assert"}}});
    options.sinks.push_back({.name = "strict", .sink = strict, .filter = {.minimum = Level::Error}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    // An Info record below strict's minimum: the fatal path still hands it to strict's fatal output.
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Info, .category = "assert", .text = "below"}));
    CHECK_MESSAGE(strict->fatal == 1, "LG-2 step 3: the level is not consulted");
    CHECK(excluded->fatal == 0);
    CHECK_MESSAGE(excluded->deadlines.size() == 1, "LG-2 step 4: every sink is flushed, also one not taking the category");
}

TEST_CASE("fatal: a second fatal record on the same Logger carries nothing of the first") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "first"}));
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "second"}));
    REQUIRE(memory->FatalJsonLines().size() == 2);
    const std::string line = memory->FatalJsonLines()[1];
    CHECK_MESSAGE(line.find("first") == std::string::npos, "LG-2: the reserved lines are cleared before each fatal record");
    CHECK(line.find("second") != std::string::npos);
    const std::string text = memory->FatalTextLines()[1];
    CHECK_MESSAGE(text.find("first") == std::string::npos, "LG-2: the reserved text line is cleared too");
}

namespace {

// A trace receiver whose path is fixed (the trace Module is not linked here: the fatal path reads the Base facade).
std::size_t FixedPath(const std::span<char> buffer) noexcept {
    constexpr std::string_view path = "kernel:Main@Match > kernel:MoveRacer@Match";
    const std::size_t size = path.size() < buffer.size() ? path.size() : buffer.size();
    std::memcpy(buffer.data(), path.data(), size);
    return size;
}
::GYO::Engine::Base::TraceToken NoBegin(std::string_view, std::string_view, std::string_view) noexcept {
    return {};
}
void NoEnd(::GYO::Engine::Base::TraceToken, bool) noexcept {}
void NoError(::GYO::Engine::Base::TraceToken, std::string_view, bool) noexcept {}
void NoMark(std::string_view, bool) noexcept {}
constexpr ::GYO::Engine::Base::TraceReceiver kPathReceiver{
    .begin = &NoBegin, .end = &NoEnd, .error = &NoError, .markCurrent = &NoMark, .path = &FixedPath};

} // namespace

TEST_CASE("fatal: with a trace receiver the fatal record carries trace_path, the current path; still one Fatal") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    const ::GYO::Engine::Base::TraceReceiver* previous = ::GYO::Engine::Base::SetTraceReceiver(&kPathReceiver);
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "x"}));
    ::GYO::Engine::Base::SetTraceReceiver(previous);
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "y"}));
    const auto records = memory->Records();
    REQUIRE(records.size() == 2);
    const auto* path = records[0].Find("trace_path");
    REQUIRE_MESSAGE(path != nullptr, "trace D4 (a): the fatal record carries trace_path");
    CHECK(path->text == "kernel:Main@Match > kernel:MoveRacer@Match");
    CHECK_MESSAGE(records[1].Find("trace_path") == nullptr, "trace D4 (a): no receiver, no trace_path");
}

TEST_CASE("fatal: trace_path is added after the record's own fields; with more than 32 fields it is left out") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    const ::GYO::Engine::Base::TraceReceiver* previous = ::GYO::Engine::Base::SetTraceReceiver(&kPathReceiver);
    const Logging::Field two[] = {{"file", "Simulation.cpp"}, {"line", 3u}};
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "two", .fields = two}));
    std::vector<std::string> keys(33);
    std::vector<Logging::Field> many;
    for (std::size_t i = 0; i < 33; ++i) {
        keys[i] = "k" + std::to_string(i);
        many.push_back(Logging::Field{keys[i], static_cast<std::uint64_t>(i)});
    }
    static_cast<void>(Logging::WriteFatalRecord(
        Record{.level = Level::Fatal, .category = "assert", .text = "thirty-two", .fields = std::span<const Logging::Field>(many.data(), 32)}));
    static_cast<void>(Logging::WriteFatalRecord(
        Record{.level = Level::Fatal, .category = "assert", .text = "thirty-three", .fields = std::span<const Logging::Field>(many.data(), 33)}));
    ::GYO::Engine::Base::SetTraceReceiver(previous);
    const auto records = memory->Records();
    REQUIRE(records.size() == 3);
    CHECK_MESSAGE((records[0].Find("file") != nullptr && records[0].Find("line") != nullptr), "trace D4 (a): the record's own fields stay");
    CHECK(records[0].Find("trace_path") != nullptr);
    CHECK_MESSAGE(records[1].Find("trace_path") != nullptr, "trace D4 (a): 32 fields still get trace_path");
    CHECK(records[1].Find("k31") != nullptr);
    CHECK_MESSAGE(records[2].Find("trace_path") == nullptr, "trace D4 (a): more than 32 fields go without trace_path");
    CHECK(records[2].Find("k32") != nullptr);
}

TEST_CASE("fatal: a record that already has a trace_path field keeps it and gets no second one") {
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    const ::GYO::Engine::Base::TraceReceiver* previous = ::GYO::Engine::Base::SetTraceReceiver(&kPathReceiver);
    const Logging::Field mine[] = {{"trace_path", "mine"}};
    static_cast<void>(Logging::WriteFatalRecord(Record{.level = Level::Fatal, .category = "assert", .text = "x", .fields = mine}));
    ::GYO::Engine::Base::SetTraceReceiver(previous);
    REQUIRE(memory->FatalJsonLines().size() == 1);
    const std::string line = memory->FatalJsonLines()[0];
    CHECK_MESSAGE(static_cast<bool>(GYO::Engine::Json::Parse(line)), "LG-5: the fatal line reads back (one trace_path key): " << line);
}
