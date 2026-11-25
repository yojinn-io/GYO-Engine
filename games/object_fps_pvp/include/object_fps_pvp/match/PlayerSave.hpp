#pragma once

// A player's save in the save service (gateway declaration D7): the body of the service's save, in this game's own
// format; the service does not read it. Only the Match writes it (the authority), when the player leaves.
//
//   {"saveVersion": 1, "matches": N, "deaths": N, "lastMatch": ID, "lastWrite": "KEY"}
//
// matches counts the Matches the player left, each once however often they left it (lastMatch is the last one);
// deaths adds the deaths of every leave. lastWrite is the idempotency key of the write that made this save: a writer
// that meets a conflict reads the save again and sees whether its own write is already in it.
//
// A save of another saveVersion is refused (this game has one version; a newer save is not read by an older Match).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::match {

inline constexpr std::uint32_t kPlayerSaveVersion = 1;

struct PlayerSave final {
    std::uint64_t matches = 0;
    std::uint64_t deaths = 0;
    std::uint64_t lastMatch = 0;
    std::string lastWrite;

    friend bool operator==(const PlayerSave&, const PlayerSave&) = default;
};

// The save after one leave of Match matchId with these deaths, written under key.
[[nodiscard]] PlayerSave Settled(PlayerSave save, std::uint64_t deaths, std::uint64_t matchId, std::string key);

[[nodiscard]] std::string EncodePlayerSave(const PlayerSave& save);

// A body the service gave back ("null": never written, the empty save). nullopt with error naming what was wrong.
[[nodiscard]] std::optional<PlayerSave> DecodePlayerSave(std::string_view body, std::string& error);

} // namespace object_fps_pvp::match
