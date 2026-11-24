// Object_Connect with its runtime (object_connect/runtime/RuntimeWorld.hpp), headless: the content is read from
// assets/object_connect through the asset Module, and the player's input goes in as the input and window Modules'
// Messages, written by a test System where sdl_window writes them (Main/PreUpdate, before the Modules' sets). What
// the game asks of the window (WarpCursor, WindowRequest) is read back.
#include <doctest/doctest.h>

#include "object_connect/runtime/GameData.hpp"
#include "object_connect/runtime/Drawing.hpp"
#include "object_connect/runtime/GameSound.hpp"
#include "object_connect/runtime/PlayfieldFit.hpp"
#include "object_connect/runtime/RuntimeWorld.hpp"
#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace AU = GYO::Engine::Audio;
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

// What the game asked of the window, kept across frames; the last frame's picture as render_world held it at its
// Submit stage (where sdl_gpu would draw it), before Cleanup empties it.
struct Asked {
    std::vector<W::WarpCursor> warps;
    GYO::Engine::Render::RenderCommands picture;
    std::size_t texturesMade = 0;
    std::size_t texturesLive = 0;
};

K::SystemResult KeepPicture(K::Res<GYO::Engine::Render::RenderCommands> commands,
                            K::Res<GYO::Engine::Render::RenderTextures> textures, K::ResMut<Asked> asked) {
    asked->picture = *commands;
    for (const auto& op : textures->Pending()) {
        if (std::holds_alternative<GYO::Engine::Render::TextureCreate>(op)) ++asked->texturesMade;
    }
    asked->texturesLive = textures->LiveCount();
    return {};
}

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
    // withMixer: a mixer in the Product's buffer where sdl_audio puts its own (no device: the test renders it).
    explicit Game(std::string contentRoot = OBJECT_CONNECT_CONTENT_ROOT, const bool withMixer = true) {
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
            K::SystemOptions keep;
            keep.name = "test.KeepPicture";
            K::Mount keepMount;
            keepMount.schedule = std::string(O::DefaultSchedules::frame);
            keepMount.stage = std::string(GYO::Engine::RenderWorld::kSubmitStage);
            keep.mounts.push_back(std::move(keepMount));
            r.AddSystem<&KeepPicture>(std::move(keep));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(Pending{});
                w.SetResource(Asked{});
                return {};
            });
        };
        O::ProductDescription description;
        description.name = "object_connect";
        description.worlds.push_back(std::move(world));
        description.configure = [withMixer](O::ProductSetup& setup) {
            if (withMixer) {
                auto mixer = AU::Mixer::Create({});
                REQUIRE(static_cast<bool>(mixer));
                setup.Buffer().Create<AU::Mixer>(std::move(mixer).value());
            }
            OR::ConfigureProduct(setup);
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

    // The mixer's counters once its audio thread has taken every queued request (one rendered block).
    [[nodiscard]] AU::MixerStats Sound() {
        AU::Mixer* mixer = product_->Buffer().Find<AU::Mixer>();
        REQUIRE(mixer != nullptr);
        std::vector<std::int16_t> block(512);
        mixer->Render(block, GYO::Engine::Time::Now());
        return mixer->Stats();
    }
    [[nodiscard]] const OR::GameSound& SoundState() { return *World().Resource<OR::GameSound>(); }

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

TEST_CASE("runtime: the music starts, ducks under a held vessel and the hold and relax sounds play") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    AU::MixerStats stats = game.Sound();
    CHECK(stats.submitted == 2); // the intro, and the loop after it
    CHECK(game.SoundState().intro.Valid());
    CHECK(game.SoundState().loop.Valid());

    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    REQUIRE(game.Session().board->IsDragging());
    stats = game.Sound();
    CHECK(stats.submitted == 3);        // the hold sound
    CHECK(stats.gainsSubmitted == 2);   // the intro and the loop duck
    CHECK(game.SoundState().holding);

    for (float x = 400.0f; x <= 920.0f; x += 40.0f) game.MoveTo(x, 360.0f);
    game.Button(920.0f, 360.0f, I::ButtonState::Released);
    stats = game.Sound();
    CHECK(stats.submitted == 4);        // the relax sound
    CHECK(stats.gainsSubmitted == 5);   // the hold fades out, the music comes back
    CHECK(stats.stopsSubmitted >= 1);   // the hold stops after its fade
    CHECK_FALSE(game.SoundState().holding);
    CHECK(game.SoundState().relax.Valid());
}

