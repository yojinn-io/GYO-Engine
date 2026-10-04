#include "doctest/doctest.h"

#include "engine/base/Fnv1a.hpp"

#include <string_view>

using Engine::Base::Fnv1a64;

// Published FNV-1a 64-bit vectors plus single bytes at both ends of the range
// and a non-ASCII (UTF-8) string, so signed char platforms are covered.
static_assert(Fnv1a64("") == 0xcbf29ce484222325ULL);
static_assert(Fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
static_assert(Fnv1a64("foobar") == 0x85944171f73967e8ULL);

TEST_CASE("Fnv1a64 matches the 64-bit FNV-1a reference vectors") {
    CHECK(Fnv1a64("") == 0xcbf29ce484222325ULL);
    CHECK(Fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(Fnv1a64("foobar") == 0x85944171f73967e8ULL);
    CHECK(Fnv1a64(std::string_view("\0", 1)) == 0xaf63bd4c8601b7dfULL);
    CHECK(Fnv1a64("\xff") == 0xaf64724c8602eb6eULL);
    CHECK(Fnv1a64("\xc3\xa9") == 0x0ac21707b7181e01ULL);
}
