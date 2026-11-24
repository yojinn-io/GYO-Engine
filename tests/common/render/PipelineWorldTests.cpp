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
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/sprite_render/SpriteRenderModule.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_render/WorldRenderModule.hpp"
#include "engine/image/PngLoader.hpp"
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
struct Seen {
    std::vector<R::Pass2D> passes;
    std::vector<R::TextureOp> asks;
};
std::vector<Seen> seen;
std::vector<std::string> trail;

K::SystemResult Record(K::Res<R::RenderCommands> commands, K::Res<R::RenderTextures> textures) {
    Seen s;
    s.passes = commands->passes;
    s.asks.assign(textures->Pending().begin(), textures->Pending().end());
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
    return units;
}

O::WorldDescription Game() {
    O::WorldDescription w;
    w.name = "Game";
    w.units = {"entity_ids", "world_asset", "transform", "camera", "world_render"};
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

std::unique_ptr<O::Product> Make(O::WorldDescription render = Render(), const GYO::Engine::WorldAsset::AssetOptions* assets = nullptr) {
    seen.clear();
    trail.clear();
    O::ProductDescription d;
    d.name = "draws";
    if (assets != nullptr) d.options.Set(*assets);
    d.worlds.push_back(Game());
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
    CHECK_MESSAGE((pass.clear == R::Color8{0, 0, 0, 255}), "[re-clear-default] the source's ClearColor (black)");
    REQUIRE(pass.draws.size() == 2);
    CHECK_MESSAGE(pass.draws[0].texture.value == 0, "[re-z-order] the shape (z 0) before the sprite (z 1)");
    CHECK_MESSAGE(pass.draws[0].indexCount == 6, "[re-strip] the strip's two triangles");
    CHECK_MESSAGE(pass.draws[1].texture.value != 0, "[re-sprite-texture] the sprite's picture");
    CHECK_MESSAGE(Count(seen[0].asks, 0) + Count(seen[1].asks, 0) == 1, "[re-texture-once] the picture is made once");
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
    REQUIRE(seen.back().passes.size() == 2);
    CHECK_MESSAGE((seen.back().passes[0].clear.has_value() && seen.back().passes[0].draws.size() == 1 &&
                   seen.back().passes[0].draws[0].texture.value != 0),
                  "[re-camera-order] the world camera first, with its sprite");
    CHECK_MESSAGE((!seen.back().passes[1].clear.has_value() && seen.back().passes[1].draws.size() == 1 &&
                   seen.back().passes[1].draws[0].texture.value == 0),
                  "[re-hud] the HUD camera second, no clear, only its layer");
    game.Remove<R::Sprite>(2);
    Frames(*p);
    CHECK_MESSAGE(seen.back().passes[0].draws.empty(), "[re-gone] a sprite taken away is not drawn");
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
    REQUIRE(seen.back().passes.size() == 2);
    const R::Pass2D& a = seen.back().passes[0];
    const R::Pass2D& b = seen.back().passes[1];
    CHECK_MESSAGE(a.clear.has_value(), "[re-split-first] the first camera clears the window");
    CHECK_MESSAGE((!b.clear.has_value() && !b.draws.empty() && b.draws[0].blend == R::BlendMode::Opaque &&
                   b.viewport.has_value() && b.viewport->x >= 1280),
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
