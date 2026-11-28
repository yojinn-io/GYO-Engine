#pragma once

// The players' names the matches service hands a Match (display-name declaration 5.1): with --player-names-stdin,
// the second line of the standard input, one JSON object,
//
//   {"players": [{"userId": 7, "name": "..."}, ...]}
//
// A name is what the identity service made of the one the player signed in with; empty: none. The Match shows names
// and decides nothing by them, so how long one may be is the World's (world::PlayerName), not the roster's. The line
// the matches service writes for a known roster is shared with its tests (services/matches/testdata/roster_line.txt).

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::match {

// The names by user. nullopt with error naming what was wrong: not JSON, no players array, a user id that is not a
// whole number other than 0 or comes twice, a name that is not a string.
[[nodiscard]] std::optional<std::map<std::uint64_t, std::string>> ParsePlayerNames(std::string_view line, std::string& error);

} // namespace object_fps_pvp::match
