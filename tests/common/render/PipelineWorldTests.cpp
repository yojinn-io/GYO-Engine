// The render World reading a source World (docs/architecture/target/v2/render.zh-Hant.md sections 5 and 7, RD-5 to
// RD-7): render_world's extract schedule, camera_render's ExtractCameras, sprite_render's ExtractSprites2d and
// Queue2dPasses, in a Product of two Worlds ("Game" with transform, camera, world_asset, world_render; "Render" with
// extractFrom "Game"). A recorder System in the Submit stage stands where sdl_gpu's GPUSubmit stands. Tags "[re-...]"
// are the mutants' expectations.
#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/camera_render/CameraRenderModule.hpp"
#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/Pipeline3d.hpp"
#include "engine/render/RenderMeshes.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/sprite_render/SpriteRenderModule.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/visibility/VisibilityModule.hpp"
#include "engine/world_render/WorldRenderModule.hpp"
#include "engine/render/RenderTargets.hpp"
#include "engine/image/PngLoader.hpp"
#include "engine/mesh_render/MeshRenderModule.hpp"
#include "engine/model/FbxLoader.hpp"
#include "engine/world_asset/AssetRequests.hpp"

#include "StoreSupport.hpp"
#include "WorldAssetSupport.hpp"

#include <fstream>
#include <iterator>

namespace C = GYO::Engine::Camera;
namespace I = GYO::Engine::Image;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace RW = GYO::Engine::RenderWorld;
namespace T = GYO::Engine::Transform;