TEST_CASE("runtime: back at the main menu after a level the music starts over") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    const AU::MixerStats before = game.Sound();
    game.Press(I::KeyCode::Escape);
    for (int step = 0; step < 3; ++step) game.Press(I::KeyCode::ArrowDown);
    game.Press(I::KeyCode::Enter); // "メインメニュー"
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::MainMenu);
    const AU::MixerStats after = game.Sound();
    CHECK(after.submitted == before.submitted + 2);
    CHECK(after.stopsMatched == before.stopsMatched + 2); // the intro and the loop, sounding or scheduled
}

TEST_CASE("runtime: a cancelled hold fades its sound and plays no relax sound") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    REQUIRE(game.SoundState().holding);
    const AU::MixerStats before = game.Sound();
    game.Press(I::KeyCode::Escape);
    const AU::MixerStats after = game.Sound();
    CHECK_FALSE(game.SoundState().holding);
    CHECK(after.submitted == before.submitted); // no relax sound
    CHECK(after.gainsSubmitted == before.gainsSubmitted + 3); // the hold fades, the music comes back
    CHECK_FALSE(game.SoundState().relax.Valid());
    CHECK(game.SoundState().fadingHold.Valid());
}

TEST_CASE("runtime: the music restarted while it comes back from a duck keeps coming back") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    // The duck is planned on the Engine clock, as the mixer runs it: wait past the delay and the 0.05 s attack.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    game.Press(I::KeyCode::Escape); // cancels the hold: the release ramp (0.25 s after the delay) starts
    game.Button(360.0f, 360.0f, I::ButtonState::Released);
    const AU::MixerStats before = game.Sound();
    game.Click(400.0f, 450.0f); // the pause menu's "メインメニュー", well within the release
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::MainMenu);
    const AU::MixerStats after = game.Sound();
    CHECK(after.submitted == before.submitted + 2);
    // The new intro and loop get the rest of the release (both started below 1.0).
    CHECK(after.gainsSubmitted == before.gainsSubmitted + 2);
}

TEST_CASE("runtime: a sound file that does not load leaves that sound silent and the game playing") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "object_connect_runtime_missing_intro";
    fs::remove_all(root);
    fs::copy(OBJECT_CONNECT_CONTENT_ROOT, root, fs::copy_options::recursive);
    fs::remove(root / "audio" / "bgm_start.wav");
    {
        Game game(root.string());
        game.LoadContent();
        game.OpenWindow();
        const AU::MixerStats stats = game.Sound();
        CHECK(stats.submitted == 1); // the loop alone
        CHECK_FALSE(game.SoundState().intro.Valid());
        CHECK(game.SoundState().loop.Valid());
        game.Press(I::KeyCode::Enter);
        CHECK(game.Screen() == GameScreen::LevelSelect);
    }
    fs::remove_all(root);
}

TEST_CASE("runtime: the duck ramps from the gain the mixer has, at the original's rate") {
    using namespace std::chrono_literals;
    namespace T = GYO::Engine::Time;
    const T::Duration delay = 60ms;
    const T::TimePoint t0 = T::Now();

    const OR::GainRamp attack = OR::PlanDuck({}, OR::kDuckGain, t0, delay);
    CHECK(attack.start == t0 + delay);
    CHECK(attack.from == doctest::Approx(1.0f));
    CHECK(attack.length == std::chrono::duration_cast<T::Duration>(50ms));
    CHECK(attack.At(t0 + delay + 25ms) == doctest::Approx(0.7f));

    // Released after the attack ended: back up over the full 0.25 s.
    const OR::GainRamp release = OR::PlanDuck(attack, 1.0f, t0 + 200ms, delay);
    CHECK(release.from == doctest::Approx(OR::kDuckGain));
    CHECK(release.length == std::chrono::duration_cast<T::Duration>(250ms));

    // Released while the attack runs: the mixer freezes it where it is when it takes the change (about now), and
    // the remaining distance goes up at the release rate.
    const OR::GainRamp early = OR::PlanDuck(attack, 1.0f, t0 + delay + 25ms, delay);
    CHECK(early.from == doctest::Approx(0.7f));
    CHECK(std::chrono::duration<double>(early.length).count() == doctest::Approx(0.125).epsilon(0.001));

    // Released before the attack started (a hold shorter than the delay): the attack never runs.
    const OR::GainRamp tooShort = OR::PlanDuck(attack, 1.0f, t0 + 30ms, delay);
    CHECK(tooShort.from == doctest::Approx(1.0f));
    CHECK(tooShort.length == T::Duration::zero());
}

TEST_CASE("runtime: without a mixer the game plays silent") {
    Game game(OBJECT_CONNECT_CONTENT_ROOT, false);
    game.LoadContent();
    game.OpenWindow();
    CHECK(game.SoundState().submitter == nullptr);
    game.Press(I::KeyCode::Enter);
    CHECK(game.Screen() == GameScreen::LevelSelect);
}

