#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/base/Log.hpp"
#include "engine/trace/TraceSink.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace Engine::Trace;
using namespace std::chrono_literals;

namespace {
std::filesystem::path TempPath(const std::string& name) {
    const auto directory = std::filesystem::temp_directory_path() / "gyo_trace_tests";
    std::filesystem::create_directories(directory);
    const auto path = directory / (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove(path);
    return path;
}

// The sink writes in text mode, like the writers it replaced: a newline is
// "\r\n" on Windows.
#if defined(_WIN32)
constexpr std::string_view Newline = "\r\n";
#else
constexpr std::string_view Newline = "\n";
#endif

std::vector<std::string> ReadLines(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return lines;
}

TraceSink OpenSink(const std::filesystem::path& path, std::size_t capacity = 8192, bool append = false) {
    auto sink = TraceSink::Open({.path = path, .append = append, .capacity = capacity, .threadName = "gyo-test-trace"});
    REQUIRE(sink.has_value());
    return std::move(*sink);
}

std::string EndLine(const TraceCounts& counts) {
    return "end written=" + std::to_string(counts.written) + " dropped=" + std::to_string(counts.Dropped());
}

// Each producer writes "p<producer> <index>"; returns the indices per producer in file order.
std::vector<std::vector<std::uint64_t>> ByProducer(const std::vector<std::string>& lines, std::size_t producers) {
    std::vector<std::vector<std::uint64_t>> result(producers);
    for (const auto& line : lines) {
        if (line.rfind("p", 0) != 0) continue;
        std::istringstream in(line.substr(1));
        std::size_t producer{};
        std::uint64_t index{};
        in >> producer >> index;
        REQUIRE(producer < producers);
        result[producer].push_back(index);
    }
    return result;
}
} // namespace

TEST_CASE("Trace writes every accepted line and then the end line last") {
    const auto path = TempPath("order");
    auto sink = OpenSink(path);
    for (int i = 0; i < 100; ++i) CHECK(sink.Push("line " + std::to_string(i)));
    TraceCounts seen;
    const auto counts = sink.Close([&](const TraceCounts& c) { seen = c; return EndLine(c); });
    CHECK(counts.written == 100);
    CHECK(counts.Dropped() == 0);
    CHECK(seen.written == counts.written);
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == 101);
    for (int i = 0; i < 100; ++i) CHECK(lines[static_cast<std::size_t>(i)] == "line " + std::to_string(i));
    CHECK(lines.back() == "end written=100 dropped=0");
    CHECK(sink.Good());
}

TEST_CASE("Trace counts every refused line exactly with eight producers and a small queue") {
    const auto path = TempPath("producers");
    auto sink = OpenSink(path, 1);
    constexpr std::size_t Producers = 8;
    constexpr std::uint64_t PerProducer = 5000;
    std::atomic<std::uint64_t> refused{0};
    std::vector<std::thread> threads;
    for (std::size_t p = 0; p < Producers; ++p)
        threads.emplace_back([&sink, &refused, p] {
            for (std::uint64_t i = 0; i < PerProducer; ++i)
                if (!sink.Push("p" + std::to_string(p) + " " + std::to_string(i))) refused.fetch_add(1);
        });
    for (auto& thread : threads) thread.join();
    const auto counts = sink.Close(EndLine);
    // A one-line queue against eight producers refuses lines; each is counted once.
    CHECK(counts.droppedFull > 0);
    CHECK(counts.droppedClosed == 0);
    CHECK(counts.droppedFull == refused.load());
    CHECK(counts.written + counts.Dropped() == Producers * PerProducer);
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == counts.written + 1);
    CHECK(lines.back() == EndLine(counts));
    // One producer's lines keep their Push order.
    for (const auto& indices : ByProducer(lines, Producers))
        for (std::size_t k = 1; k < indices.size(); ++k) CHECK(indices[k - 1] < indices[k]);
}

