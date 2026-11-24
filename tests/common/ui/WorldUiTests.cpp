// world_ui and ui_render in a Product (docs/architecture/target/v2/ui.zh-Hant.md, sections 4.6, 4.7; UI-3 to UI-7):
// input's Messages written by a test writer `before` input's set (as a backend writes them) become UiAction; the
// text input request; failures stay in the root; the render World draws the UI over its camera. "ui_lobby:" is
// PvP's lobby (porting inventory PV12) with its real document and a real font. Tags "[ui-...]" are the mutants'
// expectations.
#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/camera_render/CameraRenderModule.hpp"
#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/text/FontLoader.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/ui/UiDocumentLoader.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/ui_render/UiRenderModule.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_render/WorldRenderModule.hpp"
#include "engine/world_ui/WorldUiModule.hpp"

#include "StoreSupport.hpp"
#include "UiTestSupport.hpp"
#include "WorldAssetSupport.hpp"

namespace C = GYO::Engine::Camera;
namespace In = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace RW = GYO::Engine::RenderWorld;
namespace T = GYO::Engine::Transform;
namespace Ui = GYO::Engine::Ui;
namespace W = GYO::Engine::Window;
namespace WU = GYO::Engine::WorldUi;
using GYO::Test::UiSupport::All;
using TimePoint = GYO::Engine::Time::TimePoint;

namespace {

// ---- The test writer (a backend's place) and the recorders.

struct Pending {
    std::vector<In::KeyboardInput> keys;
    std::vector<In::MouseButtonInput> buttons;
    std::vector<In::CursorMoved> moved;
    std::vector<In::TextComposition> composition;
    std::vector<In::TextCommitted> committed;
};
Pending pending;
std::vector<Ui::UiAction> actions;
std::vector<R::RenderCommands> drawn;
std::vector<R::TextureOp> asks;
int clock = 0;

TimePoint Next() {
    ++clock;
    return TimePoint{} + std::chrono::milliseconds(clock);
}

template <class T>
void SendAll(std::vector<T>& list, K::MessageWriter<T>& writer) {
    for (T& m : list) writer.Send(std::move(m));
    list.clear();
}

K::SystemResult WriteInput(K::MessageWriter<In::KeyboardInput> keys, K::MessageWriter<In::MouseButtonInput> buttons,
                           K::MessageWriter<In::CursorMoved> moved, K::MessageWriter<In::TextComposition> composition,
                           K::MessageWriter<In::TextCommitted> committed) {
    SendAll(pending.moved, moved);
    SendAll(pending.buttons, buttons);
    SendAll(pending.keys, keys);
    SendAll(pending.composition, composition);
    SendAll(pending.committed, committed);
    return {};
}

K::SystemResult RecordActions(K::MessageReader<Ui::UiAction> read) {
    for (const Ui::UiAction* a : read.Read()) actions.push_back(*a);
    return {};
}

K::SystemResult RecordDrawn(K::Res<R::RenderCommands> commands, K::Res<R::RenderTextures> textures) {
    drawn.push_back(*commands);
    asks.insert(asks.end(), textures->Pending().begin(), textures->Pending().end());
    return {};
}

void Key(const In::KeyCode code, In::KeyModifiers modifiers = {}, const char32_t character = 0) {
    In::KeyboardInput key;
    key.keyCode = code;
    key.state = In::ButtonState::Pressed;
    key.character = character;
    key.modifiers = modifiers;
    key.time = Next();
    pending.keys.push_back(key);
}

// A press of the left button at a place of the window (logical pixels).
void Click(const float x, const float y) {
    pending.moved.push_back({{x, y}, Next()});
    pending.buttons.push_back({In::MouseButton::Left, In::ButtonState::Pressed, {x, y}, Next()});
}
void Release(const float x, const float y) {
    pending.buttons.push_back({In::MouseButton::Left, In::ButtonState::Released, {x, y}, Next()});
}

// ---- The Product.

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
    units.rows.push_back(Row("ui", true));
    Add<GYO::Engine::EntityIds::EntityIdsModule>(units, "entity_ids");
    Add<GYO::Engine::WorldAsset::WorldAssetModule>(units, "world_asset");
    Add<T::TransformModule>(units, "transform");
    Add<C::CameraModule>(units, "camera", {"transform"});
    Add<In::InputModule>(units, "input");
    Add<W::WindowModule>(units, "window");
    Add<WU::WorldUiModule>(units, "world_ui", {"ui", "input", "window", "camera", "world_asset"});
    Add<GYO::Engine::WorldRender::WorldRenderModule>(units, "world_render", {"render"});
    Add<RW::RenderWorldModule>(units, "render_world", {"render"});
    Add<GYO::Engine::CameraRender::CameraRenderModule>(units, "camera_render", {"render", "render_world"});
    Add<GYO::Engine::UiRender::UiRenderModule>(units, "ui_render", {"ui", "render", "render_world", "camera_render"});
    return units;
}

