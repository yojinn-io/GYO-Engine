#pragma once

// The Client's own files in the user's data directory (the single player's save, the device id; later the settings):
// small UTF-8 texts read and written whole, through the IO helpers (a write replaces the file in one step).

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"

#include <optional>
#include <string>

namespace object_fps_pvp::client {

// The file's text; nullopt when it is not there.
[[nodiscard]] GYO::Engine::Base::Result<std::optional<std::string>, GYO::Engine::IO::IOError> ReadLocalText(const std::string& path);

// Writes text as the whole file, making the directories above it.
[[nodiscard]] GYO::Engine::Base::Result<void, GYO::Engine::IO::IOError> WriteLocalText(const std::string& path, const std::string& text);

} // namespace object_fps_pvp::client
