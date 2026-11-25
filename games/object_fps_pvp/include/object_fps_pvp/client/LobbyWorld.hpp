#pragma once

// The lobby scene (Scenes.hpp, Scene::Lobby; port inventory PV12): the game's gyo.ui document
// (assets/object_fps_pvp/ui/pvp_lobby.json) where the player types the Match's address and presses Enter. The scene
// has no net: the Sim World of the next scene (MultiGame) connects to the address it gives (NetConnectTarget).
//
// The wiring is ui page section 9's: the Frame World carries world_ui and window_camera (and world_render), the Render
// World ui_render; one entity holds the UiRoot (the document by its asset id, canvas "lobby") and its UiBindings
// (address, status, hint); the address field's characters go through the game's filter (lobby/AddressText.hpp) as
// UiRoot::filter; the actions are read after world_ui's set.
//
//   Main/Update  LobbyActions   pvp.submit_address -> a valid A.B.C.D:P asks for MultiGame connected to it; else the
//                               status says what is wrong. pvp.quit -> back to the menu.

#include "engine/os/DatagramSocket.hpp"

#include <optional>
#include <string_view>

namespace object_fps_pvp::client {

// An address as the lobby and --connect take it: an IPv4 address and a port (A.B.C.D:P, port 1 to 65535).
[[nodiscard]] std::optional<GYO::Engine::OS::DatagramAddress> ParseMatchAddress(std::string_view text);

} // namespace object_fps_pvp::client
