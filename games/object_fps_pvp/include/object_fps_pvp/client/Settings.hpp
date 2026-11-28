#pragma once

// The player's settings (game-flow declaration 3.6): the window's size (one of four fixed sizes; the picture keeps its
// shape inside it: the UI's fit, the cameras' own projection), how the window lies (a window, or borderless
// fullscreen), VSync and, when the player wrote one, the name they sign in with (display-name declaration 4.1). One
// file per machine in the user's data directory, read when the program starts:
//
//   {"version": 1, "resolution": [1920, 1080], "windowMode": "windowed" | "borderless", "vsync": true,
//    "name": "..."}   (name: optional)

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/window/Window.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace object_fps_pvp::client {

// The sizes the settings offer (the user's: fixed, as many games have; a picture that does not fit the screen is the
// player's to change).
inline constexpr std::array<GYO::Engine::Math::Vec2i, 4> kResolutions = {
    GYO::Engine::Math::Vec2i{1280, 720}, GYO::Engine::Math::Vec2i{1600, 900}, GYO::Engine::Math::Vec2i{1920, 1080},
    GYO::Engine::Math::Vec2i{2560, 1440}};

// The window's mode is window's own (Windowed, BorderlessFullscreen); the file names them "windowed", "borderless".
// resolution is the window's size in physical pixels (what the screen shows; window's ResizeWindow and
// WindowSettings::size take logical ones: WindowSizeFor); in BorderlessFullscreen it is the size the window takes when
// it is a window again.
struct ClientSettings final {
    GYO::Engine::Math::Vec2i resolution = kResolutions[0];
    GYO::Engine::Window::WindowMode window = GYO::Engine::Window::WindowMode::Windowed;
    bool vsync = true;
    // The name the player chose; none: the machine's host name stands in (PlayerNameCandidate). An empty name is a
    // choice too: no name, so a display shows its fallback.
    std::optional<std::string> name;
    friend bool operator==(const ClientSettings&, const ClientSettings&) = default;
};

inline constexpr std::uint32_t kSettingsVersion = 1;

[[nodiscard]] std::string EncodeSettings(const ClientSettings& settings);
// nullopt with error naming what was wrong (not JSON, another version, a size not offered, an unknown mode).
[[nodiscard]] std::optional<ClientSettings> DecodeSettings(const std::string& text, std::string& error);

// The settings in the file at path: the defaults when it is not there; when it cannot be read or is not a settings
// file, the defaults and problem says why (the program goes on with them).
struct LoadedSettings final {
    ClientSettings settings;
    std::string problem;
};
[[nodiscard]] LoadedSettings LoadSettings(const std::string& path);
[[nodiscard]] GYO::Engine::Base::Result<void, GYO::Engine::IO::IOError> SaveSettings(const std::string& path, const ClientSettings& settings);

// The most bytes of the name a Client sends (the identity service keeps 32 code points of it: display-name
// declaration 4.1); a longer one is cut at a UTF-8 boundary.
inline constexpr std::size_t kNameCandidateBytes = 256;

// The name this player signs in with (display-name declaration 4.1): the settings' name when there is one (empty
// included: the player chose none), otherwise the machine's host name without a final ".local" (none known: empty).
[[nodiscard]] std::string PlayerNameCandidate(const ClientSettings& settings, const std::optional<std::string>& hostName);

// The window's size in logical pixels that shows physical pixels at a scale factor (window's WindowState::scaleFactor;
// one that is not positive counts as 1).
[[nodiscard]] GYO::Engine::Math::Vec2i WindowSizeFor(GYO::Engine::Math::Vec2i physical, float scale);

// The next size offered after this one (round), and the player's words for a size.
[[nodiscard]] GYO::Engine::Math::Vec2i NextResolution(GYO::Engine::Math::Vec2i now);
[[nodiscard]] std::string ResolutionText(GYO::Engine::Math::Vec2i size);

} // namespace object_fps_pvp::client
