#pragma once

// The player's settings (game-flow declaration 3.6): the window's size (one of four fixed sizes; the picture keeps its
// shape inside it: the UI's fit, the cameras' own projection), how the window lies (a window, or borderless
// fullscreen) and VSync. One file per machine in the user's data directory, read when the program starts:
//
//   {"version": 1, "resolution": [1920, 1080], "windowMode": "windowed" | "borderless", "vsync": true}

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/window/Window.hpp"

#include <array>
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
// resolution is the window's size in logical pixels (window's ResizeWindow and WindowSettings::size); in
// BorderlessFullscreen it is the size the window takes when it is a window again.
struct ClientSettings final {
    GYO::Engine::Math::Vec2i resolution = kResolutions[0];
    GYO::Engine::Window::WindowMode window = GYO::Engine::Window::WindowMode::Windowed;
    bool vsync = true;
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

// The next size offered after this one (round), and the player's words for a size.
[[nodiscard]] GYO::Engine::Math::Vec2i NextResolution(GYO::Engine::Math::Vec2i now);
[[nodiscard]] std::string ResolutionText(GYO::Engine::Math::Vec2i size);

} // namespace object_fps_pvp::client
