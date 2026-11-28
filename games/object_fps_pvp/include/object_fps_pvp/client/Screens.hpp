#pragma once

// The Client's screens (game-flow declaration 3.1; the start screen, playtest 2026-11-26 item 6): one Product, its Worlds replaced when the screen changes, the window
// and the World that draws staying (R1b: the Render World reads the new Frame World).
//
//   World    units (fixed: the description)               build, by the screen
//   "Solo"   the game's Local World, no net (own thread)   SoloGame: the game, its player at a random spawn; else empty
//   "Sim"    the game's client World and net (own thread)  MultiGame: the game and its NetConnectTarget; else empty
//   "Frame"  window, input, cameras, world_ui, sdl_window  each screen its own (the menu, the lobby, the game)
//   "Render" extracts from "Frame", ui_render, sdl_gpu      never replaced
//
// The screen now is the Product's one ScreenDirector (O-15, made by ConfigureClient); each World's build reads it. The
// Gateway's client (Gateway.hpp) is a Resource of every Frame World, carried from screen to screen with its session. A
// screen's System asks for the next one by writing the Frame World's ScreenRequest; between two frames the runner asks
// ScreenChangeWanted and, when it is true, calls ChangeScreen (sdl_window's SDLWindowBetweenFrames).

#include "engine/kernel/Kernel.hpp"
#include "object_fps_pvp/client/Gateway.hpp"
#include "object_fps_pvp/client/Settings.hpp"

#include "engine/net/NetService.hpp"
#include "engine/os/SocketAddress.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

enum class Screen : std::uint8_t {
    Menu,       // the main menu: solo, multiplayer, settings, quit
    Splash,     // the start screen: the logo (ui/pvp_splash.json), then the menu
    SoloRoom,   // single player: the room before a game (the merchant and the tasks have their place), the record
    SoloGame,   // a single player's game: no server
    Lobby,      // multiplayer: the Gateway's rooms (create one, or join one)
    Room,       // multiplayer: a room's members get ready; its owner starts it
    MultiGame,  // a game connected to a Match
};

// A screen to go to and what it needs.
struct ScreenChange final {
    Screen screen = Screen::Menu;
    // The line the screen shows first (why it came back); empty: its own.
    std::string status;
    // MultiGame: whom the Sim World connects to.
    std::optional<GYO::Engine::Net::NetConnectTarget> connect;
    // The Gateway's room: Room, the room entered; MultiGame, the room the game came from (0: none, --connect).
    std::uint64_t room = 0;
    // SoloGame, MultiGame: the arena played, by its id (the room's map); empty, or one this Client does not have: the
    // Client's own (ClientOptions::arena; a Match then says which it plays).
    std::string map;
    // SoloGame, MultiGame: the mode played, by its id (the room's mode); empty, or one this Client does not have: the
    // Client's own (ClientOptions::mode; a Match then says which it plays).
    std::string mode;
};

// The Frame World's request for the next screen (written by a screen's System; the runner reads it between frames).
struct ScreenRequest final {
    std::optional<ScreenChange> next;
};
inline void HashValue(GYO::Kernel::Hasher&, const ScreenRequest&) {}

// What the program reads after the runner returns, carried from screen to screen (a Resource of the Frame World,
// CarryOver).
struct FrameNotes final {
    std::int64_t framesLeft = -1;  // > 0: the frames before the window closes itself (--frames); -1: never
    bool shown = false;            // a screen's document was laid out
    std::string failure;           // why a screen's document was not shown (empty: no failure seen)
    std::string mismatch;          // the Match's content differs from this Client's
    std::string endReason;         // why the last connection ended
    std::string map;               // why the game's map did not load
};
inline void HashValue(GYO::Kernel::Hasher&, const FrameNotes&) {}

// The screen now, the Product's one (O-15). Written only between frames, by ChangeScreen.
class ScreenDirector final {
public:
    explicit ScreenDirector(ScreenChange first) : current_(std::move(first)) {}
    [[nodiscard]] const ScreenChange& Current() const noexcept { return current_; }
    void Enter(ScreenChange change) { current_ = std::move(change); }

private:
    ScreenChange current_;
};

// Product options: the first screen, the frame budget, and this player's directory (none: the menu, no budget, no
// directory -- nothing is saved).
struct ClientStart final {
    ScreenChange first;
    std::int64_t frames = -1;
    // The directory of this player's files (the user's data directory and the profile); empty: none.
    std::string dataDirectory;
    // The Gateway (its HTTP address).
    GYO::Engine::OS::SocketAddress gateway = GYO::Engine::OS::SocketAddress::Loopback(7700);
    // The settings the program started with, and their file (empty: not saved).
    ClientSettings settings;
    std::string settingsPath;
    // The name this player signs in with at the Gateway (PlayerNameCandidate; empty: none).
    std::string name;
    // How long the start screen shows its logo once it is laid out (a key or a click goes on at once).
    double splashSeconds = 2.0;
};


// An address as --connect and a room's connect give it: an IPv4 address and a port (A.B.C.D:P, port 1 to 65535).
[[nodiscard]] std::optional<GYO::Engine::OS::SocketAddress> ParseMatchAddress(std::string_view text);

// The settings in force (a Resource of every Frame World, carried from screen to screen; ClientStart's first).
inline void HashValue(GYO::Kernel::Hasher&, const ClientSettings&) {}

// A mode a room can be made with: an installed mode (world::ModeContent), its id, the name the player reads and the
// installed arenas it plays (world::GameContent::Plays), by id.
struct ModeChoice final {
    std::string id;
    std::string name;
    std::vector<std::string> maps;
};

// The Gateway's screens' words (GatewayScreen.cpp): what the player reads for a call that went wrong (gateway: its
// address), and for a room's mode (the installed mode's name; a mode this Client does not have, its id).
[[nodiscard]] std::string GatewayErrorText(const GatewayResult& result, const std::string& gateway);
[[nodiscard]] std::string ModeText(const std::vector<ModeChoice>& modes, std::string_view mode);
// What the player reads for a player (display-name declaration 5.3): the name they signed in with, or "Player <userId>"
// when they have none. The room and the game's overhead labels show the same words.
[[nodiscard]] std::string PlayerLabel(std::uint64_t userId, std::string_view name);

// Whether the Sim World plays (connects) in this screen; whether the Solo World plays.
[[nodiscard]] bool SimPlays(Screen screen) noexcept;
[[nodiscard]] bool SoloPlays(Screen screen) noexcept;

// The single player's save in the player's directory (empty directory: none).
[[nodiscard]] std::string SoloSavePath(const std::string& dataDirectory);

// The runner's part: whether the Frame World asks for another screen.
[[nodiscard]] bool ScreenChangeWanted(O::Product& product);
// Enters the requested screen: the Solo and Sim Worlds are replaced when their part changes (into or out of SoloGame,
// MultiGame), then the Frame World (the Render World reads the new one). A refused replacement is returned as it came;
// the ScreenDirector already names the new screen then (the runner ends the Product on the error: nothing reads it again).
[[nodiscard]] GYO::Engine::Base::Result<void, O::OrchestrationError> ChangeScreen(O::Product& product);

} // namespace object_fps_pvp::client