K::SystemOptions In_(std::string name, std::string_view stage, std::vector<std::string> before = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = "Main";
    m.stage = std::string(stage);
    m.before = std::move(before);
    o.mounts.push_back(std::move(m));
    return o;
}

std::unique_ptr<O::Product> Make(const bool withRender = true, const GYO::Engine::WorldAsset::AssetOptions* assets = nullptr) {
    pending = {};
    actions.clear();
    drawn.clear();
    asks.clear();
    O::ProductDescription d;
    d.name = "ui";
    if (assets != nullptr) d.options.Set(*assets);
    O::WorldDescription game;
    game.name = "Game";
    // world_render: camera_render extracts the cameras' clear settings from the source (else R-E4).
    game.units = {"entity_ids", "world_asset", "transform", "camera", "input", "window", "world_ui", "world_render"};
    game.build = [](O::WorldSetup& s) {
        s.Registrar().AddSystem<&WriteInput>(In_("TestWriteInput", O::DefaultSchedules::preUpdate, {std::string(In::kInputSet)}));
        s.Registrar().AddSystem<&RecordActions>(In_("TestRecordActions", O::DefaultSchedules::update));
    };
    d.worlds.push_back(std::move(game));
    if (withRender) {
        O::WorldDescription render;
        render.name = "Render";
        render.units = {"entity_ids", "render_world", "camera_render", "ui_render"};
        render.extractFrom = "Game";
        render.build = [](O::WorldSetup& s) { s.Registrar().AddSystem<&RecordDrawn>(In_("TestRecord", RW::kSubmitStage)); };
        d.worlds.push_back(std::move(render));
    }
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

// A 2D camera on target 0 (a window of physical size x scale).
void CameraAt(K::World& world, const K::EntityId id, const std::int32_t order = 0) {
    C::Camera camera;
    camera.order = order;
    world.Insert(id, camera);
    world.Insert(id, C::Camera2d{});
    world.Insert(id, C::CameraTarget{});
    world.Insert(id, C::Projection{C::OrthographicProjection::Default2d()});
    world.Insert(id, C::ComputedCamera{});
    T::Transform t;
    world.Insert(id, t);
    world.Insert(id, T::GlobalTransform{T::ToMatrix(t)});
}

void WindowOf(K::World& world, const int width, const int height, const float scale) {
    world.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{width, height}, scale});
}

