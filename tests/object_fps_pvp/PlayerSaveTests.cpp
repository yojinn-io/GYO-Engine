// The player's save format (gateway declaration D7): what the Match writes to the save service and reads back.
#include <doctest/doctest.h>

#include "object_fps_pvp/match/PlayerSave.hpp"

#include <string>

namespace M = object_fps_pvp::match;

TEST_CASE("player save: written and read back, settled once per match") {
    const M::PlayerSave settled = M::Settled(M::PlayerSave{2, 1}, 3);
    CHECK(settled == M::PlayerSave{3, 4});
    const std::string text = M::EncodePlayerSave(settled);
    CHECK(text == "{\"saveVersion\":1,\"matches\":3,\"deaths\":4}");
    std::string error;
    const auto read = M::DecodePlayerSave(text, error);
    REQUIRE_MESSAGE(read.has_value(), error);
    CHECK(*read == settled);
}

TEST_CASE("player save: a save never written is the empty one; another version or a broken one is refused") {
    std::string error;
    CHECK(M::DecodePlayerSave("null", error) == M::PlayerSave{});
    CHECK_FALSE(M::DecodePlayerSave("{\"saveVersion\":2,\"matches\":1,\"deaths\":0}", error).has_value());
    CHECK_MESSAGE(error.find("saveVersion is 2") != std::string::npos, error);
    CHECK_FALSE(M::DecodePlayerSave("{\"saveVersion\":1,\"matches\":1}", error).has_value());
    CHECK_FALSE(M::DecodePlayerSave("not json", error).has_value());
}