namespace {

// What the backend would see at Submit, frame by frame.
// all: every pass; clears: the whole-target clears camera_render composed (a Pass2D with a clear); passes: the other
// 2D passes (a camera's content, a later camera's viewport clear).
struct Seen {
    std::vector<R::RenderPass> all;
    std::vector<R::Color8> clears;
    std::vector<R::Pass2D> passes;
    std::vector<R::TextureOp> asks;
    std::vector<R::MeshOp> meshAsks;
};
std::vector<Seen> seen;
std::vector<std::string> trail;

K::SystemResult Record(K::Res<R::RenderCommands> commands, K::Res<R::RenderTextures> textures, K::Res<R::RenderMeshes> meshes) {
    Seen s;
    s.all = commands->passes;
    for (const R::RenderPass& pass : commands->passes) {
        const auto* p2 = std::get_if<R::Pass2D>(&pass);
        if (p2 == nullptr) continue;
        if (p2->clear) {
            s.clears.push_back(*p2->clear);
        } else {
            s.passes.push_back(*p2);
        }
    }
    s.asks.assign(textures->Pending().begin(), textures->Pending().end());
    s.meshAsks.assign(meshes->Pending().begin(), meshes->Pending().end());
    seen.push_back(std::move(s));
    return {};
}
K::SystemResult MarkExtract(K::Res<R::ExtractedCameras>) {
    trail.push_back("extract");
    return {};
}
K::SystemResult MarkFirst(K::Res<R::ExtractedCameras>) {
    trail.push_back("first");
    return {};
}

K::SystemOptions In(std::string name, std::string schedule, std::string_view stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::string(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

O::RegisterRow Row(std::string name, bool library, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = library;
    row.defaultOn = !library;
    row.depends = std::move(depends);
    return row;
}

template <class M>
void Add(O::RegisteredUnits& units, std::string name, std::vector<std::string> depends = {}) {
    units.rows.push_back(Row(name, false, std::move(depends)));
    units.modules.push_back({std::move(name), std::make_unique<M>()});
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(Row("render", true));
    Add<GYO::Engine::EntityIds::EntityIdsModule>(units, "entity_ids");
    Add<GYO::Engine::WorldAsset::WorldAssetModule>(units, "world_asset");
    Add<T::TransformModule>(units, "transform");
    Add<C::CameraModule>(units, "camera", {"transform"});
    Add<GYO::Engine::WorldRender::WorldRenderModule>(units, "world_render", {"render"});
    Add<RW::RenderWorldModule>(units, "render_world", {"render"});
    Add<GYO::Engine::CameraRender::CameraRenderModule>(units, "camera_render", {"render", "render_world"});
    Add<GYO::Engine::SpriteRender::SpriteRenderModule>(units, "sprite_render", {"render", "render_world", "camera_render"});
    Add<GYO::Engine::MeshRender::MeshRenderModule>(units, "mesh_render", {"render", "render_world", "camera_render"});
    Add<GYO::Engine::Visibility::VisibilityModule>(units, "visibility", {"render", "transform", "camera"});
    return units;
}

O::WorldDescription Game(std::vector<std::string> more = {}) {
    O::WorldDescription w;
    w.name = "Game";
    w.units = {"entity_ids", "world_asset", "transform", "camera", "world_render"};
    for (std::string& unit : more) w.units.push_back(std::move(unit));
    return w;
}

O::WorldDescription Render(std::vector<std::string> units = {"entity_ids", "render_world", "camera_render", "sprite_render"}) {
    O::WorldDescription w;
    w.name = "Render";
    w.units = std::move(units);
    w.extractFrom = "Game";
    w.build = [](O::WorldSetup& s) { s.Registrar().AddSystem<&Record>(In("TestRecord", "Main", RW::kSubmitStage)); };
    return w;
}

std::unique_ptr<O::Product> Make(O::WorldDescription render = Render(), const GYO::Engine::WorldAsset::AssetOptions* assets = nullptr,
                                 O::WorldDescription game = Game()) {
    seen.clear();
    trail.clear();
    O::ProductDescription d;
    d.name = "draws";
    if (assets != nullptr) d.options.Set(*assets);
    d.worlds.push_back(std::move(game));
    d.worlds.push_back(std::move(render));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& GameOf(O::Product& p) { return p.World("Game").GetWorld(); }

void Frames(O::Product& p, int count = 1) {
    for (int i = 0; i < count; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }
}

void Place(K::World& world, const K::EntityId id, const float x, const float y, const float z) {
    T::Transform t;
    t.translation = {x, y, z};
    world.Insert(id, t);
    world.Insert(id, T::GlobalTransform{T::ToMatrix(t)});
}

// A 2D camera showing 1280 x 720 units letterboxed into a 2560 x 1600 window (viewport (0, 80) 2560 x 1440).
void Camera2dAt(K::World& world, const K::EntityId id, const std::int32_t order = 0) {
    C::OrthographicProjection o = C::OrthographicProjection::Default2d();
    o.scalingMode = C::ScalingFixed{1280.0F, 720.0F};
    C::Camera camera;
    camera.order = order;
    world.Insert(id, camera);
    world.Insert(id, C::Camera2d{});
    world.Insert(id, C::CameraTarget{});
    world.Insert(id, C::Projection{o});
    world.Insert(id, C::ComputedCamera{});
    world.Insert(id, C::CameraView{});
    world.Insert(id, C::Letterbox{1280.0F, 720.0F});
    Place(world, id, 0.0F, 0.0F, 0.0F);
}

std::shared_ptr<const I::ImageData> Picture() {
    return std::make_shared<const I::ImageData>(2, 2, I::ColorSpace::Srgb, std::vector<std::uint8_t>(16, 200));
}

std::size_t Count(const std::vector<R::TextureOp>& asks, std::size_t index) {
    std::size_t n = 0;
    for (const R::TextureOp& op : asks) n += op.index() == index ? 1 : 0;
    return n;
}

} // namespace

TEST_CASE("render_extract: render_world puts the extract schedule before Main only in a World with extractFrom") {
    seen.clear();
    trail.clear();
    O::ProductDescription d;
    d.name = "draws";
    d.worlds.push_back(Game());
    O::WorldDescription render = Render({"entity_ids", "render_world", "camera_render"});
    render.build = [](O::WorldSetup& s) {
        s.Registrar().AddSystem<&MarkExtract>(In("TestExtract", std::string(RW::kExtractSchedule), RW::kExtractSchedule));
        s.Registrar().AddSystem<&MarkFirst>(In("TestFirst", "Main", O::DefaultSchedules::first));
    };
    d.worlds.push_back(std::move(render));
    O::WorldDescription alone;
    alone.name = "Alone";
    alone.units = {"entity_ids", "render_world"};
    alone.build = [](O::WorldSetup& s) {
        const auto& frame = s.Frame().Names();
        CHECK_MESSAGE(std::find(frame.begin(), frame.end(), std::string(RW::kExtractSchedule)) == frame.end(),
                      "[re-no-extract-without-source] no extract schedule without extractFrom");
    };
    d.worlds.push_back(std::move(alone));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    Frames(**built, 2);
    REQUIRE(trail.size() == 4);
    CHECK_MESSAGE((trail[0] == "extract" && trail[1] == "first" && trail[2] == "extract"), "[re-extract-before-main] extract, then Main");
}

TEST_CASE("render_extract: a render World of render_world alone extracts from a source without cameras or drawables") {
    // KeepTool's shape (SDL declaration section 10.1, R2b: "回頭驗 KeepTool"): no camera_render, no R-E4.
    seen.clear();
    O::ProductDescription d;
    d.name = "tool";
    O::WorldDescription document;
    document.name = "Game";
    document.units = {"entity_ids"};
    d.worlds.push_back(std::move(document));
    d.worlds.push_back(Render({"entity_ids", "render_world"}));
    auto built = O::Product::Build(std::move(d), Units());
    CHECK_MESSAGE(static_cast<bool>(built), "[re-keeptool] " << (built ? std::string() : built.error().detail));
}

TEST_CASE("render_extract: a 2D camera's pass draws the source's sprites and shapes back to front, letterboxed") {
    auto p = Make();
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{2560, 1600}, 1.0F});
    Camera2dAt(game, 1);
    R::Sprite sprite;
    sprite.image = Picture();
    game.Insert(2, sprite);
    Place(game, 2, 0.0F, 0.0F, 1.0F);
    R::Shape2d ribbon;
    ribbon.topology = R::Topology::TriangleStrip;
    ribbon.vertices = {{{0, 0}, {}, {255, 0, 0, 255}}, {{0, 10}, {}, {255, 0, 0, 255}}, {{10, 0}, {}, {255, 0, 0, 255}},
                       {{10, 10}, {}, {255, 0, 0, 255}}};
    game.Insert(3, ribbon);
    Place(game, 3, 0.0F, 0.0F, 0.0F);
    // The target size is there before frame 1: the source computes the camera in its PostUpdate and the render World
    // extracts it in the same frame.
    Frames(*p, 2);
    REQUIRE(seen.size() == 2);
    CHECK_MESSAGE(seen[0].passes.size() == 1, "[re-same-frame] the first frame already draws");
    REQUIRE(seen[1].passes.size() == 1);
    const R::Pass2D& pass = seen[1].passes[0];
    CHECK_MESSAGE((pass.viewport == R::PixelRect{0, 80, 2560, 1440}), "[re-viewport] the camera's letterbox viewport");
    CHECK_MESSAGE((seen[1].clears.size() == 1 && seen[1].clears[0] == R::Color8{0, 0, 0, 255}), "[re-clear-default] the source's ClearColor (black)");
    CHECK_MESSAGE((seen[1].all.size() == 2 && std::holds_alternative<R::Pass2D>(seen[1].all[0]) &&
                   std::get<R::Pass2D>(seen[1].all[0]).clear.has_value()),
                  "[re-clear-first] the camera's clear comes before its pass");
    REQUIRE(pass.draws.size() == 2);
    CHECK_MESSAGE(pass.draws[0].texture.value == 0, "[re-z-order] the shape (z 0) before the sprite (z 1)");
    CHECK_MESSAGE(pass.draws[0].indexCount == 6, "[re-strip] the strip's two triangles");
    CHECK_MESSAGE(pass.draws[1].texture.value != 0, "[re-sprite-texture] the sprite's picture");
    CHECK_MESSAGE(Count(seen[0].asks, 0) + Count(seen[1].asks, 0) == 1, "[re-texture-once] the picture is made once");
}

namespace {
// In the source World's Last (after its cameras are computed), from the frame after the flag is set: lets the target go.
C::CameraTarget releaseLate{};
bool releaseNow = false;
K::SystemResult ReleaseLate(K::ResMut<R::RenderTargets> targets) {
    if (releaseNow && targets.Read().Find(releaseLate) != nullptr) targets.Get().Release(releaseLate);
    return {};
}
std::size_t PassesOn(const std::vector<R::RenderPass>& passes, const std::optional<R::TextureId> target) {
    std::size_t n = 0;
    for (const R::RenderPass& pass : passes) n += R::TargetOf(pass) == target ? 1 : 0;
    return n;
}
std::vector<R::TextureId> TargetsMade(const std::vector<R::TextureOp>& asks) {
    std::vector<R::TextureId> made;
    for (const R::TextureOp& op : asks) {
        if (const auto* t = std::get_if<R::TextureTargetCreate>(&op)) made.push_back(t->id);
    }
    return made;
}
bool Released(const std::vector<R::TextureOp>& asks, const R::TextureId id) {
    for (const R::TextureOp& op : asks) {
        if (const auto* r = std::get_if<R::TextureRelease>(&op); r != nullptr && r->id == id) return true;
    }
    return false;
}
} // namespace

TEST_CASE("render_extract: a camera on a source render target computes with its size and draws on the render World's target for it") {
    releaseNow = false;
    O::WorldDescription game = Game();
    game.build = [](O::WorldSetup& s) { s.Registrar().AddSystem<&ReleaseLate>(In("TestReleaseLate", "Main", O::DefaultSchedules::last)); };
    auto p = Make(Render(), nullptr, std::move(game));
    K::World& source = GameOf(*p);
    source.ResourceMut<C::CameraTargetSizes>()->Set(C::kMainTarget, C::TargetSize{{2560, 1600}, 2.0F});
    R::RenderTargets& targets = *source.ResourceMut<R::RenderTargets>();
    // A target made and let go first: the source's ids (2, 3, 4) are not the render World's (1, 2, 3).
    targets.Release(targets.Create(R::SourceTargetDesc{}));
    const C::CameraTarget minimap = targets.Create(R::SourceTargetDesc{R::TargetSize{64, 32}});
    const C::CameraTarget hdr = targets.Create(R::SourceTargetDesc{});
    releaseLate = targets.Create(R::SourceTargetDesc{minimap});
    Camera2dAt(source, 1);
    source.Insert(1, minimap);
    source.Remove<C::Letterbox>(1);
    Camera2dAt(source, 2, 1);
    source.Insert(2, hdr);
    Camera2dAt(source, 3, 2);
    source.Insert(3, releaseLate);
    source.Remove<C::Letterbox>(3);
    Frames(*p, 2);
    // world_render copies the sizes (CM-5) before CameraUpdate: the cameras on targets compute in the same frame.
    const C::TargetSize* fixed = source.Resource<C::CameraTargetSizes>()->Find(minimap);
    REQUIRE_MESSAGE(fixed != nullptr, "[wr-target-size] the fixed target's size is in CameraTargetSizes");
    CHECK_MESSAGE((fixed->physicalSize == GYO::Engine::Math::Vec2i{64, 32} && fixed->scaleFactor == 1.0F), "[wr-target-size] 64 x 32, scale 1");
    const C::TargetSize* follows = source.Resource<C::CameraTargetSizes>()->Find(hdr);
    REQUIRE_MESSAGE(follows != nullptr, "[wr-target-size-follows] the following target's size is in CameraTargetSizes");
    CHECK_MESSAGE((follows->physicalSize == GYO::Engine::Math::Vec2i{2560, 1600} && follows->scaleFactor == 2.0F), "[wr-target-size-follows] the window's");
    const C::TargetSize* followsMinimap = source.Resource<C::CameraTargetSizes>()->Find(releaseLate);
    REQUIRE_MESSAGE(followsMinimap != nullptr, "[wr-target-size-follows-other] a target following another has its size");
    CHECK_MESSAGE(followsMinimap->physicalSize == (GYO::Engine::Math::Vec2i{64, 32}), "[wr-target-size-follows-other] the minimap's");
    CHECK_MESSAGE(source.Get<C::ComputedCamera>(1)->viewport.size == (GYO::Engine::Math::Vec2i{64, 32}), "[wr-target-camera] the camera on it computes with its size");
    // The render World made a render target for each and the cameras' passes go to them.
    REQUIRE(seen.size() == 2);
    const std::vector<R::TextureId> made = TargetsMade(seen[0].asks);
    REQUIRE_MESSAGE(made.size() == 3, "[re-target-made] one render target for each source target, in the first frame");
    CHECK_MESSAGE(PassesOn(seen[0].all, made[0]) > 0, "[wr-target-same-frame] the sizes are copied before CameraUpdate: the camera draws in the first frame");
    CHECK_MESSAGE(TargetsMade(seen[1].asks).empty(), "[re-target-kept] not made again the next frame");
    CHECK_MESSAGE((PassesOn(seen[1].all, made[0]) > 0 && PassesOn(seen[1].all, made[1]) > 0 && PassesOn(seen[1].all, made[2]) > 0),
                  "[re-target-pass] each camera draws on the render target made for its source target");
    CHECK_MESSAGE(PassesOn(seen[1].all, std::nullopt) == 0, "[re-target-not-main] no camera draws on the main target");

    // The source lets the third target go in its Last, after its camera was computed: the render World lets its target
    // go and does not extract the camera (nothing to draw on), the main target included.
    releaseNow = true;
    Frames(*p);
    REQUIRE(seen.size() == 3);
    CHECK_MESSAGE(Released(seen[2].asks, made[2]), "[re-target-released] the render World lets its target for it go");
    CHECK_MESSAGE((PassesOn(seen[2].all, made[2]) == 0 && PassesOn(seen[2].all, std::nullopt) == 0),
                  "[re-target-none] a camera whose target the source let go is not drawn");

    // The window grows: the following target's copy follows. The source lets the minimap go: its size and target go.
    source.ResourceMut<C::CameraTargetSizes>()->Set(C::kMainTarget, C::TargetSize{{1280, 800}, 1.0F});
    source.ResourceMut<R::RenderTargets>()->Release(minimap);
    Frames(*p);
    CHECK_MESSAGE(source.Resource<C::CameraTargetSizes>()->Find(hdr)->physicalSize == (GYO::Engine::Math::Vec2i{1280, 800}),
                  "[wr-target-size-follows] a new window size reaches the following target's copy");
    CHECK_MESSAGE((source.Resource<C::CameraTargetSizes>()->Find(minimap) == nullptr &&
                   source.Resource<C::CameraTargetSizes>()->Find(releaseLate) == nullptr),
                  "[wr-target-size-removed] a released target's size goes");
    REQUIRE(seen.size() == 4);
    CHECK_MESSAGE(Released(seen[3].asks, made[0]), "[re-target-released] the render World lets its target for it go");
}

TEST_CASE("render_extract: a HUD camera on another layer draws after, without clearing; a gone entity is not drawn") {
    auto p = Make();
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    Camera2dAt(game, 1);
    Camera2dAt(game, 4, 1);
    game.Insert(4, R::ClearColorConfig{R::ClearColorConfig::Mode::None, {}});
    game.Insert(4, R::RenderLayers{2});
    R::Sprite world;
    world.image = Picture();
    game.Insert(2, world);
    Place(game, 2, 0.0F, 0.0F, 0.0F);
    R::Sprite hud;
    hud.customSize = GYO::Engine::Math::Vec2{10.0F, 10.0F};
    game.Insert(3, hud);
    game.Insert(3, R::RenderLayers{2});
    Place(game, 3, 0.0F, 0.0F, 0.0F);
    Frames(*p, 2);
    // The world camera's clear, its pass, then the HUD camera's pass (no clear of its own).
    const std::vector<R::RenderPass>& all = seen.back().all;
    const auto pass2d = [&](std::size_t i) { return i < all.size() ? std::get_if<R::Pass2D>(&all[i]) : nullptr; };
    CHECK_MESSAGE((all.size() == 3 && pass2d(0) != nullptr && pass2d(0)->clear.has_value() && pass2d(1) != nullptr &&
                   pass2d(1)->draws.size() == 1 && pass2d(1)->draws[0].texture.value != 0),
                  "[re-camera-order] the world camera first, with its sprite");
    CHECK_MESSAGE((all.size() == 3 && pass2d(2) != nullptr && !pass2d(2)->clear.has_value() && pass2d(2)->draws.size() == 1 &&
                   pass2d(2)->draws[0].texture.value == 0),
                  "[re-hud] the HUD camera second, no clear, only its layer");
    REQUIRE(seen.back().passes.size() == 2);
    game.Remove<R::Sprite>(2);
    Frames(*p);
    CHECK_MESSAGE(seen.back().passes[0].draws.empty(), "[re-gone] a sprite taken away is not drawn");
    // The frame no drawable showed it lets it go at its end: the next frame's backend sees the release.
    Frames(*p);
    CHECK_MESSAGE(Count(seen.back().asks, 2) == 1, "[re-held-released] its held picture's texture is let go");
}

TEST_CASE("render_extract: no target size, an inactive camera or no GlobalTransform: no pass") {
    auto p = Make();
    K::World& game = GameOf(*p);
    Camera2dAt(game, 1);
    Frames(*p, 2);
    CHECK_MESSAGE(seen.back().passes.empty(), "[re-no-size] a camera without a target size draws nothing");
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    C::Camera off;
    off.active = false;
    game.Insert(1, off);
    Frames(*p, 2);
    CHECK_MESSAGE(seen.back().passes.empty(), "[re-inactive] an inactive camera draws nothing");
    game.Insert(1, C::Camera{});
    Frames(*p);
    CHECK(seen.back().passes.size() == 1);
}

TEST_CASE("render_extract: a shape given new vertices each frame draws only this frame's; a resize moves the letterbox") {
    auto p = Make();
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{2560, 1600}, 1.0F});
    Camera2dAt(game, 1);
    Place(game, 2, 0.0F, 0.0F, 0.0F);
    for (std::size_t n = 3; n <= 6; ++n) {
        R::Shape2d ribbon;
        ribbon.topology = R::Topology::TriangleStrip;
        for (std::size_t i = 0; i < n; ++i) ribbon.vertices.push_back({{static_cast<float>(i), static_cast<float>(i % 2)}, {}, {}});
        game.Insert(2, ribbon);
        Frames(*p);
        REQUIRE(seen.back().passes.size() == 1);
        CHECK_MESSAGE((seen.back().passes[0].vertices.size() == n && seen.back().passes[0].indices.size() == (n - 2) * 3),
                      "[re-dynamic] frame with " << n << " vertices drew " << seen.back().passes[0].vertices.size());
    }
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1000, 720}, 1.0F});
    Frames(*p);
    // 1000 x 720 is narrower than 16:9: full width, height round(1000 * 9 / 16) = 563 (562.5 rounded), bars of 78 and 79.
    CHECK_MESSAGE((seen.back().passes[0].viewport == R::PixelRect{0, 78, 1000, 563}), "[re-resize] the letterbox follows the window");
}

