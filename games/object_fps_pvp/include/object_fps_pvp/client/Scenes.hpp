#pragma once

// The Client's scenes (game-flow declaration 3.1): one Product, its Worlds replaced when the scene changes, the window
// and the World that draws staying (R1b: the Render World reads the new Frame World).
//
//   World    units (fixed: the description)               build, by the scene
//   "Sim"    the game's client World and net (own thread)  MultiGame: the game and its NetConnectTarget; else empty
//   "Frame"  window, input, cameras, world_ui, sdl_window  each scene its own (the menu, the lobby, the game)
//   "Render" extracts from "Frame", ui_render, sdl_gpu      never replaced
//
// The scene now is the Product's one SceneDirector (O-15, made by ConfigureClient); each World's build reads it. A
// scene's System asks for the next one by writing the Frame World's SceneRequest; between two frames the runner asks
// SceneChangeWanted and, when it is true, calls ChangeScene (sdl_window's SDLWindowBetweenFrames).

#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

enum class Scene : std::uint8_t {
    Menu,       // the main menu: solo, multiplayer, settings, quit
    Lobby,      // multiplayer: where the player says whom to play with
    MultiGame,  // a game connected to a Match
};

// A scene to go to and what it needs.
struct SceneChange final {
    Scene scene = Scene::Menu;
    // The line the scene shows first (why it came back); empty: its own.
    std::string status;
    // MultiGame: whom the Sim World connects to.
    std::optional<GYO::Engine::Net::NetConnectTarget> connect;
    // Lobby: the address field's text.
    std::string address;
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

// Product options: the first scene and the frame budget (none: the menu, no budget).
struct ClientStart final {
    SceneChange first;
    std::int64_t frames = -1;
};

// Whether the Sim World plays (connects) in this scene.
[[nodiscard]] bool SimPlays(Scene scene) noexcept;

// The runner's part: whether the Frame World asks for another scene.
[[nodiscard]] bool SceneChangeWanted(O::Product& product);
// Enters the requested scene: the Sim World is replaced when its part changes (into or out of MultiGame), then the
// Frame World (the Render World reads the new one). A refused replacement is returned as it came.
[[nodiscard]] GYO::Engine::Base::Result<void, O::OrchestrationError> ChangeScene(O::Product& product);

} // namespace object_fps_pvp::client
