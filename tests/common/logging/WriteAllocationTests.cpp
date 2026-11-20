// Design Invariant LG-7: a record that no sink takes is not composed: entrance
// ④ allocates nothing and reads no clock. This file replaces the global
// operator new and delete for the whole test executable; they count on the
// calling thread only. They can also refuse large allocations on every thread
// but the test's own (review S4: the file sink's writer thread under memory pressure).
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "TestClock.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/Write.hpp"

namespace {

thread_local std::size_t allocations = 0;
// When set, an allocation of at least this many bytes on another thread than refuseExcept throws.
std::atomic<std::size_t> refuseFrom{0};
std::atomic<std::thread::id> refuseExcept{};

void* Allocate(const std::size_t size) {
    ++allocations;
    if (const std::size_t from = refuseFrom.load(std::memory_order_relaxed);
        from != 0 && size >= from && std::this_thread::get_id() != refuseExcept.load(std::memory_order_relaxed)) {
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

} // namespace

void* operator new(const std::size_t size) { return Allocate(size); }
void* operator new[](const std::size_t size) { return Allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace Logging = GYO::Engine::Logging;
using Logging::Level;

TEST_CASE("alloc: a refused record allocates nothing and reads no clock; a taken one reads both clocks") {
    auto memory = std::make_shared<Logging::MemorySink>();
    Logging::LoggerOptions options;
    options.sinks.push_back(
        {.name = "memory", .sink = memory, .filter = {.exclude = {"trace."}, .minimum = Level::Warning}});
    options.clock = Logging::Test::TestClock();
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    Logging::Test::ResetClock();

    const std::size_t before = allocations;
    Logging::Write(Level::Debug, "net", "below the level", {{"bytes", 512}, {"peer", "client-3"}, {"rate", 0.25}});
    Logging::Write(Level::Error, "trace.net", "excluded category", {{"bytes", 512}});
    CHECK(allocations == before);
    CHECK(Logging::Test::clockReads.load() == 0);

    Logging::Write(Level::Warning, "net", "taken");
    CHECK(Logging::Test::clockReads.load() == 2);
    CHECK(memory->Records().size() == 1);
}

#include "engine/logging/WriteFatalRecord.hpp"

TEST_CASE("alloc: the fatal path formats its lines in the capacity Install reserved: nothing is allocated (LG-2)") {
    struct CountingSink final : Logging::Sink {
        int fatal = 0;
        void Write(const Logging::Record&) noexcept override {}
        bool WriteFatal(const Logging::Record&, std::string_view, std::string_view) noexcept override {
            ++fatal;
            return true;
        }
    };
    auto counting = std::make_shared<CountingSink>();
    Logging::LoggerOptions options;
    options.sinks.push_back({.name = "counting", .sink = counting, .filter = {}});
    options.clock = Logging::Test::TestClock();
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = Logging::Install(std::move(options));
    REQUIRE(handle);
    const Logging::Field fields[] = {{"file", "Simulation.cpp"}, {"line", 3u}, {"function", "Step"}, {"x", 0.5}};
    const Logging::Record record{.level = Level::Fatal, .category = "assert", .text = "a condition failed",
                                 .world = "Simulation", .fields = fields};
    const std::size_t before = allocations;
    const auto report = Logging::WriteFatalRecord(record);
    CHECK(allocations == before);
    CHECK(report.sinksWritten == 1);
    CHECK(counting->fatal == 1);
}

TEST_CASE("alloc: the file sink's writer thread out of memory still writes the line and does not end the process") {
    // An output that keeps no copy (a copy would itself be refused on the writer thread).
    struct MeasuringOutput final : Logging::LineOutput {
        std::atomic<std::size_t> bytes{0};
        std::atomic<bool> sawBig{false};
        bool Write(const std::string_view line) noexcept override {
            bytes.fetch_add(line.size());
            if (line.find(std::string_view("xxxxxxxxxxxxxxxx")) != std::string_view::npos && line.size() >= (2u << 20)) sawBig.store(true);
            return true;
        }
        bool Close() noexcept override { return true; }
    };
    auto output = std::make_shared<MeasuringOutput>();
    auto fatal = std::make_shared<MeasuringOutput>();
    auto opened = Logging::JsonLinesFileSink::Open(
        {.monoNs = &Logging::Test::TestMonoNs, .output = output, .fatalOutput = fatal});
    REQUIRE(opened);
    auto sink = opened.value();
    const std::string big(2u << 20, 'x');
    refuseExcept.store(std::this_thread::get_id());
    refuseFrom.store(1u << 20);
    // The line is formatted here (this thread may allocate); the writer must not copy it into its buffer.
    sink->Write(Logging::Record{.level = Level::Info, .category = "c", .text = big});
    const bool flushed = sink->Flush(std::chrono::steady_clock::now() + std::chrono::seconds{10});
    refuseFrom.store(0);
    CHECK(flushed);
    sink->Close();
    CHECK_MESSAGE(output->sawBig.load(), "S4: the whole line reached the output in one write");
    CHECK(sink->Counts().written == 1);
}
