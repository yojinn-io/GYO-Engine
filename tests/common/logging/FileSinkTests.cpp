// The JSON lines file sink (logging declaration 5.7; Design Invariant LG-6 and
// judgement 3): the bounded queue, the writer thread, the counts, the end line,
// the fatal output, and the real file through engine/os.
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "TestClock.hpp"
#include "TestOutputs.hpp"
#include "engine/json/Document.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/Write.hpp"

namespace Logging = GYO::Engine::Logging;
namespace Json = GYO::Engine::Json;
using Logging::Level;
using Logging::Record;
using Logging::Test::RecordingOutput;

namespace {

using namespace std::chrono_literals;

struct Injected {
    std::shared_ptr<RecordingOutput> output = std::make_shared<RecordingOutput>();
    std::shared_ptr<RecordingOutput> fatal = std::make_shared<RecordingOutput>();
    std::shared_ptr<Logging::JsonLinesFileSink> sink;

    explicit Injected(const std::size_t capacity = 65536) {
        auto opened = Logging::JsonLinesFileSink::Open({.capacity = capacity,
                                                        .monoNs = &Logging::Test::TestMonoNs,
                                                        .output = output,
                                                        .fatalOutput = fatal});
        REQUIRE(opened);
        sink = opened.value();
    }
};

Record At(const std::int64_t monoNs, const std::string_view text) {
    return Record{.level = Level::Info, .category = "c", .text = text, .monoNs = monoNs};
}

std::string TextOf(const std::string& line) {
    auto document = Json::Parse(line);
    REQUIRE(document);
    const auto root = document.value().Root().AsObject().value();
    if (const auto text = root.Find("text")) return std::string(text->AsString().value());
    return std::string(root.Find("kind").value().AsString().value());
}

std::filesystem::path TempPath(const std::string& name) {
    // In the build directory, never the working directory (a run from the repository root left files there).
    const auto dir = std::filesystem::path(GYO_LOGGING_TEST_BINARY_DIR) / "logging-test-files";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("file: lines are written in Write order; Close writes the end line last and closes both outputs") {
    Injected file;
    for (int i = 0; i < 100; ++i) file.sink->Write(At(i, std::to_string(i)));
    file.sink->Close();
    const auto lines = file.output->Lines();
    REQUIRE(lines.size() == 101);
    for (int i = 0; i < 100; ++i) CHECK(TextOf(lines[static_cast<std::size_t>(i)]) == std::to_string(i));
    CHECK(TextOf(lines[100]) == "end");
    CHECK(lines[100].find("\"written\":100,\"dropped_full\":0,\"dropped_closed\":0,\"fatal_direct\":0") !=
          std::string::npos);
    CHECK(file.output->Closed());
    CHECK(file.fatal->Closed());
    CHECK(file.sink->Good());
}

TEST_CASE("file: an idle writer sleeps until it is notified, never on a timer (LG-6)") {
    Injected file;
    file.sink->Write(At(0, "first"));
    REQUIRE(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    // Idle long enough that a writer woken by a timer would wake (and assert) before the next push.
    std::this_thread::sleep_for(50ms);
    file.sink->Write(At(1, "second"));
    file.sink->Close();
    const auto lines = file.output->Lines();
    REQUIRE(lines.size() == 3);
    CHECK(TextOf(lines[0]) == "first");
    CHECK(TextOf(lines[1]) == "second");
}

TEST_CASE("file: a full queue drops and counts; every Write is counted once (judgement 3)") {
    Injected file(4);
    file.output->Hold();
    file.sink->Write(At(0, "taken by the writer"));
    REQUIRE(file.output->WaitForWriteStarted(1, 5s));
    // The writer is held inside its write: the queue fills up to 4, then drops.
    for (int i = 1; i <= 10; ++i) file.sink->Write(At(i, std::to_string(i)));
    auto counts = file.sink->Counts();
    CHECK(counts.droppedFull == 6);
    file.output->Release();
    file.sink->Close();
    file.sink->Write(At(11, "after close"));
    counts = file.sink->Counts();
    CHECK(counts.written == 5);
    CHECK(counts.droppedFull == 6);
    CHECK(counts.droppedClosed == 1);
    CHECK(counts.written + counts.droppedFull + counts.droppedClosed == 12);
    const auto lines = file.output->Lines();
    REQUIRE(lines.size() == 6);
    // The four queued while the writer was held went out as one batch, in Write order (LG-6).
    CHECK(TextOf(lines[1]) == "1");
    CHECK(TextOf(lines[2]) == "2");
    CHECK(TextOf(lines[3]) == "3");
    CHECK(TextOf(lines[4]) == "4");
    // The end line carries the counts of Close: written and dropped_full are final.
    CHECK(lines.back().find("\"written\":5,\"dropped_full\":6,\"dropped_closed\":0") != std::string::npos);
}

TEST_CASE("file: a fatal line is written at once through the fatal output, while the writer is held") {
    Injected file;
    file.output->Hold();
    file.sink->Write(At(1, "queued before"));
    REQUIRE(file.output->WaitForWriteStarted(1, 5s));
    const std::string json = "{\"schema\":1,\"kind\":\"log\",\"level\":\"fatal\"}\n";
    CHECK(file.sink->WriteFatal(Record{.level = Level::Fatal}, json, "[world] fatal: x\n"));
    CHECK(file.fatal->Bytes() == json);
    CHECK(file.output->Bytes().empty());
    CHECK(file.sink->Counts().fatalDirect == 1);
    file.output->Release();
    file.sink->Close();
    const auto counts = file.sink->Counts();
    // The fatal line is not in the conservation of the queue.
    CHECK(counts.written == 1);
    CHECK(file.output->Lines().back().find("\"fatal_direct\":1") != std::string::npos);
}

TEST_CASE("file: Flush waits for the lines accepted before it, and is false at the deadline while the writer is held") {
    Injected file;
    file.sink->Write(At(1, "a"));
    CHECK(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    CHECK(file.output->Lines().size() == 1);

    file.output->Hold();
    file.sink->Write(At(2, "b"));
    REQUIRE(file.output->WaitForWriteStarted(2, 5s));
    const auto start = std::chrono::steady_clock::now();
    CHECK_FALSE(file.sink->Flush(start + 50ms));
    CHECK(std::chrono::steady_clock::now() - start >= 50ms);
    file.output->Release();
    CHECK(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    file.sink->Close();
    CHECK_FALSE(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
}

TEST_CASE("file: a failed write makes Good false and Flush false; later lines are not written") {
    Injected file;
    file.output->Fail();
    file.sink->Write(At(1, "fails"));
    CHECK_FALSE(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    CHECK_FALSE(file.sink->Good());
    file.sink->Close();
    CHECK(file.output->Bytes().empty());
}

TEST_CASE("file: a record JSON cannot represent is written as its Error record and counted as replaced") {
    Injected file;
    file.sink->Write(Record{.level = Level::Info, .category = "c", .text = "bad \xFF"});
    file.sink->Close();
    const auto lines = file.output->Lines();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find("\"category\":\"logging\"") != std::string::npos);
    CHECK(file.sink->Counts().replaced == 1);
    CHECK(file.sink->Counts().written == 1);
}

TEST_CASE("file: lines from several threads are all written, each whole") {
    Injected file;
    constexpr int kThreads = 4;
    constexpr int kEach = 2000;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kEach; ++i) file.sink->Write(At(t * kEach + i, "thread " + std::to_string(t)));
        });
    }
    for (auto& thread : threads) thread.join();
    file.sink->Close();
    const auto lines = file.output->Lines();
    REQUIRE(lines.size() == kThreads * kEach + 1);
    for (const auto& line : lines) CHECK(Json::Parse(line));
    CHECK(file.sink->Counts().written == kThreads * kEach);
}

TEST_CASE("file: the real file: Install opens it, records and the end line reach it, a second Install appends") {
    const auto path = TempPath("real.jsonl");
    for (int round = 0; round < 2; ++round) {
        Logging::LoggerOptions options;
        options.sinks.push_back({.name = "file",
                                 .sink = Logging::JsonLinesFileOptions{.path = path.string()},
                                 .filter = {.minimum = Level::Trace}});
        options.clock = Logging::Test::TestClock();
        auto handle = Logging::Install(std::move(options));
        REQUIRE(handle);
        REQUIRE(handle.value().FindSink("file") != nullptr);
        Logging::Write(Level::Info, "c", "round " + std::to_string(round));
        handle.value().Close();
        CHECK(handle.value().FindSink("file") == nullptr);
    }
    const std::string all = ReadAll(path);
    std::istringstream in(all);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    REQUIRE(lines.size() == 4);
    CHECK(TextOf(lines[0]) == "round 0");
    CHECK(TextOf(lines[1]) == "end");
    CHECK(TextOf(lines[2]) == "round 1");
    CHECK(TextOf(lines[3]) == "end");
    // LineEndings::AsWritten: no "\r\n" on any platform.
    CHECK(all.find('\r') == std::string::npos);
}

TEST_CASE("file: truncate starts the file empty") {
    const auto path = TempPath("truncate.jsonl");
    {
        std::ofstream out(path, std::ios::binary);
        out << "old content\n";
    }
    auto opened = Logging::JsonLinesFileSink::Open({.path = path.string(), .truncate = true});
    REQUIRE(opened);
    opened.value()->Write(At(1, "new"));
    opened.value()->Close();
    const std::string all = ReadAll(path);
    CHECK(all.find("old content") == std::string::npos);
    CHECK(all.find("\"text\":\"new\"") != std::string::npos);
}

TEST_CASE("file: a file that cannot be opened fails Install with OpenFailed and closes the sinks before it") {
    struct ClosingSink final : Logging::Sink {
        bool* closed;
        explicit ClosingSink(bool* c) : closed(c) {}
        void Write(const Record&) noexcept override {}
        void Close() noexcept override { *closed = true; }
    };
    bool closed = false;
    const auto missing = TempPath("no-such-dir") / "x" / "log.jsonl";
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "first", .sink = std::make_shared<ClosingSink>(&closed), .filter = {}});
    options.sinks.push_back(
        {.name = "file", .sink = Logging::JsonLinesFileOptions{.path = missing.string()}, .filter = {}});
    auto handle = Logging::Install(std::move(options));
    REQUIRE_FALSE(handle);
    CHECK(handle.error().code == Logging::LoggingErrorCode::OpenFailed);
    CHECK(closed);
    // Nothing stayed installed.
    CHECK(Logging::Enabled(Level::Trace, "anything"));
    auto again = Logging::Install({});
    CHECK(again);
}