TEST_CASE("render_extract: a split screen's second camera clears its own half, not the first camera's") {
    auto p = Make();
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{2560, 1600}, 1.0F});
    Camera2dAt(game, 1);
    Camera2dAt(game, 2, 1);
    C::Camera left;
    left.viewport = C::Viewport{{0, 0}, {1280, 1600}};
    game.Insert(1, left);
    C::Camera right;
    right.order = 1;
    right.viewport = C::Viewport{{1280, 0}, {1280, 1600}};
    game.Insert(2, right);
    Frames(*p);
    // The first camera's clear, its pass, the second camera's viewport clear, its pass.
    REQUIRE(seen.back().all.size() == 4);
    const auto* a = std::get_if<R::Pass2D>(&seen.back().all[0]);
    const auto* b = std::get_if<R::Pass2D>(&seen.back().all[2]);
    REQUIRE((a != nullptr && b != nullptr));
    CHECK_MESSAGE(a->clear.has_value(), "[re-split-first] the first camera clears the window");
    CHECK_MESSAGE((!b->clear.has_value() && b->draws.size() == 1 && b->draws[0].blend == R::BlendMode::Opaque &&
                   b->viewport.has_value() && b->viewport->x >= 1280),
                  "[re-split-second] the second camera clears only its viewport");
}

