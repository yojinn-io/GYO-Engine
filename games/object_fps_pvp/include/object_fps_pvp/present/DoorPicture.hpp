#pragma once

// How the Client shows a door (interact-door declaration section 8): the Match turns a door at once (its state is
// open or shut, the walls change in one step); the picture swings to it over a while. Content, so it is edited
// without code: ui/doors.json.

#include <optional>
#include <string>

namespace object_fps_pvp::present {

// ui/doors.json (version 1): how long a swing from shut to open (or back) takes on screen.
struct DoorPictureSettings final {
    float swingSeconds = 0.4F;
};

[[nodiscard]] std::optional<DoorPictureSettings> ParseDoorPictureSettings(const std::string& text, std::string& error);

// How far a door's picture is turned (0: shut, 1: open) after `seconds` more toward its state: at the settings' pace,
// never past it.
[[nodiscard]] float SwingToward(float turned, bool open, double seconds, const DoorPictureSettings& settings) noexcept;

} // namespace object_fps_pvp::present
