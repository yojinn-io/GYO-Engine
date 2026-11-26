#pragma once

// The game's Client in three Worlds (live-frame-sim-thread declaration 4.3):
//
//   "Sim"    the World of world/PvpWorld.hpp as a client (K3: it predicts its own player) and the net Module; the
//            program lends it to its own thread (SDLWindowOptions::ownThreadWorlds), so a window held by the OS
//            (dragged, resized) does not stop the steps nor the packets.
//   "Frame"  what a player sees, hears and does (none of it writes decision data): the window, the input, the
//            cameras, the HUD, the characters, the sounds. It shows the Sim World's players as copies by their ids.
//   "Render" extracts from "Frame".
//
// They are the game scene of the Client's scenes (Scenes.hpp): the descriptions below read the Product's
// SceneDirector, and register the game only in Scene::MultiGame (the Sim World elsewhere is empty; the Frame World is
// the scene's).
//
// Sim and Frame hand each other two things through one object of the Product (ClientExchange, each half behind its
// own lock): the Sim World's "out" puts the newest picture (its players, the arena, ClientView); the Frame World's
// "in" applies it. The Frame World's "out" adds the controls (the held keys and the view: the newest; the presses
// add up); the Sim World's "in" takes them. Neither waits for the other.
//
// Sim:
//   Main/PreUpdate   PrepareOwnInput the own Peer's PeerInput buffer
//   FixedUpdate/First WriteStepInput the controls -> the own Peer's PeerInput slot of the step that runs now, before
//                                   the net Module's ApplyInput (not while resimulating); a press goes to one step
//   Main/Update      NoteSessionEnd  the connection ended: why (ClientView::ended)
// Frame:
//   Main/PreUpdate   ReadControls   keys and mouse -> the controls (pointer taken or given back: window's cursor
//                                   grab; the view; the held keys; the presses)
//   Main/Update      ShowArena      while the World generates: the arena's map from the asset store (the same one the
//                                   Sim World reads), its regions' models made on the worker, then one Mesh3d per
//                                   region with its box (the visibility Module culls it); MapLoad says how far
//                    ShowHud        the 2D layer: crosshair, HP, ammo, what the player should do; a hit's feedback
//                                   (present/HitFeedback): the red flash, the arc toward the attacker, the HP in red
//                    PlaceCamera    the first-person camera at the own player's eye, turned by the Client's view
//                                   (and shaken by a hit: the camera only, not the view the player controls)
//                    MoveBodies     a stand-in body for every other living player follows it (or goes)
//                    AddBodies      ... and is made for one that has none
//                    PlaySounds     shots, hits, footsteps (present/SoundCues), at the moment of the frame
//                    LeaveOnEnd     the connection ended, the Match's content differs, or a map did not load: back to
//                                   the lobby (the Match plays another arena this Client has: the game again on it)
//                    TrackReadiness the loading (game-flow declaration 3.5): the game's assets with their
//                                   dependencies and both Worlds' maps (the progress), the warm-up (two frames shown with no draw skipped
//                                   after them, DrawnFrames), the own player from the Match, then one more frame: ready
//                                   (carried to the Match in the input, PlayerInput::ready)
//                    ShowLoading    GamePage::Loading (client/Pages.hpp: the overlay's pages, Bevy's States): the loading
//                                   screen's words and bar; Playing once ready and the match started
//                    PauseOnRelease Playing -> Paused when the pointer is the player's; PlayOnCapture back (single
//                                   player: the Solo World's time stops on the pause page, through the exchange)
//                    Resume         pvp.resume: the game takes the pointer again (as a click does)
//                    Evacuate       pvp.evacuate: single player settles the game (the Solo World's deaths) into the
//                                   save and goes to the menu; multiplayer goes back to the lobby (the connection closes)
//
// The pictures are drawn by "Render" (render_world, camera_render, sprite_render, mesh_render and sdl_gpu). The
// Frame World has no rollback: what it makes stays until it takes it away.

