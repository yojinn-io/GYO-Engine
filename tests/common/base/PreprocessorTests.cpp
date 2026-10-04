#include "doctest/doctest.h"

#include "engine/base/Assert.hpp"

// Proves that the standard-conforming preprocessor is active, not merely that
// nothing broke: each check below compiles only under it. MSVC's traditional
// preprocessor passes a forwarded __VA_ARGS__ to the inner macro as a single
// argument and does not know __VA_OPT__.

#define GYO_TEST_PICK_FOURTH(a, b, c, d, ...) d
#define GYO_TEST_COUNT_UP_TO_THREE(...) GYO_TEST_PICK_FOURTH(__VA_ARGS__, 3, 2, 1, 0)
#define GYO_TEST_HAS_ARGUMENTS(...) (0 __VA_OPT__(+ 1))

static_assert(GYO_TEST_COUNT_UP_TO_THREE(a) == 1);
static_assert(GYO_TEST_COUNT_UP_TO_THREE(a, b) == 2);
static_assert(GYO_TEST_COUNT_UP_TO_THREE(a, b, c) == 3);
static_assert(GYO_TEST_HAS_ARGUMENTS() == 0);
static_assert(GYO_TEST_HAS_ARGUMENTS(x) == 1);
static_assert(GYO_TEST_HAS_ARGUMENTS(x, y) == 1);

#if defined(_MSC_VER) && !defined(__clang__)
static_assert(_MSVC_TRADITIONAL == 0, "MSVC must use /Zc:preprocessor");
#endif

TEST_CASE("The standard-conforming preprocessor is active") {
    CHECK(GYO_TEST_COUNT_UP_TO_THREE(a, b, c) == 3);
    CHECK(GYO_TEST_HAS_ARGUMENTS() == 0);
}
