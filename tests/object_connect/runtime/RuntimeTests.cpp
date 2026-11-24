// Object_Connect with its runtime (object_connect/runtime/RuntimeWorld.hpp), headless: the content is read from
// assets/object_connect through the asset Module, and the player's input goes in as the input and window Modules'
// Messages, written by a test System where sdl_window writes them (Main/PreUpdate, before the Modules' sets). What
// the game asks of the window (WarpCursor, WindowRequest) is read back.
#include <doctest/doctest.h>

#include "object_connect/runtime/GameData.hpp"
#include "object_connect/runtime/RuntimeWorld.hpp"
#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;
namespace OC = object_connect;
namespace OR = object_connect::runtime;
namespace OW = object_connect::world;

using OC::GameScreen;

// What the next frame's writer sends, as sdl_window would from the OS's events.
struct Pending {
    std::vector<I::KeyboardInput> keys;
    std::vector<I::MouseButtonInput> buttons;
    std::vector<I::CursorMoved> moves;
    std::vector<I::MouseWheel> wheels;
    std::vector<I::CursorLeft> lefts;
    std::vector<W::WindowFocused> focus;
    std::vector<W::WindowResized> sizes;
};

// What the game asked of the window, kept across frames.
struct Asked {
    std::vector<W::WarpCursor> warps;
};

K::SystemResult WriteEvents(K::ResMut<Pending> pending, K::MessageWriter<I::KeyboardInput> keys,
                            K::MessageWriter<I::MouseButtonInput> buttons, K::MessageWriter<I::CursorMoved> moves,
                            K::MessageWriter<I::MouseWheel> wheels, K::MessageWriter<I::CursorLeft> lefts,
                            K::MessageWriter<W::WindowFocused> focus, K::MessageWriter<W::WindowResized> sizes) {
    Pending& p = pending.Get();
    for (auto& m : p.keys) keys.Send(m);
    for (auto& m : p.buttons) buttons.Send(m);
    for (auto& m : p.moves) moves.Send(m);
    for (auto& m : p.wheels) wheels.Send(m);
    for (auto& m : p.lefts) lefts.Send(m);
    for (auto& m : p.focus) focus.Send(m);
    for (auto& m : p.sizes) sizes.Send(m);
    p = {};
    return {};
}

K::SystemResult ReadRequests(K::MessageReader<W::WarpCursor> warps, K::ResMut<Asked> asked) {
    for (const W::WarpCursor* warp : warps.Read()) asked->warps.push_back(*warp);
    return {};
}

void HashValue(K::Hasher&, const Pending&) {}
void HashValue(K::Hasher&, const Asked&) {}

class Game {
public:
    explicit Game(std::string contentRoot = OBJECT_CONNECT_CONTENT_ROOT) {
        auto pool = GYO::Engine::Threads::WorkerPool::Start({.name = "test assets", .threadCount = 2});
        REQUIRE(static_cast<bool>(pool));
        pool_ = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(pool).value());
        executor_ = std::make_unique<WA::WorkerPoolExecutor>(*pool_);