#include "object_fps_pvp/data/ArenaMap.hpp"
#include "object_fps_pvp/present/HitFeedback.hpp"
#include "object_fps_pvp/present/SoundCues.hpp"
#include "object_fps_pvp/world/PvpWorld.hpp"

#include "engine/image/ImageData.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

// The first-person weapon (weapons/nailer_v2/viewmodel/*.json and its model): where it sits before the viewmodel
// camera, the picture of each material, its clips, and the reload's sound on the clip's magazine_in event.
struct ViewmodelDefinition final {
    std::shared_ptr<const GYO::Engine::Model::ModelData> model;  // the skeleton and clips (the store's copy is drawn)
    std::uint32_t idle = 0, shoot = 0, reload = 0, draw = 0;    // clip indices
    GYO::Engine::Math::Vec3 anchor{};                          // the anchor node at the Idle clip's start
    GYO::Engine::Math::Vec3 offset{};                          // before the camera, in its space
    GYO::Engine::Math::Quaternion rotation{};
    float verticalFov = 0.96F;
    float nearClip = 0.01F;
    float farClip = 5.0F;
    // The shot's kick (its "recoil": kick_degrees, recovery_per_second): the weapon turns up about the viewmodel
    // camera's X axis by kick x exp(-recovery x seconds since the shot). The view and the aim do not move.
    float recoilKickRadians = 0.0F;
    float recoilRecoveryPerSecond = 0.0F;
    std::vector<std::string> materialImages;                   // per material: the image's asset id (empty: none)
    float magazineInSeconds = 0.0F;                            // the event's time in the Reload clip
    std::string reloadSound;                                   // its sound's asset id
    float reloadSoundSync = 0.0F;                              // the sound's sync point (it starts this much earlier)
    float reloadSoundGain = 1.0F;
    // The third-person weapon (weapons/nailer_v2/world/character.json): its model and its picture.
    std::string heldModel;
    std::string heldImage;
};

// Where the first-person weapon sits under the viewmodel camera (its local Transform): the anchor node of the idle
// pose's start at the offset, turned by the rotation, and by recoilRadians up about the camera's X axis (the anchor
// stays where it is).
[[nodiscard]] GYO::Engine::Transform::Transform ViewmodelPlacement(const ViewmodelDefinition& viewmodel, float recoilRadians = 0.0F);

// The recoil seconds after a shot (ViewmodelDefinition's recoil), in radians.
[[nodiscard]] float RecoilRadians(const ViewmodelDefinition& viewmodel, double seconds);

// An arena's picture: one model per region of its map (with the region's world box, for culling) and the map's
// palette picture the models' uv point into. The game scene's Frame World makes it from the map while it generates.
struct ArenaPicture final {
    std::vector<data::RegionPicture> regions;
    std::shared_ptr<const GYO::Engine::Image::ImageData> palette;
};

// What the Client reads before it starts, besides the game's content: the sound and hit feedback settings and the
// first-person weapon.
struct PresentationContent final {
    present::SoundSettings sound;
    present::HitFeedbackSettings hitFeedback;
    ViewmodelDefinition viewmodel;
};

// Reads it from the game's asset directory (audio/sfx.json, ui/hit_feedback.json, weapons/nailer_v2/viewmodel/);
// nullopt with error naming the file.
[[nodiscard]] std::optional<PresentationContent> LoadPresentation(const std::string& root, std::string& error);

struct ClientOptions final {
    std::shared_ptr<const world::GameContent> game;
    std::shared_ptr<const PresentationContent> presentation;
    std::size_t arena = 0;
    std::uint64_t userId = 0;
    // Where the map's walls and pictures are made (the Product's worker; it outlives the Product). None: in the
    // System, on the World's thread.
    GYO::Engine::Asset::AssetExecutor* jobs = nullptr;
};

// The Product's part (its configure): the ClientExchange and the SceneDirector (from the product option ClientStart;
// none: the menu), the game's loaders join the asset store (the map's, world::ConfigureGame; fbx, png, fonts, sounds,
// gyo.ui documents), and a submitter of sdl_audio's mixer when there is one (none: the Client is silent).
void ConfigureClient(O::ProductSetup& setup);

