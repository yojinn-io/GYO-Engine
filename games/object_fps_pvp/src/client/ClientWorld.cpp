#include "object_fps_pvp/client/ClientWorld.hpp"

#include "client/ClientExchange.hpp"
#include "object_fps_pvp/client/Gateway.hpp"
#include "client/Pages.hpp"
#include "client/SceneBuilds.hpp"
#include "object_fps_pvp/data/ArenaMap.hpp"
#include "object_fps_pvp/present/Controls.hpp"
#include "object_fps_pvp/present/Interpolation.hpp"
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
#include "engine/render/Presentation.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/text/FontLoader.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/ui/UiDocumentLoader.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/state_machine/RegisterStateMachine.hpp"
#include "engine/text/GlyphAtlas.hpp"
#include "engine/text/TextContext.hpp"
#include "engine/text/TextLayout.hpp"
#include "engine/text/TextStyle.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/voxel_map/VoxelMapLoaders.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <random>
#include <cmath>
#include <deque>
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
namespace SM = GYO::Engine::StateMachine;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;
namespace PW = object_fps_pvp::world;
namespace VM = GYO::Engine::VoxelMap;

void HashValue(K::Hasher&, const ClientView&) {}
void HashValue(K::Hasher&, const DrawnFrames&) {}

namespace {

// ---- The Client's own entities: fixed ids outside both allotter ranges (world::kMatchIds, world::kClientIds), put
// in by the prepare function before the first step; the stand-in bodies come from the Client's allotter.
constexpr K::EntityId kCamera = 3'000'010;
constexpr K::EntityId kHudCamera = 3'000'011;
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
    // The next frame takes the pointer, as a click does (the pause's pvp.resume).
    bool resume = false;
    // This frame asks for the pause: it gave the pointer back (Tab, Esc, the window's focus lost), or without the
    // pointer Esc was pressed or the window lost its focus.
    bool pauseAsked = false;
    bool focused = true;  // the window had the focus last frame
    // The life the view was turned to (a new life faces its spawn's yaw).
    std::uint32_t facedLife = 0;
    K::EntityId facedPlayer = 0;
};
void HashValue(K::Hasher&, const ClientControls&) {}

// Whether the cameras were given their layers back (RevealUnderCover).
struct CoverState final {
    bool revealed = false;
};
void HashValue(K::Hasher&, const CoverState&) {}

// The world camera's clear colour once the game shows (black before).
const R::ClearColorConfig kSky{R::ClearColorConfig::Mode::Custom, R::Color8{120, 150, 180, 255}};

// The loading of the game scene (game-flow declaration 3.5). The steps are judged again every frame until ready, so
// a later change (a new model to load) undoes the steps after it; the progress never goes back.
struct GameReadiness final {
    double progress = 0.0;
    bool covered = false;                   // the overlay is shown: the game may be drawn under it
    bool assets = false;                    // the game's assets finished, and both Worlds' maps
    std::optional<std::uint64_t> warmFrom;  // DrawnFrames::shown when the assets were done (or a draw was last skipped)
    std::optional<std::uint64_t> warmStart; // ... when they were first done: the warm-up's limit counts from here
    std::uint64_t skippedAt = 0;            // DrawnFrames::skipped at warmFrom
    bool warmed = false;
    bool network = false;                   // the own player is there, and the arena is this Client's
    std::optional<std::uint64_t> lastFrom;  // shown when the rest was done: the next shown frame is the first
    bool ready = false;
};
void HashValue(K::Hasher&, const GameReadiness&) {}

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
    std::optional<double> viewTick;              // the time the frame showed (ViewTime); none: the confirmed tick
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

// The Frame World: the other players as the Match confirmed them, each state at its tick (PV17; port declaration,
// segment 6). The players' copies are written from here every frame at the timeline's tick (InterpolateRemotes);
// the own player is the Sim World's prediction, written by the exchange.
struct RemoteSample final {
    std::uint64_t tick = 0;
    std::optional<PW::PlayerBody> body;
    std::optional<PW::PlayerCombat> combat;
    std::optional<PW::HitRecord> hits;
    std::optional<PW::ShotRecord> shot;
};
struct RemoteHistory final {
    std::deque<RemoteSample> samples;          // ascending ticks
    std::optional<std::uint64_t> goneAt;       // the confirmed tick it left the Sim World at
};
struct RemoteView final {
    present::InterpolationTimeline timeline;
    std::map<K::EntityId, RemoteHistory> players;
    bool fresh = false;                        // a picture came since the last frame
    WT::Nanoseconds pictureAt = 0;             // the frame's time when the last one came
    std::optional<WT::Nanoseconds> lastFrameAt;
};
void HashValue(K::Hasher&, const RemoteView&) {}

const present::TimelineSettings kTimeline{};

// a toward b (present::BlendPose: where it stands and looks, not across a new life); the rest is a's.
RemoteSample Blend(const RemoteSample& a, const RemoteSample& b, const double fraction) {
    RemoteSample out = a;
    if (!a.body || !b.body) return out;
    const auto pose = [](const RemoteSample& sample) {
        return present::RemotePose{sample.body->feet, sample.body->yaw, sample.body->pitch, sample.combat ? sample.combat->lifeTick : 0};
    };
    const present::RemotePose blended = present::BlendPose(pose(a), pose(b), fraction);
    out.body->feet = blended.feet;
    out.body->yaw = blended.yaw;
    out.body->pitch = blended.pitch;
    return out;
}

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
    // The own player's last shot as this frame saw it (and whose), and when (the frame's time) a newer one came: the
    // recoil. A shot the prediction takes back (a later lastShotTick going back) kicks nothing.
    K::EntityId shotPlayer = 0;
    std::optional<std::uint64_t> shotSeen;
    std::optional<WT::Nanoseconds> recoilAt;
};
void HashValue(K::Hasher&, const ClientCharacters&) {}

struct ClientScene final {
    std::shared_ptr<const PresentationContent> presentation;
};
void HashValue(K::Hasher&, const ClientScene&) {}

// The Frame World's map while it becomes the arena's picture (ShowArena's state).
struct ArenaPictureLoading final {
    // The map's asset id (ArenaContent::map).
    std::string map;
    A::AssetExecutor* jobs = nullptr;
    A::Handle<VM::VoxelMapAsset> handle;
    PW::WorkerTask<ArenaPicture> picture;
};
void HashValue(K::Hasher&, const ArenaPictureLoading&) {}

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