        O::WorldDescription world;
        world.name = "Main";
        world.build = [](O::WorldSetup& setup) {
            OR::BuildRuntime(setup);
            K::Registrar& r = setup.Registrar();
            r.Resource<Pending>({.name = "test.Pending"});
            r.Resource<Asked>({.name = "test.Asked"});
            K::SystemOptions write;
            write.name = "test.WriteEvents";
            K::Mount writeMount;
            writeMount.schedule = std::string(O::DefaultSchedules::frame);
            writeMount.stage = std::string(O::DefaultSchedules::preUpdate);
            writeMount.before = {std::string(I::kInputSet), std::string(W::kWindowSet)};
            write.mounts.push_back(std::move(writeMount));
            r.AddSystem<&WriteEvents>(std::move(write));
            K::SystemOptions read;
            read.name = "test.ReadRequests";
            K::Mount readMount;
            readMount.schedule = std::string(O::DefaultSchedules::frame);
            readMount.stage = std::string(O::DefaultSchedules::last);
            read.mounts.push_back(std::move(readMount));
            r.AddSystem<&ReadRequests>(std::move(read));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(Pending{});
                w.SetResource(Asked{});
                return {};
            });
        };
        O::ProductDescription description;
        description.name = "object_connect";
        description.worlds.push_back(std::move(world));
        description.configure = [](O::ProductSetup& setup) {
            const auto* registry = setup.Buffer().Find<GYO::Engine::Asset::AssetLoaderRegistry>();
            REQUIRE(registry != nullptr);
            REQUIRE(!OR::RegisterLoaders(*registry).has_value());
        };
        WA::AssetOptions assets;
        assets.contentRoot = std::move(contentRoot);
        assets.executor = executor_.get();
        description.options.Set(assets);
        auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
        REQUIRE_MESSAGE(static_cast<bool>(built),
                        (built ? std::string() : built.error().message + "\n" + built.error().detail));
        product_ = std::move(built).value();

        WT::TimeSampling* sampling = World().ResourceMut<WT::TimeSampling>();
        REQUIRE(sampling != nullptr);
        sampling->mode = WT::SamplingMode::ManualDelta;
        sampling->value = 16'666'667;
    }
    ~Game() { product_.reset(); }
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    K::World& World() { return product_->World("Main").GetWorld(); }

    // Runs frames until generation ends (the content loaded), at most for a few seconds of wall time.
    void LoadContent() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (product_->Generating("Main")) {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            Frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    GYO::Engine::Base::Result<void, O::OrchestrationError> TryFrame() { return product_->RunFrame(); }

    void Frame() {
        auto ran = product_->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }

    Pending& Next() { return *World().ResourceMut<Pending>(); }

    // The window as sdl_window reports it on a new World: its size and the focus.
    void OpenWindow() {
        Next().sizes.push_back({{1280, 720}, 1.0F, {}});
        Next().focus.push_back({true, {}});
        Frame();
    }
    void Press(const I::KeyCode key) {
        Next().keys.push_back({key, I::ButtonState::Pressed, false, 0, {}, {}});
        Frame();
        Next().keys.push_back({key, I::ButtonState::Released, false, 0, {}, {}});
        Frame();
    }
    void MoveTo(const float x, const float y) {
        Next().moves.push_back({{x, y}, {}});
        Frame();
    }
    void Button(const float x, const float y, const I::ButtonState state) {
        Next().moves.push_back({{x, y}, {}});
        Next().buttons.push_back({I::MouseButton::Left, state, {x, y}, {}});
        Frame();
    }
    void Click(const float x, const float y) {
        Button(x, y, I::ButtonState::Pressed);
        Button(x, y, I::ButtonState::Released);
    }

    [[nodiscard]] GameScreen Screen() {
        const auto* state = World().Resource<OW::ScreenState>();
        REQUIRE(state != nullptr);
        return state->Current().value_or(GameScreen::MainMenu);
    }
    [[nodiscard]] const OW::Session& Session() { return *World().Resource<OW::Session>(); }
    [[nodiscard]] const OW::FrameInput& Input() { return *World().Resource<OW::FrameInput>(); }
    [[nodiscard]] const OR::CursorAppearance& Cursor() { return *World().Resource<OR::CursorAppearance>(); }
    [[nodiscard]] std::size_t LevelPage() { return World().Resource<OR::MenuPagesState>()->pages.LevelSelectPage(); }
    [[nodiscard]] std::size_t Selected() { return World().Resource<OW::MenuSelection>()->item; }

private:
    std::unique_ptr<GYO::Engine::Threads::WorkerPool> pool_;
    std::unique_ptr<WA::WorkerPoolExecutor> executor_;
    std::unique_ptr<O::Product> product_;
};

} // namespace

TEST_CASE("runtime: the content loads while the World generates") {
    Game game;
    game.LoadContent();
    K::World& w = game.World();
    const OW::Catalog* catalog = w.Resource<OW::Catalog>();
    REQUIRE(catalog != nullptr);
    CHECK(catalog->puzzles.GetPuzzles().size() == 20);
    CHECK(catalog->puzzles.GetPuzzles().front().id == "stage_01");

    const OR::GameContent* content = w.Resource<OR::GameContent>();
    const WA::AssetView* view = w.Resource<WA::AssetView>();
    REQUIRE(content != nullptr);
    REQUIRE(view != nullptr);
    CHECK(content->loaded);
    const OR::LevelTable* levels = view->Get(content->levels);
    REQUIRE(levels != nullptr);
    CHECK(levels->maps.size() == 20);
    const OR::NodeTable* nodes = view->Get(content->nodes);
    REQUIRE(nodes != nullptr);
    CHECK_FALSE(nodes->textures.empty());
    for (const auto& texture : nodes->textures) {
        const auto* image = view->Get(texture.asset);
        REQUIRE_MESSAGE(image != nullptr, texture.path);
        CHECK(image->Width() > 0);
    }
    for (const auto& cursor : content->cursors) {
        const auto* image = view->Get(cursor);
        REQUIRE(image != nullptr);
        CHECK(image->Width() > 0);
    }

    // The UI font is in the text context and has the menus' glyphs.
    OR::UiText* text = w.ResourceMut<OR::UiText>();
    REQUIRE(text != nullptr);
    REQUIRE(text->font.has_value());
    const std::string label = "ゲーム開始";
    const GYO::Engine::Text::FontId fonts[] = {*text->font};
    const GYO::Engine::Text::TextSpan spans[] = {
        {0, static_cast<std::uint32_t>(label.size()), {fonts, 28.0f, {}}}};
    const auto shaped = text->context.Shape(label, spans, {.language = "ja"});
    const auto layout = text->context.Layout(shaped, {});
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.lines[0].width > 28.0f * 4.0f);
}

