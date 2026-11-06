#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/map_markers/MapMarkers.hpp"

#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>

// Judgment G1 of the v8-arch-big-map-engine-format declaration
// (docs/architecture/plans/big-map/engine-format.zh-Hant.md, section 4b, B2):
// rejecting a deeply nested document costs memory linear in its depth.
// This binary replaces the global allocation functions to count the bytes the
// calling thread asks for while counting is on. It is its own executable
// because the replacement applies to the whole program.

namespace {

thread_local bool tCounting = false;
thread_local std::uint64_t tBytes = 0;

void* Allocate(const std::size_t size) {
    if (tCounting) tBytes += size;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}

void Free(void* p) noexcept { std::free(p); }

// The valid head of a document plus a field x that holds `depth` nested
// containers, so the rules reject it as an unknown field at the root.
std::string Nested(const std::size_t depth, const bool arrays) {
    std::string text = R"({"schema": "gyo.map_markers", "version": 1, "markers": [], "x": )";
    for (std::size_t i = 0; i < depth; ++i) text += arrays ? "[" : "{\"a\": ";
    if (!arrays) text += "{}";
    for (std::size_t i = 0; i < depth; ++i) text += arrays ? "]" : "}";
    return text + "}";
}

// Bytes asked for while parsing; the document must be rejected as above.
std::uint64_t BytesToReject(const std::string& text) {
    tBytes = 0;
    tCounting = true;
    const auto parsed = Engine::MapMarkers::ParseMapMarkers(text);
    tCounting = false;
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message.find("unknown field 'x'") != std::string::npos);
    CHECK(parsed.error().detail.empty());
    return tBytes;
}

void CheckLinear(const bool arrays) {
    const auto shallow = BytesToReject(Nested(2000, arrays));
    const auto deep = BytesToReject(Nested(4000, arrays));
    INFO("bytes at depth 2000: " << shallow << ", at depth 4000: " << deep);
    REQUIRE(shallow > 0);
    // Linear growth doubles the bytes; quadratic growth quadruples them.
    CHECK(static_cast<double>(deep) / static_cast<double>(shallow) < 3.0);
}

} // namespace

void* operator new(const std::size_t size) { return Allocate(size); }
void* operator new[](const std::size_t size) { return Allocate(size); }
void operator delete(void* p) noexcept { Free(p); }
void operator delete[](void* p) noexcept { Free(p); }
void operator delete(void* p, std::size_t) noexcept { Free(p); }
void operator delete[](void* p, std::size_t) noexcept { Free(p); }

TEST_CASE("map markers: nested arrays are rejected in memory linear in depth") { CheckLinear(true); }

TEST_CASE("map markers: nested objects are rejected in memory linear in depth") { CheckLinear(false); }