TEST_CASE("render_extract: a sprite of an image asset is skipped until loaded, then drawn in the frame the source sees it") {
    WorldAssetTest::Content content("render-sprite-asset");
    std::ifstream file(GYO_RENDER_TEST_PNG, std::ios::binary);
    REQUIRE(file.good());
    const std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    content.Add("pic.gradient", "image", "pic/gradient.png", bytes).Write();
    AssetTest::ControlledExecutor executor;
    GYO::Engine::WorldAsset::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    auto p = Make(Render(), &options);
    const auto* registry = std::as_const(p->Buffer()).Find<GYO::Engine::Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    REQUIRE(static_cast<bool>(registry->Register<I::PngLoader>()));
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    Camera2dAt(game, 1);
    R::Sprite sprite;
    sprite.image = game.ResourceMut<GYO::Engine::WorldAsset::AssetRequests>()->Load<I::ImageData>(GYO::Engine::Asset::AssetId("pic.gradient"));
    game.Insert(2, sprite);
    Place(game, 2, 0.0F, 0.0F, 0.0F);
    Frames(*p);
    REQUIRE(seen.back().passes.size() == 1);
    CHECK_MESSAGE(seen.back().passes[0].draws.empty(), "[re-asset-wait] not loaded yet: skipped");
    executor.RunAll();
    Frames(*p);
    REQUIRE(seen.back().passes.size() == 1);
    CHECK_MESSAGE((seen.back().passes[0].draws.size() == 1 && seen.back().passes[0].draws[0].texture.value != 0),
                  "[re-asset-drawn] drawn in the frame the source's view has it");
    CHECK_MESSAGE(Count(seen.back().asks, 0) == 1, "[re-asset-texture] its texture made that frame");
    const auto* made = seen.back().asks.empty() ? nullptr : std::get_if<R::TextureCreate>(&seen.back().asks.front());
    CHECK_MESSAGE((made != nullptr && made->desc.width == 16 && made->desc.height == 8), "[re-asset-size] the picture's pixels");
}

