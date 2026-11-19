// The Writer allocates nothing when the string has the capacity and the depth
// stays within its inline levels (Design Invariant J-5), and an allocation
// failure is OutOfMemory from Finish, not an exception. This file replaces the
// global operator new and delete for the whole test executable: they count,
// and fail on request, on the calling thread only.
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>

#include "engine/json/Writer.hpp"

namespace {

thread_local std::size_t allocations = 0;
thread_local bool failNext = false;

void* Allocate(const std::size_t size) {
    if (failNext) {
        failNext = false;
        throw std::bad_alloc();
    }
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

using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Writer;

namespace {

// A trace-like record of about 420 bytes, nested kInlineDepth levels at its deepest.
void WriteRecord(Writer& writer) {
    writer.BeginObject();
    writer.Key("schema_version");
    writer.Uint(3);
    writer.Key("kind");
    writer.String("input_applied");
    for (int i = 0; i < 12; ++i) {
        writer.Key("field_with_a_longer_name");
        writer.Double(1234.5678 + i);
    }
    writer.Key("note");
    writer.String("escaped \"quote\" and a\ttab");
    writer.Key("deep");
    for (std::size_t i = 1; i < Writer::kInlineDepth; ++i) writer.BeginArray();
    writer.Int(-7);
    for (std::size_t i = 1; i < Writer::kInlineDepth; ++i) writer.EndArray();
    writer.EndObject();
}

} // namespace

TEST_CASE("Writer: no allocation when the string has the capacity and the depth is inline") {
    std::string line;
    line.reserve(2048);
    const char* data = line.data();
    allocations = 0;
    {
        Writer writer(line);
        WriteRecord(writer);
        CHECK(writer.Finish().has_value());
    }
    CHECK(allocations == 0);
    CHECK(line.data() == data);
    CHECK(line.size() > 400);

    // The same string, cleared, serves the next line without allocating.
    line.clear();
    allocations = 0;
    {
        Writer writer(line);
        WriteRecord(writer);
        CHECK(writer.Finish().has_value());
    }
    CHECK(allocations == 0);
}

TEST_CASE("Writer: an allocation failure is OutOfMemory and the string is cut back") {
    std::string line = "kept";
    line.shrink_to_fit();
    Writer writer(line);
    writer.BeginObject();
    writer.Key("a");
    const std::string big(4096, 'x');
    failNext = true;  // the next allocation, the string's growth, fails
    writer.String(big);
    failNext = false;
    writer.EndObject();
    auto done = writer.Finish();
    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == JsonErrorCode::OutOfMemory);
    CHECK(line == "kept");
}
