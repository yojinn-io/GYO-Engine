#include "object_fps_pvp/client/ClientWorld.hpp"

#include "object_fps_pvp/client/VoxelModelLoader.hpp"
#include "client/ClientExchange.hpp"
#include "client/SceneBuilds.hpp"
#include "object_fps_pvp/data/VoxelMeshFile.hpp"
#include "object_fps_pvp/present/Controls.hpp"
#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"

#include "engine/asset/AssetStore.hpp"
#include "object_fps_pvp/character/CharacterRig.hpp"
#include "engine/model/FbxLoader.hpp"
#include "engine/model/Animation.hpp"
#include "engine/math/scalar/ColorSpace.hpp"
#include "data/JsonFields.hpp"
#include "engine/json/Document.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/image/PngLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/WavLoader.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/Replication.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/text/FontLoader.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/ui/UiDocumentLoader.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/text/GlyphAtlas.hpp"
#include "engine/text/TextContext.hpp"
#include "engine/text/TextLayout.hpp"
#include "engine/text/TextStyle.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <random>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace object_fps_pvp::client {

namespace K = GYO::Kernel;
namespace A = GYO::Engine::Asset;
namespace AU = GYO::Engine::Audio;
namespace Base = GYO::Engine::Base;
namespace C = GYO::Engine::Camera;
namespace I = GYO::Engine::Input;
namespace IM = GYO::Engine::Image;
namespace Math = GYO::Engine::Math;
namespace N = GYO::Engine::Net;
namespace R = GYO::Engine::Render;
namespace RB = GYO::Engine::Rollback;
namespace T = GYO::Engine::Transform;
namespace TM = GYO::Engine::Time;
namespace TX = GYO::Engine::Text;
namespace Ui = GYO::Engine::Ui;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;
namespace PW = object_fps_pvp::world;

void HashValue(K::Hasher&, const ClientView&) {}

namespace {

// ---- The Client's own entities: fixed ids outside both allotter ranges (world::kMatchIds, world::kClientIds), put
// in by the prepare function before the first step; the stand-in bodies come from the Client's allotter.
constexpr K::EntityId kCamera = 3'000'010;
constexpr K::EntityId kHudCamera = 3'000'011;
constexpr K::EntityId kArenaPicture = 3'000'012;
constexpr K::EntityId kCrosshair = 3'000'013;
constexpr K::EntityId kFlash = 3'000'014;
constexpr K::EntityId kHpText = 3'000'015;
constexpr K::EntityId kAmmoText = 3'000'016;
constexpr K::EntityId kStatusText = 3'000'017;
constexpr K::EntityId kHitDirection = 3'000'019;
constexpr K::EntityId kOverlay = 3'000'020;

// The 2D layer: 1280 x 720 units, the origin at the centre, y up; drawn by the HUD camera only.
constexpr float kHudWidth = 1280.0F;
constexpr float kHudHeight = 720.0F;
constexpr std::uint32_t kHudLayer = 2;
// The first-person weapon: a layer of its own, drawn by the viewmodel camera only (render page section 8.1).
constexpr std::uint32_t kViewLayer = 1;
constexpr K::EntityId kViewCamera = 3'000'018;

// The world camera (master's: near 0.05, far 150) and its field of view.
constexpr float kNear = 0.05F;
constexpr float kFar = 150.0F;
constexpr float kVerticalFov = 1.2F;  // about 69 degrees

[[nodiscard]] K::SystemResult Failed(std::string message) {
    return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, std::move(message)));
}

// ---- Resources

// The pictures and sounds the Client asks the asset store for, and what it has made of them.
struct ClientAssets final {
    bool requested = false;
    std::vector<A::Handle<GYO::Engine::Model::ModelData>> arenaModels;  // by arena index
    A::Handle<GYO::Engine::Model::ModelData> body;       // the character (Player_V20)
    A::Handle<GYO::Engine::Model::ModelData> heldWeapon; // the third-person weapon
    A::Handle<IM::ImageData> heldWeaponImage;
    A::Handle<GYO::Engine::Model::ModelData> viewmodel;  // the first-person arms and weapon
    std::vector<A::Handle<IM::ImageData>> viewmodelImages;  // per material
    A::Handle<TX::FontData> font;
    std::map<std::string, A::Handle<AU::Clip>> clips;  // by asset id
};
void HashValue(K::Hasher&, const ClientAssets&) {}

struct ClientControls final {
    present::Controls controls;
    // The life the view was turned to (a new life faces its spawn's yaw).
    std::uint32_t facedLife = 0;
    K::EntityId facedPlayer = 0;
};
void HashValue(K::Hasher&, const ClientControls&) {}

// The Sim World: the controls the Frame World sent, until a step takes them (WriteStepInput).
//
// The controls go stale when the Frame World has sent none for a while (it is held: a dragged window, a long load).
// Until then the steps keep the last held keys and view; past it the movement is neutral and the presses are dropped,
// the view and the crouch stay (the user's decision 2026-11-25, after master's D30). The limit follows how often the
// Frame World sends: the longest of the recent intervals between two arrivals times kStaleIntervals, and at least
// kStaleMinimum, so one long frame does not stop the player midway. An interval past the limit was a stop, not the
// Frame World's pace, and is not counted. The time is the Sim World's RealTime.
struct SimControls final {
    present::StepInput pending;
    std::optional<WT::Nanoseconds> lastArrival;  // when controls last came (none yet: stale)
    std::vector<WT::Nanoseconds> recent;         // the recent intervals between two arrivals, oldest first
    bool stale = true;
};

// The game's own numbers (not master's): 200 ms is four frames at 20 frames a second (taken here as the lowest rate
// the Client plays at); four intervals of the Frame World's own pace; sixteen of them make "recent".
constexpr WT::Nanoseconds kStaleMinimum = 200'000'000;
constexpr WT::Nanoseconds kStaleIntervals = 4;
constexpr std::size_t kRecentIntervals = 16;

WT::Nanoseconds StaleLimit(const SimControls& state) {
    const WT::Nanoseconds longest = state.recent.empty() ? 0 : *std::max_element(state.recent.begin(), state.recent.end());
    return std::max(kStaleMinimum, kStaleIntervals * longest);
}

void HashValue(K::Hasher&, const SimControls&) {}

// The Frame World: the players it shows (the Sim World's, by their ids) and the last picture applied.
struct FrameMirror final {
    std::set<K::EntityId> players;
    std::uint64_t seen = 0;
};
void HashValue(K::Hasher&, const FrameMirror&) {}

// One mesh of a player's body (its index in the body model), or the weapon in its hand (mesh: kHeld).
inline constexpr std::uint32_t kHeld = 0xFFFF'FFFFU;
struct CharacterPart final {
    K::EntityId player = 0;
    std::uint32_t mesh = 0;
};
void HashValue(K::Hasher&, const CharacterPart&) {}

// One mesh of the first-person weapon.
struct ViewmodelPart final {
    std::uint32_t mesh = 0;
};
void HashValue(K::Hasher&, const ViewmodelPart&) {}

// Each shown player's animation (the Client's own: port declaration PV6), and the first-person weapon's state.
struct ClientCharacters final {
    std::map<K::EntityId, sim::Locomotion> locomotion;
    // The reload whose sound has played (its end tick), so it plays once.
    std::uint64_t reloadSounded = 0;
};
void HashValue(K::Hasher&, const ClientCharacters&) {}

struct ClientScene final {
    std::shared_ptr<const PresentationContent> presentation;
    std::string shownArena;
};
void HashValue(K::Hasher&, const ClientScene&) {}

// The HUD's texts: one picture each, made again only when the text changes.
struct HudText final {
    std::string text;
    std::shared_ptr<const IM::ImageData> image;
};
struct ClientHud final {
    TX::TextContext text;
    std::optional<TX::FontId> font;
    TX::GlyphAtlas atlas{TX::AtlasOptions{}};
    std::map<K::EntityId, HudText> texts;
    present::LocalHitFeedback feedback;
    // This frame's hit feedback (ShowHud samples it; PlaceCamera shakes the camera by it).
    present::HitFeedbackSample sample;
};
void HashValue(K::Hasher&, const ClientHud&) {}

// How the connection ended, once it did (net's PeerDisconnected: refused, timed out, closed by the Match).
struct ClientSessionEnd final {
    bool ended = false;
    std::string reason;
};
void HashValue(K::Hasher&, const ClientSessionEnd&) {}

// What the sounds have already played for.
struct ClientSound final {
    AU::Submitter* submitter = nullptr;
    std::map<K::EntityId, std::uint64_t> shotsHeard;  // per player: the last shot tick played
    std::uint64_t confirmedHeard = 0;                 // the own ShotRecord tick played
    present::FootstepTracker steps;
    // The own player's hits, as the hit feedback sees them (a new hit of a living player sounds).
    present::LocalHitFeedback hits;
};
void HashValue(K::Hasher&, const ClientSound&) {}

// The Product's submitter (ConfigureClient).
struct SoundPort final {
    explicit SoundPort(AU::Submitter s) : submitter(std::move(s)) {}
    AU::Submitter submitter;
};

// ---- Helpers

Math::Quaternion ViewRotation(const float yaw, const float pitch) {
    // Left-handed, +Z forward (math.md): yaw about +Y turns +Z toward +X, a positive pitch about +X looks down.
    return Math::Multiply(Math::MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, yaw),
                          Math::MakeQuaternionFromAxisAngle({1.0F, 0.0F, 0.0F}, pitch));
}

void Place(K::World& world, const K::EntityId id, const T::Transform& transform) {
    world.Insert(id, transform);
    world.Insert(id, T::GlobalTransform{T::ToMatrix(transform)});
}

std::shared_ptr<const IM::ImageData> PaletteOf(const data::VoxelMeshFile& file) {
    const data::VoxelPaletteImage image = data::PaletteImage(file);
    return std::make_shared<const IM::ImageData>(image.width, image.height, IM::ColorSpace::Srgb, image.pixels);
}

