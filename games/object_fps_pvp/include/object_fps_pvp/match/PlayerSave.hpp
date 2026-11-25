#pragma once

// A player's save in the save service (gateway declaration D7): the body of the service's save, in this game's own
// format; the service does not read it. Only the Match writes it (the authority), when the player leaves (the smallest
// "extract and settle": survival settles at once; co-op's "every living player extracts" is a later rule at the same
// point).
//
//   {"saveVersion": 1, "matches": N, "deaths": N}
//
// A save of another saveVersion is refused (this game has one version; a newer save is not read by an older Match).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::match {

inline constexpr std::uint32_t kPlayerSaveVersion = 1;

struct PlayerSave final {
    // Matches the player left (settled).
    std::uint64_t matches = 0;
    // Deaths over those matches.
    std::uint64_t deaths = 0;

    friend bool operator==(const PlayerSave&, const PlayerSave&) = default;
};

// The save after one more match with these deaths.
[[nodiscard]] PlayerSave Settled(PlayerSave save, std::uint64_t deaths) noexcept;

[[nodiscard]] std::string EncodePlayerSave(const PlayerSave& save);

// A body the service gave back ("null": never written, the empty save). nullopt with error naming what was wrong.
[[nodiscard]] std::optional<PlayerSave> DecodePlayerSave(std::string_view body, std::string& error);

} // namespace object_fps_pvp::match