// Two buttons and a text field on a 100 x 100 canvas.
constexpr std::string_view kMenu = R"json({
  "schema": "gyo.ui", "version": 2,
  "design_canvas": {"size": [100, 100], "scale_mode": "fit"},
  "fonts": {"ui": "test.font"}, "default_font": "ui",
  "colors": {"bg": "#00000000", "white": "#FFFFFFFF", "n": "#202020FF", "f": "#404040FF", "p": "#606060FF"},
  "actions": [{"id": "one", "payload": "none"}, {"id": "two", "payload": "none"}, {"id": "name", "payload": "string"}],
  "bindings": [{"id": "name", "type": "string", "preview": ""}],
  "canvases": [{
    "id": "main", "backdrop_color": "bg", "default_focus": "one", "focus_order": ["one", "two", "field"],
    "children": [
      {"type": "button", "id": "one", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [0, 0], "size_delta": [50, 20]},
       "text": {"literal": "ab"}, "text_color": {"color": "white"}, "action": "one", "background": {"normal": "n", "focused": "f", "pressed": "p"}},
      {"type": "button", "id": "two", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [50, 0], "size_delta": [50, 20]},
       "text": {"literal": "cd"}, "text_color": {"color": "white"}, "action": "two", "background": {"normal": "n", "focused": "f", "pressed": "p"}},
      {"type": "text_field", "id": "field", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [10, 50], "size_delta": [80, 20]},
       "binding": "name", "max_length": 16, "point_size": 10, "text_color": {"color": "white"}, "action": "name",
       "background": {"normal": "n", "focused": "f", "pressed": "p"}}
    ]
  }]
})json";

K::EntityId Menu(K::World& world, const K::EntityId id, std::string name = "") {
    Ui::UiRoot root;
    root.document = GYO::Test::UiSupport::Parse(kMenu);
    root.canvas = "main";
    world.Insert(id, root);
    world.Insert(id, Ui::UiBindings{{{"name", std::move(name)}}});
    REQUIRE(world.ResourceMut<Ui::UiText>()->AddFont("test.font", GYO::Test::UiSupport::Font()));
    return id;
}

const Ui::UiRootState& StateOf(K::World& world, const K::EntityId id) {
    const Ui::UiRootState* state = world.Get<Ui::UiRootState>(id);
    REQUIRE(state != nullptr);
    return *state;
}

const Ui::UiAction* LastAction() { return actions.empty() ? nullptr : &actions.back(); }

} // namespace

TEST_CASE("ui_world: Tab moves the focus, Enter presses the focused button as a UiAction of the root") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    const K::EntityId root = Menu(game, 2);
    Frames(*p);
    CHECK(StateOf(game, root).runtime->InteractionState().focusedElement == "one");
    Key(In::KeyCode::Tab);
    Key(In::KeyCode::Enter);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK_MESSAGE(LastAction()->action == "two", "[ui-world-tab-enter]");
    CHECK(LastAction()->root == root);
    CHECK(LastAction()->element == "two");
    In::KeyModifiers shift;
    shift.Add(In::KeyModifier::Shift);
    Key(In::KeyCode::Tab, shift);
    Key(In::KeyCode::Enter);
    Frames(*p);
    CHECK_MESSAGE(LastAction()->action == "one", "[ui-world-shift-tab]");
}

TEST_CASE("ui_world: a click in logical pixels hits the button under it in the viewport's physical pixels") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    Menu(game, 2);
    Frames(*p);
    // Window 100 x 100 logical, 200 x 200 physical, the canvas fitted 2x: button "two" is at physical 100..200, 0..40,
    // logical 50..100, 0..20.
    Click(75.0F, 10.0F);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK_MESSAGE(LastAction()->action == "two", "[ui-world-pointer-scale]");
}

TEST_CASE("ui_world: editing a field asks for text input over it in logical pixels, and stops asking when it ends") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    Menu(game, 2);
    Frames(*p);
    CHECK_MESSAGE(!game.Resource<In::TextInputRequest>()->enabled, "[ui-world-request-quiet] not asked before editing");
    Click(50.0F, 60.0F);
    Frames(*p);
    const In::TextInputRequest request = *game.Resource<In::TextInputRequest>();
    CHECK_MESSAGE(request.enabled, "[ui-world-request-on]");
    CHECK_MESSAGE((request.areaPosition.x == 10.0F && request.areaPosition.y == 50.0F), "[ui-world-request-logical] "
                                                                                          << request.areaPosition.x << "," << request.areaPosition.y);
    CHECK(request.areaSize.x == 80.0F);
    CHECK(request.areaSize.y == 20.0F);
    Key(In::KeyCode::Escape);
    Frames(*p);
    CHECK_MESSAGE(!game.Resource<In::TextInputRequest>()->enabled, "[ui-world-request-off]");
    // Another writer's request (ImGui's) is not overwritten while world_ui's wish does not change (IN-4).
    game.ResourceMut<In::TextInputRequest>()->enabled = true;
    Frames(*p);
    CHECK_MESSAGE(game.Resource<In::TextInputRequest>()->enabled, "[ui-world-request-not-every-frame]");
}