// A text as a white picture with the glyphs' coverage in alpha (tinted by the sprite's colour).
std::shared_ptr<const IM::ImageData> TextPicture(ClientHud& hud, const std::string& utf8, const float pixelSize) {
    const TX::FontId fonts[] = {*hud.font};
    const TX::TextSpan spans[] = {{0, static_cast<std::uint32_t>(utf8.size()), {fonts, pixelSize, TX::LineHeight::Relative(1.2F)}}};
    const TX::ShapedText shaped = hud.text.Shape(utf8, spans, {});
    const TX::TextLayout layout = hud.text.Layout(shaped, {});
    const TX::RasterResult raster = hud.text.Rasterize(layout, hud.atlas);
    const auto width = static_cast<std::uint32_t>(std::ceil(layout.width)) + 4;
    const auto height = static_cast<std::uint32_t>(std::ceil(layout.height)) + 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4, 0);
    for (std::size_t i = 0; i < pixels.size(); i += 4) pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
    for (std::size_t index = 0; index < layout.glyphs.size() && index < raster.placements.size(); ++index) {
        const TX::PositionedGlyph& glyph = layout.glyphs[index];
        const TX::GlyphPlacement& placement = raster.placements[index];
        if (placement.status != TX::GlyphRasterStatus::Placed) continue;
        const TX::AtlasPageView page = hud.atlas.Page(placement.page);
        const float lineX = glyph.line < layout.lines.size() ? layout.lines[glyph.line].x : 0.0F;
        const int left = 2 + static_cast<int>(std::round(glyph.x - lineX)) + placement.left;
        const int top = 2 + static_cast<int>(std::round(glyph.y)) - placement.top;
        for (std::uint32_t y = 0; y < placement.rect.height; ++y) {
            for (std::uint32_t x = 0; x < placement.rect.width; ++x) {
                const int px = left + static_cast<int>(x);
                const int py = top + static_cast<int>(y);
                if (px < 0 || py < 0 || px >= static_cast<int>(width) || py >= static_cast<int>(height)) continue;
                const std::uint8_t coverage =
                    page.pixels[static_cast<std::size_t>(placement.rect.y + y) * page.rowPitch + placement.rect.x + x];
                std::uint8_t& alpha = pixels[(static_cast<std::size_t>(py) * width + static_cast<std::size_t>(px)) * 4 + 3];
                alpha = std::max(alpha, coverage);
            }
        }
    }
    return std::make_shared<const IM::ImageData>(width, height, IM::ColorSpace::Srgb, std::move(pixels));
}

void Rect(std::vector<R::Vertex2D>& vertices, std::vector<std::uint32_t>& indices, const float x0, const float y0, const float x1,
          const float y1, const R::Color8 color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({{x0, y0}, {0, 0}, color});
    vertices.push_back({{x1, y0}, {0, 0}, color});
    vertices.push_back({{x1, y1}, {0, 0}, color});
    vertices.push_back({{x0, y1}, {0, 0}, color});
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) indices.push_back(base + i);
}

R::Shape2d Crosshair() {
    R::Shape2d shape;
    const R::Color8 white{255, 255, 255, 220};
    Rect(shape.vertices, shape.indices, -12, -1, -4, 1, white);
    Rect(shape.vertices, shape.indices, 4, -1, 12, 1, white);
    Rect(shape.vertices, shape.indices, -1, -12, 1, -4, white);
    Rect(shape.vertices, shape.indices, -1, 4, 1, 12, white);
    return shape;
}

// The arc of dots around the crosshair toward the attacker (bearing 0 straight up, positive to the right).
R::Shape2d HitDirection(const present::HitFeedbackSettings::Direction& direction, const std::optional<float> bearing, const float alpha) {
    R::Shape2d shape;
    if (!bearing || alpha <= 0.0F || direction.dots == 0) return shape;
    const auto byte = [](const float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
    const R::Color8 color{byte(direction.color[0]), byte(direction.color[1]), byte(direction.color[2]), byte(alpha)};
    const float half = direction.dotPixels / 2;
    for (std::uint32_t i = 0; i < direction.dots; ++i) {
        const float t = direction.dots == 1 ? 0.5F : static_cast<float>(i) / static_cast<float>(direction.dots - 1);
        const float angle = *bearing + (t - 0.5F) * direction.arcRadians;
        const float x = direction.radiusPixels * std::sin(angle);
        const float y = direction.radiusPixels * std::cos(angle);
        Rect(shape.vertices, shape.indices, x - half, y - half, x + half, y + half, color);
    }
    return shape;
}

R::Shape2d Flash(const std::array<float, 3> color, const float alpha) {
    R::Shape2d shape;
    const auto byte = [](const float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
    Rect(shape.vertices, shape.indices, -kHudWidth / 2, -kHudHeight / 2, kHudWidth / 2, kHudHeight / 2,
         R::Color8{byte(color[0]), byte(color[1]), byte(color[2]), byte(alpha)});
    return shape;
}


// ---- Systems

// The pictures, the font and the sounds, asked for once; the scene and the sounds use them once they are there.
K::SystemResult RequestAssets(K::Res<WA::AssetRequests> requests, K::Res<ClientScene> scene, K::Res<PW::GameRules> rules,
                              K::ResMut<ClientAssets> assets) {
    if (assets.Read().requested) return {};
    ClientAssets& a = assets.Get();
    a.requested = true;
    if (rules->character) a.body = requests->Load<GYO::Engine::Model::ModelData>(A::AssetId(rules->character->bodyAssetId));
    a.heldWeapon = requests->Load<GYO::Engine::Model::ModelData>(A::AssetId(scene->presentation->viewmodel.heldModel));
    a.heldWeaponImage = requests->Load<IM::ImageData>(A::AssetId(scene->presentation->viewmodel.heldImage));
    a.viewmodel = requests->Load<GYO::Engine::Model::ModelData>(A::AssetId("object_fps_pvp.model.nailer_v2"));
    for (const std::string& image : scene->presentation->viewmodel.materialImages) {
        a.viewmodelImages.push_back(image.empty() ? A::Handle<IM::ImageData>{} : requests->Load<IM::ImageData>(A::AssetId(image)));
    }
    if (!scene->presentation->viewmodel.reloadSound.empty() && a.clips.count(scene->presentation->viewmodel.reloadSound) == 0) {
        const std::string& id = scene->presentation->viewmodel.reloadSound;
        a.clips[id] = requests->Load<AU::Clip>(A::AssetId(id));
    }
    a.font = requests->Load<TX::FontData>(A::AssetId("object_fps_pvp.font.ui"));
    for (const present::SoundSettings::Cue& cue : scene->presentation->sound.cues) {
        if (!cue.clip.empty() && a.clips.count(cue.clip) == 0) a.clips[cue.clip] = requests->Load<AU::Clip>(A::AssetId(cue.clip));
    }
    return {};
}

// The arena's model asset id: the game's catalog names it after its geometry file.
std::string ArenaModelId(const sim::ArenaContent& arena) {
    std::string name = arena.geometry;
    const std::string suffix = ".voxel_mesh.json";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
        name.resize(name.size() - suffix.size());
    }
    return "object_fps_pvp.model." + name;
}

// The frame's keys and mouse into the controls (the pointer, the view, the held keys, the presses), and the window
// asked to lock and hide the cursor while the game has it. Main/PreUpdate after the input and window sets: the steps
// of this frame run after it.
K::SystemResult ReadControls(K::Res<I::ButtonInput<I::KeyCode>> keys, K::Res<I::ButtonInput<I::MouseButton>> mouse,
                             K::Res<I::AccumulatedMouseMotion> motion, K::Res<I::CursorPosition> cursor,
                             K::Res<W::WindowState> window, K::Res<PW::LocalUser> user,
                             K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>> players,
                             K::ResMut<ClientControls> state, K::ResMut<W::WindowRequest> request) {
    ClientControls& c = state.Get();
    // A new life of the own player (joined or respawned) faces its spawn.
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat) {
        if (owner.userId != user->userId) return;
        if (id != c.facedPlayer || combat.lives != c.facedLife) {
            c.controls.Face(body.yaw, body.pitch);
            c.facedPlayer = id;
            c.facedLife = combat.lives;
        }
    });
    present::ControlsFrame frame;
    frame.focused = !window->known || window->focused;
    frame.insideWindow = cursor->position.has_value();
    frame.tab = keys->JustPressed(I::KeyCode::Tab);
    frame.escape = keys->JustPressed(I::KeyCode::Escape);
    frame.click = mouse->JustPressed(I::MouseButton::Left);
    frame.jump = keys->JustPressed(I::KeyCode::Space);
    frame.reload = keys->JustPressed(I::KeyCode::R);
    frame.forward = keys->Pressed(I::KeyCode::W);
    frame.back = keys->Pressed(I::KeyCode::S);
    frame.left = keys->Pressed(I::KeyCode::A);
    frame.right = keys->Pressed(I::KeyCode::D);
    frame.crouch = keys->Pressed(I::KeyCode::ControlLeft);
    frame.motion = motion->delta;
    c.controls.Apply(frame);
    // The pointer: hidden and held while the game has it (window's Locked: relative motion).
    const bool captured = c.controls.Captured();
    if (request.Read().cursorVisible == captured || (request.Read().cursorGrab == W::CursorGrab::Locked) != captured) {
        request->cursorVisible = !captured;
        request->cursorGrab = captured ? W::CursorGrab::Locked : W::CursorGrab::None;
    }
    return {};
}

// Sim World: the own Peer's input buffer, for WriteStepInput (Main/PreUpdate, after the net Module made the Peer).
K::SystemResult PrepareOwnInput(K::Res<N::NetWorldLink> link, K::Query<K::Read<N::PeerInput<PW::PlayerInput>>> inputs,
                                K::CommandsFor<K::Inserts<N::PeerInput<PW::PlayerInput>>, K::Removes<>> commands) {
    if (link->clientPeer != 0 && !inputs.Contains(link->clientPeer)) commands.Insert(link->clientPeer, N::PeerInput<PW::PlayerInput>{});
    return {};
}

// The input of the step that runs now, written into the own Peer's slot of this step before the net Module's entry
// (ApplyInput) reads it: each slot is written once, before its step runs and before it is sent (the net Module sends
// a tick's slots once and the Match keeps each (Peer, tick) once). FixedUpdate/First, not while resimulating (the
// rollback writes the recorded inputs back then). lightyear's client input System in FixedPreUpdate
// (crates/inputs/inputs/src/client.rs:41, 186-190, 418-445).
// The input of the step that runs now from the controls the Frame World sent: the held keys and the view as last
// sent; the presses go to this step only. Stale controls (SimControls): no movement, no press; the view and the crouch
// stay.
PW::PlayerInput StepInputOf(SimControls& state) {
    present::StepInput step = state.pending;
    state.pending.jump = state.pending.fire = state.pending.reload = false;
    if (state.stale) {
        step.forward = step.right = 0.0F;
        step.present = false;
        step.jump = step.fire = step.reload = false;
    }
    PW::PlayerInput input;
    input.forward = step.forward;
    input.right = step.right;
    input.yaw = step.yaw;
    input.pitch = step.pitch;
    input.present = step.present ? 1 : 0;
    input.jump = step.jump ? 1 : 0;
    input.crouch = step.crouch ? 1 : 0;
    input.fire = step.fire ? 1 : 0;
    input.reload = step.reload ? 1 : 0;
    return input;
}