TEST_CASE("render_extract: a 3D camera's mesh, then a HUD camera's sprite: composed in camera order whatever ran first") {
    WorldAssetTest::Content content("render-mesh-asset");
    std::ifstream file(GYO_RENDER_TEST_FBX, std::ios::binary);
    REQUIRE(file.good());
    const std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    content.Add("model.bar", "model", "model/skinned_bar.fbx", bytes).Write();
    AssetTest::ControlledExecutor executor;
    GYO::Engine::WorldAsset::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    auto p = Make(Render({"entity_ids", "render_world", "camera_render", "sprite_render", "mesh_render"}), &options);
    const auto* registry = std::as_const(p->Buffer()).Find<GYO::Engine::Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    REQUIRE(static_cast<bool>(registry->Register<GYO::Engine::Model::FbxLoader>()));
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    // The HUD camera has the smaller entity id: only the order puts it after the 3D camera.
    Camera2dAt(game, 1, 1);
    game.Insert(1, R::ClearColorConfig{R::ClearColorConfig::Mode::None, {}});
    game.Insert(1, R::RenderLayers{2});
    game.Insert(2, C::Camera{});
    game.Insert(2, C::Camera3d{});
    game.Insert(2, C::CameraTarget{});
    game.Insert(2, C::Projection{C::PerspectiveProjection{}});
    game.Insert(2, C::ComputedCamera{});
    game.Insert(2, C::CameraView{});
    Place(game, 2, 0.0F, 0.0F, -5.0F);
    R::Mesh3d mesh;
    mesh.model = game.ResourceMut<GYO::Engine::WorldAsset::AssetRequests>()->Load<GYO::Engine::Model::ModelData>(GYO::Engine::Asset::AssetId("model.bar"));
    mesh.blend = R::BlendMode::Alpha;
    game.Insert(3, mesh);
    Place(game, 3, 0.0F, 0.0F, 0.0F);
    R::Sprite hud;
    hud.customSize = GYO::Engine::Math::Vec2{10.0F, 10.0F};
    game.Insert(4, hud);
    game.Insert(4, R::RenderLayers{2});
    Place(game, 4, 0.0F, 0.0F, 0.0F);
    Frames(*p);
    executor.RunAll();
    Frames(*p);
    const std::vector<R::RenderPass>& all = seen.back().all;
    REQUIRE(all.size() == 3);
    CHECK_MESSAGE((std::holds_alternative<R::Pass2D>(all[0]) && std::get<R::Pass2D>(all[0]).clear.has_value()), "[re-3d-clear] the 3D camera's clear first");
    const auto* scene = std::get_if<R::Pass3D>(&all[1]);
    REQUIRE_MESSAGE(scene != nullptr, "[re-3d-order] then the 3D camera's pass");
    CHECK_MESSAGE(scene->draws.size() == 1, "[re-3d-mesh] the mesh is drawn");
    CHECK_MESSAGE((scene->draws.size() == 1 && scene->draws[0].blend == R::BlendMode::Alpha && !scene->draws[0].depthWrite),
                  "[re-3d-alpha-no-depth-write] a blended mesh writes no depth unless asked");
    const auto* overlay = std::get_if<R::Pass2D>(&all[2]);
    CHECK_MESSAGE((overlay != nullptr && overlay->draws.size() == 1 && !overlay->clear.has_value()), "[re-3d-hud] the HUD camera's 2D pass last");
}