TEST_CASE("ui_world: the input method's Enter commits, it does not submit; the next Enter submits the committed text") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    const K::EntityId root = Menu(game, 2);
    Frames(*p);
    Click(50.0F, 60.0F);
    Frames(*p);
    REQUIRE(StateOf(game, root).runtime->InteractionState().editingElement == "field");
    pending.composition.push_back({"がっこう", -1, -1, Next()});
    Frames(*p);
    // The Enter that ends the composition comes with its commit (the key at the same time as the commit).
    const TimePoint at = Next();
    In::KeyboardInput enter;
    enter.keyCode = In::KeyCode::Enter;
    enter.time = at;
    pending.keys.push_back(enter);
    pending.composition.push_back({"", -1, -1, at});
    pending.committed.push_back({"学校", at});
    Frames(*p);
    CHECK_MESSAGE(actions.empty(), "[ui-world-ime-enter] the input method's Enter does not submit");
    Key(In::KeyCode::Enter);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK(LastAction()->action == "name");
    CHECK_MESSAGE(std::get<std::string>(LastAction()->payload) == "学校", "[ui-world-ime-commit]");
}

TEST_CASE("ui_world: edit keys, select-all and paste reach the field being edited") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    Menu(game, 2, "abc");
    game.ResourceMut<W::Clipboard>()->text = "xyz";
    Frames(*p);
    Click(80.0F, 60.0F);
    Frames(*p);
    Key(In::KeyCode::Backspace);
    In::KeyModifiers control;
    control.Add(In::KeyModifier::Control);
    Frames(*p);
    Key(In::KeyCode::A, control, U'a');
    Key(In::KeyCode::V, control, U'v');
    Key(In::KeyCode::End);
    pending.committed.push_back({"!", Next()});
    Key(In::KeyCode::Enter);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK_MESSAGE(std::get<std::string>(LastAction()->payload) == "xyz!", "[ui-world-edit-keys] "
                                                                              << std::get<std::string>(LastAction()->payload));
}

TEST_CASE("ui_world: a failure stays in the root's state; the World runs on and the root is not drawn") {
    auto p = Make(true);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    const K::EntityId root = Menu(game, 2);
    game.Remove<Ui::UiBindings>(root);
    Frames(*p, 2);
    const Ui::UiRootState& state = StateOf(game, root);
    REQUIRE_MESSAGE(state.failure.has_value(), "[ui-world-failure]");
    CHECK(state.failure->code == Ui::UiFailureCode::MissingBinding);
    CHECK(state.drawList.commands.empty());
    REQUIRE(!drawn.empty());
    CHECK_MESSAGE(drawn.back().passes.size() == 1, "[ui-world-failure-not-drawn] only the camera's clear");
    game.Insert(root, Ui::UiBindings{{{"name", std::string("ok")}}});
    Frames(*p);
    CHECK(!StateOf(game, root).failure.has_value());
}

TEST_CASE("ui_world: without a camera given, a root is laid out in the camera on target 0 with the largest order") {
    auto p = Make(false);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 1.0F);
    CameraAt(game, 1, 0);
    CameraAt(game, 3, 5);
    CameraAt(game, 4, 2);
    const K::EntityId root = Menu(game, 2);
    Frames(*p);
    REQUIRE(StateOf(game, root).camera);
    CHECK_MESSAGE(*StateOf(game, root).camera == K::EntityId{3}, "[ui-default-camera]");
    Ui::UiRoot given = *game.Get<Ui::UiRoot>(root);
    given.camera = K::EntityId{1};
    game.Insert(root, given);
    Frames(*p);
    CHECK_MESSAGE(*StateOf(game, root).camera == K::EntityId{1}, "[ui-given-camera]");
}