K::SystemResult WriteStepInput(K::Res<WT::Tick> tick, K::Res<N::NetWorldLink> link, K::Res<N::NetReplicaState> replica,
                               K::Query<K::Write<N::PeerInput<PW::PlayerInput>>> inputs, K::ResMut<SimControls> state) {
    if (link->clientPeer == 0 || !inputs.Contains(link->clientPeer)) return {};
    const PW::PlayerInput input = StepInputOf(*state);
    // The time the player saw: the remote players stand at the confirmed point.
    const std::uint32_t seen = replica->latestConfirmed.value_or(0);
    inputs.GetMut<N::PeerInput<PW::PlayerInput>>(link->clientPeer)
        ->Set(static_cast<std::uint32_t>(tick->value), input, N::ViewTime{seen, 0});
    return {};
}

// The Solo World (no net): the step's input straight into the local player's PlayerInput, FixedUpdate/First (before
// the game's Systems read it).
K::SystemResult WriteLocalInput(K::Query<K::Read<PW::Owner>, K::Write<PW::PlayerInput>> players, K::Res<PW::LocalUser> user,
                                K::ResMut<SimControls> state) {
    const PW::PlayerInput input = StepInputOf(*state);
    for (const K::EntityId id : players.Entities()) {
        if (players.Get<PW::Owner>(id)->userId == user->userId) *players.GetMut<PW::PlayerInput>(id) = input;
    }
    return {};
}

// The arena's picture: the model of the arena the World plays (FollowMatchArena may have changed it).
K::SystemResult ShowArena(K::Res<PW::GameRules> rules, K::Res<WA::AssetRequests> requests, K::Res<PW::ContentRef> content,
                          K::ResMut<ClientScene> scene, K::CommandsFor<K::Inserts<R::Mesh3d>, K::Removes<>> commands) {
    const sim::ArenaContent& arena = rules->arena;
    if (scene.Read().shownArena == arena.id || !content->content) return {};
    const auto& arenas = content->content->arenas;
    for (std::size_t i = 0; i < arenas.size(); ++i) {
        if (arenas[i].content.id != arena.id) continue;
        R::Mesh3d mesh;
        mesh.model = requests->Load<GYO::Engine::Model::ModelData>(A::AssetId(ArenaModelId(arena)));
        if (i < scene->presentation->arenaPalettes.size()) mesh.image = scene->presentation->arenaPalettes[i];
        mesh.filter = R::TextureFilter::Nearest;
        commands.Insert(kArenaPicture, std::move(mesh));
        scene->shownArena = arena.id;
        break;
    }
    return {};
}

// The camera at the own player's eye, turned by the Client's own view (the input it sends).
K::SystemResult PlaceCamera(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientControls> controls,
                            K::Res<ClientHud> hud, K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>> players,
                            K::Query<K::Write<T::Transform>, K::With<C::Camera3d>> cameras) {
    players.ForEach([&](K::EntityId, const PW::Owner& owner, const PW::PlayerBody& body) {
        if (owner.userId != user->userId) return;
        const float eye = sim::EyeHeight(rules->arena, rules->stance, body.crouched != 0);
        cameras.ForEach([&](K::EntityId, K::Mut<T::Transform> camera) {
            T::Transform t = camera.Read();
            t.translation = body.feet + Math::Vec3{0.0F, eye, 0.0F};
            // The hit's shake reaches the camera only: the view the player controls (its input, its shots) stays.
            t.rotation = Math::Multiply(ViewRotation(controls->controls.Yaw(), controls->controls.Pitch() + hud->sample.shakePitchRadians),
                                        Math::MakeQuaternionFromAxisAngle({0.0F, 0.0F, 1.0F}, hud->sample.shakeRollRadians));
            camera.Get() = t;
        });
    });
    return {};
}

// ---- The characters: every other living player as the body model, posed by its animation, with the weapon in its
// hand; the own player as the first-person weapon before the viewmodel camera.

// A similarity matrix (rotation, uniform scale, translation) as a Transform.
T::Transform FromSimilarity(const Math::Matrix4& m) {
    T::Transform t;
    const auto& v = m.values;
    t.translation = {v[12], v[13], v[14]};
    const float scale = Math::Length(Math::Vec3{v[0], v[1], v[2]});
    t.scale = {scale, scale, scale};
    // The rotation from the normalized 3x3 (columns are the axes).
    const float r00 = v[0] / scale, r01 = v[4] / scale, r02 = v[8] / scale;
    const float r10 = v[1] / scale, r11 = v[5] / scale, r12 = v[9] / scale;
    const float r20 = v[2] / scale, r21 = v[6] / scale, r22 = v[10] / scale;
    Math::Quaternion q;
    const float trace = r00 + r11 + r22;
    if (trace > 0.0F) {
        const float k = 0.5F / std::sqrt(trace + 1.0F);
        q = {(r21 - r12) * k, (r02 - r20) * k, (r10 - r01) * k, 0.25F / k};
    } else if (r00 > r11 && r00 > r22) {
        const float k = 2.0F * std::sqrt(1.0F + r00 - r11 - r22);
        q = {0.25F * k, (r01 + r10) / k, (r02 + r20) / k, (r21 - r12) / k};
    } else if (r11 > r22) {
        const float k = 2.0F * std::sqrt(1.0F + r11 - r00 - r22);
        q = {(r01 + r10) / k, 0.25F * k, (r12 + r21) / k, (r02 - r20) / k};
    } else {
        const float k = 2.0F * std::sqrt(1.0F + r22 - r00 - r11);
        q = {(r02 + r20) / k, (r12 + r21) / k, 0.25F * k, (r10 - r01) / k};
    }
    t.rotation = Math::NormalizeOrIdentity(q);
    return t;
}

R::Color8 MaterialColor(const GYO::Engine::Model::ModelData& model, const std::uint32_t material) {
    if (material >= model.materials.size()) return R::Color8{255, 255, 255, 255};
    const auto& c = model.materials[material].baseColor;
    const auto byte = [](const float linear) { return static_cast<std::uint8_t>(std::lround(Math::EncodeSrgb(linear) * 255.0F)); };
    return R::Color8{byte(c[0]), byte(c[1]), byte(c[2]), static_cast<std::uint8_t>(std::lround(std::clamp(c[3], 0.0F, 1.0F) * 255.0F))};
}


// Every other living player's body parts follow its pose; the parts of a player gone or dead... stay for the death
// (the death clip plays), and go with the player.
K::SystemResult PoseCharacters(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientView> sim,
                               K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::HitRecord>> players,
                               K::Query<K::Read<CharacterPart>> parts, K::Query<K::Write<T::Transform>> transforms,
                               K::Query<K::Write<R::SkinPose>> poses, K::ResMut<ClientCharacters> characters, K::Despawn despawn) {
    if (!rules->character) return {};
    const character::CharacterRig& rig = *rules->character;
    const std::uint64_t step = sim->stepNanoseconds;
    // The other players stand at the confirmed point (the Match's step their state belongs to), not at this Client's
    // predicted step: their shots, hits and lives are timed on it (lightyear: an interpolated entity has its own
    // timeline, crates/replication/interpolation/src/timeline.rs).
    const std::uint64_t seen = sim->latestConfirmed ? *sim->latestConfirmed : sim->tick;
    // Each shown player's pose this frame.
    std::map<K::EntityId, std::vector<Math::Matrix4>> modelSpace;
    std::map<K::EntityId, const PW::PlayerBody*> bodies;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat,
                        const PW::HitRecord& hits) {
        if (owner.userId == user->userId) return;
        const sim::AnimationInput input = PW::AnimationInputOf(body, combat, hits, *rules, seen, step);
        sim::Locomotion& locomotion = characters->locomotion[id];
        sim::Advance(locomotion, input, rig.rules);
        std::vector<GYO::Engine::Model::LocalTransform> local;
        character::ComposeBody(rig, locomotion, sim::ResolveActions(locomotion, input, rig.rules), input.crouched,
                               input.seconds - input.lifeSeconds, local);
        character::ModelSpace(rig, local, modelSpace[id]);
        bodies[id] = &body;
    });
    for (auto it = characters->locomotion.begin(); it != characters->locomotion.end();) {
        it = bodies.count(it->first) == 0 ? characters->locomotion.erase(it) : std::next(it);
    }
    for (const K::EntityId part : parts.Entities()) {
        const CharacterPart& which = *parts.Get<CharacterPart>(part);
        const auto shown = bodies.find(which.player);
        if (shown == bodies.end()) {
            despawn.Entity(part);
            continue;
        }
        const PW::PlayerBody& body = *shown->second;
        const std::vector<Math::Matrix4>& nodes = modelSpace[which.player];
        const T::Transform t = which.mesh == kHeld ? FromSimilarity(character::WeaponToWorld(rig, nodes, body.feet, body.yaw))
                                                   : FromSimilarity(character::BodyToWorld(rig, body.feet, body.yaw));
        if (auto transform = transforms.GetMut<T::Transform>(part)) transform.Get() = t;
        if (which.mesh != kHeld) {
            if (auto pose = poses.GetMut<R::SkinPose>(part)) pose->nodes = nodes;
        }
    }
    return {};
}

// The parts of every other player that has none (once the models are asked for): one entity per body mesh, and the
// weapon in its hand.
K::SystemResult AddCharacters(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientAssets> assets,
                              K::Res<WA::AssetView> view,
                              K::Query<K::Read<PW::Owner>> players, K::Query<K::Read<CharacterPart>> parts,
                              K::Spawn<K::Bundle<R::Mesh3d, T::Transform, T::GlobalTransform, R::SkinPose, CharacterPart>> spawnBody,
                              K::Spawn<K::Bundle<R::Mesh3d, T::Transform, T::GlobalTransform, CharacterPart>> spawnHeld) {
    const GYO::Engine::Model::ModelData* body = view->Get(assets->body);
    if (body == nullptr) return {};
    // The poses are the rig's (read from the same file): the drawn copy must have its nodes.
    if (!rules->character || body->nodes.size() != rules->character->body->nodes.size()) {
        return Failed("the body model the store loaded does not have the character's nodes");
    }
    std::map<K::EntityId, bool> shown;
    parts.ForEach([&](K::EntityId, const CharacterPart& part) { shown[part.player] = true; });
    for (const K::EntityId player : players.Entities()) {
        if (players.Get<PW::Owner>(player)->userId == user->userId || shown.count(player) != 0) continue;
        for (std::uint32_t m = 0; m < body->meshes.size(); ++m) {
            R::Mesh3d mesh;
            mesh.model = assets->body;
            mesh.mesh = m;
            mesh.color = MaterialColor(*body, body->meshes[m].material);
            auto spawned = spawnBody.New(std::move(mesh), T::Transform{}, T::GlobalTransform{}, R::SkinPose{}, CharacterPart{player, m});
            if (!spawned) return Base::Err(std::move(spawned).error());
        }
        R::Mesh3d held;
        held.model = assets->heldWeapon;
        held.image = assets->heldWeaponImage;
        auto spawned = spawnHeld.New(std::move(held), T::Transform{}, T::GlobalTransform{}, CharacterPart{player, kHeld});
        if (!spawned) return Base::Err(std::move(spawned).error());
    }
    return {};
}

