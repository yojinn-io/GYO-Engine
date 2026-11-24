#include "object_fps_pvp/client/ClientWorld.hpp"

#include "object_fps_pvp/client/VoxelModelLoader.hpp"
#include "object_fps_pvp/data/VoxelMeshFile.hpp"
#include "object_fps_pvp/present/Controls.hpp"
#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/WavLoader.hpp"
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

#include <algorithm>
#include <cmath>
#include <map>
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
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;
namespace PW = object_fps_pvp::world;

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

// The 2D layer: 1280 x 720 units, the origin at the centre, y up; drawn by the HUD camera only.
constexpr float kHudWidth = 1280.0F;
constexpr float kHudHeight = 720.0F;
constexpr std::uint32_t kHudLayer = 2;

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
    A::Handle<GYO::Engine::Model::ModelData> standIn;
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

// A stand-in body: the player it shows.
struct StandIn final {
    K::EntityId player = 0;
};
void HashValue(K::Hasher&, const StandIn&) {}

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
};
void HashValue(K::Hasher&, const ClientHud&) {}

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

R::Shape2d Flash(const std::array<float, 3> color, const float alpha) {
    R::Shape2d shape;
    const auto byte = [](const float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
    Rect(shape.vertices, shape.indices, -kHudWidth / 2, -kHudHeight / 2, kHudWidth / 2, kHudHeight / 2,
         R::Color8{byte(color[0]), byte(color[1]), byte(color[2]), byte(alpha)});
    return shape;
}


// ---- Systems

// The pictures, the font and the sounds, asked for once; the scene and the sounds use them once they are there.
K::SystemResult RequestAssets(K::Res<WA::AssetRequests> requests, K::Res<ClientScene> scene, K::ResMut<ClientAssets> assets) {
    if (assets.Read().requested) return {};
    ClientAssets& a = assets.Get();
    a.requested = true;
    a.standIn = requests->Load<GYO::Engine::Model::ModelData>(A::AssetId("object_fps_pvp.model.stand_in"));
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
                             K::Res<W::WindowState> window, K::Res<N::NetWorldLink> link, K::Res<PW::LocalUser> user,
                             K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>> players,
                             K::Query<K::Read<N::PeerInput<PW::PlayerInput>>> inputs, K::ResMut<ClientControls> state,
                             K::ResMut<W::WindowRequest> request,
                             K::CommandsFor<K::Inserts<N::PeerInput<PW::PlayerInput>>, K::Removes<>> commands) {
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
    // The own Peer's input buffer, for WriteStepInput.
    if (link->clientPeer != 0 && !inputs.Contains(link->clientPeer)) commands.Insert(link->clientPeer, N::PeerInput<PW::PlayerInput>{});
    return {};
}

// The input of the step that runs now, written into the own Peer's slot of this step before the net Module's entry
// (ApplyInput) reads it: each slot is written once, before its step runs and before it is sent (the net Module sends
// a tick's slots once and the Match keeps each (Peer, tick) once). FixedUpdate/First, not while resimulating (the
// rollback writes the recorded inputs back then). lightyear's client input System in FixedPreUpdate
// (crates/inputs/inputs/src/client.rs:41, 186-190, 418-445).
K::SystemResult WriteStepInput(K::Res<WT::Tick> tick, K::Res<N::NetWorldLink> link, K::Res<N::NetReplicaState> replica,
                               K::Query<K::Write<N::PeerInput<PW::PlayerInput>>> inputs, K::ResMut<ClientControls> state) {
    if (link->clientPeer == 0 || !inputs.Contains(link->clientPeer)) return {};
    const present::StepInput step = state->controls.Take();
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
    // The time the player saw: the remote players stand at the confirmed point.
    const std::uint32_t seen = replica->latestConfirmed.value_or(0);
    inputs.GetMut<N::PeerInput<PW::PlayerInput>>(link->clientPeer)
        ->Set(static_cast<std::uint32_t>(tick->value), input, N::ViewTime{seen, 0});
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
                            K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>> players,
                            K::Query<K::Write<T::Transform>, K::With<C::Camera3d>> cameras) {
    players.ForEach([&](K::EntityId, const PW::Owner& owner, const PW::PlayerBody& body) {
        if (owner.userId != user->userId) return;
        const float eye = sim::EyeHeight(rules->arena, rules->stance, body.crouched != 0);
        cameras.ForEach([&](K::EntityId, K::Mut<T::Transform> camera) {
            T::Transform t = camera.Read();
            t.translation = body.feet + Math::Vec3{0.0F, eye, 0.0F};
            t.rotation = ViewRotation(controls->controls.Yaw(), controls->controls.Pitch());
            camera.Get() = t;
        });
    });
    return {};
}

// The other living players, by player.
std::map<K::EntityId, PW::PlayerBody> OthersAlive(
    K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>>& players, const std::uint64_t userId) {
    std::map<K::EntityId, PW::PlayerBody> others;
    players.ForEach([&](const K::EntityId id, const PW::Owner& owner, const PW::PlayerBody& body, const PW::PlayerCombat& combat) {
        if (owner.userId != userId && combat.life == sim::LifeState::Alive) others[id] = body;
    });
    return others;
}

T::Transform BodyTransform(const PW::GameRules& rules, const PW::PlayerBody& body) {
    const float height = sim::BodyHeight(rules.arena, rules.stance, body.crouched != 0);
    T::Transform t;
    t.translation = body.feet;
    t.rotation = Math::MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, body.yaw);
    t.scale = {1.0F, height / rules.arena.bodyHeight, 1.0F};
    return t;
}