TEST_CASE("ui_render: the render World draws a root's UI over its camera, glyphs from the atlas's textures") {
    auto p = Make(true);
    K::World& game = GameOf(*p);
    WindowOf(game, 200, 200, 2.0F);
    CameraAt(game, 1);
    Menu(game, 2, "abc");
    Frames(*p, 2);
    REQUIRE(!drawn.empty());
    const R::RenderCommands& last = drawn.back();
    REQUIRE(last.passes.size() == 2);
    const auto* clear = std::get_if<R::Pass2D>(&last.passes[0]);
    const auto* ui = std::get_if<R::Pass2D>(&last.passes[1]);
    REQUIRE((clear != nullptr && ui != nullptr));
    CHECK_MESSAGE(clear->clear.has_value(), "[ui-render-after-camera] the camera's clear first");
    REQUIRE(ui->viewport);
    CHECK((*ui->viewport == R::PixelRect{0, 0, 200, 200}));
    // The atlas's page was made as a texture and the glyph draws name it.
    bool atlasMade = false;
    R::TextureId page{};
    for (const R::TextureOp& op : asks) {
        if (const auto* create = std::get_if<R::TextureCreate>(&op)) {
            if (create->desc.filter == R::TextureFilter::Nearest) {
                atlasMade = true;
                page = create->id;
            }
        }
    }
    CHECK_MESSAGE(atlasMade, "[ui-render-atlas-texture]");
    bool glyphsDrawn = false;
    for (const R::Draw2D& draw : ui->draws) glyphsDrawn = glyphsDrawn || draw.texture == page;
    CHECK_MESSAGE(glyphsDrawn, "[ui-render-glyphs]");
    // The backdrop is transparent black; button one is focused (#404040FF): its sRGB bytes come back as written.
    bool focused = false;
    for (const R::Vertex2D& v : ui->vertices) focused = focused || (v.color == R::Color8{0x40, 0x40, 0x40, 0xFF});
    CHECK_MESSAGE(focused, "[ui-render-srgb-roundtrip]");
    // A later frame uploads only what changed: nothing new to draw, no new page.
    asks.clear();
    Frames(*p);
    std::size_t creates = 0;
    for (const R::TextureOp& op : asks) creates += std::holds_alternative<R::TextureCreate>(op) ? 1 : 0;
    CHECK_MESSAGE(creates == 0, "[ui-render-atlas-changes-only]");
}

// ---- PvP's lobby (porting inventory PV12).

namespace {

constexpr const char* kLobbyFont = "object_fps_pvp.font.ui";

Ui::UiBindingTable LobbyBindings(std::string address = "127.0.0.1:8080") {
    return {{"address", std::move(address)},
            {"room", std::string("No room found. Refresh or create one.")},
            {"status", std::string("Ready")},
            {"hint", std::string("Click the address to edit. Enter confirms.")}};
}

} // namespace