// The first-person weapon's clip and time for the own player: the reload over its span, a shot, the draw of a new
// life, else the idle.
std::pair<std::uint32_t, float> ViewmodelClip(const ViewmodelDefinition& viewmodel, const sim::AnimationInput& input) {
    const auto& clips = viewmodel.model->clips;
    if (input.reloadStart && input.reloadEnd && input.seconds < *input.reloadEnd) {
        const double progress = std::clamp((input.seconds - *input.reloadStart) / (*input.reloadEnd - *input.reloadStart), 0.0, 1.0);
        return {viewmodel.reload, static_cast<float>(progress * clips[viewmodel.reload].duration)};
    }
    if (input.shotSeconds && *input.shotSeconds >= input.lifeSeconds && input.seconds - *input.shotSeconds < clips[viewmodel.shoot].duration) {
        return {viewmodel.shoot, static_cast<float>(input.seconds - *input.shotSeconds)};
    }
    if (input.seconds - input.lifeSeconds < clips[viewmodel.draw].duration) {
        return {viewmodel.draw, static_cast<float>(input.seconds - input.lifeSeconds)};
    }
    return {viewmodel.idle, static_cast<float>(input.seconds)};
}

// The first-person weapon's pose from the own player's state (it hangs under the viewmodel camera with a fixed
// placement: ViewmodelPlacement), hidden while the player is dead, and the reload's sound at the clip's magazine_in.
K::SystemResult PoseViewmodel(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientView> sim,
                              K::Res<ClientScene> scene, K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Res<ClientSound> sound,
                              K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::HitRecord>> players,
                              K::Query<K::Read<ViewmodelPart>, K::Write<R::SkinPose>, K::Write<R::RenderLayers>> parts,
                              K::ResMut<ClientCharacters> characters) {
    const ViewmodelDefinition& viewmodel = scene->presentation->viewmodel;
    if (!viewmodel.model) return {};
    const std::uint64_t step = sim->stepNanoseconds;
    players.ForEach([&](K::EntityId, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat, const PW::HitRecord& hits) {
        if (owner.userId != user->userId) return;
        const sim::AnimationInput input = PW::AnimationInputOf(body, combat, hits, *rules, sim->tick, step);
        const auto [clip, seconds] = ViewmodelClip(viewmodel, input);
        std::vector<GYO::Engine::Model::LocalTransform> local(viewmodel.model->nodes.size());
        GYO::Engine::Model::SampleClip(viewmodel.model->nodes, viewmodel.model->clips[clip], seconds,
                                       clip == viewmodel.idle ? GYO::Engine::Model::Wrap::Loop : GYO::Engine::Model::Wrap::Clamp, local);
        std::vector<Math::Matrix4> nodes(local.size());
        GYO::Engine::Model::ComputeModelSpace(viewmodel.model->nodes, local, nodes);
        // A dead player's view has no weapon: its parts leave the viewmodel camera's layer.
        const std::uint32_t mask = combat.life == sim::LifeState::Dead ? 0U : 1U << kViewLayer;
        parts.ForEach([&](K::EntityId, const ViewmodelPart&, K::Mut<R::SkinPose> pose, K::Mut<R::RenderLayers> layers) {
            pose->nodes = nodes;
            if (layers.Read().mask != mask) layers->mask = mask;
        });
        // The reload's sound: its sync point at the clip's magazine_in, once per reload.
        if (input.reloadStart && input.reloadEnd && combat.reloadEndTick != characters->reloadSounded && sound->submitter != nullptr) {
            const double span = *input.reloadEnd - *input.reloadStart;
            const double eventAt = *input.reloadStart + span * viewmodel.magazineInSeconds / viewmodel.model->clips[viewmodel.reload].duration;
            if (input.seconds >= eventAt - viewmodel.reloadSoundSync) {
                characters->reloadSounded = combat.reloadEndTick;
                const auto found = assets->clips.find(viewmodel.reloadSound);
                if (found != assets->clips.end()) {
                    if (const AU::Clip* clip = view->Get(found->second)) {
                        static_cast<void>(sound->submitter->Play(AU::PlayRequest{*clip, TM::MonotonicClock::now(), viewmodel.reloadSoundGain,
                                                                                 viewmodel.reloadSoundGain, false}));
                    }
                }
            }
        }
    });
    return {};
}

// The first-person weapon's parts, once its model is asked for.
K::SystemResult AddViewmodel(K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Query<K::Read<ViewmodelPart>> parts,
                             K::Res<ClientScene> scene,
                             K::Spawn<K::Bundle<R::Mesh3d, T::Transform, T::GlobalTransform, R::SkinPose, ViewmodelPart, R::RenderLayers,
                                                K::ChildOf>> spawn) {
    if (parts.Count() != 0) return {};
    const GYO::Engine::Model::ModelData* model = view->Get(assets->viewmodel);
    if (model == nullptr) return {};
    for (std::uint32_t m = 0; m < model->meshes.size(); ++m) {
        R::Mesh3d mesh;
        mesh.model = assets->viewmodel;
        mesh.mesh = m;
        const std::uint32_t material = model->meshes[m].material;
        if (material < assets->viewmodelImages.size() && !assets->viewmodelImages[material].Id().Empty()) {
            mesh.image = assets->viewmodelImages[material];
        } else {
            mesh.color = MaterialColor(*model, material);
        }
        // Under the viewmodel camera, at the fixed placement: it follows the camera (Bevy's first-person view model,
        // examples/camera/first_person_view_model.rs: the arms are a child of the camera).
        const T::Transform placement = ViewmodelPlacement(scene->presentation->viewmodel);
        auto spawned = spawn.New(std::move(mesh), placement, T::GlobalTransform{T::ToMatrix(placement)}, R::SkinPose{}, ViewmodelPart{m},
                                 R::RenderLayers{1U << kViewLayer}, K::ChildOf{kViewCamera});
        if (!spawned) return Base::Err(std::move(spawned).error());
    }
    return {};
}

K::SystemResult ShowHud(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientControls> controls,
                        K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Res<ClientView> sim, K::Res<WT::ActiveTime> time,
                        K::Res<ClientScene> scene,
                        K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::HitRecord>> players,
                        K::Query<K::Write<R::Sprite>> sprites, K::Query<K::Write<R::Shape2d>> shapes, K::ResMut<ClientHud> hud) {
    ClientHud& h = hud.Get();
    if (!h.font) {
        if (const TX::FontData* font = view->Get(assets->font)) {
            auto added = h.text.AddFont(*font, 0);
            if (!added) return Failed("the UI font did not open: " + Base::Describe(added.error()));
            h.font = *added;
        }
    }
    // The own player's state.
    const PW::PlayerBody* body = nullptr;
    const PW::PlayerCombat* combat = nullptr;
    const PW::HitRecord* hits = nullptr;
    std::optional<Math::Vec3> attacker;
    std::map<K::EntityId, Math::Vec3> feet;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& b, const PW::PlayerCombat& c,
                        const PW::HitRecord& r) {
        feet[id] = b.feet;
        if (owner.userId != user->userId) return;
        body = &b;
        combat = &c;
        hits = &r;
    });
    const double now = static_cast<double>(time->elapsed) / 1e9;
    present::HitFeedbackSample feedback;
    if (body != nullptr) {
        if (hits->attacker != 0 && feet.count(hits->attacker) != 0) attacker = feet[hits->attacker];
        (void)h.feedback.Observe(present::LifeHitRecord{combat->lives, hits->hits}, combat->life, body->feet, attacker, now);
        feedback = h.feedback.Sample(now, controls->controls.Yaw(), scene->presentation->hitFeedback);
    }
    h.sample = feedback;
    if (auto flash = shapes.GetMut<R::Shape2d>(kFlash)) {
        flash.Get() = Flash(scene->presentation->hitFeedback.flash.color, feedback.flashAlpha);
    }
    if (auto arc = shapes.GetMut<R::Shape2d>(kHitDirection)) {
        arc.Get() = HitDirection(scene->presentation->hitFeedback.direction, feedback.directionRadians, feedback.directionAlpha);
    }
    // The texts.
    std::string hpText;
    std::string ammoText;
    std::string status;
    if (!sim->mismatch.empty()) {
        status = "内容が Match と違います: " + sim->mismatch;
    } else if (!sim->connected) {
        status = "Match に接続しています…";
    } else if (body == nullptr) {
        status = "参加しています…";
    } else {
        hpText = "HP " + std::to_string(sim::DisplayPoints(combat->hp));
        if (combat->life == sim::LifeState::Dead) {
            status = "倒されました。まもなく復活します";
        } else if (!controls->controls.Captured()) {
            status = "クリックで操作開始（Tab／Esc で解除）";
        }
        ammoText = combat->reloadEndTick != 0 ? "装填中…" : std::to_string(combat->ammo) + " / " + std::to_string(rules->combat.magazineCapacity);
    }
    const auto setText = [&](const K::EntityId id, const std::string& text, const float size) {
        auto sprite = sprites.GetMut<R::Sprite>(id);
        if (!sprite) return;
        HudText& cached = h.texts[id];
        if (cached.text == text && (cached.image || text.empty())) return;
        cached.text = text;
        cached.image = text.empty() || !h.font ? nullptr : TextPicture(h, text, size);
        R::Sprite s = sprite.Read();
        if (cached.image) {
            s.image = cached.image;
            s.color = R::Color8{255, 255, 255, 255};
        } else {
            s.image = std::monostate{};
            s.color = R::Color8{0, 0, 0, 0};
        }
        sprite.Get() = s;
    };
    setText(kHpText, hpText, 36.0F);
    setText(kAmmoText, ammoText, 36.0F);
    setText(kStatusText, status, 28.0F);
    // A fresh hit turns the HP red for a while.
    if (auto sprite = sprites.GetMut<R::Sprite>(kHpText); sprite && h.texts[kHpText].image) {
        const auto byte = [](const float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
        const auto& red = scene->presentation->hitFeedback.hpHighlight.color;
        const R::Color8 color = feedback.hpHighlighted ? R::Color8{byte(red[0]), byte(red[1]), byte(red[2]), 255} : R::Color8{255, 255, 255, 255};
        if (sprite.Read().color.r != color.r || sprite.Read().color.g != color.g || sprite.Read().color.b != color.b) sprite->color = color;
    }
    return {};
}

