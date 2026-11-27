// The Match's --load-threads (big-map declaration 17.8): without it the machine decides, a count in the range is
// used as given, one outside it is ignored for the machine's choice.

#include <doctest/doctest.h>

#include "object_fps_pvp/match/LoadThreads.hpp"

#include <optional>
#include <ostream> // doctest's CAPTURE of a string_view (MSVC needs the stream declared)
#include <string_view>

namespace M = object_fps_pvp::match;

TEST_CASE("load threads: without --load-threads the machine's hardware threads, between 2 and 8") {
    const std::optional<std::string_view> none;
    CHECK(M::ChooseLoadThreads(none, 0)->count == 2); // not known
    CHECK(M::ChooseLoadThreads(none, 1)->count == 2);
    CHECK(M::ChooseLoadThreads(none, 4)->count == 4);
    CHECK(M::ChooseLoadThreads(none, 8)->count == 8);
    CHECK(M::ChooseLoadThreads(none, 16)->count == 8);
    CHECK_FALSE(M::ChooseLoadThreads(none, 16)->askedOutOfRange);
}

TEST_CASE("load threads: a count from 1 to 8 is used as given, whatever the machine has") {
    for (const unsigned hardware : {0U, 2U, 16U}) {
        CHECK(M::ChooseLoadThreads("1", hardware)->count == 1);
        CHECK(M::ChooseLoadThreads("3", hardware)->count == 3);
        CHECK(M::ChooseLoadThreads("8", hardware)->count == 8);
        CHECK_FALSE(M::ChooseLoadThreads("3", hardware)->askedOutOfRange);
    }
}

TEST_CASE("load threads: a count outside 1 to 8 is not clamped: the machine decides, and it says so") {
    for (const std::string_view asked : {"-1", "0", "9", "100", "99999999999999999999"}) {
        CAPTURE(asked);
        const auto chosen = M::ChooseLoadThreads(asked, 4);
        REQUIRE(chosen);
        CHECK(chosen->count == 4);
        CHECK(chosen->askedOutOfRange);
    }
    // Not the nearest end of the range: 100 on a 16-thread machine is 8 because the machine has 16, and on a
    // 4-thread one it is 4.
    CHECK(M::ChooseLoadThreads("100", 16)->count == 8);
    CHECK(M::ChooseLoadThreads("-1", 16)->count == 8);
}

TEST_CASE("load threads: text that is not a whole number is refused") {
    for (const std::string_view asked : {"", "abc", "4x", "2.5", " 4"}) {
        CAPTURE(asked);
        CHECK_FALSE(M::ChooseLoadThreads(asked, 4).has_value());
    }
}