TEST_CASE("render_extract: with the visibility Module a mesh whose box is out of the camera's frustum is not drawn") {
    WorldAssetTest::Content content("render-mesh-visibility");
    std::ifstream file(GYO_RENDER_TEST_FBX, std::ios::binary);
    REQUIRE(file.good());
    const std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    content.Add("model.bar", "model", "model/skinned_bar.fbx", bytes).Write();
    AssetTest::ControlledExecutor executor;
    GYO::Engine::WorldAsset::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    auto p = Make(Render({"entity_ids", "render_world", "camera_render", "mesh_render"}), &options, Game({"visibility"}));
    const auto* registry = std::as_const(p->Buffer()).Find<GYO::Engine::Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    REQUIRE(static_cast<bool>(registry->Register<GYO::Engine::Model::FbxLoader>()));
    K::World& game = GameOf(*p);
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    game.Insert(1, C::Camera{});
    game.Insert(1, C::Camera3d{});
    game.Insert(1, C::CameraTarget{});
    game.Insert(1, C::Projection{C::PerspectiveProjection{}});
    game.Insert(1, C::ComputedCamera{});
    game.Insert(1, C::CameraView{});
    Place(game, 1, 0.0F, 0.0F, -5.0F);
    const auto model = game.ResourceMut<GYO::Engine::WorldAsset::AssetRequests>()->Load<GYO::Engine::Model::ModelData>(
        GYO::Engine::Asset::AssetId("model.bar"));
    const R::Aabb box{{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}}};
    const auto mesh = [&](const K::EntityId id, const float z) {
        R::Mesh3d m;
        m.model = model;
        game.Insert(id, m);
        Place(game, id, 0.0F, 0.0F, z);
    };
    mesh(2, 0.0F);    // in front, with a box: drawn
    game.Insert(2, box);
    mesh(3, -20.0F);  // behind, with a box: left out
    game.Insert(3, box);
    mesh(4, -20.0F);  // behind, with a box and NoFrustumCulling: drawn
    game.Insert(4, box);
    game.Insert(4, R::NoFrustumCulling{});
    mesh(5, -20.0F);  // behind, without a box: drawn (nothing to test)
    Frames(*p);
    executor.RunAll();
    Frames(*p);
    const R::Pass3D* scene = nullptr;
    for (const R::RenderPass& pass : seen.back().all) {
        if (const auto* p3 = std::get_if<R::Pass3D>(&pass)) scene = p3;
    }
    REQUIRE(scene != nullptr);
    CHECK_MESSAGE(scene->instances.size() == 3, "[vs-extract] the mesh behind the camera with a box is left out, the other three drawn");
}

namespace {

// A model the game makes at run time: one node at the origin, one mesh of one triangle (no skin).
std::shared_ptr<const GYO::Engine::Model::ModelData> HeldTriangle(const float size) {
    GYO::Engine::Model::ModelData model;
    model.nodes.push_back(GYO::Engine::Model::Node{});
    GYO::Engine::Model::Mesh mesh;
    mesh.positions = {{0.0F, 0.0F, 0.0F}, {size, 0.0F, 0.0F}, {0.0F, size, 0.0F}};
    mesh.indices = {0, 1, 2};
    model.meshes.push_back(std::move(mesh));
    return std::make_shared<const GYO::Engine::Model::ModelData>(std::move(model));
}

// How many of this frame's mesh asks (what the backend is told at Submit) are of each kind: 0 MeshCreate, 1 MeshRelease.
std::size_t MeshAsks(const std::size_t kind) {
    std::size_t n = 0;
    for (const R::MeshOp& op : seen.back().meshAsks) n += op.index() == kind ? 1 : 0;
    return n;
}

std::size_t Draws3d() {
    std::size_t draws = 0;
    for (const R::RenderPass& pass : seen.back().all) {
        if (const auto* p3 = std::get_if<R::Pass3D>(&pass)) draws += p3->draws.size();
    }
    return draws;
}

} // namespace