K::SystemResult PlaySounds(K::Res<PW::LocalUser> user, K::Res<ClientAssets> assets, K::Res<WA::AssetView> view,
                           K::Res<ClientScene> scene, K::Res<ClientControls> controls,
                           K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::ShotRecord>,
                                    K::Read<PW::HitRecord>> players,
                           K::Res<WT::ActiveTime> time, K::ResMut<ClientSound> sound) {
    ClientSound& s = sound.Get();
    if (s.submitter == nullptr) return {};
    const present::SoundSettings& settings = scene->presentation->sound;
    const TM::TimePoint at = TM::MonotonicClock::now();
    const auto play = [&](const present::SoundCue cue, const present::StereoGains gains) {
        const present::SoundSettings::Cue& c = settings.cues[static_cast<std::size_t>(cue)];
        const auto found = assets->clips.find(c.clip);
        if (found == assets->clips.end()) return;
        const AU::Clip* clip = view->Get(found->second);
        if (clip == nullptr) return;
        static_cast<void>(s.submitter->Play(AU::PlayRequest{*clip, at, gains.left, gains.right, false}));
    };
    const auto volume = [&](const present::SoundCue cue) { return settings.cues[static_cast<std::size_t>(cue)].volume; };
    const PW::PlayerBody* own = nullptr;
    players.ForEach([&](K::EntityId, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat&, const PW::ShotRecord&,
                        const PW::HitRecord&) {
        if (owner.userId == user->userId) own = &body;
    });
    const double now = static_cast<double>(time->elapsed) / 1e9;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat,
                        const PW::ShotRecord& shot, const PW::HitRecord& hits) {
        const bool mine = owner.userId == user->userId;
        auto heard = s.shotsHeard.find(id);
        if (heard == s.shotsHeard.end()) {
            // A player seen for the first time: what it did before is not played.
            s.shotsHeard[id] = combat.lastShotTick;
            if (mine) s.confirmedHeard = shot.tick;
        } else if (combat.lastShotTick > heard->second) {
            heard->second = combat.lastShotTick;
            if (mine) {
                play(present::SoundCue::LocalShot, present::CentredGains(volume(present::SoundCue::LocalShot)));
            } else if (own != nullptr) {
                play(present::SoundCue::RemoteShot,
                     present::SpatialGains(own->feet, controls->controls.Yaw(), body.feet, volume(present::SoundCue::RemoteShot), settings));
            }
        }
        if (mine) {
            if (shot.tick > s.confirmedHeard) {
                s.confirmedHeard = shot.tick;
                if (shot.kind == sim::ShotHitKind::Player) {
                    play(present::SoundCue::HitConfirmed, present::CentredGains(volume(present::SoundCue::HitConfirmed)));
                }
            }
            const present::HitFeedbackObservation taken =
                s.hits.Observe(present::LifeHitRecord{combat.lives, hits.hits}, combat.life, body.feet, std::nullopt, now);
            if (present::HitTakenSounds(taken.outcome)) {
                play(present::SoundCue::HitTaken, present::CentredGains(volume(present::SoundCue::HitTaken)));
            }
            const auto step = s.steps.Observe(body.feet, body.grounded != 0, combat.lives, settings);
            if (step == present::FootstepTracker::Step::Footstep) {
                play(present::SoundCue::Footstep, present::CentredGains(volume(present::SoundCue::Footstep)));
            } else if (step == present::FootstepTracker::Step::Landing) {
                play(present::SoundCue::Landing, present::CentredGains(volume(present::SoundCue::Landing)));
            }
        }
    });
    return {};
}

// The player's words for why the connection ended.
std::string EndText(const N::PeerDisconnected& end) {
    const std::string detail = end.detail.empty() ? std::string() : "（" + end.detail + "）";
    switch (end.reason) {
    case N::DisconnectReason::Refused: return "Match に断られました" + detail;
    case N::DisconnectReason::HandshakeTimeout: return "Match から応答がありません。アドレスと Match の起動を確かめてください";
    case N::DisconnectReason::Timeout: return "Match との接続が切れました（応答がなくなりました）";
    case N::DisconnectReason::ClosedByRemote: return "Match が接続を閉じました";
    case N::DisconnectReason::Closed: return "接続を閉じました";
    default: return std::string("Match と合いません: ") + N::ToString(end.reason) + detail;
    }
}

// The connection's end, as the program and the lobby show it: the window closes (the program goes back to the
// lobby with the reason).
K::SystemResult NoteSessionEnd(K::MessageReader<N::PeerDisconnected> ends, K::ResMut<ClientSessionEnd> session) {
    for (const N::PeerDisconnected* end : ends.Read()) {
        session->ended = true;
        session->reason = EndText(*end);
    }
    return {};
}

// Which game the scene is and where it goes back to: the lobby's address text (multiplayer), the single player's save.
struct GameOrigin final {
    Scene scene = Scene::MultiGame;
    std::string address;
    std::string savePath;
};
void HashValue(K::Hasher&, const GameOrigin&) {}

// The game's overlay (assets/object_fps_pvp/ui/pvp_game.json): "pause" while the pointer is the player's (Tab or Esc
// gave it back), "play" (nothing) while the game has it.
K::SystemResult ShowPause(K::Res<ClientControls> controls, K::Query<K::Write<Ui::UiRoot>> roots) {
    const bool paused = !controls->controls.Captured();
    for (const K::EntityId id : roots.Entities()) {
        auto root = roots.GetMut<Ui::UiRoot>(id);
        const std::string canvas = paused ? "pause" : "play";
        if (root->canvas != canvas) root->canvas = canvas;
        if (root->acceptsInput != paused) root->acceptsInput = paused;
    }
    return {};
}

// The deaths of the own player's game so far: each life after the first began with a death, and a dead player died
// once more.
std::uint64_t DeathsOf(const PW::PlayerCombat& combat) {
    const std::uint64_t earlier = combat.lives > 0 ? combat.lives - 1 : 0;
    return earlier + (combat.life == sim::LifeState::Dead ? 1 : 0);
}

// pvp.evacuate: the game ends here. Single player: the game is settled into the save, back to the menu with what was
// written (or why not). Multiplayer: back to the lobby (the Sim World of the lobby has no target: the connection
// closes, and the Match writes the save when it sees the player leave).
K::SystemResult Evacuate(K::MessageReader<Ui::UiAction> actions, K::Res<GameOrigin> origin, K::Res<ClientView> sim,
                         K::Query<K::Read<PW::PlayerCombat>> players, K::ResMut<SceneRequest> request) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action != "pvp.evacuate" || request.Read().next) continue;
        if (origin->scene == Scene::MultiGame) {
            request->next = SceneChange{Scene::Lobby, "撤退しました", std::nullopt, origin->address};
            continue;
        }
        const PW::PlayerCombat* combat = sim->ownPlayer != 0 ? players.Get<PW::PlayerCombat>(sim->ownPlayer) : nullptr;
        const std::uint64_t deaths = combat != nullptr ? DeathsOf(*combat) : 0;
        match::PlayerSave written;
        const std::string failure = SettleSolo(origin->savePath, deaths, written);
        const std::string status = failure.empty() ? "撤退しました（死亡 " + std::to_string(deaths) + " 回）。記録しました"
                                                   : "撤退しました。記録できません: " + failure;
        request->next = SceneChange{Scene::Menu, status, std::nullopt, {}};
    }
    return {};
}

// Frame World: the connection ended or the Match's content differs (the Sim World's picture says so): back to the
// lobby with the reason, noted for the program (FrameNotes).
K::SystemResult LeaveOnEnd(K::Res<ClientView> sim, K::Res<GameOrigin> origin, K::ResMut<FrameNotes> notes, K::ResMut<SceneRequest> request) {
    if (request.Read().next) return {};
    std::string reason;
    if (!sim->mismatch.empty()) {
        notes->mismatch = sim->mismatch;
        reason = "Match の内容がこの Client と違います: " + sim->mismatch;
    } else if (sim->ended) {
        notes->endReason = sim->endReason;
        reason = sim->endReason;
    } else {
        return {};
    }
    request->next = SceneChange{Scene::Lobby, std::move(reason), std::nullopt, origin->address};
    return {};
}

K::SystemOptions InUpdate(std::string name, std::vector<std::string> after = {}) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::update);
    mount.after = std::move(after);
    options.mounts.push_back(std::move(mount));
    return options;
}

void Mount(K::SystemOptions& options, const std::string_view schedule, const std::string_view stage) {
    K::Mount mount;
    mount.schedule = std::string(schedule);
    mount.stage = std::string(stage);
    options.mounts.push_back(std::move(mount));
}

// ---- The Sim World's exchange: in takes the controls, out puts the picture.
void TakeControls(ClientExchange& exchange, K::World& world) {
    SimControls& state = *world.ResourceMut<SimControls>();
    const auto* real = world.Resource<WT::RealTime>();
    const WT::Nanoseconds now = real != nullptr ? real->elapsed : 0;
    if (const auto controls = exchange.TakeControls()) {
        if (state.lastArrival) {
            const WT::Nanoseconds interval = now - *state.lastArrival;
            if (interval <= StaleLimit(state)) {
                state.recent.push_back(interval);
                if (state.recent.size() > kRecentIntervals) state.recent.erase(state.recent.begin());
            }
        }
        state.lastArrival = now;
        ClientExchange::Merge(state.pending, *controls);
    }
    state.stale = !state.lastArrival || now - *state.lastArrival > StaleLimit(state);
}

// T's value in the World, or null (also when the World does not register T: a Solo World has no net).
template <class T>
const T* Find(const K::World& world) {
    return world.Types().ResourceIdOf<T>() ? world.Resource<T>() : nullptr;
}

