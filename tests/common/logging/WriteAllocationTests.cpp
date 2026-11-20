// Design Invariant LG-7: a record that no sink takes is not composed: entrance
// ④ allocates nothing and reads no clock. This file replaces the global
// operator new and delete for the whole test executable; they count on the
// calling thread only.
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>

#include "TestClock.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/Write.hpp"

namespace {

thread_local std::size_t allocations = 0;

void* Allocate(const std::size_t size) {
    ++allocations;
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
    const Logging::Field fields[] = {{"kind", "Interrupted"}, {"entry", "RunFrame"}, {"sync_epoch", 3u}, {"x", 0.5}};
    const Logging::Record record{.level = Level::Fatal, .category = "world", .text = "a World broke",
                                 .product = "game", .world = "Simulation", .fields = fields};
    const std::size_t before = allocations;
    const auto report = Logging::WriteFatalRecord(record);
    CHECK(allocations == before);
    CHECK(report.sinksWritten == 1);
    CHECK(counting->fatal == 1);
}
