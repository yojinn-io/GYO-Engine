// The fatal path's last write (logging declaration 5.6; Design Invariant LG-2): standard error and every
// sink's fatal output first, without waiting for anything; only then a flush within one deadline. The
// assertion and terminate handlers.
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "AssertTestSupport.hpp"
#include "TestClock.hpp"
#include "TestOutputs.hpp"
#include "engine/json/Document.hpp"
#include "engine/logging/JsonLines.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/WriteFatalRecord.hpp"

namespace Logging = GYO::Engine::Logging;
using Logging::Level;
using Logging::Record;
using Logging::Test::RecordingOutput;

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
    options.clock = Logging::Test::TestClock();
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
    Logging::Test::ResetClock();
    auto memory = std::make_shared<Logging::MemorySink>();
    auto handle = Logging::Install(WithMemory(memory));
    REQUIRE(handle);
    Logging::Test::monoNow = 777;
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
    expected.wall = Logging::Test::TestWall();
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
        Engine::Test::ScopedAssertionHandler throwing;
        auto options = WithMemory(memory);
        options.takeAssertions = true;
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
        try {
            GYO_ASSERT(1 + 1 == 3);
        } catch (const Engine::Base::AssertionFailure& failure) {
            thrown = true;
            CHECK(std::string(failure.expression) == "1 + 1 == 3");
        }
        handle.value().Close();
        // The earlier (throwing) handler is back.
        const auto now = Engine::Base::SetAssertionHandler(nullptr);
        Engine::Base::SetAssertionHandler(now);
        CHECK(now == &Engine::Test::ThrowAssertionFailure);
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