WorldSnapshot SnapshotOf(K::World& world) {
    WorldSnapshot snapshot;
    for (const K::EntityId id : world.Keys<PW::Owner>()) {
        PlayerSnapshot player;
        player.id = id;
        player.owner = *world.Get<PW::Owner>(id);
        const auto copy = [&world, id](auto& into) {
            using T = typename std::remove_reference_t<decltype(into)>::value_type;
            if (const T* value = world.Get<T>(id)) into = *value;
        };
        copy(player.body);
        copy(player.combat);
        copy(player.hits);
        copy(player.shot);
        snapshot.players.push_back(std::move(player));
    }
    if (const auto* rules = world.Resource<PW::GameRules>()) {
        snapshot.arenaId = rules->arena.id;
        snapshot.wallsDigest = rules->wallsDigest;
    }
    ClientView& view = snapshot.view;
    if (const auto* tick = world.Resource<WT::Tick>()) view.tick = tick->value;
    if (const auto* fixed = world.Resource<WT::FixedTime>()) view.stepNanoseconds = static_cast<std::uint64_t>(fixed->accumulator.Step());
    if (const auto* replica = Find<N::NetReplicaState>(world)) {
        if (replica->latestConfirmed) view.latestConfirmed = *replica->latestConfirmed;
    }
    // Connected: the own Peer is there; a World without net (Solo) has nothing to connect to and is always there.
    const auto* link = Find<N::NetWorldLink>(world);
    view.connected = link == nullptr || (link->clientPeer != 0 && world.Alive(link->clientPeer));
    view.ownPlayer = PW::OwnPlayer(world);
    if (const auto* mismatch = world.Resource<PW::ContentMismatch>()) view.mismatch = mismatch->reason;
    if (const auto* session = Find<ClientSessionEnd>(world)) {
        view.ended = session->ended;
        view.endReason = session->reason;
    }
    return snapshot;
}

// ---- The Frame World's exchange: in applies the picture, out adds the controls.
void ApplySnapshot(ClientExchange& exchange, K::World& world) {
    FrameMirror& mirror = *world.ResourceMut<FrameMirror>();
    const std::shared_ptr<const WorldSnapshot> snapshot = exchange.TakeSnapshot(mirror.seen);
    if (!snapshot) return;
    // Written only when changed: the Frame World's Systems see what changed as it changes.
    const auto put = [&world](const K::EntityId id, const auto& value) {
        using T = std::remove_cvref_t<decltype(value)>;
        const T* now = world.Get<T>(id);
        if (now == nullptr || !(*now == value)) world.Insert(id, value);
    };
    const auto putOptional = [&world, &put](const K::EntityId id, const auto& value) {
        using T = typename std::remove_cvref_t<decltype(value)>::value_type;
        if (value) {
            put(id, *value);
        } else if (world.Get<T>(id) != nullptr) {
            world.Remove<T>(id);
        }
    };
    std::set<K::EntityId> players;
    for (const PlayerSnapshot& player : snapshot->players) {
        players.insert(player.id);
        put(player.id, player.owner);
        putOptional(player.id, player.body);
        putOptional(player.id, player.combat);
        putOptional(player.id, player.hits);
        putOptional(player.id, player.shot);
    }
    for (const K::EntityId gone : mirror.players) {
        if (!players.contains(gone)) world.Despawn(gone);
    }
    mirror.players = std::move(players);
    // The arena the Sim World follows (FollowMatchArena); one this Client does not have stays as it is (the Sim
    // World's ContentMismatch says why).
    PW::GameRules* rules = world.ResourceMut<PW::GameRules>();
    if (!snapshot->arenaId.empty() && (rules->arena.id != snapshot->arenaId || rules->wallsDigest != snapshot->wallsDigest)) {
        const auto* content = world.Resource<PW::ContentRef>();
        const PW::InstalledArena* arena = content != nullptr && content->content ? content->content->Find(snapshot->arenaId) : nullptr;
        if (arena != nullptr && arena->wallsDigest == snapshot->wallsDigest) {
            rules->arena = arena->content;
            rules->wallsDigest = arena->wallsDigest;
        }
    }
    world.SetResource(snapshot->view);
}

void BuildSim(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    r.Resource<SimControls>({.name = "object_fps_pvp.SimControls"});
    r.Resource<ClientSessionEnd>({.name = "object_fps_pvp.ClientSessionEnd"});
    K::SystemOptions own;
    own.name = "object_fps_pvp.PrepareOwnInput";
    Mount(own, O::DefaultSchedules::frame, O::DefaultSchedules::preUpdate);
    r.AddSystem<&PrepareOwnInput>(std::move(own));
    K::SystemOptions write;
    write.name = "object_fps_pvp.WriteStepInput";
    K::Mount writeMount;
    writeMount.schedule = std::string(O::DefaultSchedules::fixed);
    writeMount.stage = std::string(O::DefaultSchedules::first);
    writeMount.before = {"ApplyInput:PlayerInput"};
    write.mounts.push_back(std::move(writeMount));
    RB::RollbackEntry(setup).SkipDuringResimulation(write);
    r.AddSystem<&WriteStepInput>(std::move(write));
    r.AddSystem<&NoteSessionEnd>(InUpdate("object_fps_pvp.NoteSessionEnd"));
    ClientExchange* exchange = setup.ProductBuffer().Find<ClientExchange>();
    GYO_ASSERT(exchange != nullptr && "the Client's Worlds need ConfigureClient (it makes their ClientExchange)");
    setup.Prepare([](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        world.SetResource(SimControls{});
        world.SetResource(ClientSessionEnd{});
        return {};
    });
    setup.ExchangeIn([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        TakeControls(*exchange, world);
        return {};
    });
    setup.ExchangeOut([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        exchange->PutSnapshot(SnapshotOf(world));
        return {};
    });
}

// The Solo World's part: the controls in, the picture out (the same exchange as the Sim World's), and the step's input
// written into the local player.
void BuildSolo(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    r.Resource<SimControls>({.name = "object_fps_pvp.SimControls"});
    K::SystemOptions write;
    write.name = "object_fps_pvp.WriteLocalInput";
    Mount(write, O::DefaultSchedules::fixed, O::DefaultSchedules::first);
    r.AddSystem<&WriteLocalInput>(std::move(write));
    ClientExchange* exchange = setup.ProductBuffer().Find<ClientExchange>();
    GYO_ASSERT(exchange != nullptr && "the Client's Worlds need ConfigureClient (it makes their ClientExchange)");
    setup.Prepare([](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        world.SetResource(SimControls{});
        return {};
    });
    setup.ExchangeIn([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        TakeControls(*exchange, world);
        return {};
    });
    setup.ExchangeOut([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        exchange->PutSnapshot(SnapshotOf(world));
        return {};
    });
}

// The game scene's types, registered in every scene: the Render World reads them (an extracted type must be
// registered in the source, R1b declaration section 8 point 3).
void RegisterFrameTypes(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    // The Sim World's players, as copies (the same names: one game, two Worlds).
    r.Component<PW::Owner>({.name = "Owner"});
    r.Component<PW::PlayerBody>({.name = "PlayerBody"});
    r.Component<PW::PlayerCombat>({.name = "PlayerCombat"});
    r.Component<PW::HitRecord>({.name = "HitRecord"});
    r.Component<PW::ShotRecord>({.name = "ShotRecord"});
    r.Resource<PW::GameRules>({.name = "GameRules"});
    r.Resource<PW::LocalUser>({.name = "LocalUser"});
    r.Resource<PW::ContentRef>({.name = "ContentRef"});
    r.Resource<ClientView>({.name = "object_fps_pvp.ClientView"});
    r.Resource<FrameMirror>({.name = "object_fps_pvp.FrameMirror"});
    r.Resource<ClientAssets>({.name = "object_fps_pvp.ClientAssets"});
    r.Resource<ClientControls>({.name = "object_fps_pvp.ClientControls"});
    r.Resource<ClientScene>({.name = "object_fps_pvp.ClientScene"});
    r.Resource<ClientHud>({.name = "object_fps_pvp.ClientHud"});
    r.Resource<ClientSound>({.name = "object_fps_pvp.ClientSound"});
    r.Resource<ClientCharacters>({.name = "object_fps_pvp.ClientCharacters"});
    r.Component<CharacterPart>({.name = "object_fps_pvp.CharacterPart"});
    r.Component<ViewmodelPart>({.name = "object_fps_pvp.ViewmodelPart"});
    r.Resource<GameOrigin>({.name = "object_fps_pvp.GameOrigin"});
}

