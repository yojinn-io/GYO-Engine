#pragma once

// The game's Client: the World of world/PvpWorld.hpp as a client (K3: it predicts its own player), and around it
// what a player sees, hears and does, all of it only at the Client (none of it writes decision data):
//
//   Main/PreUpdate   ReadControls   keys and mouse -> the controls (pointer taken or given back: window's cursor
//                                   grab; the view; the held keys; the presses)
//   FixedUpdate/First WriteStepInput the input of the step that runs now -> the own Peer's PeerInput slot of this
//                                   step, before the net Module's ApplyInput (not while resimulating)
//   Main/Update      ShowArena      the arena's model (render's Mesh3d of the baked voxel file as a model asset)
//                    ShowHud        the 2D layer: crosshair, HP, ammo, what the player should do; a hit's feedback
//                                   (present/HitFeedback): the red flash, the arc toward the attacker, the HP in red
//                    PlaceCamera    the first-person camera at the own player's eye, turned by the Client's view
//                                   (and shaken by a hit: the camera only, not the view the player controls)
//                    MoveBodies     a stand-in body for every other living player follows it (or goes)
//                    AddBodies      ... and is made for one that has none
//                    PlaySounds     shots, hits, footsteps (present/SoundCues), at the moment of the frame
//
// The pictures are drawn by a second World, "Render", that extracts from this one (render_world, camera_render,
// sprite_render, mesh_render and sdl_gpu). Entities this World makes outside the step carry the rollback Module's
// KeepThroughRollback: a rollback must not take the camera or the pictures away.

#include "object_fps_pvp/present/HitFeedback.hpp"
#include "object_fps_pvp/present/SoundCues.hpp"
#include "object_fps_pvp/world/PvpWorld.hpp"

#include "engine/image/ImageData.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::client {

namespace O = GYO::Engine::Orchestration;

// What the Client reads before it starts, besides the game's content: the sound and hit feedback settings and the
// palette pictures of the voxel models (the arenas', by arena index, and the stand-in body's).
struct PresentationContent final {
    present::SoundSettings sound;
    present::HitFeedbackSettings hitFeedback;
    std::vector<std::shared_ptr<const GYO::Engine::Image::ImageData>> arenaPalettes;
    std::shared_ptr<const GYO::Engine::Image::ImageData> standInPalette;
};

// Reads it from the game's asset directory (audio/sfx.json, ui/hit_feedback.json, characters/stand_in.voxel_mesh.json);
// nullopt with error naming the file.
[[nodiscard]] std::optional<PresentationContent> LoadPresentation(const std::string& root, const world::GameContent& game,
                                                                  std::string& error);

struct ClientOptions final {
    std::shared_ptr<const world::GameContent> game;
    std::shared_ptr<const PresentationContent> presentation;
    std::size_t arena = 0;
    std::uint64_t userId = 0;
};

// The Product's part (its configure): the game's loaders join the asset store (the voxel model loader, fonts,
// sounds), and a submitter of sdl_audio's mixer when there is one (none: the Client is silent).
void ConfigureClient(O::ProductSetup& setup);

// The two Worlds: "Game" (the client World with the Client's Systems; units listed: the Modules it needs and
// sdl_window) and "Render" (extracts from it; with sdl_gpu when gpu is true, false for a test without a device).
[[nodiscard]] O::WorldDescription ClientGameWorld(ClientOptions options, bool window = true);
[[nodiscard]] O::WorldDescription ClientRenderWorld(bool gpu = true);

// What the Client shows, for its program and its tests.
struct ClientStatus final {
    bool connected = false;
    bool playing = false;  // its own player is there
    bool captured = false; // it has the pointer
    std::string mismatch;  // the content mismatch, if any
};
[[nodiscard]] ClientStatus StatusOf(GYO::Kernel::World& world);

} // namespace object_fps_pvp::client
