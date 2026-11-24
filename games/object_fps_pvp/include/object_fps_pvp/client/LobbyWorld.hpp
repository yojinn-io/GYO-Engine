#pragma once

// The lobby (port inventory PV12; port declaration, segment 5): before a Client connects, a window with the game's
// gyo.ui document (assets/object_fps_pvp/ui/pvp_lobby.json) where the player types the Match's address and presses
// Enter. It is its own Product without net (the address of a client's connection is fixed when its Product is built:
// NetProductOptions::server); the program builds the Client's Product with the address the lobby gave, and comes back
// to the lobby with the reason when that connection ends (net page D-8: a new connection is the product's call).
//
// The wiring is ui page section 9's: the source World carries world_ui and window_camera (and world_render, whose
// camera clear settings camera_render extracts), the render World ui_render; the ui and font loaders join the asset
// store; one entity holds the UiRoot (the document by its asset id, canvas "lobby") and its UiBindings (address,
// status, hint); the address field's characters go through the game's filter (lobby/AddressText.hpp) as
// UiRoot::filter; the actions are read after world_ui's set.
//
//   Main/Update  LobbyActions   pvp.submit_address -> a valid A.B.C.D:P closes the window with that address; else
//                               the status says what is wrong. pvp.quit -> closes the window with nothing.

#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/DatagramSocket.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

// An address as the lobby and --connect take it: an IPv4 address and a port (A.B.C.D:P, port 1 to 65535).
[[nodiscard]] std::optional<GYO::Engine::OS::DatagramAddress> ParseMatchAddress(std::string_view text);

// What the lobby ended with (the program reads it after the runner returns).
struct LobbyOutcome final {
    std::optional<GYO::Engine::OS::DatagramAddress> server;  // the address the player submitted
    std::string address;                                     // its text, shown again when the lobby comes back
    bool quit = false;                                       // the player chose to end the program
};

struct LobbyOptions final {
    std::string address;  // the field's first text
    std::string status;   // the status line's first text (why the lobby came back; empty: the invitation)
    std::shared_ptr<LobbyOutcome> outcome;
};

// The Product's part: the ui and font loaders join the asset store.
void ConfigureLobby(O::ProductSetup& setup);

// "Game" (the lobby: world_asset, camera, input, window, window_camera, world_ui, world_render, and sdl_window when
// window is true) and "Render" (extracts from it: render_world, camera_render, ui_render, and sdl_gpu when gpu is
// true).
[[nodiscard]] O::WorldDescription LobbyGameWorld(LobbyOptions options, bool window = true);
[[nodiscard]] O::WorldDescription LobbyRenderWorld(bool gpu = true);

} // namespace object_fps_pvp::client