TEST_CASE("Trace producers racing Close keep the count and the end line last") {
    const auto path = TempPath("close-race");
    auto sink = OpenSink(path, 64);
    constexpr std::size_t Producers = 8;
    std::atomic<bool> go{false};
    std::atomic<std::uint64_t> pushed{0};
    std::vector<std::thread> threads;
    for (std::size_t p = 0; p < Producers; ++p)
        threads.emplace_back([&, p] {
            while (!go.load()) std::this_thread::yield();
            for (std::uint64_t i = 0; i < 20000; ++i) {
                static_cast<void>(sink.Push("p" + std::to_string(p) + " " + std::to_string(i)));
                pushed.fetch_add(1);
            }
        });
    go.store(true);
    while (pushed.load() < 1000) std::this_thread::yield();
    const auto counts = sink.Close(EndLine);
    for (auto& thread : threads) thread.join();
    const auto lines = ReadLines(path);
    REQUIRE(!lines.empty());
    CHECK(lines.back() == EndLine(counts));
    CHECK(lines.size() == counts.written + 1);
    const auto final = sink.Counts();
    CHECK(final.written == counts.written);
    CHECK(final.droppedFull == counts.droppedFull);
    CHECK(final.written + final.Dropped() == pushed.load());
}

TEST_CASE("Trace Close writes the lines still waiting behind a busy writer before the end line") {
    const auto path = TempPath("close-busy");
    auto sink = OpenSink(path);
    // The writer is busy with the first 64 MB line when Close starts (the pause
    // lets it take that line alone), so the second one and the short lines are
    // taken only after the stop is seen.
    std::string first(std::size_t{64} << 20, 'b');
    std::string second(std::size_t{64} << 20, 'c');
    REQUIRE(sink.Push(std::move(first)));
    std::this_thread::sleep_for(10ms);
    REQUIRE(sink.Push(std::move(second)));
    for (int i = 0; i < 5; ++i) REQUIRE(sink.Push("waiting " + std::to_string(i)));
    const auto counts = sink.Close(EndLine);
    CHECK(counts.written == 7);
    std::ifstream input(path);
    std::string line;
    std::vector<std::string> tail;
    while (std::getline(input, line)) if (line.size() < 64) tail.push_back(line);
    REQUIRE(tail.size() == 6);
    for (int i = 0; i < 5; ++i) CHECK(tail[static_cast<std::size_t>(i)] == "waiting " + std::to_string(i));
    CHECK(tail.back() == "end written=7 dropped=0");
    input.close();
    std::filesystem::remove(path);
}

TEST_CASE("Trace refuses and counts a line after Close, and Close is idempotent") {
    const auto path = TempPath("closed");
    auto sink = OpenSink(path);
    CHECK(sink.Push("before"));
    const auto first = sink.Close(EndLine);
    CHECK_FALSE(sink.Push("after"));
    CHECK(sink.Counts().droppedClosed == 1);
    const auto second = sink.Close(EndLine);
    CHECK(second.written == first.written);
    CHECK(second.droppedClosed == first.droppedClosed);
    CHECK_FALSE(sink.FlushNow());
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "before");
    CHECK(lines[1] == "end written=1 dropped=0");
}

TEST_CASE("Trace a pushed line wakes the writer without any flush request") {
    const auto path = TempPath("wake");
    auto sink = OpenSink(path);
    CHECK(sink.Push("woken"));
    // Structural bound only: the line must arrive; it never does without a wake.
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    bool seen = false;
    while (!seen && std::chrono::steady_clock::now() < deadline) {
        const auto lines = ReadLines(path);
        seen = !lines.empty() && lines.front() == "woken";
        if (!seen) std::this_thread::sleep_for(1ms);
    }
    CHECK(seen);
    // A second burst, after the writer has been waiting for a while, wakes it again.
    std::this_thread::sleep_for(20ms);
    CHECK(sink.Push("again"));
    seen = false;
    while (!seen && std::chrono::steady_clock::now() < deadline) {
        const auto lines = ReadLines(path);
        seen = lines.size() == 2 && lines.back() == "again";
        if (!seen) std::this_thread::sleep_for(1ms);
    }
    CHECK(seen);
}

