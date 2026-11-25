// The player's save format (gateway declaration D7): what the Match writes to the save service and reads back.
#include <doctest/doctest.h>

#include "object_fps_pvp/match/PlayerSave.hpp"

#include <string>

namespace M = object_fps_pvp::match;

TEST_CASE("player save: written and read back; a Match counts once however often the player leaves it") {
    const M::PlayerSave first = M::Settled(M::PlayerSave{2, 1, 5, "old"}, 3, 9, "match-9-user-1-leave-1");
    CHECK(first == M::PlayerSave{3, 4, 9, "match-9-user-1-leave-1"});
    const M::PlayerSave again = M::Settled(first, 1, 9, "match-9-user-1-leave-2");
    CHECK_MESSAGE((again == M::PlayerSave{3, 5, 9, "match-9-user-1-leave-2"}), "a second leave of the same Match counted a match");
    const std::string text = M::EncodePlayerSave(again);
    CHECK(text == "{\"saveVersion\":1,\"matches\":3,\"deaths\":5,\"lastMatch\":9,\"lastWrite\":\"match-9-user-1-leave-2\"}");
    std::string error;
    const auto read = M::DecodePlayerSave(text, error);
    REQUIRE_MESSAGE(read.has_value(), error);
    CHECK(*read == again);
}

TEST_CASE("player save: a save never written is the empty one; another version or a broken one is refused") {
    std::string error;
    CHECK(M::DecodePlayerSave("null", error) == M::PlayerSave{});
    CHECK_FALSE(M::DecodePlayerSave("{\"saveVersion\":2,\"matches\":1,\"deaths\":0,\"lastMatch\":0,\"lastWrite\":\"\"}", error).has_value());
    CHECK_MESSAGE(error.find("saveVersion is 2") != std::string::npos, error);
    CHECK_FALSE(M::DecodePlayerSave("{\"saveVersion\":1,\"matches\":1}", error).has_value());
    CHECK_FALSE(M::DecodePlayerSave("not json", error).has_value());
}
