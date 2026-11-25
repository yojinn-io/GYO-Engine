#pragma once

// The Client's own files in the user's data directory (the single player's save; later the settings and the device
// id): small UTF-8 texts read and written whole, through the IO helpers (a write replaces the file in one step).

#include <optional>
#include <string>

namespace object_fps_pvp::client {

// The file's text; nullopt when it is not there (error empty) or could not be read (error says why).
[[nodiscard]] std::optional<std::string> ReadLocalText(const std::string& path, std::string& error);

// Writes text as the whole file, making the directories above it; empty when written, else why not.
[[nodiscard]] std::string WriteLocalText(const std::string& path, const std::string& text);

} // namespace object_fps_pvp::client