TEST_CASE("runtime: content that does not load ends the World with what failed") {
    Game game(std::string{});
    std::optional<std::string> failure;
    for (int frame = 0; frame < 10 && !failure; ++frame) {
        auto ran = game.TryFrame();
        if (!ran) failure = ran.error().message + "\n" + ran.error().detail;
    }
    REQUIRE(failure.has_value());
    CHECK(failure->find("object_connect.levels") != std::string::npos);
}

TEST_CASE("runtime: the keys and the mouse play through the menus to a solved level") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    CHECK(game.Screen() == GameScreen::MainMenu);

    game.Press(I::KeyCode::Enter); // "ゲーム開始"
    CHECK(game.Screen() == GameScreen::LevelSelect);

    game.Click(350.0f, 215.0f); // the first tile
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    REQUIRE(game.Session().puzzleIndex == std::size_t{0});

    // stage_01: the heart (3 x 3 tiles at tile 21, 21) to the brain (tile 56, 21), 16 px tiles.
    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    CHECK(game.Session().board->IsDragging());
    for (float x = 400.0f; x <= 920.0f; x += 40.0f) game.MoveTo(x, 360.0f);
    game.Button(920.0f, 360.0f, I::ButtonState::Released);
    game.Frame();
    CHECK(game.Session().board->IsSolved());
    CHECK(game.Screen() == GameScreen::Solved);
}

TEST_CASE("runtime: Escape pauses, a lost focus cancels the drag, and quitting closes the window") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter); // the first level
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);

    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    REQUIRE(game.Session().board->IsDragging());
    game.Press(I::KeyCode::Escape);
    CHECK_FALSE(game.Session().board->IsDragging());
    CHECK(game.Screen() == GameScreen::Paused);
    game.Button(360.0f, 360.0f, I::ButtonState::Released);
    game.Press(I::KeyCode::Enter); // "再開"
    CHECK(game.Screen() == GameScreen::Playing);

    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    REQUIRE(game.Session().board->IsDragging());
    game.Next().focus.push_back({false, {}});
    game.Frame();
    game.Frame();
    CHECK_FALSE(game.Session().board->IsDragging());
    CHECK(game.Screen() == GameScreen::Paused);

    // Back with the focus: the pause menu's last entry quits.
    game.Next().focus.push_back({true, {}});
    game.Frame();
    for (int step = 0; step < 4; ++step) game.Press(I::KeyCode::ArrowDown);
    CHECK_FALSE(game.World().Resource<W::WindowRequest>()->close);
    game.Press(I::KeyCode::Enter);
    CHECK(game.World().Resource<W::WindowRequest>()->close);
}

TEST_CASE("runtime: on a wrapEdges level the cursor is warped to the opposite edge") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    const OW::Catalog* catalog = game.World().Resource<OW::Catalog>();
    std::optional<std::size_t> wrapLevel;
    for (std::size_t index = 0; index < catalog->puzzles.GetPuzzles().size(); ++index) {
        if (catalog->puzzles.GetPuzzles()[index].wrapEdges) {
            wrapLevel = index;
            break;
        }
    }
    REQUIRE(wrapLevel.has_value());
    game.Press(I::KeyCode::Enter);
    for (std::size_t step = 0; step < *wrapLevel; ++step) game.Press(I::KeyCode::ArrowDown);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    REQUIRE(game.Session().board->GetDefinition().wrapEdges);

    // Take hold of the first root and drag left past the window's edge.
    const auto& nodes = game.Session().board->GetDefinition().nodes;
    std::optional<OC::Vec2> root;
    for (const auto& node : nodes) {
        if (node.type == OC::NodeType::Root && node.tilePosition) {
            root = OC::Vec2{(static_cast<float>(node.tilePosition->x) + node.widthTiles * 0.5f) * 16.0f,
                            (static_cast<float>(node.tilePosition->y) + node.heightTiles * 0.5f) * 16.0f};
            break;
        }
    }
    REQUIRE(root.has_value());
    game.Button(root->x, root->y, I::ButtonState::Pressed);
    REQUIRE(game.Session().board->IsDragging());
    for (float x = root->x - 8.0f; x > 0.0f; x -= 8.0f) game.MoveTo(x, root->y);
    game.MoveTo(-2.0f, root->y); // the OS keeps the capture while the button is held
    const Asked* asked = game.World().Resource<Asked>();
    REQUIRE(asked->warps.size() == 1);
    // Two pixels past the left edge come back two pixels inside the right one: the overshoot is kept (the original
    // InputSystem crosses at the last pixel only while the cursor is still on the edge).
    const W::WarpCursor warp = asked->warps.back();
    CHECK(warp.position.x == doctest::Approx(1278.0f));
    CHECK(warp.position.y == doctest::Approx(std::round(root->y)));
    REQUIRE(game.Input().pointer.unwrappedPosition.has_value());
    CHECK(game.Input().pointer.unwrappedPosition->x == doctest::Approx(-2.0f));

    // The backend applies the warp before the next frame and reports the cursor there (sdl_window, Main/Last).
    game.MoveTo(warp.position.x, warp.position.y);
    game.MoveTo(warp.position.x - 20.0f, warp.position.y);
    CHECK(asked->warps.size() == 1);
    CHECK(game.Session().board->IsDragging());
    CHECK(game.Input().pointer.position.x == doctest::Approx(1258.0f));
    REQUIRE(game.Input().pointer.unwrappedPosition.has_value());
    CHECK(game.Input().pointer.unwrappedPosition->x == doctest::Approx(-22.0f));
}