// The picture as data (the 2D pass sdl_gpu would draw): one pass a frame over the 1280 x 720 playfield, the menu's
// texts as glyph quads from the atlas, the level's pictures and the game's cursor as textures.
namespace {
[[nodiscard]] std::size_t TexturedDraws(const GYO::Engine::Render::Pass2D& pass) {
    std::size_t count = 0;
    for (const auto& draw : pass.draws) count += draw.texture.value != 0 ? 1 : 0;
    return count;
}
} // namespace

TEST_CASE("runtime: each frame draws one 2D pass of the playfield with the menu's text and the game's cursor") {
    Game game;
    game.LoadContent();
    game.OpenWindow();
    game.MoveTo(400.0f, 290.0f);
    const Asked& asked = *game.World().Resource<Asked>();
    REQUIRE(asked.picture.passes.size() == 1);
    REQUIRE(std::holds_alternative<GYO::Engine::Render::Pass2D>(asked.picture.passes.front()));
    const auto& pass = std::get<GYO::Engine::Render::Pass2D>(asked.picture.passes.front());
    REQUIRE(pass.view.has_value());
    CHECK(pass.view->width == doctest::Approx(1280.0f));
    CHECK(pass.view->height == doctest::Approx(720.0f));
    REQUIRE(pass.clear.has_value());
    CHECK(pass.indices.size() % 3 == 0);
    for (const auto& draw : pass.draws) {
        CHECK(draw.indexOffset + draw.indexCount <= pass.indices.size());
    }
    for (const std::uint32_t index : pass.indices) {
        REQUIRE(index < pass.vertices.size());
    }
    // The node pictures (nodes.csv's 3 and the maps' 4), the cursors (3) and the atlas's page are textures.
    const OR::DrawResources& resources = *game.World().Resource<OR::DrawResources>();
    CHECK(resources.nodeTextures.size() == 7);
    CHECK(asked.texturesLive == 7 + 3 + resources.atlasPages.size());
    REQUIRE_FALSE(resources.atlasPages.empty());
    CHECK(TexturedDraws(pass) >= 2);
    // The order: the rectangles, then the texts (from the atlas), the cursor last (the original's Game::Impl::Draw).
    REQUIRE_FALSE(pass.draws.empty());
    CHECK(pass.draws.back().texture == resources.cursors[static_cast<std::size_t>(OC::MouseCursorIcon::Interactive)]);
    std::size_t lastWhite = 0;
    std::size_t firstAtlas = pass.draws.size();
    for (std::size_t index = 0; index < pass.draws.size(); ++index) {
        const auto texture = pass.draws[index].texture;
        if (texture.value == 0) lastWhite = index;
        if (texture == resources.atlasPages.front() && firstAtlas == pass.draws.size()) firstAtlas = index;
    }
    CHECK(lastWhite < firstAtlas);
    CHECK(firstAtlas < pass.draws.size() - 1);
    CHECK(game.World().Resource<W::WindowRequest>()->cursorVisible == false); // the game draws its own

    // In the first level the board is drawn too: more vertices than the menu.
    const std::size_t menuVertices = pass.vertices.size();
    game.Press(I::KeyCode::Enter);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    game.Button(360.0f, 360.0f, I::ButtonState::Pressed);
    for (float x = 400.0f; x <= 600.0f; x += 40.0f) game.MoveTo(x, 360.0f);
    const auto& playing = std::get<GYO::Engine::Render::Pass2D>(game.World().Resource<Asked>()->picture.passes.front());
    CHECK(playing.vertices.size() > menuVertices);
    // stage_01's heart and brain are drawn as their pictures (not as placeholder rectangles).
    std::size_t pictureDraws = 0;
    for (const auto& draw : playing.draws) {
        for (const auto& [path, id] : resources.nodeTextures) {
            if (draw.texture == id && (path.ends_with("heart.png") || path.ends_with("brain.png"))) ++pictureDraws;
        }
    }
    CHECK(pictureDraws >= 2);
}

TEST_CASE("runtime: the colours are the original's linear values encoded as its sRGB target stored them") {
    using OR::ToColor8;
    CHECK(ToColor8({0.0f, 0.0f, 0.0f, 0.0f}) == GYO::Engine::Render::Color8{0, 0, 0, 0});
    CHECK(ToColor8({1.0f, 1.0f, 1.0f, 1.0f}) == GYO::Engine::Render::Color8{255, 255, 255, 255});
    CHECK(ToColor8({0.5f, 0.5f, 0.5f, 0.5f}) == GYO::Engine::Render::Color8{188, 188, 188, 128});
    CHECK(ToColor8({0.002f, 0.2f, 2.0f, 1.0f}) == GYO::Engine::Render::Color8{7, 124, 255, 255});
}