// The Frame World's own entities (the stand-in bodies): outside the Match's and the Sim World's ranges, whose
// players it copies by their ids.
inline constexpr GYO::Engine::EntityIds::EntityIdsOptions kFrameIds{2'000'001, 3'000'000};

// The three Worlds: "Sim" (the client World and the Client's input; its NetConnectTarget is the scene's connect),
// "Frame" (units listed: the Modules it needs and sdl_window when window is true) and "Render" (extracts from "Frame";
// with sdl_gpu when gpu is true, false for a test without a device; its Extract copies the player's VSync into render's
// Presentation). Sim and Frame need ConfigureClient (their
// ClientExchange and SceneDirector).
[[nodiscard]] O::WorldDescription ClientSimWorld(ClientOptions options);
// "Solo": the game's Local World (world::LocalWorld; no net), the single player's game; the same exchange as "Sim".
[[nodiscard]] O::WorldDescription ClientSoloWorld(ClientOptions options);
[[nodiscard]] O::WorldDescription ClientFrameWorld(ClientOptions options, bool window = true);
[[nodiscard]] O::WorldDescription ClientRenderWorld(bool gpu = true);

// What the Frame World knows of the Sim World, as of the last picture it applied (a Resource of the Frame World).
struct ClientView final {
    std::uint64_t tick = 0;                         // the Sim World's step
    std::uint64_t stepNanoseconds = 0;              // its length
    std::optional<std::uint64_t> latestConfirmed;   // the newest step the Match confirmed (NetReplicaState)
    bool synced = false;                            // the clock sync has an estimate of the Match's tick (ClockSync)
    double remoteTick = 0.0;                        // the Match's tick now, estimated (ClockSync::remoteTick)
    double rttTicks = 0.0;                          // ClockSync::rttTicks
    double jitterTicks = 0.0;                       // ClockSync::jitterTicks
    bool connected = false;                         // the own Peer is there
    GYO::Kernel::EntityId ownPlayer = 0;            // world::OwnPlayer of the Sim World (0: not yet)
    std::string mismatch;                           // world::ContentMismatch::reason
    std::string mismatchArena;                      // world::ContentMismatch::matchArena
    world::MapLoad mapLoad;                         // how far the Sim (or Solo) World has come with its map
    std::uint64_t deaths = 0;                       // a Local World's: its player's deaths so far (world::LocalDeaths)
    bool started = false;                           // the match has started (world::MatchInfo::phase)
    bool ended = false;                             // the connection ended (refused, timed out, closed by the Match)
    std::string endReason;                          // why, in the player's words
};
void HashValue(GYO::Kernel::Hasher&, const ClientView&);

// What the Client shows, for its program and its tests.
struct ClientStatus final {
    bool connected = false;
    bool ready = false;     // loaded and drawn (the loading screen's work is done; the Match is told)
    double progress = 0.0;  // the loading's share done, 0 to 1
    bool started = false;   // the match has started (the loading screen is gone)
    bool playing = false;  // its own player is there
    bool captured = false; // it has the pointer
    std::string mismatch;  // the content mismatch, if any
    bool ended = false;    // the connection ended (refused, timed out, closed by the Match)
    std::string endReason; // why, in the player's words
};
// Of the Frame World.
[[nodiscard]] ClientStatus StatusOf(GYO::Kernel::World& world);

// What the World that draws has shown (a Resource of the game scene's Frame World): the program writes it between
// frames from the drawing's status (sdl_gpu's SDLGPUStatus, through an exchange function), for the loading screen's
// warm-up (game-flow declaration 3.5). Not known (a test without a device): the warm-up is not waited for.
struct DrawnFrames final {
    bool known = false;
    std::uint64_t shown = 0;    // frames drawn and shown
    std::uint64_t skipped = 0;  // draws skipped (a texture, mesh or pipeline not there yet)
};
void HashValue(GYO::Kernel::Hasher&, const DrawnFrames&);

} // namespace object_fps_pvp::client
