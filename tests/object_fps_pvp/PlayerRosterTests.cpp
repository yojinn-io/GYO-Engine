#include <doctest/doctest.h>

#include "object_fps_pvp/match/PlayerRoster.hpp"

#include <fstream>
#include <map>
#include <string>

namespace PM = object_fps_pvp::match;

TEST_CASE("roster: the matches service's line gives each user's name") {
    std::string error;
    const auto names = PM::ParsePlayerNames(R"({"players":[{"userId":7,"name":"甲 A, \"q\""},{"userId":9,"name":""}]})", error);
    REQUIRE_MESSAGE(names.has_value(), error);
    CHECK(names->size() == 2);
    CHECK(names->at(7) == "甲 A, \"q\"");
    CHECK(names->at(9).empty());
    const auto none = PM::ParsePlayerNames(R"({"players":[]})", error);
    REQUIRE_MESSAGE(none.has_value(), error);
    CHECK(none->empty());
    // How long a name may be is the World's (it cuts one the wire does not carry): the roster takes any.
    const auto longName = PM::ParsePlayerNames(R"({"players":[{"userId":7,"name":")" + std::string(300, 'x') + R"("}]})", error);
    REQUIRE_MESSAGE(longName.has_value(), error);
    CHECK(longName->at(7).size() == 300);
}

TEST_CASE("roster: the line the matches service writes for a known roster (the vector its tests share) reads back") {
    std::ifstream file(PVP_ROSTER_VECTOR, std::ios::binary);
    REQUIRE_MESSAGE(file.good(), PVP_ROSTER_VECTOR);
    std::string line;
    std::getline(file, line);
    std::string error;
    const auto names = PM::ParsePlayerNames(line, error);
    REQUIRE_MESSAGE(names.has_value(), error);
    CHECK(*names == std::map<std::uint64_t, std::string>{{7, "甲 A, \"q\""}, {9, ""}});
}

TEST_CASE("roster: a line that is not the players' names is refused, saying what was wrong") {
    const auto refused = [](const std::string& line) {
        std::string error;
        const bool parsed = PM::ParsePlayerNames(line, error).has_value();
        INFO(line << " -> " << error);
        CHECK_FALSE(parsed);
        CHECK_FALSE(error.empty());
    };
    refused("");
    refused("7,甲");
    refused(R"({"names":[]})");
    refused(R"({"players":{}})");
    refused(R"({"players":[{"userId":"7","name":"a"}]})");
    refused(R"({"players":[{"userId":0,"name":"a"}]})");
    refused(R"({"players":[{"userId":-7,"name":"a"}]})");
    refused(R"({"players":[{"userId":7}]})");
    refused(R"({"players":[{"userId":7,"name":7}]})");
    refused(R"({"players":[{"userId":7,"name":"a"},{"userId":7,"name":"b"}]})");
}