TEST_CASE("Trace FlushNow returns with every earlier line in the file") {
    const auto path = TempPath("flush");
    auto sink = OpenSink(path);
    // A 64 MB line keeps the writer busy, so the marker after it is not in the
    // file yet unless FlushNow waits for it.
    const std::string busy(std::size_t{64} << 20, 'b');
    REQUIRE(sink.Push(busy));
    REQUIRE(sink.Push("marker"));
    CHECK(sink.FlushNow(60s));
    const auto size = std::filesystem::file_size(path);
    const std::string marker = "marker" + std::string(Newline);
    CHECK(size == busy.size() + Newline.size() + marker.size());
    std::ifstream input(path, std::ios::binary);
    input.seekg(-static_cast<std::streamoff>(marker.size()), std::ios::end);
    std::string tail(marker.size(), '\0');
    input.read(tail.data(), static_cast<std::streamsize>(tail.size()));
    CHECK(tail == marker);
    input.close();
    static_cast<void>(sink.Close());
    std::filesystem::remove(path);
}

TEST_CASE("Trace appends to an existing file when asked") {
    const auto path = TempPath("append");
    { std::ofstream(path) << "kept\n"; }
    auto sink = OpenSink(path, 16, true);
    CHECK(sink.Push("added"));
    static_cast<void>(sink.Close());
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "kept");
    CHECK(lines[1] == "added");
}

TEST_CASE("Trace reports a file it cannot open") {
    const auto path = TempPath("missing-directory") / "nested" / "trace.jsonl";
    const auto sink = TraceSink::Open({.path = path});
    REQUIRE_FALSE(sink.has_value());
    CHECK(sink.error().code == TraceErrorCode::OpenFailed);
}

#if defined(__linux__)
TEST_CASE("Trace a failed write makes the sink not good") {
    auto sink = OpenSink("/dev/full", 16, true);
    CHECK(sink.Push(std::string(1 << 16, 'x')));
    static_cast<void>(sink.Close());
    CHECK_FALSE(sink.Good());
}
#endif

namespace {
std::atomic<std::uint64_t> firstHandlerLines{0};
std::atomic<std::uint64_t> secondHandlerLines{0};
void FirstHandler(const Engine::Base::LogMessage&) noexcept { firstHandlerLines.fetch_add(1); }
void SecondHandler(const Engine::Base::LogMessage& message) noexcept {
    if (message.category == "test" && message.text == "x") secondHandlerLines.fetch_add(1);
}
} // namespace

TEST_CASE("Log handlers can be replaced while other threads log") {
    using namespace Engine::Base;
    const auto previous = SetLogHandler(&FirstHandler);
    constexpr int Threads = 4;
    constexpr int PerThread = 20000;
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t)
        threads.emplace_back([] { for (int i = 0; i < PerThread; ++i) Log(LogLevel::Info, "test", "x"); });
    for (int k = 0; k < 1000; ++k) SetLogHandler(k % 2 ? &FirstHandler : &SecondHandler);
    for (auto& thread : threads) thread.join();
    SetLogHandler(previous);
    // Every line reached exactly one of the installed handlers.
    CHECK(firstHandlerLines.load() + secondHandlerLines.load() == static_cast<std::uint64_t>(Threads * PerThread));
}

TEST_CASE("Log without a handler writes one line to std::cerr") {
    using namespace Engine::Base;
    const auto previous = SetLogHandler(nullptr);
    std::ostringstream captured;
    auto* original = std::cerr.rdbuf(captured.rdbuf());
    Log(LogLevel::Warning, "SDL_GPU", "slow swapchain acquire");
    std::cerr.rdbuf(original);
    SetLogHandler(previous);
    CHECK(captured.str() == "[SDL_GPU] warning: slow swapchain acquire\n");
}
