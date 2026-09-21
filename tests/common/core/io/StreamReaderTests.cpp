#include "doctest/doctest.h"

#include "engine/io/stream/MemoryStream.hpp"
#include "engine/io/stream/StreamReader.hpp"

using namespace Engine::IO::Stream;

TEST_CASE("StreamReader: text and binary methods share unread bytes") {
    MemoryStream stream;
    const std::string content = "header\r\nABCDtail\n";
    REQUIRE(stream.Write(content.data(), content.size()));
    REQUIRE(stream.Seek(0, SeekWhence::Begin));
    StreamReader reader(stream);
    std::string line;
    REQUIRE(reader.ReadLine(line));
    CHECK(line == "header");
    SUBCASE("exact and line") {
        char body[4]{};
        REQUIRE(reader.ReadExactly(body, 4));
        CHECK(std::string(body, 4) == "ABCD");
        REQUIRE(reader.ReadLine(line));
        CHECK(line == "tail");
    }
    SUBCASE("integer and text") {
        const auto value = reader.ReadU32LE();
        REQUIRE(value);
        CHECK(value.value() == 0x44434241u);
        const auto text = reader.ReadAllText();
        REQUIRE(text);
        CHECK(text.value() == "tail\n");
    }
    SUBCASE("all bytes") {
        const auto bytes = reader.ReadAllBytes();
        REQUIRE(bytes);
        CHECK(std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()) == "ABCDtail\n");
    }
    SUBCASE("maximum includes prefetched bytes") {
        CHECK_FALSE(reader.ReadAllBytes(2));
    }
}

TEST_CASE("StreamReader: mixed reads cross the line buffer boundary and preserve EOF") {
    MemoryStream stream;
    const std::string payload(5000, 'x');
    const std::string content = "head\n" + payload + "\nlast";
    REQUIRE(stream.Write(content.data(), content.size()));
    REQUIRE(stream.Seek(0, SeekWhence::Begin));
    StreamReader reader(stream);
    char prefix[2]{};
    REQUIRE(reader.ReadExactly(prefix, 2));
    CHECK(std::string(prefix, 2) == "he");
    std::string line;
    REQUIRE(reader.ReadLine(line));
    CHECK(line == "ad");
    std::string body(payload.size(), '\0');
    REQUIRE(reader.ReadExactly(body.data(), body.size()));
    CHECK(body == payload);
    auto empty = reader.ReadLine(line);
    REQUIRE(empty);
    CHECK(empty.value());
    CHECK(line.empty());
    REQUIRE(reader.ReadLine(line));
    CHECK(line == "last");
    const auto eof = reader.ReadLine(line);
    REQUIRE(eof);
    CHECK_FALSE(eof.value());
    CHECK_FALSE(reader.ReadU8());
}