TEST_CASE("file: lines taken after a failed write are still counted as written (judgement 3)") {
    Injected file;
    file.output->Fail();
    for (int i = 0; i < 3; ++i) file.sink->Write(At(i, "fails"));
    file.sink->Close();
    const auto counts = file.sink->Counts();
    CHECK_MESSAGE(counts.written == 3, "LG-6: taken lines are counted written, failed or not");
    CHECK(counts.droppedFull == 0);
    CHECK(counts.droppedClosed == 0);
}

TEST_CASE("file: a fatal line the fatal output refuses is reported false and not counted in fatal_direct") {
    Injected file;
    file.fatal->Fail();
    CHECK_FALSE(file.sink->WriteFatal(Record{.level = Level::Fatal}, "{}\n", "x\n"));
    file.sink->Close();
    CHECK_MESSAGE(file.sink->Counts().fatalDirect == 0, "judgement 3: fatal_direct counts lines handed on");
}

TEST_CASE("file: one batch is handed on in one write, and a large one in 64 KiB steps (LG-6)") {
    Injected file;
    file.output->Hold();
    file.sink->Write(At(0, "first"));
    REQUIRE(file.output->WaitForWriteStarted(1, 5s));
    for (int i = 1; i <= 50; ++i) file.sink->Write(At(i, "small"));
    file.output->Release();
    REQUIRE(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    CHECK_MESSAGE(file.output->Writes() == 2, "LG-6: the 50 small lines queued meanwhile went in one write");
    file.output->Hold();
    file.sink->Write(At(100, "second"));
    REQUIRE(file.output->WaitForWriteStarted(3, 5s));
    const std::string big(4000, 'b');
    for (int i = 0; i < 50; ++i) file.sink->Write(At(200 + i, big));
    file.output->Release();
    REQUIRE(file.sink->Flush(std::chrono::steady_clock::now() + 5s));
    // About 200 KB in one batch: at least three writes of up to 64 KiB, and far fewer than one per line.
    const auto writes = file.output->Writes() - 3;
    CHECK_MESSAGE(writes >= 3, "LG-6: a large batch goes in 64 KiB steps");
    CHECK(writes <= 6);
    file.sink->Close();
}