void BuildFrame(O::WorldSetup& setup, const ClientOptions& options, const SceneChange& change, const std::string& dataDirectory) {
    K::Registrar& r = setup.Registrar();
    r.AddSystem<&RequestAssets>(InUpdate("object_fps_pvp.RequestAssets"));
    K::SystemOptions read;
    read.name = "object_fps_pvp.ReadControls";
    K::Mount readMount;
    readMount.schedule = std::string(O::DefaultSchedules::frame);
    readMount.stage = std::string(O::DefaultSchedules::preUpdate);
    readMount.after = {std::string(I::kInputSet), std::string(W::kWindowSet)};
    read.mounts.push_back(std::move(readMount));
    r.AddSystem<&ReadControls>(std::move(read));
    r.AddSystem<&ShowArena>(InUpdate("object_fps_pvp.ShowArena"));
    r.AddSystem<&PlaceCamera>(InUpdate("object_fps_pvp.PlaceCamera", {"object_fps_pvp.ShowHud"}));
    r.AddSystem<&PoseCharacters>(InUpdate("object_fps_pvp.PoseCharacters"));
    r.AddSystem<&AddCharacters>(InUpdate("object_fps_pvp.AddCharacters", {"object_fps_pvp.PoseCharacters", "object_fps_pvp.RequestAssets"}));
    r.AddSystem<&PoseViewmodel>(InUpdate("object_fps_pvp.PoseViewmodel", {"object_fps_pvp.PlaceCamera"}));
    r.AddSystem<&AddViewmodel>(InUpdate("object_fps_pvp.AddViewmodel", {"object_fps_pvp.PoseViewmodel", "object_fps_pvp.RequestAssets"}));
    r.AddSystem<&ShowHud>(InUpdate("object_fps_pvp.ShowHud"));
    r.AddSystem<&PlaySounds>(InUpdate("object_fps_pvp.PlaySounds"));
    r.AddSystem<&LeaveOnEnd>(InUpdate("object_fps_pvp.LeaveOnEnd"));
    r.AddSystem<&ShowPause>(InUpdate("object_fps_pvp.ShowPause"));
    r.AddSystem<&Evacuate>(InUpdate("object_fps_pvp.Evacuate"));
    SoundPort* port = setup.ProductBuffer().Find<SoundPort>();
    ClientExchange* exchange = setup.ProductBuffer().Find<ClientExchange>();
    GYO_ASSERT(exchange != nullptr && "the Client's Worlds need ConfigureClient (it makes their ClientExchange)");
    GameOrigin origin{change.scene, change.address, SoloSavePath(dataDirectory)};
    setup.Prepare([options, port, origin](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        world.SetResource(origin);
        // The rules the Sim World starts with (world::BuildWorld); the arena follows the Sim World's.
        const PW::InstalledArena& arena = options.game->arenas[options.arena];
        PW::GameRules rules;
        rules.arena = arena.content;
        rules.stance = options.game->stance;
        rules.combat = options.game->combat;
        rules.wallsDigest = arena.wallsDigest;
        rules.character = options.game->character;
        world.SetResource(std::move(rules));
        world.SetResource(PW::LocalUser{options.userId});
        world.SetResource(PW::ContentRef{options.game});
        world.SetResource(ClientView{});
        world.SetResource(FrameMirror{});
        world.SetResource(ClientAssets{});
        world.SetResource(ClientControls{});
        world.SetResource(ClientScene{options.presentation, {}});
        world.SetResource(ClientHud{});
        world.SetResource(ClientCharacters{});
        ClientSound sound;
        sound.submitter = port != nullptr ? &port->submitter : nullptr;
        world.SetResource(std::move(sound));
        // The world camera.
        C::PerspectiveProjection perspective;
        perspective.verticalFov = kVerticalFov;
        perspective.nearClip = kNear;
        perspective.farClip = kFar;
        world.Insert(kCamera, C::Camera{});
        world.Insert(kCamera, C::Camera3d{});
        world.Insert(kCamera, C::CameraTarget{});
        world.Insert(kCamera, C::Projection{perspective});
        world.Insert(kCamera, C::ComputedCamera{});
        world.Insert(kCamera, R::ClearColorConfig{R::ClearColorConfig::Mode::Custom, R::Color8{120, 150, 180, 255}});
        Place(world, kCamera, T::Transform{});
        // The viewmodel camera: after the world camera, its own depth, the first-person weapon's layer only.
        const ViewmodelDefinition& vm = options.presentation->viewmodel;
        C::PerspectiveProjection close;
        close.verticalFov = vm.verticalFov;
        close.nearClip = vm.nearClip;
        close.farClip = vm.farClip;
        C::Camera view;
        view.order = 1;
        world.Insert(kViewCamera, view);
        world.Insert(kViewCamera, C::Camera3d{});
        world.Insert(kViewCamera, C::CameraTarget{});
        world.Insert(kViewCamera, C::Projection{close});
        world.Insert(kViewCamera, C::ComputedCamera{});
        world.Insert(kViewCamera, R::ClearColorConfig{R::ClearColorConfig::Mode::None, {}});
        world.Insert(kViewCamera, R::RenderLayers{1U << kViewLayer});
        Place(world, kViewCamera, T::Transform{});
        // The arena's picture (its Mesh3d comes with the model).
        Place(world, kArenaPicture, T::Transform{});
        // The HUD camera: 2D, after the world camera, clears nothing, sees the HUD layer only.
        C::OrthographicProjection flat = C::OrthographicProjection::Default2d();
        flat.scalingMode = C::ScalingFixed{kHudWidth, kHudHeight};
        C::Camera hud;
        hud.order = 2;
        world.Insert(kHudCamera, hud);
        world.Insert(kHudCamera, C::Camera2d{});
        world.Insert(kHudCamera, C::CameraTarget{});
        world.Insert(kHudCamera, C::Projection{flat});
        world.Insert(kHudCamera, C::ComputedCamera{});
        world.Insert(kHudCamera, R::ClearColorConfig{R::ClearColorConfig::Mode::None, {}});
        world.Insert(kHudCamera, R::RenderLayers{1U << kHudLayer});
        Place(world, kHudCamera, T::Transform{});
        const auto hudThing = [&world](const K::EntityId id, const Math::Vec3 at) {
            T::Transform t;
            t.translation = at;
            Place(world, id, t);
            world.Insert(id, R::RenderLayers{1U << kHudLayer});
        };
        hudThing(kFlash, {0, 0, 0});
        world.Insert(kFlash, R::Shape2d{});
        hudThing(kCrosshair, {0, 0, 1});
        world.Insert(kCrosshair, Crosshair());
        hudThing(kHitDirection, {0, 0, 1});
        world.Insert(kHitDirection, R::Shape2d{});
        const auto text = [&](const K::EntityId id, const Math::Vec3 at, const Math::Vec2 anchor) {
            hudThing(id, at);
            R::Sprite sprite;
            sprite.anchor = anchor;
            sprite.color = R::Color8{0, 0, 0, 0};
            world.Insert(id, sprite);
        };
        text(kHpText, {-kHudWidth / 2 + 24, -kHudHeight / 2 + 20, 2}, {-0.5F, -0.5F});
        text(kAmmoText, {kHudWidth / 2 - 24, -kHudHeight / 2 + 20, 2}, {0.5F, -0.5F});
        text(kStatusText, {0, kHudHeight / 2 - 80, 2}, {0.0F, 0.5F});
        // The overlay (pause, evacuate) over the HUD camera; ShowPause picks its canvas.
        Ui::UiRoot overlay;
        overlay.document = world.ResourceMut<WA::AssetRequests>()->Load<Ui::UiAsset>(A::AssetId("object_fps_pvp.ui.game"));
        overlay.canvas = "play";
        overlay.camera = kHudCamera;
        overlay.acceptsInput = false;
        world.Insert(kOverlay, std::move(overlay));
        world.Insert(kOverlay, Ui::UiRootState{});
        world.Insert(kOverlay, Ui::UiBindings{});
        return {};
    });
    setup.ExchangeIn([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        ApplySnapshot(*exchange, world);
        return {};
    });
    setup.ExchangeOut([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        exchange->AddControls(world.ResourceMut<ClientControls>()->controls.Take());
        return {};
    });
}

std::optional<std::string> ReadText(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& path, std::string& error) {
    auto uri = GYO::Engine::IO::FS::NativeFileUri(path);
    if (!uri) {
        error = path + ": " + Base::Describe(uri.error());
        return std::nullopt;
    }
    auto text = GYO::Engine::IO::Helpers::ReadAllTextUtf8(fs, *uri);
    if (!text) {
        error = path + ": " + Base::Describe(text.error());
        return std::nullopt;
    }
    return std::move(*text);
}

std::vector<std::byte> ReadBytes(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& path) {
    auto uri = GYO::Engine::IO::FS::NativeFileUri(path);
    if (!uri) throw std::runtime_error(path + ": " + Base::Describe(uri.error()));
    auto bytes = GYO::Engine::IO::Helpers::ReadAllBytes(fs, *uri);
    if (!bytes) throw std::runtime_error(path + ": " + Base::Describe(bytes.error()));
    return std::move(*bytes);
}

std::string Text(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& path) {
    const auto bytes = ReadBytes(fs, path);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

Math::Vec3 VectorOf(const GYO::Engine::Json::Value& value, const std::string& what) {
    auto array = value.AsArray();
    if (!array || array->Size() != 3) throw std::runtime_error(what + " is not three numbers");
    float v[3]{};
    std::size_t i = 0;
    for (const auto item : *array) v[i++] = static_cast<float>(data::Number(item, what));
    return {v[0], v[1], v[2]};
}

ViewmodelDefinition LoadViewmodel(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& dir) {
    ViewmodelDefinition viewmodel;
    const std::string definitionPath = dir + "nailer_v2_viewmodel.json";
    const auto definitionDocument = data::ParseDocument(Text(fs, definitionPath));
    const auto definition = data::AsObject(definitionDocument.Root(), definitionPath);
    // The model (its skeleton and clips; the textures are mapped below, as the FBX names JPGs the catalog has as PNGs).
    const std::string modelPath = dir + "Nailer1_FPS.fbx";
    const std::vector<std::byte> bytes = ReadBytes(fs, modelPath);
    GYO::Engine::Model::FbxLoader loader;
    GYO::Engine::Model::FbxSettings settings;
    settings.textures = false;
    A::LoadContext context(A::AssetId("object_fps_pvp.viewmodel"), modelPath, bytes.size());
    auto model = loader.Load(bytes, settings, context);
    if (!model) throw std::runtime_error(modelPath + ": " + Base::Describe(model.error()));
    auto shared = std::make_shared<GYO::Engine::Model::ModelData>(std::move(*model));
    const std::string setPath = dir + "viewmodel.animset.json";
    const auto setDocument = data::ParseDocument(Text(fs, setPath));
    const auto clips = data::AsObject(data::Member(data::AsObject(setDocument.Root(), setPath), "clips"), setPath + " clips");
    const auto clipOf = [&](const std::string_view role) {
        const auto entry = data::AsObject(data::Member(clips, role), setPath + " clip");
        const std::string name = data::String(data::Member(entry, "clip"), "clip");
        const auto found = shared->FindClip(name);
        if (!found) throw std::runtime_error(modelPath + ": no clip '" + name + "'");
        return *found;
    };
    viewmodel.idle = clipOf("Idle");
    viewmodel.shoot = clipOf("Shoot");
    viewmodel.reload = clipOf("Reload");
    viewmodel.draw = clipOf("Draw");
    // Where it sits: the anchor node of the idle pose's start at the offset, turned (master's ComposeEulerXYZ: X, then Y, then Z).
    const std::string anchorName = data::String(data::Member(definition, "anchor_node"), "anchor_node");
    const auto anchor = shared->FindNode(anchorName);
    if (!anchor) throw std::runtime_error(modelPath + ": no node '" + anchorName + "'");
    std::vector<GYO::Engine::Model::LocalTransform> local(shared->nodes.size());
    GYO::Engine::Model::SampleClip(shared->nodes, shared->clips[viewmodel.idle], 0.0F, GYO::Engine::Model::Wrap::Clamp, local);
    std::vector<Math::Matrix4> nodes(local.size());
    GYO::Engine::Model::ComputeModelSpace(shared->nodes, local, nodes);
    viewmodel.anchor = {nodes[*anchor].values[12], nodes[*anchor].values[13], nodes[*anchor].values[14]};
    viewmodel.offset = VectorOf(data::Member(definition, "offset_meters"), "offset_meters");
    const Math::Vec3 degrees = VectorOf(data::Member(definition, "rotation_degrees"), "rotation_degrees");
    const float k = Math::Pi / 180.0F;
    viewmodel.rotation = Math::Multiply(Math::MakeQuaternionFromAxisAngle({0, 0, 1}, degrees.z * k),
                                        Math::Multiply(Math::MakeQuaternionFromAxisAngle({0, 1, 0}, degrees.y * k),
                                                       Math::MakeQuaternionFromAxisAngle({1, 0, 0}, degrees.x * k)));
    viewmodel.verticalFov = static_cast<float>(data::Number(data::Member(definition, "vertical_fov_degrees"), "vertical_fov_degrees")) * k;
    viewmodel.nearClip = static_cast<float>(data::Number(data::Member(definition, "near_clip"), "near_clip"));
    viewmodel.farClip = static_cast<float>(data::Number(data::Member(definition, "far_clip"), "far_clip"));
    const auto materials = data::AsObject(data::Member(definition, "materials"), "materials");
    for (const auto& material : shared->materials) {
        viewmodel.materialImages.push_back(data::HasMember(materials, material.name)
                                               ? data::String(data::Member(materials, material.name), "a material's image")
                                               : std::string{});
    }
    // The reload's event and its sound.
    const std::string eventsPath = dir + "viewmodel.anim_events.json";
    const auto eventsDocument = data::ParseDocument(Text(fs, eventsPath));
    const auto eventClips = data::AsObject(data::Member(data::AsObject(eventsDocument.Root(), eventsPath), "clips"), "clips");
    const auto reloadEvents = data::Member(eventClips, shared->clips[viewmodel.reload].name).AsArray();
    if (!reloadEvents) throw std::runtime_error(eventsPath + ": the reload clip's events are not an array");
    bool found = false;
    for (const auto item : *reloadEvents) {
        const auto event = data::AsObject(item, "an event");
        if (data::String(data::Member(event, "event"), "event") == "magazine_in") {
            viewmodel.magazineInSeconds = static_cast<float>(data::Number(data::Member(event, "seconds"), "seconds"));
            found = true;
        }
    }
    if (!found) throw std::runtime_error(eventsPath + ": the reload clip has no magazine_in");
    const std::string soundsPath = dir + "viewmodel.event_sounds.json";
    const auto soundsDocument = data::ParseDocument(Text(fs, soundsPath));
    const auto soundEvents = data::AsObject(data::Member(data::AsObject(soundsDocument.Root(), soundsPath), "events"), "events");
    const auto magazine = data::AsObject(data::Member(soundEvents, "magazine_in"), "magazine_in");
    viewmodel.reloadSound = data::String(data::Member(magazine, "sound_asset_id"), "sound_asset_id");
    viewmodel.reloadSoundSync = static_cast<float>(data::Number(data::Member(magazine, "sync_seconds"), "sync_seconds"));
    viewmodel.reloadSoundGain = static_cast<float>(data::Number(data::Member(magazine, "gain"), "gain"));
    // The third-person weapon: its model and the picture its one material uses.
    const std::string heldPath = dir + "../world/character.json";
    const auto heldDocument = data::ParseDocument(Text(fs, heldPath));
    const auto held = data::AsObject(heldDocument.Root(), heldPath);
    viewmodel.heldModel = data::String(data::Member(held, "model_asset_id"), "model_asset_id");
    const auto overrides = data::AsObject(data::Member(held, "material_overrides"), "material_overrides");
    for (const auto member : overrides) {
        const auto entry = data::AsObject(member.value, "a material override");
        viewmodel.heldImage = data::String(data::Member(entry, "texture_asset_id"), "texture_asset_id");
    }
    viewmodel.model = std::move(shared);
    return viewmodel;
}

} // namespace