TEST_CASE("render_extract: a Mesh3d of a model the author holds is drawn without an asset; its GPU mesh is kept while drawn, let go after") {
    auto p = Make(Render({"entity_ids", "render_world", "camera_render", "mesh_render"}));
    K::World& game = GameOf(*p);
    K::World& render = p->World("Render").GetWorld();
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    game.Insert(1, C::Camera{});
    game.Insert(1, C::Camera3d{});
    game.Insert(1, C::CameraTarget{});
    game.Insert(1, C::Projection{C::PerspectiveProjection{}});
    game.Insert(1, C::ComputedCamera{});
    game.Insert(1, C::CameraView{});
    Place(game, 1, 0.0F, 0.0F, -5.0F);
    R::Mesh3d mesh;
    mesh.model = HeldTriangle(1.0F);
    game.Insert(2, mesh);
    Place(game, 2, 0.0F, 0.0F, 0.0F);
    const auto kept = [&render] { return render.Resource<R::RenderModelMeshes>()->Count(); };
    const auto live = [&render] { return render.Resource<R::RenderMeshes>()->LiveCount(); };

    Frames(*p, 2);
    CHECK_MESSAGE(Draws3d() == 1, "[mh-held-drawn] a held model is drawn with no asset store");
    CHECK_MESSAGE((kept() == 1 && live() == 1), "[mh-held-kept] its GPU mesh is made once and kept while it is drawn");

    // A new ModelData is a new model: its mesh is made, the old one's let go after the frame.
    mesh.model = HeldTriangle(2.0F);
    game.Insert(2, mesh);
    Frames(*p);
    CHECK_MESSAGE(Draws3d() == 1, "[mh-held-replaced] the new model is drawn");
    CHECK_MESSAGE((kept() == 1 && live() == 1), "[mh-held-let-go] the old model's GPU mesh is let go after the frame");
    CHECK_MESSAGE((MeshAsks(0) == 1 && MeshAsks(1) == 1), "[mh-held-released] the backend is told: the new mesh made, the old one released");

    // No Mesh3d draws it any more: its mesh goes too.
    game.Remove<R::Mesh3d>(2);
    Frames(*p);
    CHECK_MESSAGE((kept() == 0 && live() == 0), "[mh-held-let-go] a held model no Mesh3d draws is let go");
    CHECK_MESSAGE(MeshAsks(1) == 1, "[mh-held-released] the backend is told its mesh is released");
}

TEST_CASE("render_extract: a handed model's vertices are taken once its GPU meshes are made, and it is still drawn") {
    auto p = Make(Render({"entity_ids", "render_world", "camera_render", "mesh_render"}));
    K::World& game = GameOf(*p);
    K::World& render = p->World("Render").GetWorld();
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    game.Insert(1, C::Camera{});
    game.Insert(1, C::Camera3d{});
    game.Insert(1, C::CameraTarget{});
    game.Insert(1, C::Projection{C::PerspectiveProjection{}});
    game.Insert(1, C::ComputedCamera{});
    game.Insert(1, C::CameraView{});
    Place(game, 1, 0.0F, 0.0F, -5.0F);
    // Two meshes; the Mesh3d draws the first only.
    GYO::Engine::Model::ModelData model = *HeldTriangle(1.0F);
    model.meshes.push_back(model.meshes.front());
    auto whole = std::make_shared<const GYO::Engine::Model::ModelData>(std::move(model));
    const std::weak_ptr<const GYO::Engine::Model::ModelData> original = whole;
    auto handed = std::make_shared<R::HandedModel>(std::move(whole));
    R::Mesh3d mesh;
    mesh.model = handed;
    game.Insert(2, mesh);
    Place(game, 2, 0.0F, 0.0F, 0.0F);

    Frames(*p);
    CHECK_MESSAGE(handed->Taken(), "[mh-handed-taken] its vertices are taken in the frame its GPU meshes are made");
    CHECK_MESSAGE(render.Resource<R::RenderModelMeshes>()->Count() == 2, "[mh-handed-all-meshes] every mesh without a skin is made, drawn or not");
    CHECK_MESSAGE(MeshAsks(0) == 2, "[mh-handed-all-meshes] the backend is told to make both");
    const auto left = handed->Model();
    CHECK_MESSAGE((left->meshes.size() == 2 && left->meshes[0].positions.empty() && left->meshes[0].indices.empty() &&
                   left->meshes[1].positions.empty() && left->nodes.size() == 1),
                  "[mh-handed-left] what stays: the nodes, the meshes without their vertices and indices");
    CHECK_MESSAGE(original.expired(), "[mh-handed-freed] the whole model is gone once nothing else holds it");
    Frames(*p, 2);
    CHECK_MESSAGE(Draws3d() == 1, "[mh-handed-drawn] it is still drawn, from its GPU mesh");
    CHECK_MESSAGE(MeshAsks(0) == 0, "[mh-handed-once] no mesh is made again");
    CHECK_MESSAGE(render.Resource<R::RenderModelMeshes>()->Count() == 2, "[mh-handed-kept] the mesh no Mesh3d draws stays too");
    // Taken off for a while (the author still holds the model): its GPU meshes stay, and it is drawn again.
    game.Remove<R::Mesh3d>(2);
    Frames(*p, 2);
    CHECK_MESSAGE(render.Resource<R::RenderModelMeshes>()->Count() == 2, "[mh-handed-kept] held by the author, its meshes stay undrawn");
    game.Insert(2, mesh);
    Frames(*p);
    CHECK_MESSAGE((Draws3d() == 1 && MeshAsks(0) == 0), "[mh-handed-back] put back, it is drawn again from the same GPU mesh");
    // Nothing holds it any more: its meshes go (Bevy's last Handle).
    game.Remove<R::Mesh3d>(2);
    mesh.model = HeldTriangle(1.0F);
    handed.reset();
    Frames(*p);
    CHECK_MESSAGE((render.Resource<R::RenderModelMeshes>()->Count() == 0 && MeshAsks(1) == 2), "[mh-handed-released] its GPU meshes go");

    // With room for one mesh a frame (MeshRenderOptions::meshBytesPerFrame): the second mesh waits a frame, and the
    // vertices are taken only once it is made.
    O::WorldDescription limited = Render({"entity_ids", "render_world", "camera_render", "mesh_render"});
    GYO::Engine::MeshRender::MeshRenderOptions options;
    options.meshBytesPerFrame = 1;
    limited.options.Set(options);
    auto q = Make(std::move(limited));
    K::World& game2 = GameOf(*q);
    game2.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    game2.Insert(1, C::Camera{});
    game2.Insert(1, C::Camera3d{});
    game2.Insert(1, C::CameraTarget{});
    game2.Insert(1, C::Projection{C::PerspectiveProjection{}});
    game2.Insert(1, C::ComputedCamera{});
    game2.Insert(1, C::CameraView{});
    Place(game2, 1, 0.0F, 0.0F, -5.0F);
    GYO::Engine::Model::ModelData two = *HeldTriangle(1.0F);
    two.meshes.push_back(two.meshes.front());
    auto slow = std::make_shared<R::HandedModel>(std::make_shared<const GYO::Engine::Model::ModelData>(std::move(two)));
    R::Mesh3d drawn;
    drawn.model = slow;
    game2.Insert(2, drawn);
    Place(game2, 2, 0.0F, 0.0F, 0.0F);
    Frames(*q);
    CHECK_MESSAGE((!slow->Taken() && q->World("Render").GetWorld().Resource<R::RenderModelMeshes>()->Count() == 1),
                  "[mh-handed-all-meshes] one mesh made: the vertices stay until the other is");
    const auto* status = std::as_const(q->Buffer()).Find<GYO::Engine::MeshRender::MeshRenderStatus>();
    REQUIRE(status != nullptr);
    CHECK_MESSAGE(status->MeshesWaiting() == 1, "[mh-status-waiting] the program is told one mesh waits");
    Frames(*q);
    CHECK_MESSAGE((slow->Taken() && q->World("Render").GetWorld().Resource<R::RenderModelMeshes>()->Count() == 2),
                  "[mh-handed-all-meshes] both made: taken");
    CHECK_MESSAGE(status->MeshesWaiting() == 0, "[mh-status-waiting] none waits any more");
}

