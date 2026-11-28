#pragma once

// The Frame World's registration of each scene (Scenes.hpp): ClientFrameWorld's build calls the common part, then the
// scene's.

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/client/Gateway.hpp"
#include "object_fps_pvp/client/Scenes.hpp"
#include "object_fps_pvp/client/SoloSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/ui/UiRoot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::client {

namespace K = GYO::Kernel;

// Fixed ids of a menu-like scene's entities (outside the allotters' ranges, as the game scene's: ClientWorld.cpp).
inline constexpr K::EntityId kSceneCamera = 3'000'030;
inline constexpr K::EntityId kSceneRoot = 3'000'031;

// A System in Main/Update (after the given Systems).
[[nodiscard]] K::SystemOptions InSceneUpdate(std::string name, std::vector<std::string> after = {});

// A 2D camera on the window for a scene's documents (kSceneCamera).
void PlaceSceneCamera(K::World& world);
// That camera, and the document's root entity (its UiRoot, asked for by its asset
// id, with the text fields' filter, and UiBindings).
void PlaceSceneDocument(K::World& world, std::string_view documentId, std::string_view canvas, GYO::Engine::Ui::UiBindings bindings,
                        GYO::Engine::Ui::UiTextFilter filter);

// Every scene's part: SceneRequest, FrameNotes and the GatewayClient (both carried to the next scene), the frame
// budget (closes the window when it runs out) and the watch of the scene's document (FrameNotes::shown, failure).
void BuildSceneCommon(O::WorldSetup& setup);

// The scenes' own parts.
void BuildMenu(O::WorldSetup& setup, const SceneChange& change);
// The start screen: the logo document for seconds after it is laid out, then the menu.
void BuildSplash(O::WorldSetup& setup, double seconds);
// dataDirectory: the player's (ClientStart); modes: the installed modes, in order, each with the arenas it plays (the
// single player plays the first, on one of its arenas).
void BuildSoloRoom(O::WorldSetup& setup, const SceneChange& change, const std::string& dataDirectory, const std::vector<ModeChoice>& modes);
// What the Gateway's scenes need from the Product: this player's directory (the device id), the Gateway, the modes a
// room can be made with (the installed modes, in order, each with the arenas it plays).
struct GatewayContext final {
    std::string dataDirectory;
    GYO::Engine::OS::SocketAddress gateway;
    std::vector<ModeChoice> modes;
};
void BuildLobby(O::WorldSetup& setup, const SceneChange& change, const GatewayContext& context);
void BuildRoom(O::WorldSetup& setup, const SceneChange& change, const GatewayContext& context);

// The device id file in the player's directory.
[[nodiscard]] std::string DeviceIdPath(const std::string& dataDirectory);
// The Frame World's GatewayClient, when the World has none that can call (later scenes get it carried): needed (a
// scene that calls the Gateway), with this player's device id (read, or made the first time) or without one (it then
// calls nothing and says why); not needed, one that calls nothing (no file is read or made for the menu or single
// player).
void PlaceGatewayClient(K::World& world, const ClientStart& start, bool needed);

} // namespace object_fps_pvp::client
