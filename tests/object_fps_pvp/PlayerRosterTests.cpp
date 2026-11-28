#include <doctest/doctest.h>

#include "object_fps_pvp/match/PlayerRoster.hpp"

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
    const auto longest = PM::ParsePlayerNames(R"({"players":[{"userId":7,"name":")" + std::string(PM::kMaxPlayerNameBytes, 'x') + R"("}]})", error);
    REQUIRE_MESSAGE(longest.has_value(), error);
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
    refused(R"({"players":[{"userId":7,"name":")" + std::string(PM::kMaxPlayerNameBytes + 1, 'x') + R"("}]})");
}