TEST_CASE("render_extract: a scene of one model repeated is one instanced draw per model and image; culled copies are not instances") {
    // big-map declaration section 16, I1 to I3: crates (one model the game holds, one image), a few of them with
    // another image, one lamp of another model; crates behind the camera carry a box and the visibility Module culls
    // them.
    auto p = Make(Render({"entity_ids", "render_world", "camera_render", "mesh_render"}), nullptr, Game({"visibility"}));
    K::World& game = GameOf(*p);
    K::World& render = p->World("Render").GetWorld();
    game.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{1280, 720}, 1.0F});
    game.Insert(1, C::Camera{});
    game.Insert(1, C::Camera3d{});
    game.Insert(1, C::CameraTarget{});
    game.Insert(1, C::Projection{C::PerspectiveProjection{}});
    game.Insert(1, C::ComputedCamera{});
    game.Insert(1, C::CameraView{});
    Place(game, 1, 0.0F, 0.0F, -5.0F);
    const std::shared_ptr<const GYO::Engine::Model::ModelData> crate = HeldTriangle(0.5F);
    const std::shared_ptr<const I::ImageData> wood = Picture();
    const std::shared_ptr<const I::ImageData> paint = Picture();
    const R::Aabb box{{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}}};
    const auto put = [&](const K::EntityId id, const std::shared_ptr<const GYO::Engine::Model::ModelData>& model,
                         const std::shared_ptr<const I::ImageData>& image, const float x, const float z) {
        R::Mesh3d m;
        m.model = model;
        m.image = image;
        m.color = R::Color8{static_cast<std::uint8_t>(id), 255, 255, 255};
        game.Insert(id, m);
        game.Insert(id, box);
        Place(game, id, x, 0.0F, z);
    };
    for (K::EntityId id = 10; id < 30; ++id) put(id, crate, wood, static_cast<float>(id % 5) - 2.0F, 5.0F + static_cast<float>(id - 10));
    for (K::EntityId id = 30; id < 35; ++id) put(id, crate, wood, 0.0F, -20.0F);  // behind the camera
    for (K::EntityId id = 40; id < 43; ++id) put(id, crate, paint, 1.0F, 3.0F);
    put(50, HeldTriangle(0.25F), wood, 0.0F, 4.0F);
    Frames(*p, 2);

    const R::Pass3D* scene = nullptr;
    for (const R::RenderPass& pass : seen.back().all) {
        if (const auto* p3 = std::get_if<R::Pass3D>(&pass)) scene = p3;
    }
    REQUIRE(scene != nullptr);
    CHECK_MESSAGE(scene->draws.size() == 3, "[in-scene-draws] one draw for the wooden crates, one for the painted, one for the lamp");
    CHECK_MESSAGE(scene->instances.size() == 24, "[in-scene-culled] 20 + 3 + 1 instances: the crates behind the camera are none");
    const R::Draw3D* crates = nullptr;
    for (const R::Draw3D& d : scene->draws) {
        if (d.instanceCount == 20) crates = &d;
    }
    REQUIRE_MESSAGE(crates != nullptr, "[in-scene-one-draw] the twenty wooden crates in front are one draw");
    std::vector<std::uint8_t> tints;
    for (std::uint32_t i = 0; i < crates->instanceCount; ++i) {
        const R::Instance3D& instance = scene->instances[crates->instanceOffset + i];
        tints.push_back(instance.color.r);
        CHECK_MESSAGE(instance.world.values[14] >= 5.0F, "[in-scene-instance-world] each instance placed where its crate stands");
    }
    std::sort(tints.begin(), tints.end());
    std::vector<std::uint8_t> expected;
    for (std::uint8_t id = 10; id < 30; ++id) expected.push_back(id);
    CHECK_MESSAGE(tints == expected, "[in-scene-instance-color] each crate's own colour, every front crate once");
    CHECK_MESSAGE(render.Resource<R::RenderModelMeshes>()->Count() == 2, "[in-scene-mesh-once] one retained mesh per model");

    // Five more crates in front: still three draws.
    for (K::EntityId id = 60; id < 65; ++id) put(id, crate, wood, 0.0F, 30.0F);
    Frames(*p, 2);
    std::size_t draws = 0;
    std::size_t instances = 0;
    for (const R::RenderPass& pass : seen.back().all) {
        if (const auto* p3 = std::get_if<R::Pass3D>(&pass)) {
            draws += p3->draws.size();
            instances += p3->instances.size();
        }
    }
    CHECK_MESSAGE((draws == 3 && instances == 29), "[in-scene-draws-constant] more copies are more instances, not more draws");
}
