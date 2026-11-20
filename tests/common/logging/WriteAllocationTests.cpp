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