// The stand-in bodies follow their players; a body whose player is gone or dead goes.
K::SystemResult MoveBodies(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user,
                           K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>> players,
                           K::Query<K::Read<StandIn>, K::Write<T::Transform>> bodies, K::Despawn despawn) {
    const std::map<K::EntityId, PW::PlayerBody> others = OthersAlive(players, user->userId);
    for (const K::EntityId body : bodies.Entities()) {
        const auto shown = others.find(bodies.Get<StandIn>(body)->player);
        if (shown == others.end()) {
            despawn.Entity(body);
            continue;
        }
        const T::Transform t = BodyTransform(*rules, shown->second);
        if (!(t == *bodies.Get<T::Transform>(body))) *bodies.GetMut<T::Transform>(body) = t;
    }
    return {};
}

// A stand-in body for every other living player that has none (once the model asset is asked for).
K::SystemResult AddBodies(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientAssets> assets, K::Res<ClientScene> scene,
                          K::Query<K::Read<PW::Owner>, K::Read<PW::PlayerBody>, K::Read<PW::PlayerCombat>> players,
                          K::Query<K::Read<StandIn>> bodies,
                          K::Spawn<K::Bundle<R::Mesh3d, T::Transform, T::GlobalTransform, StandIn, RB::KeepThroughRollback>> spawn) {
    if (assets->standIn.Id().Empty()) return {};
    std::map<K::EntityId, PW::PlayerBody> others = OthersAlive(players, user->userId);
    bodies.ForEach([&](K::EntityId, const StandIn& standIn) { others.erase(standIn.player); });
    for (const auto& [player, body] : others) {
        R::Mesh3d mesh;
        mesh.model = assets->standIn;
        mesh.image = scene->presentation->standInPalette;
        mesh.filter = R::TextureFilter::Nearest;
        const T::Transform t = BodyTransform(*rules, body);
        auto spawned = spawn.New(std::move(mesh), t, T::GlobalTransform{T::ToMatrix(t)}, StandIn{player}, RB::KeepThroughRollback{});
        if (!spawned) return Base::Err(std::move(spawned).error());
    }
    return {};
}

