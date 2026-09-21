#include "doctest/doctest.h"

#include "engine/io/stream/BufferedStream.hpp"
#include "engine/io/stream/MemoryStream.hpp"

#include <limits>

using namespace Engine::IO::Stream;

TEST_CASE("BufferedStream: seeks use the logical position after read ahead") {
    auto inner = std::make_unique<MemoryStream>();
    REQUIRE(inner->Write("abcdefgh", 8));
    REQUIRE(inner->Seek(0, SeekWhence::Begin));
    BufferingOptions options;
    options.readBufferSize = 4;
    BufferedStream stream(std::move(inner), options);
    char value = 0;
    REQUIRE(stream.Read(&value, 1));
    CHECK(value == 'a');
    REQUIRE(stream.Tell());
    CHECK(stream.Tell().value() == 1);

    SUBCASE("current zero") {
        const auto seek = stream.Seek(0, SeekWhence::Current);
        REQUIRE(seek);
        CHECK(seek.value() == 1);
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'b');
    }
    SUBCASE("current forward and backward") {
        const auto forward = stream.Seek(2, SeekWhence::Current);
        REQUIRE(forward);
        CHECK(forward.value() == 3);
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'd');
        const auto backward = stream.Seek(-2, SeekWhence::Current);
        REQUIRE(backward);
        CHECK(backward.value() == 2);
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'c');
    }
    SUBCASE("begin and end") {
        REQUIRE(stream.Seek(2, SeekWhence::Begin));
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'c');
        REQUIRE(stream.Seek(-1, SeekWhence::End));
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'h');
    }
    SUBCASE("failed seek preserves buffered bytes") {
        CHECK_FALSE(stream.Seek(-10, SeekWhence::Current));
        CHECK(stream.Tell().value() == 1);
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'b');
    }
    SUBCASE("offset underflow preserves buffered bytes") {
        CHECK_FALSE(stream.Seek((std::numeric_limits<std::int64_t>::min)(), SeekWhence::Current));
        CHECK(stream.Tell().value() == 1);
        REQUIRE(stream.Read(&value, 1));
        CHECK(value == 'b');
    }
}

TEST_CASE("BufferedStream: seek flushes writes before resolving the new position") {
    BufferingOptions options;
    options.writeBufferSize = 16;
    BufferedStream stream(std::make_unique<MemoryStream>(), options);
    REQUIRE(stream.Write("abc", 3));
    CHECK(stream.Tell().value() == 3);
    const auto seek = stream.Seek(-2, SeekWhence::Current);
    REQUIRE(seek);
    CHECK(seek.value() == 1);
    char value = 0;
    REQUIRE(stream.Read(&value, 1));
    CHECK(value == 'b');
}