TEST_CASE("ui_lobby: the lobby loads through world_asset with its font, and keys, clicks and the input method drive it") {
    WorldAssetTest::Content content("ui-lobby");
    content.Add("object_fps_pvp.ui.lobby", "ui", "ui/pvp_lobby.json", GYO::Test::UiSupport::ReadFile(GYO_UI_TEST_LOBBY))
        .Add(kLobbyFont, "font", "fonts/ui.ttf", GYO::Test::UiSupport::ReadFile(GYO_UI_TEST_FONT))
        .Write();
    AssetTest::ControlledExecutor executor;
    GYO::Engine::WorldAsset::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    auto p = Make(true, &options);
    const auto* registry = std::as_const(p->Buffer()).Find<GYO::Engine::Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    REQUIRE(static_cast<bool>(registry->Register<Ui::UiDocumentLoader>()));
    REQUIRE(static_cast<bool>(registry->Register<GYO::Engine::Text::FontLoader>()));
    K::World& game = GameOf(*p);
    // A 1280 x 720 window at twice the pixels (Retina).
    WindowOf(game, 2560, 1440, 2.0F);
    CameraAt(game, 1);
    Ui::UiRoot root;
    root.document = game.ResourceMut<GYO::Engine::WorldAsset::AssetRequests>()->Load<Ui::UiAsset>(
        GYO::Engine::Asset::AssetId("object_fps_pvp.ui.lobby"));
    root.canvas = "lobby";
    const K::EntityId lobby = 2;
    game.Insert(lobby, root);
    game.Insert(lobby, Ui::UiBindings{LobbyBindings()});
    Frames(*p);
    CHECK_MESSAGE(!StateOf(game, lobby).runtime, "[ui-lobby-wait] not loaded yet");
    for (int i = 0; i < 4; ++i) {
        executor.RunAll();
        Frames(*p);
    }
    const Ui::UiRootState& state = StateOf(game, lobby);
    REQUIRE_MESSAGE(state.runtime != nullptr, "[ui-lobby-loaded]");
    CHECK(!state.failure.has_value());
    CHECK_MESSAGE(game.Resource<Ui::UiText>()->HasFont(kLobbyFont), "[ui-lobby-font]");
    CHECK_MESSAGE(!All<Ui::UiGlyphDraw>(state.drawList).empty(), "[ui-lobby-text-drawn]");
    CHECK(state.runtime->InteractionState().focusedElement == "refresh");

    // focus_order: refresh -> create; Enter presses it.
    Key(In::KeyCode::Tab);
    Key(In::KeyCode::Enter);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK_MESSAGE(LastAction()->action == "pvp.create", "[ui-lobby-focus-order]");

    // Join Room at design (800, 398) 260 x 50: logical pixels equal design pixels in a 1280 x 720 window.
    Click(930.0F, 423.0F);
    Frames(*p);
    Release(930.0F, 423.0F);
    Frames(*p);
    CHECK_MESSAGE(LastAction()->action == "pvp.join", "[ui-lobby-click]");

    // The address field: a click edits it and asks for the input method over it.
    Click(640.0F, 262.0F);
    Frames(*p);
    REQUIRE(state.runtime->InteractionState().editingElement == "address");
    const In::TextInputRequest request = *game.Resource<In::TextInputRequest>();
    CHECK(request.enabled);
    CHECK_MESSAGE((request.areaPosition.x == 220.0F && request.areaPosition.y == 238.0F && request.areaSize.x == 840.0F &&
                   request.areaSize.y == 48.0F),
                  "[ui-lobby-request] " << request.areaPosition.x << "," << request.areaPosition.y);
    In::KeyModifiers command;
    command.Add(In::KeyModifier::Super);
    Key(In::KeyCode::A, command, U'a');
    pending.composition.push_back({"ろーかる", 2, -1, Next()});
    Frames(*p);
    CHECK_MESSAGE(!All<Ui::UiQuadDraw>(state.drawList).empty(), "[ui-lobby-composition-drawn]");
    const TimePoint at = Next();
    pending.composition.push_back({"", -1, -1, at});
    pending.committed.push_back({"localhost:9000", at});
    Frames(*p);
    Key(In::KeyCode::Enter);
    Frames(*p);
    REQUIRE(LastAction() != nullptr);
    CHECK(LastAction()->action == "pvp.submit_address");
    CHECK_MESSAGE(std::get<std::string>(LastAction()->payload) == "localhost:9000", "[ui-lobby-submit]");
    CHECK_MESSAGE(!game.Resource<In::TextInputRequest>()->enabled, "[ui-lobby-request-off]");

    // The render World draws the lobby over the camera on the whole 2560 x 1440 target.
    REQUIRE(!drawn.empty());
    const R::RenderCommands& last = drawn.back();
    REQUIRE(last.passes.size() == 2);
    const auto* ui = std::get_if<R::Pass2D>(&last.passes[1]);
    REQUIRE(ui != nullptr);
    CHECK_MESSAGE((ui->viewport && *ui->viewport == R::PixelRect{0, 0, 2560, 1440}), "[ui-lobby-pass]");
    CHECK(ui->draws.size() >= 2);
}