K::SystemResult ShowHud(K::Res<PW::GameRules> rules, K::Res<PW::LocalUser> user, K::Res<ClientControls> controls,
                        K::Res<ClientAssets> assets, K::Res<WA::AssetView> view, K::Res<N::NetWorldLink> link,
                        K::Res<PW::ContentMismatch> mismatch, K::Res<WT::ActiveTime> time,
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
    if (auto flash = shapes.GetMut<R::Shape2d>(kFlash)) {
        flash.Get() = Flash(scene->presentation->hitFeedback.flash.color, feedback.flashAlpha);
    }
    // The texts.
    std::string hpText;
    std::string ammoText;
    std::string status;
    if (!mismatch->reason.empty()) {
        status = "内容が Match と違います: " + mismatch->reason;
    } else if (link->clientPeer == 0) {
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

void BuildClient(O::WorldSetup& setup, const ClientOptions& options) {
    K::Registrar& r = setup.Registrar();
    r.Resource<ClientAssets>({.name = "object_fps_pvp.ClientAssets"});
    r.Resource<ClientControls>({.name = "object_fps_pvp.ClientControls"});
    r.Resource<ClientScene>({.name = "object_fps_pvp.ClientScene"});
    r.Resource<ClientHud>({.name = "object_fps_pvp.ClientHud"});
    r.Resource<ClientSound>({.name = "object_fps_pvp.ClientSound"});
    r.Component<StandIn>({.name = "object_fps_pvp.StandIn"});
    r.AddSystem<&RequestAssets>(InUpdate("object_fps_pvp.RequestAssets"));
    K::SystemOptions read;
    read.name = "object_fps_pvp.ReadControls";
    K::Mount readMount;
    readMount.schedule = std::string(O::DefaultSchedules::frame);
    readMount.stage = std::string(O::DefaultSchedules::preUpdate);
    readMount.after = {std::string(I::kInputSet), std::string(W::kWindowSet)};
    read.mounts.push_back(std::move(readMount));
    r.AddSystem<&ReadControls>(std::move(read));
    K::SystemOptions write;
    write.name = "object_fps_pvp.WriteStepInput";
    K::Mount writeMount;
    writeMount.schedule = std::string(O::DefaultSchedules::fixed);
    writeMount.stage = std::string(O::DefaultSchedules::first);
    writeMount.before = {"ApplyInput:PlayerInput"};
    write.mounts.push_back(std::move(writeMount));
    RB::RollbackEntry(setup).SkipDuringResimulation(write);
    r.AddSystem<&WriteStepInput>(std::move(write));
    r.AddSystem<&ShowArena>(InUpdate("object_fps_pvp.ShowArena"));
    r.AddSystem<&PlaceCamera>(InUpdate("object_fps_pvp.PlaceCamera"));
    r.AddSystem<&MoveBodies>(InUpdate("object_fps_pvp.MoveBodies"));
    r.AddSystem<&AddBodies>(InUpdate("object_fps_pvp.AddBodies", {"object_fps_pvp.MoveBodies", "object_fps_pvp.RequestAssets"}));
    r.AddSystem<&ShowHud>(InUpdate("object_fps_pvp.ShowHud"));
    r.AddSystem<&PlaySounds>(InUpdate("object_fps_pvp.PlaySounds"));
    SoundPort* port = setup.ProductBuffer().Find<SoundPort>();
    setup.Prepare([options, port](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        world.SetResource(ClientAssets{});
        world.SetResource(ClientControls{});
        world.SetResource(ClientScene{options.presentation, {}});
        world.SetResource(ClientHud{});
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
        world.Insert(kCamera, RB::KeepThroughRollback{});
        // The arena's picture (its Mesh3d comes with the model).
        Place(world, kArenaPicture, T::Transform{});
        world.Insert(kArenaPicture, RB::KeepThroughRollback{});
        // The HUD camera: 2D, after the world camera, clears nothing, sees the HUD layer only.
        C::OrthographicProjection flat = C::OrthographicProjection::Default2d();
        flat.scalingMode = C::ScalingFixed{kHudWidth, kHudHeight};
        C::Camera hud;
        hud.order = 1;
        world.Insert(kHudCamera, hud);
        world.Insert(kHudCamera, C::Camera2d{});
        world.Insert(kHudCamera, C::CameraTarget{});
        world.Insert(kHudCamera, C::Projection{flat});
        world.Insert(kHudCamera, C::ComputedCamera{});
        world.Insert(kHudCamera, R::ClearColorConfig{R::ClearColorConfig::Mode::None, {}});
        world.Insert(kHudCamera, R::RenderLayers{1U << kHudLayer});
        Place(world, kHudCamera, T::Transform{});
        world.Insert(kHudCamera, RB::KeepThroughRollback{});
        const auto hudThing = [&world](const K::EntityId id, const Math::Vec3 at) {
            T::Transform t;
            t.translation = at;
            Place(world, id, t);
            world.Insert(id, R::RenderLayers{1U << kHudLayer});
            world.Insert(id, RB::KeepThroughRollback{});
        };
        hudThing(kFlash, {0, 0, 0});
        world.Insert(kFlash, R::Shape2d{});
        hudThing(kCrosshair, {0, 0, 1});
        world.Insert(kCrosshair, Crosshair());
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

} // namespace

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
    {
        const std::string path = root + "/characters/stand_in.voxel_mesh.json";
        auto text = ReadText(fs, path, error);
        if (!text) return std::nullopt;
        auto file = data::ParseVoxelMeshFile(*text, error);
        if (!file) {
            error = path + ": " + error;
            return std::nullopt;
        }
        content.standInPalette = PaletteOf(*file);
    }
    for (const world::InstalledArena& arena : game.arenas) content.arenaPalettes.push_back(PaletteOf(arena.geometry));
    return content;
}

void ConfigureClient(O::ProductSetup& setup) {
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
    if (refused("font", registry->template Register<TX::FontLoader>())) return;
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

O::WorldDescription ClientGameWorld(ClientOptions options, const bool window) {
    O::WorldDescription world = PW::ClientWorld({options.game, options.arena, options.userId, nullptr});
    for (const char* unit : {"world_asset", "camera", "input", "window", "window_camera", "world_render"}) world.units.push_back(unit);
    if (window) world.units.push_back("sdl_window");
    world.build = [inner = std::move(world.build), options = std::move(options)](O::WorldSetup& setup) {
        inner(setup);
        BuildClient(setup, options);
    };
    return world;
}

O::WorldDescription ClientRenderWorld(const bool gpu) {
    O::WorldDescription world;
    world.name = "Render";
    world.extractFrom = "Game";
    world.units = {"entity_ids", "render_world", "camera_render", "sprite_render", "mesh_render"};
    if (gpu) world.units.push_back("sdl_gpu");
    return world;
}

ClientStatus StatusOf(K::World& world) {
    ClientStatus status;
    if (const auto* link = world.Resource<N::NetWorldLink>()) status.connected = link->clientPeer != 0 && world.Alive(link->clientPeer);
    status.playing = PW::OwnPlayer(world) != 0;
    if (const auto* controls = world.Resource<ClientControls>()) status.captured = controls->controls.Captured();
    if (const auto* mismatch = world.Resource<PW::ContentMismatch>()) status.mismatch = mismatch->reason;
    return status;
}

} // namespace object_fps_pvp::client