T::Transform ViewmodelPlacement(const ViewmodelDefinition& viewmodel) {
    T::Transform t;
    t.rotation = viewmodel.rotation;
    t.translation = viewmodel.offset - Math::Rotate(viewmodel.rotation, viewmodel.anchor);
    return t;
}

std::optional<PresentationContent> LoadPresentation(const std::string& root, const world::GameContent& game, std::string& error) {
    error.clear();
    GYO::Engine::IO::FS::NativeFileSystem fs;
    PresentationContent content;
    {
        auto text = ReadText(fs, root + "/audio/sfx.json", error);
        if (!text) return std::nullopt;
        auto settings = present::ParseSoundSettings(*text, error);
        if (!settings) {
            error = root + "/audio/sfx.json: " + error;
            return std::nullopt;
        }
        content.sound = std::move(*settings);
    }
    {
        auto text = ReadText(fs, root + "/ui/hit_feedback.json", error);
        if (!text) return std::nullopt;
        auto settings = present::ParseHitFeedbackSettings(*text, error);
        if (!settings) {
            error = root + "/ui/hit_feedback.json: " + error;
            return std::nullopt;
        }
        content.hitFeedback = *settings;
    }
    try {
        content.viewmodel = LoadViewmodel(fs, root + "/weapons/nailer_v2/viewmodel/");
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
    for (const world::InstalledArena& arena : game.arenas) content.arenaPalettes.push_back(PaletteOf(arena.geometry));
    return content;
}

void ConfigureClient(O::ProductSetup& setup) {
    // First: the Worlds need them whether or not the rest is there. The scene the Product starts in (ClientStart;
    // none: the menu).
    setup.Buffer().Create<ClientExchange>();
    const ClientStart* start = setup.Options<ClientStart>();
    const ClientStart first = start != nullptr ? *start : ClientStart{};
    setup.Buffer().Create<ClientStart>(first);
    setup.Buffer().Create<SceneDirector>(first.first);
    const auto* registry = setup.Buffer().Find<A::AssetLoaderRegistry>();
    if (registry == nullptr) {
        setup.Report("NoAssetStore", "the asset store is not there (world_asset is not carried)");
        return;
    }
    const auto refused = [&setup](const char* what, const auto& result) {
        if (result) return false;
        setup.Report("LoaderRefused", std::string("the ") + what + " loader was refused: " + Base::Describe(result.error()));
        return true;
    };
    if (refused("voxel model", registry->template Register<VoxelModelLoader>())) return;
    if (refused("fbx", registry->template Register<GYO::Engine::Model::FbxLoader>())) return;
    if (refused("png", registry->template Register<IM::PngLoader>())) return;
    if (refused("font", registry->template Register<TX::FontLoader>())) return;
    if (refused("ui", registry->template Register<GYO::Engine::Ui::UiDocumentLoader>())) return;
    if (refused("wav", registry->template Register<AU::WavLoader>())) return;
    AU::Mixer* mixer = setup.Buffer().Find<AU::Mixer>();
    if (mixer == nullptr) return;  // no sdl_audio: silent
    auto submitter = mixer->CreateSubmitter();
    if (!submitter) {
        setup.Report("NoSubmitter", "the mixer gave no submitter: " + Base::Describe(submitter.error()));
        return;
    }
    setup.Buffer().Create<SoundPort>(std::move(submitter).value());
}

O::WorldDescription ClientSimWorld(ClientOptions options) {
    O::WorldDescription world = PW::ClientWorld({options.game, options.arena, options.userId, nullptr});
    world.name = "Sim";
    // The game only in the scene that plays it; elsewhere the World is its units alone, without a target (no
    // connection: net-connect-target).
    world.build = [inner = std::move(world.build)](O::WorldSetup& setup) {
        const auto* director = setup.ProductBuffer().Find<SceneDirector>();
        GYO_ASSERT(director != nullptr && "the Client's Worlds need ConfigureClient (it makes their SceneDirector)");
        const SceneChange& scene = director->Current();
        if (!SimPlays(scene.scene)) {
            // The rollback strategy of the description still asks which schedule is the step (RB-E10).
            RB::RollbackEntry(setup).StepSchedule();
            return;
        }
        inner(setup);
        BuildSim(setup);
        if (scene.connect) {
            setup.Prepare([target = *scene.connect](K::World& w) -> Base::Result<void, O::OrchestrationError> {
                w.SetResource(target);
                return {};
            });
        }
    };
    return world;
}

O::WorldDescription ClientSoloWorld(ClientOptions options) {
    O::WorldDescription world = PW::LocalWorld({options.game, options.arena, options.userId, nullptr, 0});
    world.name = "Solo";
    // The game only in the single player's game scene; its player at a spawn picked at random for each game.
    world.build = [options](O::WorldSetup& setup) {
        const auto* director = setup.ProductBuffer().Find<SceneDirector>();
        GYO_ASSERT(director != nullptr && "the Client's Worlds need ConfigureClient (it makes their SceneDirector)");
        if (!SoloPlays(director->Current().scene)) return;
        std::random_device device;
        PW::BuildWorld(setup, {options.game, options.arena, options.userId, nullptr, device()}, PW::WorldEnd::Local);
        BuildSolo(setup);
    };
    return world;
}

O::WorldDescription ClientFrameWorld(ClientOptions options, const bool window) {
    O::WorldDescription world;
    world.name = "Frame";
    world.units = {"entity_ids", "world_time", "transform", "world_asset", "camera", "input", "window", "window_camera",
                   "world_render", "world_ui"};
    if (window) world.units.push_back("sdl_window");
    world.options.Set(kFrameIds);
    // Per-frame Systems only: the steps are the Sim World's.
    WT::WorldTimeOptions time;
    time.fixedStep = false;
    world.options.Set(std::move(time));
    world.build = [options = std::move(options)](O::WorldSetup& setup) {
        const auto* director = setup.ProductBuffer().Find<SceneDirector>();
        GYO_ASSERT(director != nullptr && "the Client's Worlds need ConfigureClient (it makes their SceneDirector)");
        const SceneChange& scene = director->Current();
        RegisterFrameTypes(setup);
        BuildSceneCommon(setup);
        const auto* start = setup.ProductBuffer().Find<ClientStart>();
        const std::string directory = start != nullptr ? start->dataDirectory : std::string();
        switch (scene.scene) {
        case Scene::Menu: BuildMenu(setup, scene); break;
        case Scene::SoloRoom: BuildSoloRoom(setup, scene, directory); break;
        case Scene::Lobby: BuildLobby(setup, scene, options.userId); break;
        case Scene::SoloGame:
        case Scene::MultiGame: BuildFrame(setup, options, scene, directory); break;
        }
    };
    return world;
}

O::WorldDescription ClientRenderWorld(const bool gpu) {
    O::WorldDescription world;
    world.name = "Render";
    world.extractFrom = "Frame";
    world.units = {"entity_ids", "render_world", "camera_render", "sprite_render", "mesh_render", "ui_render"};
    if (gpu) world.units.push_back("sdl_gpu");
    return world;
}

ClientStatus StatusOf(K::World& world) {
    ClientStatus status;
    if (const auto* view = world.Resource<ClientView>()) {
        status.connected = view->connected;
        status.playing = view->ownPlayer != 0;
        status.mismatch = view->mismatch;
        status.ended = view->ended;
        status.endReason = view->endReason;
    }
    if (const auto* controls = world.Resource<ClientControls>()) status.captured = controls->controls.Captured();
    return status;
}

} // namespace object_fps_pvp::client
