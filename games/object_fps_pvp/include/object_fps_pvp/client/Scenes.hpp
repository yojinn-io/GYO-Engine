#pragma once

// The Client's scenes (game-flow declaration 3.1): one Product, its Worlds replaced when the scene changes, the window
// and the World that draws staying (R1b: the Render World reads the new Frame World).
//
//   World    units (fixed: the description)               build, by the scene
//   "Solo"   the game's Local World, no net (own thread)   SoloGame: the game, its player at a random spawn; else empty
//   "Sim"    the game's client World and net (own thread)  MultiGame: the game and its NetConnectTarget; else empty
//   "Frame"  window, input, cameras, world_ui, sdl_window  each scene its own (the menu, the lobby, the game)
//   "Render" extracts from "Frame", ui_render, sdl_gpu      never replaced
//
// The scene now is the Product's one SceneDirector (O-15, made by ConfigureClient); each World's build reads it. A
// scene's System asks for the next one by writing the Frame World's SceneRequest; between two frames the runner asks
// SceneChangeWanted and, when it is true, calls ChangeScene (sdl_window's SDLWindowBetweenFrames).

#include "engine/kernel/Kernel.hpp"
#include "object_fps_pvp/client/Gateway.hpp"

#include "engine/net/NetService.hpp"
#include "engine/os/DatagramSocket.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

enum class Scene : std::uint8_t {
    Menu,       // the main menu: solo, multiplayer, settings, quit
    SoloRoom,   // single player: the room before a game (the merchant and the tasks have their place), the record
    SoloGame,   // a single player's game: no server
    Lobby,      // multiplayer: the Gateway's rooms (create one, or join one)
    Room,       // multiplayer: a room's members get ready; its owner starts it
    MultiGame,  // a game connected to a Match
};

// A scene to go to and what it needs.
struct SceneChange final {
    Scene scene = Scene::Menu;
    // The line the scene shows first (why it came back); empty: its own.
    std::string status;
    // MultiGame: whom the Sim World connects to.
    std::optional<GYO::Engine::Net::NetConnectTarget> connect;
    // The Gateway's room: Room, the room entered; MultiGame, the room the game came from; Lobby, a room to leave first
    // (0: none).
    std::uint64_t room = 0;
};

// The Frame World's request for the next scene (written by a scene's System; the runner reads it between frames).
struct SceneRequest final {
    std::optional<SceneChange> next;
};
inline void HashValue(GYO::Kernel::Hasher&, const SceneRequest&) {}

// What the program reads after the runner returns, carried from scene to scene (a Resource of the Frame World,
// CarryOver).
struct FrameNotes final {
    std::int64_t framesLeft = -1;  // > 0: the frames before the window closes itself (--frames); -1: never
    bool shown = false;            // a scene's document was laid out
    std::string failure;           // why a scene's document was not shown (empty: no failure seen)
    std::string mismatch;          // the Match's content differs from this Client's
    std::string endReason;         // why the last connection ended
};
inline void HashValue(GYO::Kernel::Hasher&, const FrameNotes&) {}

// The scene now, the Product's one (O-15). Written only between frames, by ChangeScene.
class SceneDirector final {
public:
    explicit SceneDirector(SceneChange first) : current_(std::move(first)) {}
    [[nodiscard]] const SceneChange& Current() const noexcept { return current_; }
    void Enter(SceneChange change) { current_ = std::move(change); }

private:
    SceneChange current_;
};

// Product options: the first scene, the frame budget, and this player's directory (none: the menu, no budget, no
// directory -- nothing is saved).
struct ClientStart final {
    SceneChange first;
    std::int64_t frames = -1;
    // The directory of this player's files (the user's data directory and the profile); empty: none.
    std::string dataDirectory;
    // The Gateway (its HTTP address).
    GYO::Engine::OS::DatagramAddress gateway = GYO::Engine::OS::DatagramAddress::Loopback(7700);
};

// The Gateway's session, carried from scene to scene (a Resource of the Frame World, CarryOver): opened by the first
// scene that calls the Gateway.
struct GatewayState final {
    std::optional<GatewaySession> session;
};
inline void HashValue(GYO::Kernel::Hasher&, const GatewayState&) {}

// An address as --connect and a room's connect give it: an IPv4 address and a port (A.B.C.D:P, port 1 to 65535).
[[nodiscard]] std::optional<GYO::Engine::OS::DatagramAddress> ParseMatchAddress(std::string_view text);

// Whether the Sim World plays (connects) in this scene; whether the Solo World plays.
[[nodiscard]] bool SimPlays(Scene scene) noexcept;
[[nodiscard]] bool SoloPlays(Scene scene) noexcept;

// The single player's save in the player's directory (empty directory: none).
[[nodiscard]] std::string SoloSavePath(const std::string& dataDirectory);

// The runner's part: whether the Frame World asks for another scene.
[[nodiscard]] bool SceneChangeWanted(O::Product& product);
// Enters the requested scene: the Solo and Sim Worlds are replaced when their part changes (into or out of SoloGame,
// MultiGame), then the Frame World (the Render World reads the new one). A refused replacement is returned as it came;
// the SceneDirector already names the new scene then (the runner ends the Product on the error: nothing reads it again).
[[nodiscard]] GYO::Engine::Base::Result<void, O::OrchestrationError> ChangeScene(O::Product& product);

} // namespace object_fps_pvp::client