std::shared_ptr<const IM::ImageData> PaletteOf(const data::ArenaMap& map) {
    const GYO::Engine::VoxelMap::VoxelPaletteImage image = GYO::Engine::VoxelMap::ExpandVoxelPalette(map.index);
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

// The frame's keys and mouse into the controls (the pointer, the view, the held keys, the presses), and the window
// asked to lock and hide the cursor while the game has it. Main/PreUpdate after the input and window sets: the steps
// of this frame run after it.
K::SystemResult ReadControls(K::Res<I::ButtonInput<I::KeyCode>> keys, K::Res<I::ButtonInput<I::MouseButton>> mouse,
                             K::Res<I::AccumulatedMouseMotion> motion, K::Res<I::CursorPosition> cursor,
                             K::Res<W::WindowState> window, K::Res<PW::LocalUser> user, K::Res<GamePageState> page,
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
    // While the loading screen is up the pointer stays the player's: a click or Tab is for the screen (its 中止).
    const bool loading = page->Current().value_or(GamePage::Loading) == GamePage::Loading;
    present::ControlsFrame frame;
    frame.focused = !window->known || window->focused;
    frame.insideWindow = cursor->position.has_value();
    frame.tab = !loading && keys->JustPressed(I::KeyCode::Tab);
    frame.escape = keys->JustPressed(I::KeyCode::Escape);
    // The pause's resume takes the pointer as a click would, unless the click that pressed it already took it (then a
    // second click would be a shot).
    const bool resume = std::exchange(c.resume, false) && !c.controls.Captured();
    frame.click = !loading && (mouse->JustPressed(I::MouseButton::Left) || resume);
    frame.jump = keys->JustPressed(I::KeyCode::Space);
    frame.reload = keys->JustPressed(I::KeyCode::R);
    frame.forward = keys->Pressed(I::KeyCode::W);
    frame.back = keys->Pressed(I::KeyCode::S);
    frame.left = keys->Pressed(I::KeyCode::A);
    frame.right = keys->Pressed(I::KeyCode::D);
    frame.crouch = keys->Pressed(I::KeyCode::ControlLeft);
    frame.motion = motion->delta;
    const bool before = c.controls.Captured();
    c.controls.Apply(frame);
    const bool focusLost = c.focused && !frame.focused;
    c.focused = frame.focused;
    c.pauseAsked = (before && !c.controls.Captured()) || frame.escape || focusLost;
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
    // Readiness is not the player's input: stale controls keep it.
    input.ready = state.pending.ready ? 1 : 0;
    return input;
}

// The input of the step that runs now, written into the own Peer's slot of this step before the net Module's entry
// (ApplyInput) reads it: each slot is written once, before its step runs and before it is sent (the net Module sends
// a tick's slots once and the Match keeps each (Peer, tick) once). FixedUpdate/First, not while resimulating (the
// rollback writes the recorded inputs back then). lightyear's client input System in FixedPreUpdate
// (crates/inputs/inputs/src/client.rs:41, 186-190, 418-445).
K::SystemResult WriteStepInput(K::Res<WT::Tick> tick, K::Res<N::NetWorldLink> link, K::Res<N::NetReplicaState> replica,
                               K::Query<K::Write<N::PeerInput<PW::PlayerInput>>> inputs, K::ResMut<SimControls> state) {
    if (link->clientPeer == 0 || !inputs.Contains(link->clientPeer)) return {};
    const PW::PlayerInput input = StepInputOf(*state);
    // The time the player saw (net F2): the other players' timeline of the frame the controls came from (a tick and
    // its fraction in a byte), or the confirmed point before it is timed.
    N::ViewTime seen{replica->latestConfirmed.value_or(0), 0};
    if (const auto view = state.Read().viewTick; view && *view >= 0.0) {
        const double whole = std::floor(*view);
        seen.tick = static_cast<std::uint32_t>(whole);
        seen.overstep = static_cast<std::uint8_t>(std::clamp(std::floor((*view - whole) * 256.0), 0.0, 255.0));
    }
    inputs.GetMut<N::PeerInput<PW::PlayerInput>>(link->clientPeer)->Set(static_cast<std::uint32_t>(tick->value), input, seen);
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

// Main/Update while the Frame World generates: the arena's picture. The map comes from the asset store (the Sim or
// Solo World reads the same one: the store reads it once); only its faces reach the worker, which makes one model per
// region and the palette picture; then one entity per region, a Mesh3d of the model the Client holds with the region's
// box (the visibility Module culls it). The World generates until they are there; a map that fails keeps it
// generating, and MapLoad says why (LeaveOnEnd leaves).
K::SystemResult ShowArena(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view, K::ResMut<ArenaPictureLoading> loading,
                          K::ResMut<PW::MapLoad> load, K::ResMut<O::WorldGeneration> generation,
                          K::Spawn<K::Bundle<T::Transform, R::Mesh3d, R::Aabb>> spawn) {
    const PW::MapStage stage = load.Read().stage;
    if (stage == PW::MapStage::Done) return {};
    // Every frame but the one the map is done in holds the generation.
    const auto hold = [&generation] {
        generation->Hold();
        return K::SystemResult{};
    };
    if (stage == PW::MapStage::Failed) return hold();
    ArenaPictureLoading& arena = loading.Get();
    if (stage == PW::MapStage::Reading) {
        if (arena.handle.Id().Empty()) arena.handle = requests->Load<VM::VoxelMapAsset>(A::AssetId(arena.map));
        const PW::MapReading reading = PW::ReadMap(*view, arena.handle);
        if (!reading.failure.empty()) {
            arena.handle = {};
            load->stage = PW::MapStage::Failed;
            load->failure = arena.map + ": " + reading.failure;
            return hold();
        }
        if (load.Read().done != reading.done || load.Read().total != reading.total) {
            load->done = reading.done;
            load->total = reading.total;
        }
        if (!reading.complete) return hold();
        data::ArenaMap faces = data::CopyMapPart(*reading.index, reading.regions, data::MapPart::Faces);
        arena.handle = {};
        arena.picture = PW::WorkerTask<ArenaPicture>::Start(arena.jobs, [faces = std::move(faces)] {
            return ArenaPicture{data::RegionPictures(faces), PaletteOf(faces)};
        });
        load->stage = PW::MapStage::Building;
        return hold();
    }
    std::optional<ArenaPicture> picture = arena.picture.Take();
    if (!picture) return hold();
    for (data::RegionPicture& region : picture->regions) {
        R::Mesh3d mesh;
        mesh.model = std::move(region.model);
        mesh.image = picture->palette;
        mesh.filter = R::TextureFilter::Nearest;
        auto id = spawn.New(T::Transform{}, std::move(mesh), R::Aabb{region.bounds});
        if (!id) return Base::Err(std::move(id).error());
    }
    load->stage = PW::MapStage::Done;
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
K::SystemResult PoseCharacters(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientView> sim, K::Res<RemoteView> remote,
                               K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::HitRecord>> players,
                               K::Query<K::Read<CharacterPart>> parts, K::Query<K::Write<T::Transform>> transforms,
                               K::Query<K::Write<R::SkinPose>> poses, K::ResMut<ClientCharacters> characters, K::Despawn despawn) {
    if (!rules->character) return {};
    const character::CharacterRig& rig = *rules->character;
    const std::uint64_t step = sim->stepNanoseconds;
    // The other players are timed on the interpolation timeline (their copies are their states at its tick), not on
    // this Client's predicted step: their shots, hits and lives are timed on it (lightyear: an interpolated entity has
    // its own timeline, crates/replication/interpolation/src/timeline.rs). Before it is timed: the confirmed point.
    const double shownTick = remote->timeline.synced ? std::max(remote->timeline.tick, 0.0)
                                                     : static_cast<double>(sim->latestConfirmed ? *sim->latestConfirmed : sim->tick);
    const auto seen = static_cast<std::uint64_t>(shownTick);
    // Each shown player's pose this frame.
    std::map<K::EntityId, std::vector<Math::Matrix4>> modelSpace;
    std::map<K::EntityId, const PW::PlayerBody*> bodies;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat,
                        const PW::HitRecord& hits) {
        if (owner.userId == user->userId) return;
        sim::AnimationInput input = PW::AnimationInputOf(body, combat, hits, *rules, seen, step);
        input.seconds = shownTick * static_cast<double>(step) / 1e9;
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
K::SystemResult PoseViewmodel(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientView> sim, K::Res<WT::RealTime> real,
                              K::Res<ClientScene> scene, K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Res<ClientSound> sound,
                              K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>, K::Read<PW::HitRecord>> players,
                              K::Query<K::Read<ViewmodelPart>, K::Write<R::SkinPose>, K::Write<R::RenderLayers>, K::Write<T::Transform>> parts,
                              K::ResMut<ClientCharacters> characters) {
    const ViewmodelDefinition& viewmodel = scene->presentation->viewmodel;
    if (!viewmodel.model) return {};
    const std::uint64_t step = sim->stepNanoseconds;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat,
                        const PW::HitRecord& hits) {
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
        // The recoil: from the frame that first sees a new shot (the prediction's, at once), on the frame's clock.
        if (characters->shotPlayer != id) {
            characters->shotPlayer = id;
            characters->shotSeen.reset();
            characters->recoilAt.reset();
        }
        if (characters->shotSeen && combat.lastShotTick > *characters->shotSeen) characters->recoilAt = real->elapsed;
        characters->shotSeen = combat.lastShotTick;
        const float recoil = characters->recoilAt
                                 ? RecoilRadians(viewmodel, static_cast<double>(real->elapsed - *characters->recoilAt) / 1e9)
                                 : 0.0F;
        const T::Transform placement = ViewmodelPlacement(viewmodel, recoil);
        parts.ForEach([&](K::EntityId, const ViewmodelPart&, K::Mut<R::SkinPose> pose, K::Mut<R::RenderLayers> layers,
                          K::Mut<T::Transform> transform) {
            pose->nodes = nodes;
            if (layers.Read().mask != mask) layers->mask = mask;
            // (Written only when it moves: at rest nothing changes.)
            const T::Transform& now = transform.Read();
            const bool same = now.translation.x == placement.translation.x && now.translation.y == placement.translation.y &&
                              now.translation.z == placement.translation.z && now.rotation.x == placement.rotation.x &&
                              now.rotation.y == placement.rotation.y && now.rotation.z == placement.rotation.z &&
                              now.rotation.w == placement.rotation.w;
            if (!same) transform.Get() = placement;
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

// The connection's end, as the program and the lobby show it (the Frame World's LeaveOnEnd goes back to the lobby with
// the reason).
K::SystemResult NoteSessionEnd(K::MessageReader<N::PeerDisconnected> ends, K::ResMut<ClientSessionEnd> session) {
    for (const N::PeerDisconnected* end : ends.Read()) {
        session->ended = true;
        session->reason = EndText(*end);
    }
    return {};
}

// Which game the scene is and where it goes back to: the Gateway's room the lobby leaves (multiplayer), the single
// player's save.
struct GameOrigin final {
    Scene scene = Scene::MultiGame;
    std::uint64_t room = 0;
    std::string savePath;
    std::string map;  // the scene's (SceneChange::map): the single player's room comes back to it
    std::optional<N::NetConnectTarget> connect;  // the scene's (SceneChange::connect): the game again on another arena
    std::shared_ptr<const PW::GameContent> game;  // the arenas this Client has
};
void HashValue(K::Hasher&, const GameOrigin&) {}

// The share of the bar each step reaches, and how many frames the warm-up waits at most for draws to stop being
// skipped (a texture or a pipeline that cannot be made would hold it for ever).
constexpr double kAssetsShare = 0.8;
constexpr double kWarmShare = 0.85;
constexpr double kNetworkShare = 0.9;
constexpr std::uint64_t kWarmFramesAtMost = 120;

// The steps of the loading, in order; each waits for the ones before it.
K::SystemResult TrackReadiness(K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Res<ClientView> sim, K::Res<DrawnFrames> drawn,
                               K::Res<PW::MapLoad> map, K::Query<K::Read<Ui::UiRoot>, K::Read<Ui::UiRootState>> roots,
                               K::ResMut<GameReadiness> readiness) {
    if (readiness.Read().ready) return {};
    GameReadiness next = readiness.Read();
    next.covered = next.covered || (roots.Get<Ui::UiRootState>(kOverlay) != nullptr && roots.Get<Ui::UiRootState>(kOverlay)->runtime.has_value());
    // The assets: the game's and the overlay's document, and the map: this World's picture of it (its regions count
    // in the bar) and the Sim (or Solo) World's walls.
    bool assetsDone = false;
    if (assets->requested) {
        std::vector<WA::UntypedHandle> handles;
        const auto add = [&handles](const WA::UntypedHandle handle) {
            if (!handle.Id().Empty()) handles.push_back(handle);  // an empty Handle names nothing to wait for
        };
        const ClientAssets& a = *assets;
        add(a.body);
        add(a.heldWeapon);
        add(a.viewmodel);
        add(a.heldWeaponImage);
        add(a.font);
        for (const auto& image : a.viewmodelImages) add(image);
        for (const auto& [id, clip] : a.clips) add(clip);
        if (const Ui::UiRoot* overlay = roots.Get<Ui::UiRoot>(kOverlay)) {
            if (const auto* document = std::get_if<A::Handle<Ui::UiAsset>>(&overlay->document)) add(*document);
        }
        const WA::AssetReadiness ready = view->Ready(handles);
        // A failed asset does not hold the game: it shows without it (game-flow declaration, revision 4).
        const std::size_t total = handles.size() + map->total;
        const double done = total == 0 ? 0.0 : static_cast<double>(ready.loaded + ready.failed + map->done) / static_cast<double>(total);
        next.progress = std::max(next.progress, kAssetsShare * done);
        assetsDone = ready.allFinished && ready.dependenciesFinished && map->stage == PW::MapStage::Done &&
                     sim->mapLoad.stage == PW::MapStage::Done;
    }
    if (!assetsDone) {
        // Not (or no more): everything after it again.
        next.assets = false;
        next.warmed = next.network = false;
        next.warmFrom.reset();
        next.lastFrom.reset();
        readiness.Get() = std::move(next);
        return {};
    }
    next.assets = true;
    // The warm-up: the game drawn under the overlay until two frames are shown with no draw skipped (the textures,
    // meshes and pipelines made), at most kWarmFramesAtMost frames after the assets were done.
    if (next.covered && !next.warmed) {
        if (!drawn->known) {
            next.warmed = true;
        } else if (!next.warmFrom) {
            next.warmFrom = drawn->shown;
            if (!next.warmStart) next.warmStart = drawn->shown;
            next.skippedAt = drawn->skipped;
        } else if (drawn->skipped != next.skippedAt) {
            next.warmFrom = drawn->shown;
            next.skippedAt = drawn->skipped;
        }
        if (drawn->known && next.warmFrom &&
            (drawn->shown >= *next.warmFrom + 2 || drawn->shown >= *next.warmStart + kWarmFramesAtMost)) {
            next.warmed = true;
        }
        next.progress = std::max(next.progress, kWarmShare);
    }
    if (next.warmed) {
        next.network = sim->ownPlayer != 0 && sim->mismatch.empty();
        if (next.network) next.progress = std::max(next.progress, kNetworkShare);
    }
    if (next.network) {
        // One more frame drawn with all of it: the first frame the player will see.
        if (!next.lastFrom) {
            next.lastFrom = drawn->known ? drawn->shown : 0;
        } else if (!drawn->known || drawn->shown > *next.lastFrom) {
            next.ready = true;
            next.progress = 1.0;
        }
    } else {
        next.lastFrom.reset();
    }
    readiness.Get() = std::move(next);
    return {};
}

// Nothing of the game is drawn before the overlay covers it (its document loads first): the cameras see no layer and
// the world camera clears to black; then they see theirs.
K::SystemResult RevealUnderCover(K::Res<GameReadiness> readiness, K::Query<K::Write<R::RenderLayers>, K::Write<R::ClearColorConfig>> cameras,
                                 K::ResMut<CoverState> cover) {
    if (cover.Read().revealed || !readiness->covered) return {};
    cover->revealed = true;
    if (auto layers = cameras.GetMut<R::RenderLayers>(kCamera)) layers->mask = 1;
    if (auto layers = cameras.GetMut<R::RenderLayers>(kViewCamera)) layers->mask = 1U << kViewLayer;
    if (auto layers = cameras.GetMut<R::RenderLayers>(kHudCamera)) layers->mask = 1U << kHudLayer;
    if (auto clear = cameras.GetMut<R::ClearColorConfig>(kCamera)) *clear = kSky;
    return {};
}

// The player's words for where the loading is.
std::string LoadingText(const GameReadiness& readiness, const ClientView& sim, const PW::MapLoad& map, const Scene scene) {
    if (!readiness.assets) {
        if (map.stage == PW::MapStage::Reading && map.total > 0) {
            return "地図を読み込んでいます…（" + std::to_string(map.done) + "/" + std::to_string(map.total) + "）";
        }
        if (map.stage == PW::MapStage::Building || sim.mapLoad.stage == PW::MapStage::Building) return "地図を組み立てています…";
        return "データを読み込んでいます…";
    }
    if (!readiness.warmed) return "描画の準備をしています…";
    if (!readiness.network) return scene == Scene::MultiGame ? "対戦に接続しています…" : "準備しています…";
    if (!readiness.ready) return "最初の画面を描いています…";
    if (!sim.started) return "ほかのプレイヤーを待っています…";
    return {};
}

// ---- The game scene's pages (client/Pages.hpp, GamePage; assets/object_fps_pvp/ui/pvp_game.json): Loading (canvas
// "loading") until the game is ready and the match started, then Playing (no overlay), and Paused (canvas "pause")
// while the pointer is the player's. Entering a page puts its root on kOverlay (the HUD camera's), leaving takes it.

// The overlay's document, held for the scene (each page's root finds it loaded), and the pause's title.
struct OverlayDocument final {
    A::Handle<Ui::UiAsset> document;
    std::string title;
};
void HashValue(K::Hasher&, const OverlayDocument&) {}

using OverlayRoots = K::CommandsFor<K::Inserts<Ui::UiRoot, Ui::UiRootState, Ui::UiBindings>, K::Removes<Ui::UiRoot, Ui::UiRootState, Ui::UiBindings>>;

void PutOverlay(OverlayRoots& roots, const OverlayDocument& overlay, std::string canvas, Ui::UiBindings bindings) {
    Ui::UiRoot root;
    root.document = overlay.document;
    root.canvas = std::move(canvas);
    root.camera = kHudCamera;
    roots.Insert(kOverlay, std::move(root));
    roots.Insert(kOverlay, Ui::UiRootState{});
    roots.Insert(kOverlay, std::move(bindings));
}

K::SystemResult TakeOverlay(OverlayRoots roots) {
    roots.Remove<Ui::UiRoot>(kOverlay);
    roots.Remove<Ui::UiRootState>(kOverlay);
    roots.Remove<Ui::UiBindings>(kOverlay);
    return {};
}

// GamePage.Enter.Loading / Enter.Paused (each Exit is TakeOverlay; Playing has no overlay).
K::SystemResult EnterLoading(K::Res<OverlayDocument> overlay, OverlayRoots roots) {
    Ui::UiBindings bindings;
    bindings.Set("loading", std::string("データを読み込んでいます…"));
    bindings.Set("progress", 0.0);
    PutOverlay(roots, *overlay, "loading", std::move(bindings));
    return {};
}
K::SystemResult EnterPaused(K::Res<OverlayDocument> overlay, OverlayRoots roots) {
    Ui::UiBindings bindings;
    bindings.Set("title", overlay->title);
    PutOverlay(roots, *overlay, "pause", std::move(bindings));
    return {};
}

// Loading: the loading screen's words and bar; the game, once ready and started.
K::SystemResult ShowLoading(K::Res<GameReadiness> readiness, K::Res<ClientView> sim, K::Res<PW::MapLoad> map, K::Res<GameOrigin> origin,
                            K::Query<K::Write<Ui::UiBindings>> roots, K::ResMut<GamePageRequest> page) {
    if (readiness->ready && sim->started) {
        page->Request(GamePage::Playing);
        return {};
    }
    auto bindings = roots.GetMut<Ui::UiBindings>(kOverlay);
    if (!bindings) return {};
    const std::string text = LoadingText(*readiness, *sim, *map, origin->scene);
    const auto* shown = bindings->Find("loading");
    if (shown == nullptr || std::get<std::string>(*shown) != text) bindings->Set("loading", text);
    const auto* progress = bindings->Find("progress");
    if (progress == nullptr || std::get<double>(*progress) != readiness->progress) bindings->Set("progress", readiness->progress);
    return {};
}

// Playing -> Paused when the pointer is given back (Tab, Esc, the focus lost) or Esc is pressed: a change, not a pointer
// the game never had (a game that begins shows itself; its first click takes the pointer, ReadControls). Paused ->
// Playing when the game takes it again (a click, Tab, 続ける).
K::SystemResult PauseOnRelease(K::Res<ClientControls> controls, K::ResMut<GamePageRequest> page) {
    if (controls->pauseAsked) page->Request(GamePage::Paused);
    return {};
}
K::SystemResult PlayOnCapture(K::Res<ClientControls> controls, K::ResMut<GamePageRequest> page) {
    if (controls->controls.Captured()) page->Request(GamePage::Playing);
    return {};
}

// pvp.resume: the game takes the pointer again (ReadControls, next frame, as for a click).
K::SystemResult Resume(K::MessageReader<Ui::UiAction> actions, K::ResMut<ClientControls> controls) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action == "pvp.resume") controls->resume = true;
    }
    return {};
}

// pvp.evacuate: the game ends here. Single player: the game is settled into the save (the Solo World counted the
// deaths), back to the menu with what was written (or why not). Multiplayer: back to the lobby (the Sim World of the
// lobby has no target: the connection closes, and the Match writes the save when it sees the player leave).
K::SystemResult Evacuate(K::MessageReader<Ui::UiAction> actions, K::Res<GameOrigin> origin, K::Res<ClientView> sim,
                         K::ResMut<GatewayClient> gateway, K::ResMut<SceneRequest> request) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (request.Read().next) continue;
        // pvp.abort (the loading screen's 中止): no game was played. Single player: back to its room, nothing settled;
        // multiplayer: as an evacuation (the room is left).
        if (action->action == "pvp.abort" && origin->scene == Scene::SoloGame) {
            request->next = SceneChange{.scene = Scene::SoloRoom, .status = "中止しました", .map = origin->map};
            continue;
        }
        if (action->action != "pvp.evacuate" && action->action != "pvp.abort") continue;
        if (origin->scene == Scene::MultiGame) {
            // The player leaves the room too (it gives no connect any more: a later game is a new room). The call
            // goes on in the lobby, which the GatewayClient is carried to.
            if (origin->room != 0) gateway->Ask("leave", LeaveRequest(origin->room, RandomHex(16)));
            request->next = SceneChange{.scene = Scene::Lobby, .status = "撤退しました"};
            continue;
        }
        const auto settled = SettleSolo(origin->savePath, sim->deaths);
        const std::string status = settled ? "撤退しました（死亡 " + std::to_string(sim->deaths) + " 回）。記録しました"
                                           : "撤退しました。記録できません: " + SoloSaveText(settled.error());
        request->next = SceneChange{.scene = Scene::Menu, .status = status};
    }
    return {};
}

// Frame World: a map did not load (this World's picture or the Sim World's walls), the Match's content differs, or the
// connection ended (the Sim World's picture says so): back to where the game came from with the reason, noted for the
// program (FrameNotes). The Match plays another arena this Client has (a Match reached by its address): the game again
// on that arena, as a World plays one arena (big-map declaration 15.3).
K::SystemResult LeaveOnEnd(K::Res<ClientView> sim, K::Res<PW::MapLoad> map, K::Res<PW::GameRules> rules, K::Res<GameOrigin> origin,
                           K::ResMut<FrameNotes> notes, K::ResMut<SceneRequest> request) {
    if (request.Read().next) return {};
    const GameOrigin& from = *origin;
    const std::string& mapFailure = !map->failure.empty() ? map->failure : sim->mapLoad.failure;
    std::string reason;
    if (!mapFailure.empty()) {
        notes->map = mapFailure;
        reason = "地図を読み込めません: " + mapFailure;
    } else if (!sim->mismatch.empty()) {
        if (from.scene == Scene::MultiGame && from.connect && !sim->mismatchArena.empty() && sim->mismatchArena != rules->arena.id &&
            from.game && from.game->Find(sim->mismatchArena) != nullptr) {
            request->next = SceneChange{.scene = Scene::MultiGame, .connect = from.connect, .room = from.room, .map = sim->mismatchArena};
            return {};
        }
        notes->mismatch = sim->mismatch;
        reason = "Match の内容がこの Client と違います: " + sim->mismatch;
    } else if (sim->ended) {
        notes->endReason = sim->endReason;
        reason = sim->endReason;
    } else {
        return {};
    }
    if (from.scene == Scene::SoloGame) {
        request->next = SceneChange{.scene = Scene::SoloRoom, .status = std::move(reason), .map = from.map};
        return {};
    }
    // The room is not left: the player is still its member, and may come back to the Match (the room gives a new
    // connect while the Match is on; the disconnect rules keep the place).
    request->next = SceneChange{.scene = Scene::Lobby, .status = std::move(reason)};
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
        ClientExchange::Merge(state.pending, controls->step);
        state.viewTick = controls->viewTick;
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
    if (const auto* clock = Find<N::ClockSync>(world); clock != nullptr && clock->synced) {
        view.synced = true;
        view.remoteTick = clock->remoteTick;
        view.rttTicks = clock->rttTicks;
        view.jitterTicks = clock->jitterTicks;
    }
    view.ownPlayer = PW::OwnPlayer(world);
    if (const auto* mismatch = world.Resource<PW::ContentMismatch>()) {
        view.mismatch = mismatch->reason;
        view.mismatchArena = mismatch->matchArena;
    }
    if (const auto* map = Find<PW::MapLoad>(world)) view.mapLoad = *map;
    if (const auto* deaths = Find<PW::LocalDeaths>(world)) view.deaths = deaths->count;
    if (const auto* info = Find<PW::MatchInfo>(world)) view.started = info->phase == PW::MatchPhase::Playing;
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
    // The own player: the prediction, as it is. The others: recorded at the confirmed tick (they are not simulated
    // at the Client, so the picture holds the Match's state of that tick); InterpolateRemotes writes their copies.
    const std::uint64_t localUser = world.Resource<PW::LocalUser>()->userId;
    RemoteView& remote = *world.ResourceMut<RemoteView>();
    remote.fresh = true;
    const std::optional<std::uint64_t> confirmed = snapshot->view.latestConfirmed;
    std::set<K::EntityId> players;
    for (const PlayerSnapshot& player : snapshot->players) {
        players.insert(player.id);
        put(player.id, player.owner);
        if (player.owner.userId == localUser) {
            putOptional(player.id, player.body);
            putOptional(player.id, player.combat);
            putOptional(player.id, player.hits);
            putOptional(player.id, player.shot);
            continue;
        }
        RemoteHistory& history = remote.players[player.id];
        history.goneAt.reset();
        if (confirmed && (history.samples.empty() || history.samples.back().tick < *confirmed)) {
            history.samples.push_back(RemoteSample{*confirmed, player.body, player.combat, player.hits, player.shot});
        }
    }
    for (const K::EntityId gone : mirror.players) {
        if (players.contains(gone)) continue;
        // Another player goes when the timeline passes the tick it left at (lightyear's delayed despawn); the own
        // player at once.
        const auto found = remote.players.find(gone);
        if (found != remote.players.end()) {
            if (!found->second.goneAt) found->second.goneAt = confirmed.value_or(0);
        } else {
            world.Despawn(gone);
        }
    }
    mirror.players = std::move(players);
    // The walls the Sim World built for the arena both Worlds play.
    if (const PW::GameRules* rules = world.Resource<PW::GameRules>();
        snapshot->arenaId == rules->arena.id && rules->wallsDigest != snapshot->wallsDigest) {
        world.ResourceMut<PW::GameRules>()->wallsDigest = snapshot->wallsDigest;
    }
    world.SetResource(snapshot->view);
}

// Frame World, Main/PreUpdate (after AdvanceTime, before the Systems that show the players): the timeline follows
// the newest state this Client can have minus the delay, and every other player's copy is its history at the
// timeline's tick. The newest state is the Match's tick estimated by the Sim World's clock sync less half the RTT
// (the state of the Match's tick now is still on its way). lightyear's objective is the estimate itself less the
// delay (crates/replication/interpolation/src/timeline.rs:189-215): with a one-way time longer than the delay its
// timeline runs past every state it has and holds the newest (a step at a time again); the design review of this
// segment measured that at about 86 ms of RTT, so the half RTT is taken off here. Before the clock sync has an
// estimate the newest state is shown.
K::SystemResult InterpolateRemotes(K::Res<ClientView> sim, K::Res<WT::RealTime> real, K::ResMut<RemoteView> remote,
                                   K::Query<K::Write<PW::PlayerBody>> bodies, K::Query<K::Write<PW::PlayerCombat>> combats,
                                   K::Query<K::Write<PW::HitRecord>> hits, K::Query<K::Write<PW::ShotRecord>> shots,
                                   K::CommandsFor<K::Inserts<PW::PlayerBody, PW::PlayerCombat, PW::HitRecord, PW::ShotRecord>,
                                                  K::Removes<PW::PlayerBody, PW::PlayerCombat, PW::HitRecord, PW::ShotRecord>> commands,
                                   K::Despawn despawn) {
    RemoteView& view = remote.Get();
    const auto step = static_cast<double>(sim->stepNanoseconds);
    if (view.fresh) {
        view.pictureAt = real->elapsed;
        view.fresh = false;
    }
    if (sim->synced && step > 0.0) {
        const double since = static_cast<double>(real->elapsed - view.pictureAt) / step;
        const double objective =
            sim->remoteTick - sim->rttTicks / 2.0 + since - present::InterpolationDelay(kTimeline, sim->jitterTicks, step / 1e9);
        const double delta = view.lastFrameAt ? static_cast<double>(real->elapsed - *view.lastFrameAt) / step : 0.0;
        present::Advance(view.timeline, delta, objective, kTimeline);
    } else {
        view.timeline = {};
    }
    view.lastFrameAt = real->elapsed;
    const bool timed = view.timeline.synced;
    const auto write = [&](const K::EntityId id, auto& query, const auto& value) {
        using T = typename std::remove_cvref_t<decltype(value)>::value_type;
        auto now = query.template GetMut<T>(id);
        if (!value) {
            if (now) commands.template Remove<T>(id);
        } else if (!now) {
            commands.Insert(id, *value);
        } else if (!(now.Read() == *value)) {
            now.Get() = *value;
        }
    };
    for (auto it = view.players.begin(); it != view.players.end();) {
        const K::EntityId id = it->first;
        RemoteHistory& history = it->second;
        if (history.goneAt && (!timed || view.timeline.tick >= static_cast<double>(*history.goneAt))) {
            despawn.Entity(id);
            it = view.players.erase(it);
            continue;
        }
        if (history.samples.empty()) {
            ++it;
            continue;
        }
        std::vector<std::uint64_t> ticks;
        ticks.reserve(history.samples.size());
        for (const RemoteSample& sample : history.samples) ticks.push_back(sample.tick);
        RemoteSample shown = history.samples.back();
        if (timed) {
            const std::size_t drop = present::Drainable(ticks, view.timeline.tick);
            history.samples.erase(history.samples.begin(), history.samples.begin() + static_cast<std::ptrdiff_t>(drop));
            ticks.erase(ticks.begin(), ticks.begin() + static_cast<std::ptrdiff_t>(drop));
            const present::Bracket bracket = present::BracketAt(ticks, view.timeline.tick);
            shown = bracket.end ? Blend(history.samples[bracket.start], history.samples[*bracket.end], bracket.fraction)
                                : history.samples[bracket.start];
        } else if (history.samples.size() > 2) {
            history.samples.erase(history.samples.begin(), history.samples.end() - 2);
        }
        write(id, bodies, shown.body);
        write(id, combats, shown.combat);
        write(id, hits, shown.hits);
        write(id, shots, shown.shot);
        ++it;
    }
    return {};
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

// The player of a game scene: the connection's user (the Gateway's user id, or --user with --connect), else this
// Client's own (single player).
std::uint64_t LocalUserOf(const SceneChange& scene, const ClientOptions& options) {
    return scene.connect ? scene.connect->userId : options.userId;
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
        // The pause: virtual time stops (world_time's TimeControl), so do the steps.
        WT::TimeControl& time = *world.ResourceMut<WT::TimeControl>();
        const std::uint8_t paused = exchange->Paused() ? 1 : 0;
        if (time.paused != paused) time.paused = paused;
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
    r.Resource<PW::MapLoad>({.name = "MapLoad"});
    r.Resource<ClientView>({.name = "object_fps_pvp.ClientView"});
    r.Resource<FrameMirror>({.name = "object_fps_pvp.FrameMirror"});
    r.Resource<RemoteView>({.name = "object_fps_pvp.RemoteView"});
    r.Resource<ClientAssets>({.name = "object_fps_pvp.ClientAssets"});
    r.Resource<ClientControls>({.name = "object_fps_pvp.ClientControls"});
    r.Resource<ClientScene>({.name = "object_fps_pvp.ClientScene"});
    r.Resource<ClientHud>({.name = "object_fps_pvp.ClientHud"});
    r.Resource<ClientSound>({.name = "object_fps_pvp.ClientSound"});
    r.Resource<ClientCharacters>({.name = "object_fps_pvp.ClientCharacters"});
    r.Component<CharacterPart>({.name = "object_fps_pvp.CharacterPart"});
    r.Component<ViewmodelPart>({.name = "object_fps_pvp.ViewmodelPart"});
    r.Resource<GameOrigin>({.name = "object_fps_pvp.GameOrigin"});
    r.Resource<GameReadiness>({.name = "object_fps_pvp.GameReadiness"});
    r.Resource<CoverState>({.name = "object_fps_pvp.CoverState"});
    r.Resource<DrawnFrames>({.name = "object_fps_pvp.DrawnFrames"});
}

// The arena a game scene plays: the one its change names when this Client has it, else the Client's own.
std::size_t ArenaOf(const SceneChange& change, const ClientOptions& options) {
    const auto& arenas = options.game->arenas;
    for (std::size_t i = 0; i < arenas.size(); ++i) {
        if (arenas[i].id == change.map) return i;
    }
    return options.arena;
}

void BuildGame(O::WorldSetup& setup, const ClientOptions& options, const SceneChange& change, const std::string& dataDirectory) {
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
    K::SystemOptions interpolate;
    interpolate.name = "object_fps_pvp.InterpolateRemotes";
    Mount(interpolate, O::DefaultSchedules::frame, O::DefaultSchedules::preUpdate);
    r.AddSystem<&InterpolateRemotes>(std::move(interpolate));
    {
        K::SystemOptions show = InUpdate("object_fps_pvp.ShowArena");
        show.mounts.front().conditions.push_back(O::WhileGenerating());
        r.AddSystem<&ShowArena>(std::move(show));
    }
    r.Resource<ArenaPictureLoading>({.name = "object_fps_pvp.ArenaPictureLoading"});
    r.AddSystem<&PlaceCamera>(InUpdate("object_fps_pvp.PlaceCamera", {"object_fps_pvp.ShowHud"}));
    r.AddSystem<&PoseCharacters>(InUpdate("object_fps_pvp.PoseCharacters"));
    r.AddSystem<&AddCharacters>(InUpdate("object_fps_pvp.AddCharacters", {"object_fps_pvp.PoseCharacters", "object_fps_pvp.RequestAssets"}));
    r.AddSystem<&PoseViewmodel>(InUpdate("object_fps_pvp.PoseViewmodel", {"object_fps_pvp.PlaceCamera"}));
    r.AddSystem<&AddViewmodel>(InUpdate("object_fps_pvp.AddViewmodel", {"object_fps_pvp.PoseViewmodel", "object_fps_pvp.RequestAssets"}));
    r.AddSystem<&ShowHud>(InUpdate("object_fps_pvp.ShowHud"));
    r.AddSystem<&PlaySounds>(InUpdate("object_fps_pvp.PlaySounds"));
    r.AddSystem<&LeaveOnEnd>(InUpdate("object_fps_pvp.LeaveOnEnd"));
    r.AddSystem<&TrackReadiness>(InUpdate("object_fps_pvp.TrackReadiness", {"object_fps_pvp.RequestAssets", "object_fps_pvp.ShowArena"}));
    r.AddSystem<&RevealUnderCover>(InUpdate("object_fps_pvp.RevealUnderCover", {"object_fps_pvp.TrackReadiness"}));
    r.Resource<OverlayDocument>({.name = "object_fps_pvp.OverlayDocument"});
    SM::RegisterStateMachine<GamePage>(setup);
    const auto onPage = [](std::string name, std::string schedule) {
        K::SystemOptions options;
        options.name = std::move(name);
        Mount(options, schedule, SM::kEnterExitStage);
        return options;
    };
    const auto inPage = [](std::string name, K::ConditionRef page, std::vector<std::string> after = {}) {
        K::SystemOptions options = InUpdate(std::move(name), std::move(after));
        options.mounts.front().conditions.push_back(std::move(page));
        return options;
    };
    r.AddSystem<&EnterLoading>(onPage("object_fps_pvp.EnterLoading", SM::EnterScheduleName(GamePage::Loading)));
    r.AddSystem<&TakeOverlay>(onPage("object_fps_pvp.ExitLoading", SM::ExitScheduleName(GamePage::Loading)));
    r.AddSystem<&EnterPaused>(onPage("object_fps_pvp.EnterPaused", SM::EnterScheduleName(GamePage::Paused)));
    r.AddSystem<&TakeOverlay>(onPage("object_fps_pvp.ExitPaused", SM::ExitScheduleName(GamePage::Paused)));
    r.AddSystem<&ShowLoading>(inPage("object_fps_pvp.ShowLoading", SM::InState<GamePage, GamePage::Loading>(), {"object_fps_pvp.TrackReadiness"}));
    r.AddSystem<&PauseOnRelease>(inPage("object_fps_pvp.PauseOnRelease", SM::InState<GamePage, GamePage::Playing>()));
    r.AddSystem<&PlayOnCapture>(inPage("object_fps_pvp.PlayOnCapture", SM::InState<GamePage, GamePage::Paused>()));
    r.AddSystem<&Resume>(inPage("object_fps_pvp.Resume", SM::InState<GamePage, GamePage::Paused>()));
    r.AddSystem<&Evacuate>(InUpdate("object_fps_pvp.Evacuate"));
    SoundPort* port = setup.ProductBuffer().Find<SoundPort>();
    ClientExchange* exchange = setup.ProductBuffer().Find<ClientExchange>();
    GYO_ASSERT(exchange != nullptr && "the Client's Worlds need ConfigureClient (it makes their ClientExchange)");
    GameOrigin origin{change.scene, change.room, change.scene == Scene::SoloGame ? SoloSavePath(dataDirectory) : std::string(), change.map,
                      change.connect, options.game};
    const std::uint64_t user = LocalUserOf(change, options);
    const std::size_t played = ArenaOf(change, options);
    setup.Prepare([options, port, origin, user, played](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        world.SetResource(origin);
        world.SetResource(GameReadiness{});
        world.SetResource(CoverState{});
        if (world.Resource<DrawnFrames>() == nullptr) world.SetResource(DrawnFrames{});
        // The rules the Sim World starts with (world::BuildWorld); the arena follows the Sim World's.
        const sim::ArenaContent& arena = options.game->arenas[played];
        PW::GameRules rules;
        rules.arena = arena;
        rules.stance = options.game->stance;
        rules.combat = options.game->combat;
        rules.character = options.game->character;
        world.SetResource(std::move(rules));
        world.SetResource(PW::LocalUser{user});
        world.SetResource(PW::MapLoad{});
        world.SetResource(ArenaPictureLoading{.map = arena.map, .jobs = options.jobs});
        world.SetResource(ClientView{});
        world.SetResource(FrameMirror{});
        world.SetResource(RemoteView{});
        world.SetResource(ClientAssets{});
        world.SetResource(ClientControls{});
        world.SetResource(ClientScene{options.presentation});
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
        // Black, seeing no layer, until the overlay covers the game (RevealUnderCover).
        world.Insert(kCamera, R::ClearColorConfig{R::ClearColorConfig::Mode::Custom, R::Color8{0, 0, 0, 255}});
        world.Insert(kCamera, R::RenderLayers{0});
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
        world.Insert(kViewCamera, R::RenderLayers{0});
        Place(world, kViewCamera, T::Transform{});
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
        world.Insert(kHudCamera, R::RenderLayers{0});
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
        // The overlay's document (the pages put their roots on kOverlay, there from the start: GamePage).
        Place(world, kOverlay, T::Transform{});
        world.SetResource(OverlayDocument{world.ResourceMut<WA::AssetRequests>()->Load<Ui::UiAsset>(A::AssetId("object_fps_pvp.ui.game")),
                                          origin.scene == Scene::SoloGame ? "一時停止" : "メニュー（対戦は続いています）"});
        return {};
    });
    setup.ExchangeIn([exchange](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        ApplySnapshot(*exchange, world);
        return {};
    });
    const bool solo = change.scene == Scene::SoloGame;
    setup.ExchangeOut([exchange, solo](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        ClientControls& controls = *world.ResourceMut<ClientControls>();
        present::StepInput step = controls.controls.Take();
        const GameReadiness& readiness = *world.Resource<GameReadiness>();
        step.ready = readiness.ready;
        const RemoteView& remote = *world.Resource<RemoteView>();
        exchange->AddControls(step, remote.timeline.synced ? std::optional<double>(remote.timeline.tick) : std::nullopt);
        // Single player's pause: the Solo World stops on the pause page (a Match does not wait; the loading neither:
        // the start waits for the player's readiness in the steps).
        exchange->SetPaused(solo && world.Resource<GamePageState>()->Current() == GamePage::Paused);
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
    const auto recoil = data::AsObject(data::Member(definition, "recoil"), "recoil");
    viewmodel.recoilKickRadians = static_cast<float>(data::Number(data::Member(recoil, "kick_degrees"), "recoil kick_degrees")) * k;
    viewmodel.recoilRecoveryPerSecond = static_cast<float>(data::Number(data::Member(recoil, "recovery_per_second"), "recoil recovery_per_second"));
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

T::Transform ViewmodelPlacement(const ViewmodelDefinition& viewmodel, const float recoilRadians) {
    T::Transform t;
    // (Left-handed, +Z ahead: a positive turn about +X looks down, so up is the negative one.)
    t.rotation = recoilRadians == 0.0F ? viewmodel.rotation
                                       : Math::Multiply(Math::MakeQuaternionFromAxisAngle({1, 0, 0}, -recoilRadians), viewmodel.rotation);
    t.translation = viewmodel.offset - Math::Rotate(t.rotation, viewmodel.anchor);
    return t;
}

float RecoilRadians(const ViewmodelDefinition& viewmodel, const double seconds) {
    if (seconds < 0.0) return 0.0F;
    const double left = std::exp(-static_cast<double>(viewmodel.recoilRecoveryPerSecond) * seconds);
    // Below a thousandth of the kick it is at rest (the weapon stops being written).
    return left < 1e-3 ? 0.0F : viewmodel.recoilKickRadians * static_cast<float>(left);
}

std::optional<PresentationContent> LoadPresentation(const std::string& root, std::string& error) {
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
    PW::ConfigureGame(setup);
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
    O::WorldDescription world = PW::ClientWorld({.content = options.game, .arena = options.arena, .localUser = options.userId});
    world.name = "Sim";
    // The game only in the scene that plays it; elsewhere the World is its units alone, without a target (no
    // connection: net-connect-target). The player is the connection's user (the Gateway's user id).
    world.build = [options](O::WorldSetup& setup) {
        const auto* director = setup.ProductBuffer().Find<SceneDirector>();
        GYO_ASSERT(director != nullptr && "the Client's Worlds need ConfigureClient (it makes their SceneDirector)");
        const SceneChange& scene = director->Current();
        if (!SimPlays(scene.scene)) {
            // The rollback strategy of the description still asks which schedule is the step (RB-E10).
            RB::RollbackEntry(setup).StepSchedule();
            return;
        }
        PW::BuildWorld(setup, {.content = options.game, .arena = ArenaOf(scene, options), .localUser = LocalUserOf(scene, options),
                               .jobs = options.jobs},
                       PW::WorldEnd::Client);
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
    O::WorldDescription world = PW::LocalWorld({.content = options.game, .arena = options.arena, .localUser = options.userId});
    world.name = "Solo";
    // The game only in the single player's game scene; its player at a spawn picked at random for each game.
    world.build = [options](O::WorldSetup& setup) {
        const auto* director = setup.ProductBuffer().Find<SceneDirector>();
        GYO_ASSERT(director != nullptr && "the Client's Worlds need ConfigureClient (it makes their SceneDirector)");
        const SceneChange& scene = director->Current();
        if (!SoloPlays(scene.scene)) return;
        std::random_device device;
        PW::BuildWorld(setup, {.content = options.game, .arena = ArenaOf(scene, options), .localUser = options.userId, .spawn = device(),
                               .jobs = options.jobs},
                       PW::WorldEnd::Local);
        BuildSolo(setup);
    };
    return world;
}

O::WorldDescription ClientFrameWorld(ClientOptions options, const bool window) {
    O::WorldDescription world;
    world.name = "Frame";
    world.units = {"entity_ids", "world_time", "transform", "world_asset", "camera", "input", "window", "window_camera",
                   "world_render", "visibility", "world_ui"};
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
        GYO_ASSERT(start != nullptr && "the Client's Worlds need ConfigureClient (it keeps their ClientStart)");
        const std::string& directory = start->dataDirectory;
        // The maps a room offers: the installed arenas, by id.
        std::vector<std::string> maps;
        for (const sim::ArenaContent& arena : options.game->arenas) maps.push_back(arena.id);
        GatewayContext gateway{directory, start->gateway, maps};
        switch (scene.scene) {
        case Scene::Menu: BuildMenu(setup, scene); break;
        case Scene::SoloRoom: BuildSoloRoom(setup, scene, directory, maps); break;
        case Scene::Lobby: BuildLobby(setup, scene, gateway); break;
        case Scene::Room: BuildRoom(setup, scene, gateway); break;
        case Scene::SoloGame:
        case Scene::MultiGame: BuildGame(setup, options, scene, directory); break;
        }
    };
    return world;
}

// Render World, Extract: the player's VSync (the Frame World's ClientSettings) into render's Presentation, written
// only when it differs (sdl_gpu sets the swapchain's present mode when Presentation changes).
K::SystemResult CopyPresentation(K::ExtractRes<ClientSettings> settings, K::ResMut<R::Presentation> presentation) {
    const R::PresentMode wanted = settings->vsync ? R::PresentMode::AutoVsync : R::PresentMode::AutoNoVsync;
    if (presentation.Read().presentMode != wanted) presentation->presentMode = wanted;
    return {};
}

O::WorldDescription ClientRenderWorld(const bool gpu) {
    O::WorldDescription world;
    world.name = "Render";
    world.extractFrom = "Frame";
    world.units = {"entity_ids", "render_world", "camera_render", "sprite_render", "mesh_render", "ui_render"};
    if (gpu) world.units.push_back("sdl_gpu");
    world.build = [](O::WorldSetup& setup) {
        K::SystemOptions copy;
        copy.name = "object_fps_pvp.CopyPresentation";
        Mount(copy, GYO::Engine::RenderWorld::kExtractSchedule, GYO::Engine::RenderWorld::kExtractSchedule);
        setup.Registrar().AddSystem<&CopyPresentation>(std::move(copy));
    };
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
    if (world.Types().ResourceIdOf<GameReadiness>()) {
        if (const auto* readiness = world.Resource<GameReadiness>()) {
            status.ready = readiness->ready;
            status.progress = readiness->progress;
        }
    }
    if (const auto* view = world.Resource<ClientView>()) status.started = view->started;
    return status;
}

} // namespace object_fps_pvp::client
