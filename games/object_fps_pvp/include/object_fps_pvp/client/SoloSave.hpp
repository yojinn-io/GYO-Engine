#pragma once

// The single player's save (game-flow declaration 3.3): match/PlayerSave.hpp's format in a file of the player's
// directory (Screens.hpp SoloSavePath), read by the single player's room and settled when a game is evacuated.

#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/base/Result.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace object_fps_pvp::client {

// The single player's save (SoloSavePath): why it could not be read or written.
enum class SoloSaveFailureCode : std::uint8_t {
    NoDirectory = 1,  // the player has no directory (ClientStart::dataDirectory)
    ReadFailed,       // the file is there and could not be read (the IO error's text in the detail)
    BadSave,          // the file is not a save of this game (match::DecodePlayerSave's text)
    WriteFailed,      // the file could not be written
};
[[nodiscard]] const char* ToString(SoloSaveFailureCode code) noexcept;
using SoloSaveFailure = GYO::Engine::Base::Error<SoloSaveFailureCode>;

// The save at path; nullopt when it is not there (the empty save).
[[nodiscard]] GYO::Engine::Base::Result<std::optional<match::PlayerSave>, SoloSaveFailure> ReadSoloSave(const std::string& path);
// One game settled into the save at path (one more game, these deaths); the save written.
[[nodiscard]] GYO::Engine::Base::Result<match::PlayerSave, SoloSaveFailure> SettleSolo(const std::string& path, std::uint64_t deaths);
// The player's words for a failure.
[[nodiscard]] std::string SoloSaveText(const SoloSaveFailure& failure);

} // namespace object_fps_pvp::client