TEST_CASE("runtime: the playfield keeps its aspect in a window of another shape, black bars around it") {
    using OR::FitPlayfield;
    const auto same = FitPlayfield({1280, 720}, 1.0F);
    REQUIRE(same.has_value());
    CHECK(same->physical == GYO::Engine::Render::PixelRect{0, 0, 1280, 720});
    const auto retina = FitPlayfield({2560, 1440}, 2.0F);
    REQUIRE(retina.has_value());
    CHECK(retina->physical == GYO::Engine::Render::PixelRect{0, 0, 2560, 1440});
    CHECK(retina->logicalSize.x == doctest::Approx(1280.0f));
    const auto wide = FitPlayfield({1600, 720}, 1.0F);
    REQUIRE(wide.has_value());
    CHECK(wide->physical == GYO::Engine::Render::PixelRect{160, 0, 1280, 720});
    const auto tall = FitPlayfield({2560, 1600}, 2.0F);
    REQUIRE(tall.has_value());
    CHECK(tall->physical == GYO::Engine::Render::PixelRect{0, 80, 2560, 1440});
    CHECK(tall->logicalOrigin.y == doctest::Approx(40.0f));
    CHECK_FALSE(FitPlayfield({0, 720}, 1.0F).has_value());

    // In a 1600 x 720 window the picture is drawn in the middle 1280 and the pointer is mapped through it.
    Game game;
    game.LoadContent();
    game.Next().sizes.push_back({{1600, 720}, 1.0F, {}});
    game.Next().focus.push_back({true, {}});
    game.Frame();
    game.Click(160.0f + 885.0f, 290.0f); // "ゲーム開始"'s right end (390 to 890), moved right by the bar
    game.Frame();
    CHECK(game.Screen() == GameScreen::LevelSelect);
    const auto& pass = std::get<GYO::Engine::Render::Pass2D>(game.World().Resource<Asked>()->picture.passes.front());
    REQUIRE(pass.viewport.has_value());
    CHECK(*pass.viewport == GYO::Engine::Render::PixelRect{160, 0, 1280, 720});
    game.Click(100.0f, 290.0f); // in the left bar: outside the playfield, nothing is hit
    CHECK(game.Screen() == GameScreen::LevelSelect);
    game.Click(160.0f + 345.0f, 215.0f); // the first tile's left edge (344); unshifted it would be the second tile
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    CHECK(game.Session().puzzleIndex == std::size_t{0});
}

TEST_CASE("runtime: in a letterboxed window the wrap crosses the playfield's edges and warps inside it") {
    Game game;
    game.LoadContent();
    game.Next().sizes.push_back({{1600, 720}, 1.0F, {}}); // a 160 px bar on each side
    game.Next().focus.push_back({true, {}});
    game.Frame();
    const OW::Catalog* catalog = game.World().Resource<OW::Catalog>();
    std::size_t wrapLevel = 0;
    while (!catalog->puzzles.GetPuzzles()[wrapLevel].wrapEdges) ++wrapLevel;
    game.Press(I::KeyCode::Enter);
    for (std::size_t step = 0; step < wrapLevel; ++step) game.Press(I::KeyCode::ArrowDown);
    game.Press(I::KeyCode::Enter);
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    std::optional<OC::Vec2> root;
    for (const auto& node : game.Session().board->GetDefinition().nodes) {
        if (node.type == OC::NodeType::Root && node.tilePosition) {
            root = OC::Vec2{(static_cast<float>(node.tilePosition->x) + node.widthTiles * 0.5f) * 16.0f,
                            (static_cast<float>(node.tilePosition->y) + node.heightTiles * 0.5f) * 16.0f};
            break;
        }
    }
    REQUIRE(root.has_value());
    constexpr float kBar = 160.0f;
    game.Button(kBar + root->x, root->y, I::ButtonState::Pressed);
    REQUIRE(game.Session().board->IsDragging());
    for (float x = root->x - 8.0f; x > 0.0f; x -= 8.0f) game.MoveTo(kBar + x, root->y);
    game.MoveTo(kBar - 2.0f, root->y); // two pixels into the left bar: past the playfield's left edge
    const Asked* asked = game.World().Resource<Asked>();
    REQUIRE(asked->warps.size() == 1);
    CHECK(asked->warps.back().position.x == doctest::Approx(kBar + 1278.0f)); // inside the playfield's right edge
}