TEST_CASE("runtime: a warp the window did not apply is not taken as motion") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    const OW::Catalog* catalog = game.World().Resource<OW::Catalog>();
    std::size_t wrapLevel = 0;
    while (!catalog->puzzles.GetPuzzles()[wrapLevel].wrapEdges) ++wrapLevel;
    game.Press(I::KeyCode::Enter);
    for (std::size_t step = 0; step < wrapLevel; ++step) game.Press(I::KeyCode::ArrowDown);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    std::optional<OC::Vec2> root;
    for (const auto& node : game.Session().board->GetDefinition().nodes) {
        if (node.type == OC::NodeType::Root && node.tilePosition) {
            root = OC::Vec2{(static_cast<float>(node.tilePosition->x) + node.widthTiles * 0.5f) * 16.0f,
                            (static_cast<float>(node.tilePosition->y) + node.heightTiles * 0.5f) * 16.0f};
            break;
        }
    }
    REQUIRE(root.has_value());
    game.Button(root->x, root->y, I::ButtonState::Pressed);
    for (float x = root->x - 8.0f; x > 0.0f; x -= 8.0f) game.MoveTo(x, root->y);
    game.MoveTo(-2.0f, root->y);
    const Asked* asked = game.World().Resource<Asked>();
    REQUIRE(asked->warps.size() == 1);
    // No CursorMoved to the warp's place: the cursor stays where it was.
    game.Frame();
    game.Frame();
    CHECK(asked->warps.size() == 1);
    REQUIRE(game.Input().pointer.unwrappedPosition.has_value());
    CHECK(game.Input().pointer.unwrappedPosition->x == doctest::Approx(-2.0f));
}

TEST_CASE("runtime: the level select's arrows and wheel turn pages, and the cursor follows the menus") {
    Game game;
    game.LoadContent();
    game.OpenWindow();

    game.MoveTo(400.0f, 290.0f); // over "ゲーム開始"
    CHECK(game.Cursor().icon == OC::MouseCursorIcon::Interactive);
    CHECK(game.Cursor().custom);
    game.MoveTo(400.0f, 340.0f); // the gap between the entries (334 to 350)
    CHECK(game.Cursor().icon == OC::MouseCursorIcon::Normal);

    game.Click(400.0f, 290.0f);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::LevelSelect);
    CHECK(game.LevelPage() == 0);

    game.Click(1070.0f, 290.0f); // the next page's arrow
    CHECK(game.LevelPage() == 1);
    CHECK(game.Selected() == 10);

    game.Next().wheels.push_back({{0.0f, -1.0f}, {}}); // one notch down (toward the user)
    game.Frame();
    CHECK(game.LevelPage() == 1); // page 1 is the last of 20 levels
    game.Next().wheels.push_back({{0.0f, 1.0f}, {}}); // one notch up
    game.Frame();
    CHECK(game.LevelPage() == 0);
    CHECK(game.Selected() == 0);

    game.Click(1070.0f, 290.0f);
    game.Click(350.0f, 215.0f); // the first tile of page 1
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    CHECK(game.Session().puzzleIndex == std::size_t{10});

    // The cursor leaves the window: the game's cursor gives way to the system's.
    game.Next().lefts.push_back({});
    game.Frame();
    CHECK_FALSE(game.Cursor().custom);
}

TEST_CASE("runtime: the menus take no hover once the cursor has left the window") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.MoveTo(400.0f, 360.0f); // over "終了"
    CHECK(game.Selected() == 1);
    game.Next().lefts.push_back({});
    game.Frame();
    game.Press(I::KeyCode::ArrowUp);
    game.Frame();
    CHECK(game.Selected() == 0); // the hover no longer pulls the selection back
    CHECK_FALSE(game.Input().menu.hoveredItem.has_value());
}
