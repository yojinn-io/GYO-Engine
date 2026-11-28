#pragma once

// The players' names the matches service hands a Match (display-name declaration 5.1): with --player-names-stdin,
// the second line of the standard input, one JSON object,
//
//   {"players": [{"userId": 7, "name": "..."}, ...]}
//
// A name is what the identity service made of the one the player signed in with (at most 32 code points, 128 bytes);
// empty: none. The Match shows names and decides nothing by them.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::match {

// The longest name (bytes) a roster may give, and the replicated PlayerName carries.
inline constexpr std::size_t kMaxPlayerNameBytes = 128;

// The names by user. nullopt with error naming what was wrong: not JSON, no players array, a user id that is not a
// whole number other than 0 or comes twice, a name that is not a string or is longer than kMaxPlayerNameBytes.
[[nodiscard]] std::optional<std::map<std::uint64_t, std::string>> ParsePlayerNames(std::string_view line, std::string& error);

} // namespace object_fps_pvp::match
