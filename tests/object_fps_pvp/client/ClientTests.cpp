// The Client (client/ClientWorld.hpp) without a window or a GPU: its Sim World (the client World), its Frame World
// with the Client's Systems (both on the test's thread here: the frame runs Sim, then Frame), its Render World without sdl_gpu (what it would draw is read at the Submit stage), a Match and a second plain Client
// over memory links, the content read from assets/object_fps_pvp through the asset Module. The input and window
// Messages are written by a test System where sdl_window writes them.
#include <doctest/doctest.h>

#include "../world/WorldTestSupport.hpp"

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/data/ArenaMap.hpp"
#include "object_fps_pvp/client/Gateway.hpp"
#include "object_fps_pvp/client/LocalFiles.hpp"
#include "object_fps_pvp/client/Overhead.hpp"
#include "object_fps_pvp/client/SoloSave.hpp"

#include "gyo_test/LoopbackListener.hpp"
#include "object_fps_pvp/client/Scenes.hpp"

#include "engine/audio/Mixer.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/model/Animation.hpp"
#include "engine/net/NetService.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/window/WindowModule.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline3d.hpp"
#include "engine/render/Presentation.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"

#include <atomic>
#include <functional>
#include <cstdlib>
#include <cctype>
#include <mutex>
#include <numbers>
#include <optional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <set>
#include <thread>
#include <variant>

using namespace object_fps_pvp::test;
namespace PC = object_fps_pvp::client;
namespace I = GYO::Engine::Input;
namespace W = GYO::Engine::Window;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Transform;
namespace WA = GYO::Engine::WorldAsset;
namespace sim = object_fps_pvp::sim;

namespace {

// The shipped map an arena plays, read from its files (its catalog id names maps/<name>/), as the asset store reads it.
object_fps_pvp::data::ArenaMap ShippedMapOf(const object_fps_pvp::sim::ArenaContent& arena) {
    const std::string prefix = "object_fps_pvp.map.";
    REQUIRE(arena.map.rfind(prefix, 0) == 0);
    const std::string dir = std::string(PVP_CONTENT_ROOT) + "/maps/" + arena.map.substr(prefix.size()) + "/";
    const auto read = [](const std::string& path) -> std::optional<std::string> {
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    };
    std::string error;
    auto index = read(dir + "map.json");
    REQUIRE(index.has_value());
    auto map = object_fps_pvp::data::ReadArenaMap(
        *index, [&](const std::string& file, std::string&) { return read(dir + file); }, error);
    REQUIRE_MESSAGE(map.has_value(), error);
    return std::move(*map);
}

// Whether a Mesh3d's picture is the map's palette (the Frame World makes its own copy: compared by its pixels).
bool ShowsPalette(const R::Mesh3d& mesh, const object_fps_pvp::data::ArenaMap& map) {
    const auto* image = std::get_if<std::shared_ptr<const GYO::Engine::Image::ImageData>>(&mesh.image);
    if (image == nullptr || *image == nullptr) return false;
    const auto expected = GYO::Engine::VoxelMap::ExpandVoxelPalette(map.index);
    const auto pixels = (*image)->Pixels();
    return (*image)->Width() == expected.width && std::equal(pixels.begin(), pixels.end(), expected.pixels.begin(), expected.pixels.end());
}

// What the next frame's writer sends, as sdl_window would.
struct Pending final {
    std::vector<I::KeyboardInput> keys;
    std::vector<I::MouseButtonInput> buttons;
    std::vector<I::CursorMoved> moves;
    std::vector<I::MouseMotion> motions;
    std::vector<W::WindowFocused> focus;
    std::vector<W::WindowResized> sizes;
    std::vector<I::TextCommitted> committed;
};
void HashValue(K::Hasher&, const Pending&) {}

K::SystemResult WriteEvents(K::ResMut<Pending> pending, K::MessageWriter<I::KeyboardInput> keys,
                            K::MessageWriter<I::MouseButtonInput> buttons, K::MessageWriter<I::CursorMoved> moves,
                            K::MessageWriter<I::MouseMotion> motions, K::MessageWriter<W::WindowFocused> focus,
                            K::MessageWriter<W::WindowResized> sizes, K::MessageWriter<I::TextCommitted> committed) {
    Pending& p = pending.Get();
    for (auto& m : p.keys) keys.Send(m);
    for (auto& m : p.buttons) buttons.Send(m);
    for (auto& m : p.moves) moves.Send(m);
    for (auto& m : p.motions) motions.Send(m);
    for (auto& m : p.focus) focus.Send(m);
    for (auto& m : p.sizes) sizes.Send(m);
    for (auto& m : p.committed) committed.Send(m);
    p = {};
    return {};
}

// The last frame's picture as the Render World held it at its Submit stage.
struct Picture final {
    std::size_t passes3d = 0;
    std::size_t draws3d = 0;
    std::size_t passes2d = 0;
    std::size_t draws2d = 0;
};
Picture lastPicture;
K::SystemResult KeepPicture(K::Res<R::RenderCommands> commands) {
    Picture p;
    for (const R::RenderPass& pass : commands->passes) {
        if (const auto* p3 = std::get_if<R::Pass3D>(&pass)) {
            ++p.passes3d;
            p.draws3d += p3->draws.size();
        } else if (const auto* p2 = std::get_if<R::Pass2D>(&pass)) {
            ++p.passes2d;
            p.draws2d += p2->draws.size();
        }
    }
    lastPicture = p;
    return {};
}

std::shared_ptr<PW::Seats> SeatsOf(const std::uint32_t capacity) {
    return std::make_shared<PW::Seats>(capacity);
}

I::KeyboardInput Key(I::KeyCode code, I::ButtonState state = I::ButtonState::Pressed);

// The game connected to the Match over the memory link.
PC::SceneChange GameScene() {
    return PC::SceneChange{.scene = PC::Scene::MultiGame, .connect = N::NetConnectTarget{N::MemoryLink::EndpointA(), 1, {}}};
}

// Whether the World has a value of Resource T (an empty scene's World may not even register T).
template <class T>
bool HasResource(const K::World& world) {
    return world.Types().ResourceIdOf<T>().has_value() && world.Resource<T>() != nullptr;
}

struct Setup final {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::pair<N::MemoryLink, N::MemoryLink> first = N::MemoryLink::Pair({});
    std::pair<N::MemoryLink, N::MemoryLink> second = N::MemoryLink::Pair({});
    std::unique_ptr<GYO::Engine::Threads::WorkerPool> pool;
    std::unique_ptr<WA::WorkerPoolExecutor> executor;
    std::unique_ptr<O::Product> match, client, other;

    // framesPerStep: the Client's frames per fixed step (a real Client draws several frames a step); the Match and the
    // other Client run one frame (one step) every framesPerStep frames, and the clock moves by a Client frame.
    int framesPerStep = 1;
    int frame = 0;
    // The Match does not run (its packets wait): a slow Match.
    bool holdMatch = false;

    std::shared_ptr<const PC::PresentationContent> presentation;
    // The other Client's input every step (none: it stands).
    std::optional<PW::PlayerInput> otherInput;

    // matchArena: the arena the Match plays (the Client starts with arena 0 and follows it). first: the Client's first
    // scene (none: the game, connected to the Match over the memory link). delayAdvances: a fixed latency of this many
    // Advance calls each way between the Match and this Client (one Advance a frame).
    explicit Setup(const int perStep = 1, const std::uint32_t seats = 4, const std::size_t matchArena = 0,
                   std::optional<PC::SceneChange> firstScene = std::nullopt, std::string dataDirectory = {},
                   std::optional<GYO::Engine::OS::SocketAddress> gateway = std::nullopt, std::vector<std::uint64_t> expectUsers = {},
                   const std::uint32_t delayAdvances = 0, const std::function<void(PC::ClientStart&)>& adjust = {},
                   GYO::Engine::Asset::AssetExecutor* jobs = nullptr, std::shared_ptr<const PW::GameContent> content = ShippedContent(),
                   const std::size_t matchMode = 0, std::shared_ptr<const PW::GameContent> matchContent = nullptr)
        : framesPerStep(perStep) {
        if (delayAdvances != 0) {
            N::MemoryLinkSettings link;
            link.aToB.fixedDelaySteps = delayAdvances;
            link.bToA.fixedDelaySteps = delayAdvances;
            first = N::MemoryLink::Pair(link);
        }
        auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "test assets", .threadCount = 2});
        REQUIRE(static_cast<bool>(started));
        pool = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(started).value());
        executor = std::make_unique<WA::WorkerPoolExecutor>(*pool);
        auto reader = [c = clock] { return *c; };
        // The Match's content (none: the Client's) and the other Client's, which plays the Match's arena and mode.
        if (!matchContent) matchContent = content;
        match = BuildMatch(matchContent, std::make_shared<HubLink>(std::vector<N::MemoryLink>{first.first, second.first}), SeatsOf(seats),
                           reader, matchArena, std::move(expectUsers), 60, matchMode);
        other = BuildClient(matchContent, std::make_shared<N::MemoryLink>(second.second), 2, reader, matchArena, matchMode);

        std::string error;
        auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, error);
        REQUIRE_MESSAGE(presentation.has_value(), error);
        this->presentation = std::make_shared<const PC::PresentationContent>(std::move(*presentation));
        PC::ClientOptions options{content, this->presentation, 0, 1, jobs};
        O::WorldDescription solo = Stepped(PC::ClientSoloWorld(options), kStep / framesPerStep);
        O::WorldDescription sim = Stepped(PC::ClientSimWorld(options), kStep / framesPerStep);
        O::WorldDescription game = Stepped(PC::ClientFrameWorld(options, false), kStep / framesPerStep);
        game.build = [inner = std::move(game.build)](O::WorldSetup& setup) {
            inner(setup);
            K::Registrar& r = setup.Registrar();
            r.Resource<Pending>({.name = "test.Pending"});
            K::SystemOptions write;
            write.name = "test.WriteEvents";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(O::DefaultSchedules::preUpdate);
            mount.before = {std::string(I::kInputSet), std::string(W::kWindowSet)};
            write.mounts.push_back(std::move(mount));
            r.AddSystem<&WriteEvents>(std::move(write));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                Pending first;
                first.focus.push_back(W::WindowFocused{true, {}});
                first.sizes.push_back(W::WindowResized{{1280, 720}, 1.0F, {}});
                first.moves.push_back(I::CursorMoved{{640.0F, 360.0F}, {}});
                w.SetResource(std::move(first));
                return {};
            });
        };
        O::WorldDescription render = PC::ClientRenderWorld(false);
        render.build = [inner = std::move(render.build)](O::WorldSetup& setup) {
            inner(setup);
            K::SystemOptions keep;
            keep.name = "test.KeepPicture";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(GYO::Engine::RenderWorld::kSubmitStage);
            keep.mounts.push_back(std::move(mount));
            setup.Registrar().AddSystem<&KeepPicture>(std::move(keep));
        };
        O::ProductDescription description;
        description.name = "pvp-client";
        N::NetProductOptions net;
        net.role = N::NetRole::Client;
        net.link = std::make_shared<N::MemoryLink>(first.second);
        net.clock = reader;
        description.options.Set(net);
        // Whom it connects to (net-connect-target: the game scene's NetConnectTarget, put by the Sim World's build).
        PC::ClientStart start;
        start.first = firstScene.value_or(GameScene());
        start.dataDirectory = std::move(dataDirectory);
        // No Gateway given: one nobody answers at (port 1).
        start.gateway = gateway.value_or(GYO::Engine::OS::SocketAddress::Loopback(1));
        if (adjust) adjust(start);
        description.options.Set(std::move(start));
        description.worlds.push_back(std::move(solo));
        description.worlds.push_back(std::move(sim));
        description.worlds.push_back(std::move(game));
        description.worlds.push_back(std::move(render));
        description.configure = [](O::ProductSetup& setup) {
            auto mixer = GYO::Engine::Audio::Mixer::Create({});
            REQUIRE(static_cast<bool>(mixer));
            setup.Buffer().Create<GYO::Engine::Audio::Mixer>(std::move(mixer).value());
            PC::ConfigureClient(setup);
        };
        WA::AssetOptions assets;
        assets.contentRoot = PVP_CONTENT_ROOT;
        assets.executor = executor.get();
        description.options.Set(assets);
        auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        client = std::move(built).value();
    }
    ~Setup() {
        client.reset();
        other.reset();
        match.reset();
    }
    // The Frame World (what the player sees) and the Sim World (the client World).
    K::World& Client() { return client->World("Frame").GetWorld(); }
    K::World& Sim() { return client->World("Sim").GetWorld(); }
    K::World& Solo() { return client->World("Solo").GetWorld(); }
    Pending& Next() { return *Client().ResourceMut<Pending>(); }
    // Runs frames until the game scene's Worlds have their map (they generate until then; the map is read on the asset
    // threads) and the own player is there, at most a few seconds: the frames a test counts start from a game that is
    // played.
    void Loaded() {
        const auto waiting = [this] {
            return client->Generating("Sim") || client->Generating("Solo") || client->Generating("Frame") ||
                   !PC::StatusOf(Client()).playing;
        };
        for (int i = 0; i < 2000 && waiting(); ++i) {
            Frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE_FALSE(waiting());
    }
    // Frames until the game shows: loaded (Loaded), the loading screen's work done and the match started, and the loading
    // page gone (its request is applied the next frame). Before that a click or Tab is the loading screen's, not the
    // game's (ReadControls), however long the own player has been there.
    void Shown() {
        Loaded();
        const auto shown = [this] { return PC::StatusOf(Client()).ready && PC::StatusOf(Client()).started; };
        for (int i = 0; i < 2000 && !shown(); ++i) {
            Frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE_MESSAGE(shown(), "the game shows (loaded, drawn and started)");
        Frames(2);
    }
    void Frame() {
        const bool step = frame++ % framesPerStep == 0;
        if (step && !holdMatch) REQUIRE(static_cast<bool>(match->RunFrame()));
        auto ran = client->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
        // The scene changes between two frames, as sdl_window's runner does (SDLWindowBetweenFrames).
        if (PC::SceneChangeWanted(*client)) {
            auto changed = PC::ChangeScene(*client);
            REQUIRE_MESSAGE(static_cast<bool>(changed), (changed ? std::string() : changed.error().message + "\n" + changed.error().detail));
        }
        if (step && other) {
            // The other Client's input for its next step, when the test steers it.
            if (otherInput) {
                if (auto* local = N::LocalInput<PW::PlayerInput>(GameWorld(*other))) {
                    const auto target = static_cast<std::uint32_t>(TickOf(GameWorld(*other)) + 1);
                    local->Set(target, *otherInput, N::ViewTime{target, 0});
                }
            }
            REQUIRE(static_cast<bool>(other->RunFrame()));
        }
        first.first.Advance();
        second.first.Advance();
        *clock += N::NetTime(std::chrono::nanoseconds(kStep / framesPerStep));
        // The asset threads load in the background.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    void Frames(const int n) {
        for (int i = 0; i < n; ++i) Frame();
    }
    [[nodiscard]] const PC::SceneChange& Scene() { return client->Buffer().Find<PC::SceneDirector>()->Current(); }
    [[nodiscard]] const PC::FrameNotes& Notes() { return *Client().Resource<PC::FrameNotes>(); }
    // The scene's document root (its UiRoot, its state, its bindings), or 0.
    K::EntityId Root() {
        for (const K::EntityId id : Client().Entities()) {
            if (Client().Get<GYO::Engine::Ui::UiRoot>(id) != nullptr) return id;
        }
        return 0;
    }
    std::string Binding(const std::string_view id) {
        const auto* bindings = Client().Get<GYO::Engine::Ui::UiBindings>(Root());
        if (bindings == nullptr) return {};
        const auto* value = bindings->Find(id);
        if (value == nullptr) return {};
        const auto* text = std::get_if<std::string>(value);
        return text != nullptr ? *text : std::string();
    }
    // Runs frames until the scene's document is laid out (the asset threads load it and its font in the background).
    void Load() {
        for (int i = 0; i < 400; ++i) {
            Frames(1);
            const auto* state = Client().Get<GYO::Engine::Ui::UiRootState>(Root());
            if (state != nullptr && state->runtime) return;
        }
    }
    void Press(const I::KeyCode code) {
        Next().keys.push_back(Key(code));
        Frames(1);
    }
};

// HUD texts drawn as pictures (HP, ammo, status).
std::size_t TextPictures(K::World& world) {
    std::size_t n = 0;
    for (const K::EntityId id : world.Entities()) {
        const auto* sprite = world.Get<R::Sprite>(id);
        if (sprite != nullptr && std::holds_alternative<std::shared_ptr<const GYO::Engine::Image::ImageData>>(sprite->image)) ++n;
    }
    return n;
}

// The entities whose Mesh3d is placed at a player's feet (the meshes of its body).
std::size_t BodiesAt(K::World& world, const GYO::Engine::Math::Vec3 feet) {
    std::size_t n = 0;
    for (const K::EntityId id : world.Entities()) {
        const auto* mesh = world.Get<R::Mesh3d>(id);
        const auto* t = world.Get<T::Transform>(id);
        if (mesh != nullptr && t != nullptr && GYO::Engine::Math::Length(t->translation - feet) < 0.5F) ++n;
    }
    return n;
}

std::uint64_t SoundsSubmitted(O::Product& product) {
    const auto* mixer = std::as_const(product.Buffer()).Find<GYO::Engine::Audio::Mixer>();
    return mixer != nullptr ? mixer->Stats().submitted : 0;
}

I::KeyboardInput Key(const I::KeyCode code, const I::ButtonState state) {
    I::KeyboardInput key;
    key.keyCode = code;
    key.state = state;
    return key;
}

} // namespace

TEST_CASE("client: it connects, shows the arena and the other player, takes the pointer with a click and walks") {
    Setup setup;
    setup.Loaded();
    setup.Frames(120);
    PC::ClientStatus status = PC::StatusOf(setup.Client());
    REQUIRE(status.connected);
    REQUIRE(status.playing);
    CHECK_FALSE(status.captured);
    CHECK(status.mismatch.empty());
    // The picture: a 3D pass with the arena and the other player's stand-in body, then the HUD's 2D pass.
    CHECK_MESSAGE(lastPicture.passes3d >= 1, "the world camera draws a 3D pass");
    CHECK_MESSAGE(lastPicture.draws3d >= 2 + ShippedContent()->character->body->meshes.size(),
                  "the arena, the other player's body and the first-person weapon are drawn: " << lastPicture.draws3d);
    CHECK_MESSAGE(lastPicture.passes2d >= 1, "the HUD camera draws a 2D pass");
    CHECK_MESSAGE(lastPicture.draws2d >= 1, "the HUD draws");
    CHECK_MESSAGE(TextPictures(setup.Client()) >= 3, "the HUD shows HP, ammo and the status as text pictures");
    const K::EntityId rival = PlayerOf(setup.Client(), 2);
    REQUIRE(rival != 0);
    const std::size_t bodyMeshes = ShippedContent()->character->body->meshes.size();
    CHECK_MESSAGE(BodiesAt(setup.Client(), setup.Client().Get<PW::PlayerBody>(rival)->feet) == bodyMeshes,
                  "the other player's body (every mesh of the model) where it stands");
    CHECK_MESSAGE(lastPicture.passes3d >= 2, "the world camera and the viewmodel camera each draw a 3D pass");
    // The camera at the own player's eye.
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    const PW::PlayerBody before = *setup.Client().Get<PW::PlayerBody>(own);
    bool cameraAtEye = false;
    for (const K::EntityId id : setup.Client().Entities()) {
        if (setup.Client().Get<GYO::Engine::Camera::Camera3d>(id) == nullptr) continue;
        const auto* t = setup.Client().Get<T::Transform>(id);
        cameraAtEye = t != nullptr && t->translation.x == before.feet.x && t->translation.z == before.feet.z &&
                      t->translation.y == doctest::Approx(before.feet.y + 1.6F);
    }
    CHECK(cameraAtEye);
    // A click takes the pointer (and is no shot); the window is asked to lock and hide the cursor.
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.state = I::ButtonState::Pressed;
    click.position = {640.0F, 360.0F};
    setup.Next().buttons.push_back(click);
    setup.Frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    setup.Frame();
    CHECK(PC::StatusOf(setup.Client()).captured);
    CHECK(setup.Client().Resource<W::WindowRequest>()->cursorGrab == W::CursorGrab::Locked);
    CHECK_FALSE(setup.Client().Resource<W::WindowRequest>()->cursorVisible);
    // The mouse turns the view: the camera turns, and the player at the Match turns with it.
    const auto cameraRotation = [&setup] {
        for (const K::EntityId id : setup.Client().Entities()) {
            if (setup.Client().Get<GYO::Engine::Camera::Camera3d>(id) != nullptr) return setup.Client().Get<T::Transform>(id)->rotation;
        }
        return GYO::Engine::Math::Quaternion{};
    };
    const auto turnedFrom = cameraRotation();
    setup.Next().motions.push_back(I::MouseMotion{{200.0F, 0.0F}, {}});
    setup.Frames(20);
    const auto turnedTo = cameraRotation();
    CHECK_MESSAGE((turnedTo.x != turnedFrom.x || turnedTo.y != turnedFrom.y), "the camera turned with the mouse");
    CHECK(GameWorld(*setup.match).Get<PW::PlayerBody>(own)->yaw == doctest::Approx(GYO::Engine::Math::WrapRadians(before.yaw + 200.0F * 0.0025F)));
    // W held for a while: the player walks at the Match.
    setup.Next().keys.push_back(Key(I::KeyCode::W));
    setup.Frames(40);
    setup.Next().keys.push_back(Key(I::KeyCode::W, I::ButtonState::Released));
    setup.Frames(40);
    const PW::PlayerBody* atMatch = GameWorld(*setup.match).Get<PW::PlayerBody>(own);
    REQUIRE(atMatch != nullptr);
    CHECK_MESSAGE(GYO::Engine::Math::Length(atMatch->feet - before.feet) > 1.0F, "the player walked at the Match");
    CHECK(GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots == 0);
    // A second click fires, and the shot sounds.
    const std::uint64_t soundsBefore = SoundsSubmitted(*setup.client);
    click.state = I::ButtonState::Pressed;
    setup.Next().buttons.push_back(click);
    setup.Frames(30);
    CHECK(GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots == 1);
    CHECK_MESSAGE(SoundsSubmitted(*setup.client) > soundsBefore, "the shot was played");
    // Escape gives the pointer back.
    setup.Next().keys.push_back(Key(I::KeyCode::Escape));
    setup.Frame();
    CHECK_FALSE(PC::StatusOf(setup.Client()).captured);
    CHECK(setup.Client().Resource<W::WindowRequest>()->cursorVisible);
}

TEST_CASE("client: a click in a frame that runs no step still fires at the Match (four frames a step)") {
    Setup setup(4);
    setup.Loaded();
    setup.Frames(480);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.position = {640.0F, 360.0F};
    const auto press = [&](const I::ButtonState state) {
        click.state = state;
        setup.Next().buttons.push_back(click);
        setup.Frame();
    };
    press(I::ButtonState::Pressed);  // takes the pointer
    press(I::ButtonState::Released);
    REQUIRE(PC::StatusOf(setup.Client()).captured);
    setup.Frames(2);
    // Four shots, each clicked at another place in the four frames of a step, past the cooldown (11 steps).
    for (int shot = 0; shot < 4; ++shot) {
        setup.Frames(shot);
        press(I::ButtonState::Pressed);
        press(I::ButtonState::Released);
        setup.Frames(4 * 14 - shot);
    }
    setup.Frames(4 * 20);
    CHECK_MESSAGE(GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots == 4, "every click reached the Match");
    CHECK(setup.Client().Get<PW::PlayerCombat>(own)->shots == 4);
}

// live-frame-sim-thread declaration J5 (R5): the program lends the Sim World to a thread of its own; here the test's
// "Sim thread" runs its rounds with the Match and the MemoryLink (whose ends are used from one thread), one round a
// step in real time, while the main thread runs the Frame World's frames and stops 1500 ms between two of them, as
// when the OS holds the window. The stop is longer than J5's 250 ms so it also passes the controls' stale limit
// (SimControls: at least 200 ms, longer when the Frame World's frames are slow, as on a CI machine): first the held W goes on, then the movement is neutral and the view stays (the
// user's decision 2026-11-25, after master's D30); the Match gets an input every step all the while.
TEST_CASE("client: J5 while the main thread stops, the lent Sim World steps on, the Match gets its input every step, and stale controls stop the movement") {
    Setup setup;
    setup.other.reset();
    setup.Loaded();
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.position = {640.0F, 360.0F};
    click.state = I::ButtonState::Pressed;
    setup.Next().buttons.push_back(click);
    setup.Frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    setup.Frame();
    REQUIRE(PC::StatusOf(setup.Client()).captured);
    // W and Space held from here on: every step's input walks forward (and, with the shipped hold_jump, asks to jump)
    // while the controls are fresh.
    setup.Next().keys.push_back(Key(I::KeyCode::W));
    setup.Next().keys.push_back(Key(I::KeyCode::Space));
    setup.Frames(2);

    auto lent = setup.client->Lend("Sim");
    REQUIRE_MESSAGE(static_cast<bool>(lent), (lent ? std::string() : lent.error().message));
    O::WorldLease lease = std::move(lent).value();
    struct Round final {
        int phase = 0;  // 0 before the stop, 1 during it, 2 after
        std::uint64_t simTick = 0;
        std::uint64_t consumed = 0, missing = 0;  // the Match's counts of this Client's inputs
        float forward = 0.0F;                     // the input the Match gave the own player this step
        std::uint8_t present = 0;
        std::uint8_t jump = 0;
        float yaw = 0.0F;                         // the own player's view at the Match
    };
    std::vector<Round> rounds;
    std::atomic<int> phase{0};
    std::atomic<bool> stop{false};
    std::string simFailure;
    std::thread simThread([&] {
        // A round a step of real time (the Sim World's time is the test's step a round: ManualDelta).
        auto next = std::chrono::steady_clock::now();
        while (!stop.load()) {
            Round round;
            round.phase = phase.load();
            auto match = setup.match->RunFrame();
            auto sim = lease.RunRound();
            if (!match || !sim) {
                simFailure = !match ? match.error().message : sim.error().message;
                return;
            }
            setup.first.first.Advance();
            setup.second.first.Advance();
            *setup.clock += N::NetTime(std::chrono::nanoseconds(kStep));
            K::World& matchWorld = GameWorld(*setup.match);
            for (const K::EntityId id : matchWorld.Entities()) {
                if (const auto* inputs = matchWorld.Get<N::PeerInput<PW::PlayerInput>>(id)) {
                    round.consumed = inputs->counts.consumed;
                    round.missing = inputs->counts.missing;
                }
            }
            if (const auto* input = matchWorld.Get<PW::PlayerInput>(own)) {
                round.forward = input->forward;
                round.present = input->present;
                round.jump = input->jump;
            }
            if (const auto* body = matchWorld.Get<PW::PlayerBody>(own)) round.yaw = body->yaw;
            round.simTick = TickOf(lease.World());
            rounds.push_back(round);
            next += std::chrono::nanoseconds(kStep);
            std::this_thread::sleep_until(next);
        }
    });
    std::string mainFailure;
    const auto frames = [&](const int n) {
        for (int i = 0; i < n && mainFailure.empty(); ++i) {
            auto ran = setup.client->RunFrame();
            if (!ran) mainFailure = ran.error().message;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    };
    frames(20);
    phase = 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    phase = 2;
    frames(40);
    stop = true;
    simThread.join();
    setup.client->Return(std::move(lease));
    REQUIRE_MESSAGE(simFailure.empty(), simFailure);
    REQUIRE_MESSAGE(mainFailure.empty(), mainFailure);
    const std::uint64_t frameTick = setup.Client().Resource<PC::ClientView>()->tick;

    std::vector<Round> stopped;
    for (const Round& round : rounds) {
        if (round.phase == 1) stopped.push_back(round);
    }
    REQUIRE_MESSAGE(stopped.size() >= 10, "[pvp-sim-thread-steps] rounds while the main thread stopped: " << stopped.size());
    const Round& first = stopped.front();
    const Round& last = stopped.back();
    CHECK_MESSAGE(last.simTick - first.simTick == stopped.size() - 1, "[pvp-sim-thread-steps] a step a round: " << first.simTick << ".." << last.simTick);
    CHECK_MESSAGE(last.consumed - first.consumed == stopped.size() - 1,
                  "[pvp-sim-thread-input] the Match took this Client's input every step: " << last.consumed - first.consumed);
    CHECK_MESSAGE(last.missing == first.missing, "[pvp-sim-thread-input] no step without its input: " << last.missing - first.missing);
    // Fresh at the stop's start: the held W goes on. Stale by its end (1500 ms past the last controls): no movement,
    // and once stale it stays so; the view stays where it was.
    CHECK_MESSAGE(first.forward > 0.0F, "[pvp-sim-thread-input] the held W goes on at the stop's start");
    CHECK_MESSAGE((last.forward == 0.0F && last.present == 0), "[pvp-sim-thread-stale] stale controls move nothing: forward " << last.forward);
    CHECK_MESSAGE(first.jump != 0, "[pvp-sim-thread-input] the held Space asks to jump at the stop's start");
    CHECK_MESSAGE(last.jump == 0, "[pvp-hold-stale] stale controls do not jump on a held Space");
    std::size_t neutralAt = stopped.size();
    for (std::size_t i = 0; i < stopped.size(); ++i) {
        if (stopped[i].forward == 0.0F && neutralAt == stopped.size()) neutralAt = i;
        if (neutralAt < i) CHECK_MESSAGE(stopped[i].forward == 0.0F, "[pvp-sim-thread-stale] W again at round " << i << " of the stop");
    }
    // Not at once: the limit is at least 200 ms of the Sim World's time, 12.8 of its steps after the last controls
    // (which came at most a round or so before the stop), and the Match applies the input of a tick the Client sent a
    // few steps ahead (its prediction's lead). (No upper bound here: the limit grows with the Frame World's slowest
    // recent frames; 15 or 16 rounds on the author's machine, 30 on a CI runner.)
    CHECK_MESSAGE(neutralAt >= 8, "[pvp-sim-thread-stale-limit] neutral from round " << neutralAt << " of the stop");
    CHECK_MESSAGE(last.yaw == doctest::Approx(first.yaw), "[pvp-sim-thread-stale] the view stays");
    // After the stop the Frame World shows the newest picture, not the ones it missed; its controls come again and the
    // held W with them.
    CHECK_MESSAGE(frameTick > last.simTick, "[pvp-sim-thread-newest] the Frame World's tick " << frameTick << " after the stop's last " << last.simTick);
    bool walksAgain = false;
    for (const Round& round : rounds) walksAgain = walksAgain || (round.phase == 2 && round.forward > 0.0F);
    CHECK_MESSAGE(walksAgain, "[pvp-sim-thread-stale] fresh controls walk again after the stop");
    // The Sim World is back on the main thread; still connected, still playing.
    setup.Next().keys.push_back(Key(I::KeyCode::W, I::ButtonState::Released));
    setup.Next().keys.push_back(Key(I::KeyCode::Space, I::ButtonState::Released));
    setup.Frames(10);
    CHECK(PC::StatusOf(setup.Client()).connected);
    CHECK(PlayerOf(GameWorld(*setup.match), 1) != 0);
}

// The Sim World lent: the Frame World runs several frames between two of its rounds, and the presses of those frames
// add up in the exchange (a click in an early frame is not overwritten by the later frames' controls).
TEST_CASE("client: with the Sim World lent, a click among several frames between two of its rounds fires once") {
    Setup setup;
    setup.other.reset();
    setup.Loaded();
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.position = {640.0F, 360.0F};
    const auto press = [&](const I::ButtonState state) {
        click.state = state;
        setup.Next().buttons.push_back(click);
        setup.Frame();
    };
    press(I::ButtonState::Pressed);  // takes the pointer
    press(I::ButtonState::Released);
    REQUIRE(PC::StatusOf(setup.Client()).captured);
    setup.Frames(20);
    const std::uint32_t shotsBefore = GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots;
    O::WorldLease lease = std::move(setup.client->Lend("Sim")).value();
    const auto frame = [&setup] { REQUIRE(static_cast<bool>(setup.client->RunFrame())); };
    const auto round = [&] {
        REQUIRE(static_cast<bool>(setup.match->RunFrame()));
        REQUIRE(static_cast<bool>(lease.RunRound()));
        setup.first.first.Advance();
        setup.second.first.Advance();
        *setup.clock += N::NetTime(std::chrono::nanoseconds(kStep));
    };
    // Four Frame frames, the click in the first, then one Sim round; then rounds and frames until it reached the Match.
    click.state = I::ButtonState::Pressed;
    setup.Next().buttons.push_back(click);
    frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    frame();
    frame();
    frame();
    round();
    for (int i = 0; i < 20; ++i) {
        frame();
        round();
    }
    setup.client->Return(std::move(lease));
    CHECK_MESSAGE(GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots == shotsBefore + 1,
                  "[pvp-exchange-accumulates] one click, one shot: " << GameWorld(*setup.match).Get<PW::PlayerCombat>(own)->shots - shotsBefore);
}

// PV17 (port declaration, segment 6): the other player walks; at four frames a step the Frame World shows it at a new
// place every frame (blended between two confirmed states, not a step at a time), behind the newest confirmed state;
// the inputs this Client sends carry the time the frame showed (a tick and its fraction).
TEST_CASE("client: the other player walks smoothly, a little behind the Match, and the inputs carry the time shown") {
  // No latency, and 13 Advance calls each way (about 100 ms of RTT at 64 Hz: the design review's case where a timeline
  // following the Match's tick now, as lightyear's does, runs past every state it has).
  for (const std::uint32_t delay : {0U, 13U}) {
    CAPTURE(delay);
    Setup setup(4, 4, 0, std::nullopt, {}, std::nullopt, {}, delay);
    setup.Loaded();
    setup.Frames(480);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId rival = PlayerOf(setup.Client(), 2);
    REQUIRE(rival != 0);
    PW::PlayerInput walk;
    walk.forward = 1.0F;
    walk.present = 1;
    walk.yaw = setup.Sim().Get<PW::PlayerBody>(rival)->yaw;
    setup.otherInput = walk;
    setup.Frames(4 * 6);
    // Our own Peer at the Match: the one that controls our player.
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    const auto ownView = [&setup, own]() -> std::optional<N::ViewTime> {
        K::World& match = GameWorld(*setup.match);
        for (const K::EntityId id : match.Entities()) {
            const auto* controls = match.Get<N::PeerControls>(id);
            const auto* inputs = match.Get<N::PeerInput<PW::PlayerInput>>(id);
            if (controls != nullptr && inputs != nullptr && controls->entity == own) return inputs->lastView;
        }
        return std::nullopt;
    };
    int moved = 0, behind = 0, fractions = 0, viewsAhead = 0;
    const int frames = 4 * 20;
    GYO::Engine::Math::Vec3 last = setup.Client().Get<PW::PlayerBody>(rival)->feet;
    for (int i = 0; i < frames; ++i) {
        setup.Frame();
        const GYO::Engine::Math::Vec3 shown = setup.Client().Get<PW::PlayerBody>(rival)->feet;
        const GYO::Engine::Math::Vec3 newest = setup.Sim().Get<PW::PlayerBody>(rival)->feet;
        if (GYO::Engine::Math::Length(shown - last) > 1e-4F) ++moved;
        if (GYO::Engine::Math::Length(newest - shown) > 0.01F) ++behind;
        last = shown;
        if (const auto view = ownView()) {
            if (view->overstep != 0) ++fractions;
            const auto confirmed = setup.Sim().Resource<N::NetReplicaState>()->latestConfirmed;
            if (confirmed && view->tick > *confirmed) ++viewsAhead;
        }
    }
    setup.otherInput.reset();
    CHECK_MESSAGE(moved >= frames * 3 / 4, "[pvp-interp-smooth] the other player moved in " << moved << " of " << frames << " frames");
    CHECK_MESSAGE(behind >= frames * 3 / 4, "[pvp-interp-behind] shown behind the newest confirmed state in " << behind << " of " << frames << " frames");
    CHECK_MESSAGE(fractions > 0, "[pvp-interp-view] the view time sent carries the timeline's fraction");
    CHECK_MESSAGE(viewsAhead == 0, "[pvp-interp-view] the view time sent is never past the confirmed tick: " << viewsAhead);
  }
}

namespace {
// The colour of the hit marker the HUD draws (its strokes run along the diagonals; the crosshair's along the axes), or
// none when it draws none.
std::optional<R::Color8> HitMarkerColor(K::World& world) {
    std::optional<R::Color8> found;
    for (const K::EntityId id : world.Entities()) {
        const auto* shape = world.Get<R::Shape2d>(id);
        if (shape == nullptr || shape->vertices.size() < 2) continue;
        const auto& a = shape->vertices[0].position;
        const auto& b = shape->vertices[1].position;
        if (a.x != b.x && a.y != b.y) found = shape->vertices[0].color;
    }
    return found;
}
} // namespace

// The own shot that hits shows an X beside the crosshair, another colour for the head (playtest 2026-11-26, item 8). The
// Match's judgement (ShotRecord) is written here as JudgeShots writes it.
TEST_CASE("client: an own shot that hits shows the hit marker, in the head colour for a head; it fades") {
    Setup setup;
    setup.Loaded();
    setup.Frames(60);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    const K::EntityId rival = PlayerOf(GameWorld(*setup.match), 2);
    REQUIRE(own != 0);
    REQUIRE(rival != 0);
    CHECK_FALSE(HitMarkerColor(setup.Client()).has_value());
    const auto& marker = setup.presentation->hitFeedback.hitMarker;
    const auto judged = [&](const sim::HitPart part) {
        K::World& match = GameWorld(*setup.match);
        PW::ShotRecord record;
        record.tick = TickOf(match);
        record.kind = sim::ShotHitKind::Player;
        record.target = rival;
        record.part = part;
        record.damage = 1;
        *match.GetMut<PW::ShotRecord>(own) = record;
    };
    const auto byte = [](const float v) { return static_cast<std::uint8_t>(std::lround(v * 255.0F)); };
    judged(sim::HitPart::Body);
    std::optional<R::Color8> body;
    for (int i = 0; i < 20 && !body; ++i) {
        setup.Frame();
        body = HitMarkerColor(setup.Client());
    }
    REQUIRE_MESSAGE(body.has_value(), "[pvp-hit-marker] a body hit shows the marker");
    CHECK(body->r == byte(marker.color[0]));
    CHECK(body->g == byte(marker.color[1]));
    CHECK(body->b == byte(marker.color[2]));
    // Gone after its seconds (64 frames a second here).
    setup.Frames(static_cast<int>(marker.seconds * 64) + 4);
    CHECK_MESSAGE(!HitMarkerColor(setup.Client()).has_value(), "[pvp-hit-marker] the marker fades away");
    judged(sim::HitPart::Head);
    std::optional<R::Color8> head;
    for (int i = 0; i < 20 && !head; ++i) {
        setup.Frame();
        head = HitMarkerColor(setup.Client());
    }
    REQUIRE_MESSAGE(head.has_value(), "[pvp-hit-marker] a head hit shows the marker");
    CHECK_MESSAGE(head->g == byte(marker.headColor[1]), "[pvp-hit-marker-head] the head colour");
    CHECK(head->b == byte(marker.headColor[2]));
}

// The hit is heard where it lands, by distance (the user's decision of 2026-11-26, playtest item 1: B): a near target's
// hit sounds, a target across the 250 m map is too far to hear (the hit marker still shows). The Match's judgement is
// written as JudgeShots writes it.
TEST_CASE("client: an own hit sounds at the target: heard near, not across the big map") {
    std::size_t big = 0;
    for (std::size_t i = 0; i < ShippedContent()->arenas.size(); ++i) {
        if (ShippedContent()->arenas[i].id == "kiriyu_extraction_250m") big = i;
    }
    REQUIRE(big != 0);
    for (const std::size_t arena : {std::size_t{0}, big}) {
        CAPTURE(arena);
        Setup setup(1, 4, arena);
        setup.Loaded();
        setup.Frames(60);
        const K::EntityId own = PW::OwnPlayer(setup.Sim());
        const K::EntityId rival = PlayerOf(GameWorld(*setup.match), 2);
        REQUIRE(own != 0);
        REQUIRE(rival != 0);
        REQUIRE(setup.Client().Get<PW::PlayerBody>(rival) != nullptr);
        const float apart = GYO::Engine::Math::Length(setup.Client().Get<PW::PlayerBody>(rival)->feet -
                                                      setup.Client().Get<PW::PlayerBody>(own)->feet);
        const auto& cue = setup.presentation->sound.cues[static_cast<std::size_t>(object_fps_pvp::present::SoundCue::HitConfirmed)];
        const bool near = arena == 0;
        REQUIRE((near ? apart < cue.maximumDistance : apart > cue.maximumDistance));
        const std::uint64_t before = SoundsSubmitted(*setup.client);
        K::World& match = GameWorld(*setup.match);
        PW::ShotRecord record;
        record.tick = TickOf(match);
        record.kind = sim::ShotHitKind::Player;
        record.target = rival;
        record.part = sim::HitPart::Body;
        record.damage = 1;
        *match.GetMut<PW::ShotRecord>(own) = record;
        bool marked = false;
        for (int i = 0; i < 20; ++i) {
            setup.Frame();
            marked = marked || HitMarkerColor(setup.Client()).has_value();
        }
        CHECK_MESSAGE(marked, "the hit marker shows near and far");
        if (near) {
            CHECK_MESSAGE(SoundsSubmitted(*setup.client) == before + 1, "[pvp-hit-sound-near] the hit on a near target sounds");
        } else {
            CHECK_MESSAGE(SoundsSubmitted(*setup.client) == before, "[pvp-hit-sound-far] the hit on a far target is not heard");
        }
    }
}

// Space held jumps again on every landing (the shipped movement rules' hold_jump; playtest 2026-11-26, item 7); let go,
// the player stays down. The press of a tap still jumps once.
TEST_CASE("client: holding Space jumps again on every landing; letting go stops") {
    Setup setup;
    setup.Loaded();
    setup.Frames(60);
    REQUIRE(ShippedContent()->modes.front().stance.holdJump);
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.state = I::ButtonState::Pressed;
    click.position = {640.0F, 360.0F};
    setup.Next().buttons.push_back(click);
    setup.Frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    setup.Frame();
    REQUIRE(PC::StatusOf(setup.Client()).captured);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    REQUIRE(own != 0);
    const auto takeoffs = [&](const int frames) {
        int count = 0;
        bool grounded = GameWorld(*setup.match).Get<PW::PlayerBody>(own)->grounded != 0;
        for (int i = 0; i < frames; ++i) {
            setup.Frame();
            const bool now = GameWorld(*setup.match).Get<PW::PlayerBody>(own)->grounded != 0;
            if (grounded && !now) ++count;
            grounded = now;
        }
        return count;
    };
    // A jump lasts about 0.52 s (2 x sqrt(2 h / g) at 0.6 m and 18 m/s^2): two seconds held is three or more.
    setup.Next().keys.push_back(Key(I::KeyCode::Space));
    const int held = takeoffs(128);
    CHECK_MESSAGE(held >= 3, "[pvp-hold-jump] two seconds of Space held jumped " << held << " times");
    setup.Next().keys.push_back(Key(I::KeyCode::Space, I::ButtonState::Released));
    setup.Frames(64);
    CHECK_MESSAGE(takeoffs(64) == 0, "[pvp-hold-jump] let go, the player stays down");
    // A tap: one jump.
    setup.Next().keys.push_back(Key(I::KeyCode::Space));
    setup.Frame();
    setup.Next().keys.push_back(Key(I::KeyCode::Space, I::ButtonState::Released));
    CHECK_MESSAGE(takeoffs(128) == 1, "a tap jumps once");
}

// The other players' steps are heard too, where they walk; a crouched walk is silent (playtest 2026-11-26, item 2).
TEST_CASE("client: the other player's steps sound while it walks standing, not while it walks crouched") {
    Setup setup(4);
    setup.Loaded();
    setup.Frames(60);
    const K::EntityId rival = PlayerOf(setup.Client(), 2);
    REQUIRE(rival != 0);
    PW::PlayerInput walk;
    walk.forward = 1.0F;
    walk.present = 1;
    walk.yaw = setup.Sim().Get<PW::PlayerBody>(rival)->yaw;
    walk.crouch = 1;
    setup.otherInput = walk;
    // Crouching down and walking two seconds (about three metres): no sound at all (our player stands still).
    setup.Frames(4 * 16);
    const std::uint64_t crouchedBefore = SoundsSubmitted(*setup.client);
    const GYO::Engine::Math::Vec3 crouchedFrom = setup.Client().Get<PW::PlayerBody>(rival)->feet;
    setup.Frames(4 * 128);
    REQUIRE(GYO::Engine::Math::Length(setup.Client().Get<PW::PlayerBody>(rival)->feet - crouchedFrom) > 2.0F);
    CHECK_MESSAGE(SoundsSubmitted(*setup.client) == crouchedBefore, "[pvp-crouch-silent] a crouched walk made no sound");
    // Standing up and walking one more second (three metres, two strides of 1.5): its steps sound.
    walk.crouch = 0;
    walk.yaw += 3.14159265F;
    setup.otherInput = walk;
    const std::uint64_t standingBefore = SoundsSubmitted(*setup.client);
    setup.Frames(4 * 64);
    CHECK_MESSAGE(SoundsSubmitted(*setup.client) >= standingBefore + 1, "[pvp-remote-steps] the other player's steps were played");
    setup.otherInput.reset();
}

// A respawn (a new life somewhere else) is a jump, not a walk: the Frame World's copy is at the old place or at the
// new one, never between (port declaration, segment 6, design 3).
TEST_CASE("client: the other player's new life is not blended from where it was") {
    Setup setup(4);
    setup.Loaded();
    setup.Frames(480);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId rival = PlayerOf(setup.Client(), 2);
    REQUIRE(rival != 0);
    K::World& match = GameWorld(*setup.match);
    const GYO::Engine::Math::Vec3 from = match.Get<PW::PlayerBody>(rival)->feet;
    // Another spawn, far from where it stands.
    GYO::Engine::Math::Vec3 to = from;
    for (const auto& spawn : ShippedContent()->arenas[0].spawns) {
        if (GYO::Engine::Math::Length(spawn.position - from) > GYO::Engine::Math::Length(to - from)) to = spawn.position;
    }
    REQUIRE(GYO::Engine::Math::Length(to - from) > 2.0F);
    match.GetMut<PW::PlayerBody>(rival)->feet = to;
    match.GetMut<PW::PlayerCombat>(rival)->lifeTick = static_cast<std::uint32_t>(TickOf(match));
    int between = 0, atNew = 0;
    for (int i = 0; i < 4 * 20; ++i) {
        setup.Frame();
        const GYO::Engine::Math::Vec3 shown = setup.Client().Get<PW::PlayerBody>(rival)->feet;
        const bool old = GYO::Engine::Math::Length(shown - from) < 0.05F;
        const bool fresh = GYO::Engine::Math::Length(shown - to) < 0.05F;
        if (!old && !fresh) ++between;
        if (fresh) ++atNew;
    }
    CHECK_MESSAGE(between == 0, "[pvp-interp-life] frames between the death place and the spawn: " << between);
    CHECK(atNew > 0);
}

// The viewmodel's recoil (port declaration, segment 6): a shot turns the weapon up, the anchor stays, and it comes back.
TEST_CASE("client: a shot kicks the first-person weapon up and it comes back") {
    Setup setup;
    setup.Loaded();
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const PC::ViewmodelDefinition& viewmodel = setup.presentation->viewmodel;
    REQUIRE(viewmodel.recoilKickRadians > 0.0F);
    CHECK(PC::RecoilRadians(viewmodel, 0.0) == doctest::Approx(viewmodel.recoilKickRadians));
    CHECK(PC::RecoilRadians(viewmodel, 1.0) < viewmodel.recoilKickRadians * 0.01F);
    // The kicked placement keeps the anchor where it is and turns the weapon's forward up.
    const T::Transform rest = PC::ViewmodelPlacement(viewmodel);
    const T::Transform kicked = PC::ViewmodelPlacement(viewmodel, viewmodel.recoilKickRadians);
    const auto anchorAt = [&viewmodel](const T::Transform& t) { return GYO::Engine::Math::Rotate(t.rotation, viewmodel.anchor) + t.translation; };
    CHECK(GYO::Engine::Math::Length(anchorAt(kicked) - anchorAt(rest)) < 1e-4F);
    // The weapon's forward: what points ahead (+Z, left-handed) at rest.
    const GYO::Engine::Math::Vec3 forward =
        GYO::Engine::Math::Rotate(GYO::Engine::Math::Conjugate(rest.rotation), GYO::Engine::Math::Vec3{0.0F, 0.0F, 1.0F});
    CHECK(GYO::Engine::Math::Rotate(kicked.rotation, forward).y > 0.0F);
    // The weapon's parts in the Frame World.
    const auto partRotationY = [&setup, forward]() -> std::optional<float> {
        for (const K::EntityId id : setup.Client().Entities()) {
            const auto* t = setup.Client().Get<T::Transform>(id);
            const auto* layers = setup.Client().Get<R::RenderLayers>(id);
            if (t != nullptr && layers != nullptr && setup.Client().Get<R::SkinPose>(id) != nullptr &&
                setup.Client().Get<K::ChildOf>(id) != nullptr) {
                return GYO::Engine::Math::Rotate(t->rotation, forward).y;
            }
        }
        return std::nullopt;
    };
    REQUIRE(partRotationY().has_value());
    CHECK(*partRotationY() == doctest::Approx(0.0F).epsilon(1e-4));
    // The loading screen gone: a click before it is the screen's, not the game's.
    setup.Shown();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.position = {640.0F, 360.0F};
    const auto press = [&](const I::ButtonState state) {
        click.state = state;
        setup.Next().buttons.push_back(click);
        setup.Frame();
    };
    press(I::ButtonState::Pressed);  // takes the pointer
    press(I::ButtonState::Released);
    setup.Frames(4);
    press(I::ButtonState::Pressed);  // fires
    press(I::ButtonState::Released);
    setup.Frames(2);
    CHECK_MESSAGE(*partRotationY() > 0.01F, "[pvp-recoil] the weapon turned up after the shot: " << *partRotationY());
    setup.Frames(64);
    CHECK_MESSAGE(*partRotationY() == doctest::Approx(0.0F).epsilon(1e-3), "[pvp-recoil] back at rest a second later: " << *partRotationY());
}

namespace {

// Runs frames until done says so (at most n); an answer may come from another thread (the Gateway, the asset threads).
template <class Done>
bool FramesUntil(Setup& setup, Done done, const int n = 600) {
    for (int i = 0; i < n; ++i) {
        if (done()) return true;
        setup.Frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return done();
}

} // namespace

// The loading waits for both Worlds' maps: the picture made in the Frame World is not enough while the Sim World's
// walls are still being built (its fixed steps have not started).
TEST_CASE("client: the loading screen waits for the Sim World's walls, not only for the Frame World's picture") {
    HeldJobs jobs;
    Setup setup(1, 4, 0, std::nullopt, {}, std::nullopt, {}, 0, {}, &jobs);
    // The two Worlds read the map from one asset store, each at its own apply point, so either may see it complete
    // first (a region may finish loading between them). The Frame World asks for the picture in the frame its map
    // turns Building: the picture is the job asked for after the walls when they were already there (the Sim World
    // runs first in a frame), else the first.
    std::optional<std::size_t> picture;
    REQUIRE(FramesUntil(setup, [&] {
        if (!picture && setup.Client().Resource<PW::MapLoad>()->stage == PW::MapStage::Building) picture = jobs.Waiting() - 1;
        return jobs.Waiting() == 2;
    }));
    REQUIRE(picture);
    REQUIRE(jobs.RunAt(*picture));
    REQUIRE(FramesUntil(setup, [&] { return setup.Client().Resource<PW::MapLoad>()->stage == PW::MapStage::Done; }));
    REQUIRE(setup.Client().Resource<PC::ClientView>()->mapLoad.stage == PW::MapStage::Building);
    for (int i = 0; i < 120; ++i) {
        setup.Frame();
        REQUIRE_MESSAGE(!PC::StatusOf(setup.Client()).ready, "[pvp-loading-sim-walls] not ready before the Sim World's walls");
    }
    CHECK_MESSAGE(setup.Binding("loading") == "地図を組み立てています…",
                  "[pvp-loading-sim-walls] the assets step waits for the walls: " << setup.Binding("loading"));
    REQUIRE(jobs.RunOne());
    CHECK(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).ready; }));
}

// The Match plays another arena than the one the Client starts with (a Match reached by its address): a World plays one
// arena, so the game is built again on the Match's (big-map declaration 15.3).
TEST_CASE("client: a game whose Match plays another arena is built again on it") {
    Setup setup(1, 4, 1);
    const object_fps_pvp::sim::ArenaContent& played = ShippedContent()->arenas[1];
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().map == played.id; }),
                    "[pvp-arena-again] the game is built again on the Match's arena");
    setup.Loaded();
    setup.Frames(240);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    REQUIRE(setup.Sim().Resource<PW::GameRules>()->arena.id == played.id);
    CHECK_MESSAGE(setup.Client().Resource<PW::GameRules>()->arena.id == played.id, "[pvp-frame-arena] the Frame World's rules");
    const std::uint64_t matchWalls = GameWorld(*setup.match).Resource<PW::GameRules>()->wallsDigest;
    CHECK(setup.Sim().Resource<PW::GameRules>()->wallsDigest == matchWalls);
    CHECK(setup.Client().Resource<PW::GameRules>()->wallsDigest == matchWalls);
    CHECK(PC::StatusOf(setup.Client()).mismatch.empty());
    const object_fps_pvp::data::ArenaMap map = ShippedMapOf(played);
    bool drawn = false;
    for (const K::EntityId id : setup.Client().Entities()) {
        const auto* mesh = setup.Client().Get<R::Mesh3d>(id);
        if (mesh != nullptr && ShowsPalette(*mesh, map)) drawn = true;
    }
    CHECK_MESSAGE(drawn, "[pvp-frame-arena] the played arena's picture is drawn");
}

// The Match plays another mode than the one the Client starts with (it has both): a World plays one mode, so the game is
// built again on the Match's (mode-config declaration 3.3); a mode of the same id with other rules cannot be played.
TEST_CASE("client: a game whose Match plays another mode is built again on it; the same mode with other rules, or another character, is left") {
    {
        auto two = std::make_shared<PW::GameContent>(*ShippedContent());
        PW::ModeContent second = two->modes.front();
        second.id = "second";
        second.rulesDigest ^= 0x5a5a;
        two->modes.push_back(second);
        Setup setup(1, 4, 0, std::nullopt, {}, std::nullopt, {}, 0, {}, nullptr, two, 1);
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().mode == "second"; }),
                        "[pvp-mode-again] the game is built again on the Match's mode");
        setup.Loaded();
        setup.Frames(120);
        CHECK(PC::StatusOf(setup.Client()).playing);
        CHECK(setup.Sim().Resource<PW::ActiveMode>()->id == "second");
        CHECK(setup.Sim().Resource<PW::ContentMismatch>()->reason.empty());
    }
    {
        // The Client's core_pvp has other rules than the Match's: it cannot play it, and goes back with the reason.
        auto other = std::make_shared<PW::GameContent>(*ShippedContent());
        other->modes.front().rulesDigest ^= 0x5a5a;
        Setup setup(1, 4, 0, std::nullopt, {}, std::nullopt, {}, 0, {}, nullptr, other, 0, ShippedContent());
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Lobby; }),
                        "[pvp-mode-rules-left] a mode with other rules is not played");
        CHECK_MESSAGE(setup.Scene().status.find("の規則が Match と違います") != std::string::npos, setup.Scene().status);
    }
    {
        // Another character (the user's decision U1): not played again, back with the reason.
        auto other = std::make_shared<PW::GameContent>(*ShippedContent());
        other->characterDigest ^= 1;
        Setup setup(1, 4, 0, std::nullopt, {}, std::nullopt, {}, 0, {}, nullptr, other, 0, ShippedContent());
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Lobby; }),
                        "[pvp-character-left] a game of another character is not played");
        CHECK_MESSAGE(setup.Scene().status.find("キャラクター") != std::string::npos, setup.Scene().status);
    }
}

// The arena's map drawn region by region (big-map declaration section 15, L5): one Mesh3d per region of the played
// arena's map, each the region's model (read while the Frame World generated), with the region's box, and the 3D
// camera culled by the visibility Module.
TEST_CASE("client: the arena is drawn as one Mesh3d per region of its map, each with its box, culled per camera") {
    Setup setup(1, 4, 1);
    const object_fps_pvp::sim::ArenaContent& played = ShippedContent()->arenas[1];
    REQUIRE(FramesUntil(setup, [&] { return setup.Scene().map == played.id; }));
    setup.Loaded();
    setup.Frames(240);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const auto pictures = object_fps_pvp::data::RegionPictures(ShippedMapOf(played));
    REQUIRE(!pictures.empty());
    std::size_t regions = 0;
    for (const K::EntityId id : setup.Client().Entities()) {
        const auto* mesh = setup.Client().Get<R::Mesh3d>(id);
        if (mesh == nullptr) continue;
        // The region's model is handed to the render World: drawn once, its vertices are only on the GPU.
        const auto* handed = std::get_if<std::shared_ptr<R::HandedModel>>(&mesh->model);
        if (handed == nullptr || *handed == nullptr) continue;
        const auto* box = setup.Client().Get<R::Aabb>(id);
        REQUIRE(box != nullptr);
        for (const auto& region : pictures) {
            if (box->local.minimum.x != region.bounds.minimum.x || box->local.minimum.z != region.bounds.minimum.z) continue;
            ++regions;
            CHECK_MESSAGE(box->local.maximum.x == region.bounds.maximum.x, "[pvp-region-box] the region's box");
            CHECK_MESSAGE(((*handed)->Taken() && (*handed)->Model()->meshes.front().positions.empty()),
                          "[pvp-region-handed] the region's model is handed over and its vertices taken once drawn");
            // What the render World drew for the entity: the region's own triangles.
            std::uint32_t indices = 0;
            for (const auto& item : setup.client->World("Render").GetWorld().Resource<R::Extracted3dItems>()->items) {
                if (item.entity == id) indices = item.indexCount;
            }
            CHECK_MESSAGE(indices == region.model->meshes.front().indices.size(), "[pvp-region-model] the region's faces: " << indices);
        }
    }
    CHECK_MESSAGE(regions == pictures.size(), "[pvp-region-meshes] one Mesh3d per region of the map: " << regions);
    bool culledList = false;
    for (const K::EntityId id : setup.Client().Entities()) {
        if (setup.Client().Get<GYO::Engine::Camera::Camera3d>(id) != nullptr && setup.Client().Get<R::CulledEntities>(id) != nullptr) {
            culledList = true;
        }
    }
    CHECK_MESSAGE(culledList, "[pvp-region-culled] the visibility Module checks the 3D camera");
}

// The loading's warm-up waits for the meshes mesh_render has not made yet (its bytes a frame), as for skipped draws.
TEST_CASE("client: the loading screen waits while meshes wait for their frame") {
    Setup setup;
    std::uint64_t shown = 0;
    // Meshes waiting all along, nothing skipped. Until the own player comes no frame is shown (so the warm-up's limit
    // of frames, counted in frames shown, is not spent however slow the machine is); then frames are shown, and still
    // it is not ready.
    ++shown;
    REQUIRE(FramesUntil(setup, [&] {
        setup.Client().SetResource(PC::DrawnFrames{true, shown, 0, 3});
        return PC::StatusOf(setup.Client()).playing;
    }));
    for (int i = 0; i < 30; ++i) {
        setup.Client().SetResource(PC::DrawnFrames{true, ++shown, 0, 3});
        setup.Frame();
        REQUIRE_MESSAGE(!PC::StatusOf(setup.Client()).ready, "[pvp-loading-waits-meshes] not ready while meshes wait (frame " << shown << ")");
    }
    REQUIRE_MESSAGE(FramesUntil(setup, [&] {
                        setup.Client().SetResource(PC::DrawnFrames{true, ++shown, 0, 0});
                        return PC::StatusOf(setup.Client()).ready;
                    }),
                    "[pvp-loading-waits-meshes] ready once none waits");
}

// The regions' pictures made on several threads (one job per region): every region drawn, in region order.
TEST_CASE("client: a map's regions made on several threads are all drawn, in region order") {
    auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "test pictures", .threadCount = 4});
    REQUIRE(static_cast<bool>(started));
    WA::WorkerPoolExecutor jobs(*started);
    const auto* town = ShippedContent()->Find("kiriyu_town_graybox");
    REQUIRE(town != nullptr);
    const std::size_t index = static_cast<std::size_t>(town - ShippedContent()->arenas.data());
    {
        Setup setup(1, 4, index, std::nullopt, {}, std::nullopt, {}, 0, {}, &jobs);
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().map == town->id; }), "[pvp-arena-again]");
        setup.Loaded();
        const auto pictures = object_fps_pvp::data::RegionPictures(ShippedMapOf(*town));
        REQUIRE(pictures.size() == 4);
        std::vector<GYO::Engine::Math::Aabb> drawn;
        for (const K::EntityId id : setup.Client().Entities()) {  // ascending: the order they were made in
            const auto* mesh = setup.Client().Get<R::Mesh3d>(id);
            const auto* box = setup.Client().Get<R::Aabb>(id);
            if (mesh != nullptr && box != nullptr && std::holds_alternative<std::shared_ptr<R::HandedModel>>(mesh->model)) drawn.push_back(box->local);
        }
        REQUIRE_MESSAGE(drawn.size() == pictures.size(), "[pvp-regions-parallel] every region: " << drawn.size());
        for (std::size_t i = 0; i < drawn.size(); ++i) {
            CAPTURE(i);
            CHECK_MESSAGE((drawn[i].minimum.x == pictures[i].bounds.minimum.x && drawn[i].minimum.z == pictures[i].bounds.minimum.z),
                          "[pvp-regions-parallel] in region order");
        }
    }
}

TEST_CASE("client: the other player's stand-in goes when it leaves; closing tells the Match at once") {
    Setup setup;
    setup.Loaded();
    setup.Frames(120);
    const K::EntityId rival = PlayerOf(setup.Client(), 2);
    REQUIRE(rival != 0);
    const GYO::Engine::Math::Vec3 at = setup.Client().Get<PW::PlayerBody>(rival)->feet;
    REQUIRE(BodiesAt(setup.Client(), at) > 0);
    // The other Client closes, as the program does when its window closes: the Match drops it in a few frames.
    auto* service = setup.other->Buffer().Find<N::NetService>();
    REQUIRE(service != nullptr);
    service->Close();
    service->Flush(service->Now(), N::Stamp{});
    setup.other.reset();
    // The Frame World's copy goes after the Sim World's player: when the timeline passes the tick it left at.
    int simGone = -1, frameGone = -1;
    for (int i = 0; i < 20; ++i) {
        setup.Frame();
        if (simGone < 0 && !setup.Sim().Alive(rival)) simGone = i;
        if (frameGone < 0 && !setup.Client().Alive(rival)) frameGone = i;
    }
    CHECK_MESSAGE((simGone >= 0 && frameGone > simGone), "[pvp-interp-despawn] the copy goes after the Sim World's player: sim " << simGone << ", frame " << frameGone);
    CHECK_MESSAGE(PlayerOf(GameWorld(*setup.match), 2) == 0, "the Match took the player off without waiting for the timeout");
    CHECK_MESSAGE(PlayerOf(setup.Client(), 2) == 0, "[pvp-frame-mirror] the Frame World's copy went with the Sim World's player");
    CHECK(BodiesAt(setup.Client(), at) == 0);
}

TEST_CASE("client: the first-person weapon's idle pose sits before the viewmodel camera, low on the right") {
    std::string error;
    const auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, error);
    REQUIRE_MESSAGE(presentation.has_value(), error);
    const PC::ViewmodelDefinition& viewmodel = presentation->viewmodel;
    const auto& model = *viewmodel.model;
    std::vector<GYO::Engine::Model::LocalTransform> local(model.nodes.size());
    GYO::Engine::Model::SampleClip(model.nodes, model.clips[viewmodel.idle], 0.0F, GYO::Engine::Model::Wrap::Loop, local);
    std::vector<Math::Matrix4> nodes(local.size());
    GYO::Engine::Model::ComputeModelSpace(model.nodes, local, nodes);
    const T::Transform placement = PC::ViewmodelPlacement(viewmodel);
    // At rest it is turned by the definition's rotation, with the anchor at the offset.
    for (const Math::Vec3 axis : {Math::Vec3{1, 0, 0}, Math::Vec3{0, 1, 0}, Math::Vec3{0, 0, 1}}) {
        CHECK(Math::Length(Math::Rotate(placement.rotation, axis) - Math::Rotate(viewmodel.rotation, axis)) < 1e-5F);
    }
    CHECK(Math::Length(Math::Rotate(placement.rotation, viewmodel.anchor) + placement.translation - viewmodel.offset) < 1e-4F);
    const Math::Matrix4 toCamera = T::ToMatrix(placement);
    // Every skinned vertex in the camera's space (+Z ahead, +Y up, +X right).
    Math::Vec3 sum{};
    std::size_t count = 0;
    float nearest = 1e9F;
    float farthest = -1e9F;
    std::size_t outside = 0;
    const float tanHalf = std::tan(viewmodel.verticalFov / 2);
    for (const auto& mesh : model.meshes) {
        REQUIRE(mesh.skin != GYO::Engine::Model::kNoSkin);
        const auto& skin = model.skins[mesh.skin];
        for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
            Math::Vec3 skinned{};
            for (std::size_t k = 0; k < 4; ++k) {
                const float weight = mesh.vertexWeights[v][k];
                if (weight == 0.0F) continue;
                const std::uint16_t joint = mesh.vertexJoints[v][k];
                const Math::Matrix4 m = Math::Multiply(nodes[skin.joints[joint]], skin.inverseBindMatrices[joint]);
                skinned = skinned + Math::TransformPoint(m, mesh.positions[v]) * weight;
            }
            const Math::Vec3 p = Math::TransformPoint(toCamera, skinned);
            sum = sum + p;
            ++count;
            nearest = std::min(nearest, p.z);
            farthest = std::max(farthest, p.z);
            if (std::abs(p.y) > p.z * tanHalf) ++outside;
        }
    }
    REQUIRE(count > 0);
    const Math::Vec3 centre = sum * (1.0F / static_cast<float>(count));
    INFO("centre " << centre.x << " " << centre.y << " " << centre.z << ", z " << nearest << " to " << farthest);
    // Ahead of the near plane and within the far one; the weight of it right of and below the view's middle.
    CHECK(nearest > viewmodel.nearClip);
    CHECK(farthest < viewmodel.farClip);
    CHECK(centre.x > 0.0F);
    CHECK(centre.y < 0.0F);
    // Most of it in the view's height (the arms may leave the bottom edge).
    CHECK(outside < count / 4);
}

TEST_CASE("client: a hit shows where it came from: the arc toward the attacker, the HP in red, the camera shaken") {
    Setup setup;
    setup.Loaded();
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    const K::EntityId rival = PlayerOf(GameWorld(*setup.match), 2);
    REQUIRE(rival != 0);
    // The Match records a hit on the own player by the other one (as JudgeShots does).
    {
        K::World& match = GameWorld(*setup.match);
        auto hits = match.GetMut<PW::HitRecord>(own);
        hits->tick = TickOf(match);
        ++hits->hits;
        hits->attacker = rival;
        hits->part = object_fps_pvp::sim::HitPart::Body;
        match.GetMut<PW::PlayerCombat>(own)->hp -= 25'000;
    }
    const auto arc = [&setup]() -> const R::Shape2d* {
        // The only HUD shape of dots: four corners each (ui/hit_feedback.json: seven dots).
        for (const K::EntityId id : setup.Client().Entities()) {
            const auto* shape = setup.Client().Get<R::Shape2d>(id);
            if (shape != nullptr && setup.Client().Get<R::Sprite>(id) == nullptr && shape->vertices.size() == 7 * 4) return shape;
        }
        return nullptr;
    };
    const auto hpRed = [&setup] {
        for (const K::EntityId id : setup.Client().Entities()) {
            const auto* sprite = setup.Client().Get<R::Sprite>(id);
            if (sprite != nullptr && sprite->color.r == 255 && sprite->color.g == 77 && sprite->color.b == 64) return true;
        }
        return false;
    };
    // The camera's right axis tips out of the horizontal only when it rolls.
    const auto cameraRoll = [&setup] {
        for (const K::EntityId id : setup.Client().Entities()) {
            if (setup.Client().Get<GYO::Engine::Camera::Camera3d>(id) == nullptr) continue;
            return std::abs(GYO::Engine::Math::Rotate(setup.Client().Get<T::Transform>(id)->rotation, {1.0F, 0.0F, 0.0F}).y);
        }
        return 0.0F;
    };
    CHECK(arc() == nullptr);
    CHECK_FALSE(hpRed());
    CHECK(cameraRoll() < 1e-5F);
    setup.Frames(4);
    REQUIRE(setup.Client().Get<PW::HitRecord>(own)->hits == 1);
    const R::Shape2d* shown = arc();
    REQUIRE_MESSAGE(shown != nullptr, "[pvp-hit-direction] the arc of dots is drawn");
    // Its middle points from the crosshair toward the attacker, relative to where the player looks.
    GYO::Engine::Math::Vec2 middle{};
    for (const auto& vertex : shown->vertices) middle = middle + vertex.position;
    const GYO::Engine::Math::Vec3 to = setup.Client().Get<PW::PlayerBody>(rival)->feet - setup.Client().Get<PW::PlayerBody>(own)->feet;
    const float bearing = GYO::Engine::Math::WrapRadians(std::atan2(to.x, to.z) - setup.Client().Get<PW::PlayerBody>(own)->yaw);
    CHECK(GYO::Engine::Math::WrapRadians(std::atan2(middle.x, middle.y) - bearing) == doctest::Approx(0.0F).epsilon(0.05));
    CHECK(shown->vertices.front().color.a > 0);
    CHECK_MESSAGE(hpRed(), "the HP turns red");
    CHECK_MESSAGE(cameraRoll() > 1e-3F, "the camera rolls with the hit");
    // A second and a half later all of it has faded.
    setup.Frames(96);
    CHECK(arc() == nullptr);
    CHECK_FALSE(hpRed());
    CHECK(cameraRoll() < 1e-5F);
}

TEST_CASE("client: a Match that refuses (no seat) ends the connection: back to the lobby, which says why") {
    Setup setup(1, 0);
    const K::World* render = &setup.client->World("Render").GetWorld();
    for (int i = 0; i < 120 && setup.Scene().scene == PC::Scene::MultiGame; ++i) setup.Frame();
    REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::Lobby, "[pvp-session-end] the game scene went back to the lobby");
    CHECK_MESSAGE(setup.Scene().status.find("断られました") != std::string::npos, setup.Scene().status);
    CHECK_MESSAGE(setup.Scene().status.find("the match is full") != std::string::npos, setup.Scene().status);
    CHECK_MESSAGE(setup.Notes().endReason.find("the match is full") != std::string::npos,
                  "[pvp-notes-carried] the reason is noted for the program, carried into the lobby's Frame World");
    CHECK_FALSE_MESSAGE(setup.Client().Resource<W::WindowRequest>()->close, "[pvp-scene-window-stays] the window stays open");
    CHECK_MESSAGE(&setup.client->World("Render").GetWorld() == render, "[pvp-scene-render-stays] the Render World was not replaced");
    // The lobby's Sim World plays nothing and connects to nothing.
    CHECK(!HasResource<N::NetConnectTarget>(setup.Sim()));
    CHECK(!HasResource<PW::GameRules>(setup.Sim()));
    setup.Load();
    CHECK_MESSAGE(setup.Binding("status").find("the match is full") != std::string::npos, "[pvp-lobby-reason] " << setup.Binding("status"));
}

namespace {

// The Client starting in the lobby (the Match and the other Client run beside it, unused).
PC::SceneChange LobbyFirst(std::string status = {}) { return PC::SceneChange{.scene = PC::Scene::Lobby, .status = std::move(status)}; }

} // namespace

// Portable mode (the user's decision of 2026-11-26, after Godot's self-contained mode): a file named "portable" beside
// the program puts the settings and the profiles there; without it, the user's data directory.
TEST_CASE("client: portable mode: the marker beside the program puts the files there; without it, the user's data directory") {
    std::set<std::string> files;
    const auto exists = [&files](const std::string& path) { return files.count(path) != 0; };
    const std::optional<std::string> user = std::string("/home/a/.local/share/GYO/object_fps_pvp");
    // No marker: the user's data directory.
    auto place = PC::ChooseDataPlace(std::string("/opt/gyo/bin/object_fps_pvp"), exists, user);
    REQUIRE(place.has_value());
    CHECK_FALSE(place->portable);
    CHECK_MESSAGE(place->directory == *user, "[pvp-portable] without the marker the user's data directory");
    // The marker beside the program.
    files.insert("/opt/gyo/bin/portable");
    place = PC::ChooseDataPlace(std::string("/opt/gyo/bin/object_fps_pvp"), exists, user);
    REQUIRE(place.has_value());
    CHECK_MESSAGE(place->portable, "[pvp-portable] the marker turns portable mode on");
    CHECK_MESSAGE(place->directory == "/opt/gyo/bin", "[pvp-portable] the files beside the program");
    // A marker somewhere else does not count.
    files = {"/opt/gyo/portable", "/opt/gyo/bin/object_fps_pvp/portable"};
    CHECK_FALSE(PC::ChooseDataPlace(std::string("/opt/gyo/bin/object_fps_pvp"), exists, user)->portable);
    // Windows' separators.
    CHECK(PC::PortableDirectory("C:\\Games\\GYO\\object_fps_pvp.exe") == "C:\\Games\\GYO");
    // A macOS app bundle: the directory that holds the .app (the bundle is read-only).
    CHECK_MESSAGE(PC::PortableDirectory("/Users/a/Games/GYO.app/Contents/MacOS/object_fps_pvp") == "/Users/a/Games",
                  "[pvp-portable-bundle] beside the .app");
    files = {"/Users/a/Games/portable"};
    place = PC::ChooseDataPlace(std::string("/Users/a/Games/GYO.app/Contents/MacOS/object_fps_pvp"), exists, user);
    REQUIRE(place.has_value());
    CHECK_MESSAGE((place->portable && place->directory == "/Users/a/Games"), "[pvp-portable-bundle] the marker beside the .app");
    // Not a bundle: a MacOS directory without Contents in an .app is the program's own directory.
    CHECK(PC::PortableDirectory("/Users/a/MacOS/object_fps_pvp") == "/Users/a/MacOS");
    CHECK(PC::PortableDirectory("/Users/a/Tools/Contents/MacOS/object_fps_pvp") == "/Users/a/Tools/Contents/MacOS");
    // The program's path not known: no portable mode; no user data directory either: nowhere.
    CHECK_FALSE(PC::ChooseDataPlace(std::nullopt, exists, user)->portable);
    CHECK_FALSE(PC::ChooseDataPlace(std::nullopt, exists, std::nullopt).has_value());
    files = {"/opt/gyo/bin/portable"};
    CHECK(PC::ChooseDataPlace(std::string("/opt/gyo/bin/object_fps_pvp"), exists, std::nullopt)->portable);
}

// The start screen (playtest 2026-11-26, item 6): the logo on black, then the menu after its seconds, or at once on a key.
TEST_CASE("client: the start screen shows the logo document, then the menu after its seconds or at once on a key") {
    for (const bool key : {false, true}) {
        CAPTURE(key);
        Setup setup(1, 4, 0, PC::SceneChange{.scene = PC::Scene::Splash}, {}, std::nullopt, {}, 0,
                    [key](PC::ClientStart& start) { start.splashSeconds = key ? 60.0 : 0.5; });
        setup.Load();
        REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::Splash, "[pvp-splash-time] the start screen stays once laid out");
        const auto* root = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
        REQUIRE(root != nullptr);
        CHECK(root->canvas == "splash");
        const auto* state = setup.Client().Get<GYO::Engine::Ui::UiRootState>(setup.Root());
        REQUIRE(state != nullptr);
        CHECK_MESSAGE((state->runtime && !state->failure), "[pvp-splash] the logo document is laid out");
        CHECK(setup.Notes().failure.empty());
        if (key) {
            // Long seconds: still the start screen a while later; a key goes on.
            setup.Frames(30);
            CHECK_MESSAGE(setup.Scene().scene == PC::Scene::Splash, "[pvp-splash-time] the start screen waits its seconds");
            setup.Press(I::KeyCode::Enter);
            setup.Frames(2);
            CHECK_MESSAGE(setup.Scene().scene == PC::Scene::Menu, "[pvp-splash-skip] a key goes on to the menu");
        } else {
            // Half a second after it was laid out (the Frame World's time moves 1/64 s a frame here): not before, then the menu.
            setup.Frames(24);
            CHECK_MESSAGE(setup.Scene().scene == PC::Scene::Splash, "[pvp-splash-time] the start screen waits its seconds");
            setup.Frames(16);
            CHECK_MESSAGE(setup.Scene().scene == PC::Scene::Menu, "[pvp-splash-time] after its seconds the menu");
        }
        setup.Load();
        CHECK(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "menu");
    }
}

TEST_CASE("client: the menu goes to the lobby and back; the Render World stays and is told of each new Frame World") {
    Setup setup(1, 4, 0, PC::SceneChange{});
    setup.Load();
    REQUIRE(setup.Scene().scene == PC::Scene::Menu);
    CHECK(setup.Binding("status") == "モードを選んでください");
    CHECK_MESSAGE(!HasResource<PW::GameRules>(setup.Sim()), "[pvp-menu-sim-empty] the menu's Sim World plays nothing");
    CHECK_MESSAGE(setup.client->Buffer().Find<N::NetService>()->Client() == nullptr, "[pvp-menu-no-connection]");
    const K::World* render = &setup.client->World("Render").GetWorld();
    const std::uint64_t swaps = render->Resource<O::ExtractSourceSwaps>()->count;
    // Multiplayer (the second button), Enter: the lobby.
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    I::KeyboardInput back = Key(I::KeyCode::Tab);
    back.modifiers.Add(I::KeyModifier::Shift);
    REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::Lobby, "[pvp-menu-multi]");
    setup.Load();
    setup.Frames(1);
    CHECK_MESSAGE((lastPicture.passes2d >= 1 && lastPicture.draws2d >= 1), "[pvp-lobby-drawn] the render World draws the lobby's UI pass");
    CHECK_MESSAGE(&setup.client->World("Render").GetWorld() == render, "[pvp-scene-render-stays]");
    CHECK_MESSAGE(render->Resource<O::ExtractSourceSwaps>()->count == swaps + 1, "[pvp-scene-swaps] the Render World was told");
    setup.Load();
    // The lobby's Back (the last of its focus order: Shift+Tab from the first, Enter): the menu again.
    setup.Next().keys.push_back(back);
    setup.Frames(1);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::Menu, "[pvp-lobby-back]");
    setup.Load();
    CHECK(setup.Binding("status") == "モードを選んでください");
    CHECK_FALSE(setup.Client().Resource<W::WindowRequest>()->close);
}

TEST_CASE("client: the menu's Quit closes the window; a reason given to the lobby is its first status") {
    {
        Setup setup(1, 4, 0, PC::SceneChange{});
        setup.Load();
        for (int i = 0; i < 3; ++i) setup.Press(I::KeyCode::Tab);
        setup.Press(I::KeyCode::Enter);
        CHECK_MESSAGE(setup.Client().Resource<W::WindowRequest>()->close, "[pvp-menu-quit] the window closes");
    }
    Setup setup(1, 4, 0, LobbyFirst("Match に断られました（the match is full）"));
    setup.Load();
    CHECK(setup.Binding("status") == "Match に断られました（the match is full）");
}


namespace {

// A directory of its own under the system's temporary directory, removed afterwards.
struct TempDirectory final {
    std::filesystem::path path;
    TempDirectory() {
        std::random_device device;
        path = std::filesystem::temp_directory_path() / ("gyo-pvp-solo-" + std::to_string(device()));
        std::filesystem::remove_all(path);
    }
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    [[nodiscard]] std::string Utf8() const { return path.generic_string(); }
};

} // namespace

TEST_CASE("client: single player: the room, a game without a server, the pause, the evacuation settles the save") {
    TempDirectory directory;
    Setup setup(1, 4, 0, PC::SceneChange{}, directory.Utf8());
    const auto noSocket = [&setup] { return !setup.client->Buffer().Find<N::NetService>()->UdpAddress().has_value(); };
    setup.Load();
    // The menu's first button: single player.
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::SoloRoom, "[pvp-menu-solo]");
    setup.Load();
    CHECK_MESSAGE(setup.Binding("record") == "記録はまだありません", setup.Binding("record"));
    bool socketless = noSocket();
    for (int game = 1; game <= 2; ++game) {
        // The room's first button: the game.
        setup.Press(I::KeyCode::Enter);
        REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::SoloGame, "[pvp-solo-start]");
        CHECK_MESSAGE(HasResource<PW::GameRules>(setup.Solo()), "[pvp-solo-world] the Solo World plays");
        CHECK_MESSAGE(!HasResource<PW::GameRules>(setup.Sim()), "the Sim World does not");
        // The map is read on the asset threads: wait as Setup::Loaded does (a few seconds at most), not a count of frames
        // (L1 linux-x64 2026-11-27, with the client tests in four parallel entries: 60 frames were not enough).
        for (int i = 0; i < 2000 && !PC::StatusOf(setup.Client()).playing; ++i) {
            setup.Frame();
            socketless = socketless && noSocket();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE_MESSAGE(PC::StatusOf(setup.Client()).playing, "[pvp-solo-playing] the own player reached the Frame World");
        // The loading screen until the game is loaded and drawn; then the Local World starts.
        CHECK_MESSAGE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "loading", "[pvp-loading-shown]");
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).started; }), "[pvp-solo-started]");
        CHECK(PC::StatusOf(setup.Client()).ready);
        setup.Frames(3);  // the loading page leaves (a request, applied next frame)
        const K::EntityId own = PW::OwnPlayer(setup.Solo());
        REQUIRE(own != 0);
        if (game == 1) {
            // Take the pointer and walk forward: the Solo World moves the player.
            const GYO::Engine::Math::Vec3 start = setup.Solo().Get<PW::PlayerBody>(own)->feet;
    I::MouseButtonInput click;
            click.button = I::MouseButton::Left;
            click.state = I::ButtonState::Pressed;
            click.position = {640.0F, 360.0F};
            setup.Next().buttons.push_back(click);
            setup.Frame();
            click.state = I::ButtonState::Released;
            setup.Next().buttons.push_back(click);
            setup.Frame();
            REQUIRE(PC::StatusOf(setup.Client()).captured);
            setup.Next().keys.push_back(Key(I::KeyCode::W));
            setup.Frames(40);
            CHECK_MESSAGE(GYO::Engine::Math::Length(setup.Solo().Get<PW::PlayerBody>(own)->feet - start) > 0.5F, "[pvp-solo-walks]");
            setup.Next().keys.push_back(Key(I::KeyCode::W, I::ButtonState::Released));
            // Killed once (as a shot leaves it: to respawn some steps later): it respawns, and the game will settle one death.
            PW::PlayerCombat dead = *setup.Solo().Get<PW::PlayerCombat>(own);
            dead.hp = 0;
            dead.life = object_fps_pvp::sim::LifeState::Dead;
            dead.respawnTick = TickOf(setup.Solo()) + 10;
            setup.Solo().Insert(own, dead);
            setup.Frames(16);
            CHECK(setup.Solo().Get<PW::PlayerCombat>(own)->life == object_fps_pvp::sim::LifeState::Alive);
        }
        // Give the pointer back: the pause canvas, and the Solo World's time stops.
        setup.Press(I::KeyCode::Escape);
        setup.Frames(2);
        REQUIRE_MESSAGE(setup.Root() != 0, "[pvp-pause-shown] the pause page's root is there");
        const auto* overlay = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
        CHECK_MESSAGE(overlay->canvas == "pause", "[pvp-pause-shown] " << overlay->canvas);
        const std::uint64_t pausedAt = TickOf(setup.Solo());
        setup.Frames(10);
        CHECK_MESSAGE(TickOf(setup.Solo()) == pausedAt, "[pvp-pause-stops] the single player's game stands still");
        for (int i = 0; i < 400; ++i) {
            const auto* state = setup.Client().Get<GYO::Engine::Ui::UiRootState>(setup.Root());
            if (state != nullptr && state->runtime) break;
            setup.Frames(1);
        }
        // Enter on the focused button (撤退する) evacuates.
        setup.Press(I::KeyCode::Enter);
        REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::Menu, "[pvp-solo-evacuate] back to the menu: " << setup.Binding("status"));
        CHECK_MESSAGE(setup.Scene().status.find("記録しました") != std::string::npos, setup.Scene().status);
        CHECK_MESSAGE(!HasResource<PW::GameRules>(setup.Solo()), "the menu's Solo World plays nothing");
        const auto save = PC::ReadSoloSave(PC::SoloSavePath(directory.Utf8()));
        REQUIRE_MESSAGE((save && save->has_value()), "[pvp-solo-saved]");
        CHECK_MESSAGE((*save)->matches == static_cast<std::uint64_t>(game), "[pvp-solo-games] " << (*save)->matches);
        CHECK_MESSAGE((*save)->deaths == 1, "[pvp-solo-deaths] the first game's death: " << (*save)->deaths);
        // The room reads it.
        setup.Load();
        setup.Press(I::KeyCode::Enter);
        REQUIRE(setup.Scene().scene == PC::Scene::SoloRoom);
        setup.Load();
        CHECK_MESSAGE(setup.Binding("record") == "出撃 " + std::to_string(game) + " 回・死亡 1 回", "[pvp-solo-record] " << setup.Binding("record"));
    }
    CHECK_MESSAGE(socketless, "[pvp-solo-no-socket] no UDP socket was opened throughout");
    CHECK_MESSAGE(!std::filesystem::exists(directory.path / "device-id"), "[pvp-solo-no-device-id] single player makes no Gateway device id");
    CHECK_MESSAGE(setup.Notes().failure.empty(), "[pvp-solo-documents] every scene's document was shown: " << setup.Notes().failure);
}

TEST_CASE("client: leaving the game closes the connection; a new game connects again and sees nothing of the last") {
    Setup setup;
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).playing; }));
    REQUIRE(PlayerOf(GameWorld(*setup.match), 1) != 0);
    // To the lobby (as its scene's System would ask): the lobby's Sim World has no target, so the session closes.
    setup.Client().ResourceMut<PC::SceneRequest>()->next = PC::SceneChange{.scene = PC::Scene::Lobby};
    setup.Frame();
    REQUIRE(setup.Scene().scene == PC::Scene::Lobby);
    setup.Frames(20);
    CHECK_MESSAGE(setup.client->Buffer().Find<N::NetService>()->Client() == nullptr, "[pvp-scene-leave-closes] no session");
    CHECK_MESSAGE(PlayerOf(GameWorld(*setup.match), 1) == 0, "[pvp-scene-leave-match] the Match took the player off");
    // The game again. While the Sim World is held elsewhere (lent, as on its own thread), the new Frame World's first
    // frame sees no picture of the last game.
    setup.Client().ResourceMut<PC::SceneRequest>()->next = GameScene();
    setup.Frame();
    REQUIRE(setup.Scene().scene == PC::Scene::MultiGame);
    {
        auto lent = setup.client->Lend("Sim");
        REQUIRE(static_cast<bool>(lent));
        REQUIRE(static_cast<bool>(setup.client->RunFrame()));
        setup.client->Return(std::move(*lent));
    }
    CHECK_MESSAGE(!PC::StatusOf(setup.Client()).playing, "[pvp-scene-exchange-reset] the last game's own player is not shown");
    CHECK_MESSAGE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).playing; }), "[pvp-scene-reconnects] the new game is played");
    CHECK(setup.Scene().scene == PC::Scene::MultiGame);
}

namespace {

// A Gateway that speaks the external protocol v1 (gateway-client-handoff section 2) for one user, on a loopback
// listener (tests/common/os): every request it is sent, in order; a room made by the user, readied and started.
class FakeGateway final {
public:
    struct Seen final {
        std::string method;
        std::string target;
        std::string authorization;
        std::string key;
        std::string body;
    };

    FakeGateway() {
        REQUIRE(listener_.Ok());
        thread_ = std::thread([this] { Serve(); });
    }
    ~FakeGateway() {
        stop_ = true;
        thread_.join();
    }
    [[nodiscard]] GYO::Engine::OS::SocketAddress Address() const { return listener_.Address(); }
    [[nodiscard]] std::vector<Seen> Requests() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return seen_;
    }
    // The open room of another user the lobby lists (and the user can join).
    void OfferRoom(const std::uint64_t id) {
        const std::lock_guard<std::mutex> lock(mutex_);
        other_ = id;
    }
    // Calls of method to target are held (not answered) until released: a call in flight while the player acts.
    // Held() counts the calls that came while held.
    void Hold(std::string method, std::string target) {
        const std::lock_guard<std::mutex> lock(mutex_);
        held_ = {std::move(method), std::move(target)};
    }
    void Release() {
        const std::lock_guard<std::mutex> lock(mutex_);
        held_.reset();
    }
    [[nodiscard]] int Held() const { return heldCount_; }
    // Reads of room 3 answer 503 busy (the Gateway's trouble) while on.
    void FailRoomReads(const bool fail) { failRoomReads_ = fail; }
    // A started room 3 gives a connect whose server is not an address.
    void BadConnect(const bool bad) { badConnect_ = bad; }
    // The room's match has ended: reads of the room answer 410 match_ended.
    void EndMatch() {
        const std::lock_guard<std::mutex> lock(mutex_);
        ended_ = true;
    }
    // Room 3's member (user 7) is named this in its answers (empty: no name field, as before names).
    void NameMember(std::string name) {
        const std::lock_guard<std::mutex> lock(mutex_);
        memberName_ = std::move(name);
    }
    // The next read of the rooms answers 401 session_expired (once).
    void ExpireSession() {
        const std::lock_guard<std::mutex> lock(mutex_);
        expire_ = true;
    }

private:
    static std::string Header(const std::string& request, const std::string& name) {
        std::string lower = request;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const std::string key = "\r\n" + name + ":";
        const std::size_t at = lower.find(key);
        if (at == std::string::npos) return {};
        std::size_t start = at + key.size();
        while (start < request.size() && request[start] == ' ') ++start;
        return request.substr(start, request.find("\r\n", start) - start);
    }
    static std::string Answer(const int status, const std::string& body) {
        return "HTTP/1.1 " + std::to_string(status) + " X\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
               "\r\n\r\n" + body;
    }
    std::string RoomJson(const bool withConnect) const {
        std::string json = "{\"id\":3,\"mode\":\"" + mode_ + "\",\"map\":\"" + map_ + "\",\"players\":1,\"capacity\":4,\"state\":\"" +
                           (started_ ? "started" : "open") + "\",\"owner\":7,\"members\":[{\"userId\":7," +
                           (memberName_.empty() ? std::string() : "\"name\":\"" + memberName_ + "\",") + "\"ready\":" + (ready_ ? "true" : "false") + "}]";
        if (withConnect && started_) {
            json += std::string(",\"connect\":{\"server\":\"") + (badConnect_ ? "not-an-address" : "127.0.0.1:47000") + "\",\"userId\":7,\"token\":\"00ff\"}";
        }
        return json + "}";
    }
    std::pair<int, std::string> Route(const std::string& method, const std::string& target, const std::string& body) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (method == "POST" && target == "/v1/session") return {200, "{\"userId\":7,\"sessionToken\":\"t" + std::to_string(++sessions_) + "\",\"expiresAt\":99}"};
        if (method == "GET" && target == "/v1/rooms") {
            if (std::exchange(expire_, false)) return {401, "{\"error\":\"session_expired\"}"};
            std::string rooms;
            if (other_ != 0) rooms = "{\"id\":" + std::to_string(other_) + ",\"mode\":\"core_pvp\",\"map\":\"pvp_corners_v1\",\"players\":2,\"capacity\":4,\"state\":\"open\"}";
            return {200, "{\"rooms\":[" + rooms + "]}"};
        }
        if (method == "GET" && target == "/v1/saves/me") {
            return {200, "{\"userId\":7,\"version\":2,\"body\":{\"saveVersion\":1,\"matches\":3,\"deaths\":2,\"lastMatch\":9,\"lastWrite\":\"k\"}}"};
        }
        if (method == "POST" && target == "/v1/rooms") {
            const auto field = [&body](const std::string& name) {
                const std::size_t at = body.find("\"" + name + "\":\"");
                if (at == std::string::npos) return std::string();
                const std::size_t start = at + name.size() + 4;
                return body.substr(start, body.find('"', start) - start);
            };
            mode_ = field("mode");
            map_ = field("map");
            return {200, RoomJson(false)};
        }
        if (method == "POST" && target == "/v1/rooms/" + std::to_string(other_) + "/join" && other_ != 0) {
            return {200, "{\"id\":" + std::to_string(other_) + ",\"mode\":\"core_pvp\",\"map\":\"pvp_corners_v1\",\"players\":3,\"capacity\":4,\"state\":\"open\",\"owner\":9,\"members\":[]}"};
        }
        if (target == "/v1/rooms/" + std::to_string(other_) && other_ != 0 && method == "GET") {
            return {200, "{\"id\":" + std::to_string(other_) + ",\"mode\":\"core_pvp\",\"map\":\"pvp_corners_v1\",\"players\":3,\"capacity\":4,\"state\":\"open\",\"owner\":9,\"members\":[{\"userId\":9,\"ready\":true},{\"userId\":7,\"ready\":false}]}"};
        }
        if (method == "GET" && target == "/v1/rooms/3") {
            if (failRoomReads_) return {503, "{\"error\":\"busy\"}"};
            if (ended_) return {410, "{\"error\":\"match_ended\"}"};
            return {200, RoomJson(true)};
        }
        if (method == "POST" && target == "/v1/rooms/3/ready") {
            ready_ = body.find("true") != std::string::npos;
            return {200, RoomJson(false)};
        }
        if (method == "POST" && target == "/v1/rooms/3/start") {
            if (!ready_) return {409, "{\"error\":\"not_all_ready\"}"};
            started_ = true;
            return {200, RoomJson(true)};
        }
        if (method == "POST" && target.ends_with("/leave")) return {200, "{\"left\":true}"};
        return {404, "{\"error\":\"unknown_route\"}"};
    }
    void Serve() {
        while (!stop_) {
            if (!listener_.Accept(std::chrono::milliseconds(50))) continue;
            const auto request = listener_.ReceiveRequest();
            if (!request) {
                listener_.CloseAccepted();
                continue;
            }
            const std::size_t space = request->find(' ');
            const std::size_t space2 = request->find(' ', space + 1);
            const std::string method = request->substr(0, space);
            const std::string target = request->substr(space + 1, space2 - space - 1);
            const std::size_t bodyAt = request->find("\r\n\r\n");
            const std::string body = bodyAt == std::string::npos ? std::string() : request->substr(bodyAt + 4);
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                seen_.push_back({method, target, Header(*request, "authorization"), Header(*request, "idempotency-key"), body});
            }
            const auto holding = [&] {
                const std::lock_guard<std::mutex> lock(mutex_);
                return held_ && held_->first == method && held_->second == target;
            };
            if (holding()) {
                ++heldCount_;
                while (!stop_ && holding()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            const auto [status, answer] = Route(method, target, body);
            (void)listener_.Send(Answer(status, answer));
            listener_.CloseAccepted();
        }
    }

    GYO::Test::LoopbackListener listener_;
    std::atomic<bool> stop_{false};
    std::optional<std::pair<std::string, std::string>> held_;
    std::atomic<int> heldCount_{0};
    std::atomic<bool> failRoomReads_{false};
    std::atomic<bool> badConnect_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::vector<Seen> seen_;
    std::uint64_t other_ = 0;
    bool expire_ = false;
    bool ended_ = false;
    int sessions_ = 0;
    std::string mode_ = "core_pvp";
    std::string memberName_;
    std::string map_;
    bool ready_ = false;
    bool started_ = false;
};

bool Asked(FakeGateway& gateway, const std::string& method, const std::string& target) {
    for (const FakeGateway::Seen& seen : gateway.Requests()) {
        if (seen.method == method && seen.target == target) return true;
    }
    return false;
}

std::string ListLine(Setup& setup, const std::size_t row, const std::string_view id = "rooms") {
    const auto* bindings = setup.Client().Get<GYO::Engine::Ui::UiBindings>(setup.Root());
    if (bindings == nullptr) return {};
    const auto* value = bindings->Find(id);
    const auto* list = value != nullptr ? std::get_if<GYO::Engine::Ui::UiList>(value) : nullptr;
    if (list == nullptr || row >= list->items.size()) return {};
    const auto found = list->items[row].fields.find("line");
    const auto* text = found != list->items[row].fields.end() ? std::get_if<std::string>(&found->second) : nullptr;
    return text != nullptr ? *text : std::string();
}

} // namespace

TEST_CASE("client: gateway: the requests carry the session's token, an idempotency key on each change, and the answers read") {
    std::optional<PC::GatewaySession> session = PC::GatewaySession{7, "tok", 99};
    const auto create = PC::ToHttp(PC::CreateRoomRequest("core_pvp", "pvp_training_v1", "k1"), "127.0.0.1:7700", session);
    CHECK(create.method == "POST");
    CHECK(create.target == "/v1/rooms");
    CHECK(create.host == "127.0.0.1:7700");
    CHECK(create.body == "{\"mode\":\"core_pvp\",\"map\":\"pvp_training_v1\"}");
    bool bearer = false, key = false;
    for (const auto& header : create.headers) {
        bearer = bearer || (header.name == "Authorization" && header.value == "Bearer tok");
        key = key || (header.name == "Idempotency-Key" && header.value == "k1");
    }
    CHECK_MESSAGE(bearer, "[pvp-gw-bearer]");
    CHECK_MESSAGE(key, "[pvp-gw-key]");
    const auto list = PC::ToHttp(PC::ListRoomsRequest(), "127.0.0.1:7700", session);
    for (const auto& header : list.headers) CHECK(header.name != "Idempotency-Key");
    CHECK(PC::ReadyRequest(3, false, "k").body == "{\"ready\":false}");
    CHECK(PC::ReadyRequest(3, true, "k").target == "/v1/rooms/3/ready");

    std::string error;
    const auto room = PC::ParseRoom("{\"id\":3,\"mode\":\"core_pvp\",\"map\":\"m\",\"players\":1,\"capacity\":4,\"state\":\"started\",\"owner\":7,"
                                    "\"members\":[{\"userId\":7,\"ready\":true}],\"connect\":{\"server\":\"1.2.3.4:5\",\"userId\":7,\"token\":\"ab\"}}",
                                    error);
    REQUIRE_MESSAGE(room.has_value(), error);
    CHECK(room->state == "started");
    REQUIRE(room->connect.has_value());
    CHECK(room->connect->server == "1.2.3.4:5");
    CHECK(room->members.size() == 1);
    CHECK_FALSE(PC::ParseRoom("{\"id\":\"x\"}", error).has_value());
    const auto save = PC::ParseSave("{\"userId\":7,\"version\":2,\"body\":null}", error);
    REQUIRE(save.has_value());
    CHECK(save->body == "null");
    CHECK(PC::ErrorCodeOf("{\"error\":\"room_full\"}") == "room_full");
    CHECK(PC::RandomHex(16).size() == 32);
}

TEST_CASE("client: name: the settings' name, or the host name's first label; the settings keep it; an old file has none") {
    int asked = 0;
    const auto host = [&asked](std::optional<std::string> name) {
        return [&asked, name] {
            ++asked;
            return name;
        };
    };
    PC::ClientSettings settings;
    CHECK_MESSAGE(PC::PlayerNameCandidate(settings, host("Kuro-MacBook-Pro.local")) == "Kuro-MacBook-Pro", "[pvp-name-first-label]");
    CHECK(PC::PlayerNameCandidate(settings, host("kuro-pc.example.com")) == "kuro-pc");
    CHECK(PC::PlayerNameCandidate(settings, host("desktop-7")) == "desktop-7");
    CHECK(PC::PlayerNameCandidate(settings, host(".local")).empty());
    CHECK_MESSAGE(PC::PlayerNameCandidate(settings, host(std::nullopt)).empty(), "[pvp-name-no-host]");
    CHECK(asked == 5);
    settings.name = "甲 A";
    CHECK_MESSAGE(PC::PlayerNameCandidate(settings, host("host.local")) == "甲 A", "[pvp-name-settings]");
    settings.name = "";
    CHECK_MESSAGE(PC::PlayerNameCandidate(settings, host("host.local")).empty(), "[pvp-name-empty-chosen]");
    CHECK_MESSAGE(asked == 5, "[pvp-name-host-not-asked] the host name is asked although the settings have a name");
    // Cut at a UTF-8 boundary: 86 three-byte characters are 258 bytes, 85 of them fit in 256.
    std::string long_;
    for (int i = 0; i < 86; ++i) long_ += "名";
    settings.name = long_;
    const std::string cut = PC::PlayerNameCandidate(settings, host(std::nullopt));
    CHECK_MESSAGE(cut.size() == 255, "[pvp-name-cut] " << cut.size());
    CHECK(cut == long_.substr(0, 255));

    std::string error;
    for (const std::optional<std::string>& name : {std::optional<std::string>{}, std::optional<std::string>{""}, std::optional<std::string>{"名前 \"q\""}}) {
        PC::ClientSettings chosen;
        chosen.name = name;
        const auto decoded = PC::DecodeSettings(PC::EncodeSettings(chosen), error);
        REQUIRE_MESSAGE(decoded.has_value(), error);
        CHECK_MESSAGE(decoded->name == name, "[pvp-settings-name] " << PC::EncodeSettings(chosen));
    }
    const auto old = PC::DecodeSettings("{\"version\":1,\"resolution\":[1280,720],\"windowMode\":\"windowed\",\"vsync\":true}", error);
    REQUIRE_MESSAGE(old.has_value(), error);
    CHECK_FALSE(old->name.has_value());
    CHECK_FALSE(PC::DecodeSettings("{\"version\":1,\"resolution\":[1280,720],\"windowMode\":\"windowed\",\"vsync\":true,\"name\":7}", error).has_value());
}

TEST_CASE("client: name: the session is opened with the name; the room's names are read; a player without one is Player N") {
    CHECK_MESSAGE(PC::SessionRequest({"d", "名前"}).body == "{\"deviceId\":\"d\",\"name\":\"名前\"}", "[pvp-gw-session-name]");
    std::string error;
    // The identity service's answer has the name too; the Client does not need it (the room and the Match give it).
    CHECK(PC::ParseSession("{\"userId\":7,\"name\":\"甲\",\"sessionToken\":\"t\",\"expiresAt\":9}", error).has_value());
    const auto room = PC::ParseRoom("{\"id\":3,\"mode\":\"core_pvp\",\"map\":\"m\",\"players\":2,\"capacity\":4,\"state\":\"open\",\"owner\":7,"
                                    "\"members\":[{\"userId\":7,\"name\":\"甲\",\"ready\":true},{\"userId\":9,\"ready\":false}]}",
                                    error);
    REQUIRE_MESSAGE(room.has_value(), error);
    REQUIRE(room->members.size() == 2);
    CHECK_MESSAGE(room->members[0].name == "甲", "[pvp-gw-member-name]");
    CHECK(room->members[1].name.empty());
    CHECK_FALSE(PC::ParseRoom("{\"id\":3,\"mode\":\"c\",\"map\":\"m\",\"players\":1,\"capacity\":4,\"state\":\"open\",\"owner\":7,"
                              "\"members\":[{\"userId\":7,\"name\":1,\"ready\":true}]}",
                              error)
                     .has_value());
    CHECK_MESSAGE(PC::PlayerLabel(12, "") == "Player 12", "[pvp-label-fallback]");
    CHECK_MESSAGE(PC::PlayerLabel(12, "甲 A") == "甲 A", "[pvp-label-name]");
}

TEST_CASE("client: name: the lobby signs in with the start's name; the room shows each member's name") {
    TempDirectory directory;
    FakeGateway gateway;
    gateway.NameMember("甲 A");
    Setup setup(1, 4, 0, LobbyFirst(), directory.Utf8(), gateway.Address(), {}, 0, [](PC::ClientStart& start) { start.name = "テスト 名前"; });
    setup.Load();
    REQUIRE(FramesUntil(setup, [&] { return !gateway.Requests().empty(); }));
    const FakeGateway::Seen first = gateway.Requests().front();
    REQUIRE(first.target == "/v1/session");
    CHECK_MESSAGE(first.body.find("\"name\":\"テスト 名前\"") != std::string::npos, "[pvp-gw-session-name] " << first.body);
    // Make a room (the sixth of the focus order: join, prev, next, mode, map, create).
    for (int i = 0; i < 5; ++i) setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Room; }), setup.Binding("status"));
    setup.Load();
    CHECK_MESSAGE(FramesUntil(setup, [&] { return ListLine(setup, 0, "members") == "甲 A（部屋主）（自分）  準備中"; }),
                  "[pvp-room-member-name] " << ListLine(setup, 0, "members"));
}

TEST_CASE("client: gateway: the lobby lists and records; a made room is readied and started; the game connects where the room says") {
    TempDirectory directory;
    FakeGateway gateway;
    Setup setup(1, 4, 0, LobbyFirst(), directory.Utf8(), gateway.Address());
    setup.Load();
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("record") == "戦績：出撃 3 回・死亡 2 回"; }),
                    "[pvp-gw-record] " << setup.Binding("record") << " / " << setup.Binding("notice"));
    const auto requests = gateway.Requests();
    REQUIRE_FALSE(requests.empty());
    CHECK_MESSAGE((requests.front().method == "POST" && requests.front().target == "/v1/session"), "[pvp-gw-session-first]");
    // The device id is kept in the player's directory.
    const auto device = PC::ReadLocalText(directory.Utf8() + "/device-id");
    REQUIRE((device && device->has_value()));
    CHECK((*device)->size() == 64);
    // Make a room on the second map: the map is the fifth of the focus order (join, prev, next, mode, map, create).
    for (int i = 0; i < 4; ++i) setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    const object_fps_pvp::sim::ArenaContent& picked = ShippedContent()->arenas[1];
    REQUIRE(setup.Binding("map") == "マップ: " + picked.id);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Room; }), "[pvp-gw-create] " << setup.Binding("status"));
    CHECK(setup.Scene().room == 3);
    CHECK(Asked(gateway, "POST", "/v1/rooms"));
    setup.Load();
    CHECK_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("map") == "マップ: " + picked.id; }), "[pvp-room-map] " << setup.Binding("map"));
    // The room's mode by its name: the installed core PvP, the lobby's first (mode-config declaration, M1).
    CHECK_MESSAGE(setup.Binding("mode") == "モード: 核心 PvP", "[pvp-room-mode] " << setup.Binding("mode"));
    // Ready (the first button), then start (the second): the game, connected where the started room says.
    REQUIRE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備する"; }));
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備をやめる"; }), "[pvp-gw-ready] " << setup.Binding("status"));
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::MultiGame; }), "[pvp-gw-start] " << setup.Binding("status"));
    REQUIRE(setup.Scene().connect.has_value());
    const auto* server = std::get_if<GYO::Engine::OS::SocketAddress>(&setup.Scene().connect->server);
    REQUIRE(server != nullptr);
    CHECK_MESSAGE(server->ToString() == "127.0.0.1:47000", "[pvp-gw-connect]");
    CHECK(setup.Scene().connect->userId == 7);
    CHECK(setup.Scene().connect->token == std::vector<std::byte>{std::byte{0x00}, std::byte{0xff}});
    CHECK(setup.Scene().room == 3);
    CHECK_MESSAGE(HasResource<N::NetConnectTarget>(setup.Sim()), "[pvp-scene-sim-target] the game's Sim World has the target");
    CHECK_MESSAGE(setup.Sim().Resource<PW::LocalUser>()->userId == 7, "[pvp-gw-user] the player is the Gateway's user");
    // The room's map is the arena the game starts with (the Sim World's and the Frame World's), before any Match says.
    CHECK_MESSAGE(setup.Scene().map == picked.id, "[pvp-gw-map] " << setup.Scene().map);
    CHECK_MESSAGE(setup.Scene().mode == "core_pvp", "[pvp-gw-mode] " << setup.Scene().mode);
    CHECK_MESSAGE(setup.Sim().Resource<PW::GameRules>()->arena.id == picked.id, "[pvp-gw-map-sim]");
    CHECK_MESSAGE(setup.Client().Resource<PW::GameRules>()->arena.id == picked.id, "[pvp-gw-map-frame]");
    CHECK(setup.Client().Resource<PW::LocalUser>()->userId == 7);
    // Every call after the session's carried its token; every change its own key.
    for (const FakeGateway::Seen& seen : gateway.Requests()) {
        if (seen.target == "/v1/session") continue;
        CHECK_MESSAGE(seen.authorization == "Bearer t1", "[pvp-gw-token] " << seen.method << " " << seen.target);
        if (seen.method == "POST") CHECK_MESSAGE(!seen.key.empty(), "[pvp-gw-keys] " << seen.target);
    }
    // Give up (the loading screen's 中止, the focused button: this test's Match never answers, so the game never
    // loads): the room is left and the lobby comes back.
    for (int i = 0; i < 400; ++i) {
        const auto* state = setup.Client().Get<GYO::Engine::Ui::UiRootState>(setup.Root());
        if (state != nullptr && state->runtime) break;
        setup.Frames(1);
    }
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return Asked(gateway, "POST", "/v1/rooms/3/leave"); }), "[pvp-gw-leave-sent]");
    CHECK_MESSAGE(setup.Scene().scene == PC::Scene::Lobby, "[pvp-gw-leave]");
}

TEST_CASE("client: gateway: a listed room is joined; a lost session is opened again; a Gateway nobody answers at is told") {
    {
        TempDirectory directory;
        FakeGateway gateway;
        gateway.OfferRoom(5);
        Setup setup(1, 4, 0, LobbyFirst(), directory.Utf8(), gateway.Address());
        setup.Load();
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return ListLine(setup, 0).find("#5") != std::string::npos; }), "[pvp-gw-list] " << setup.Binding("notice"));
        gateway.ExpireSession();
        // Within a few lists the session is opened again (a second POST /v1/session) and the list goes on.
        REQUIRE_MESSAGE(FramesUntil(setup, [&] {
                            int sessions = 0;
                            for (const auto& seen : gateway.Requests()) sessions += seen.target == "/v1/session" ? 1 : 0;
                            return sessions == 2;
                        }),
                        "[pvp-gw-reopen] the session is opened again");
        CHECK_MESSAGE(FramesUntil(setup, [&] {
                          const auto all = gateway.Requests();
                          return !all.empty() && all.back().target == "/v1/rooms" && all.back().authorization == "Bearer t2";
                      }),
                      "[pvp-gw-reopen] the list is asked again with the new token");
        // Join: the first button.
        setup.Press(I::KeyCode::Enter);
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Room; }), "[pvp-gw-join] " << setup.Binding("status"));
        CHECK(setup.Scene().room == 5);
        CHECK(Asked(gateway, "POST", "/v1/rooms/5/join"));
        // Not the owner (user 9 is): the start is the owner's, and pressing it says so -- the word stays through the
        // reads of the room that follow; nothing is asked of the Gateway.
        setup.Load();
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("start") == "開始（部屋主のみ）"; }), "[pvp-room-start-owner] " << setup.Binding("start"));
        setup.Press(I::KeyCode::Tab);
        setup.Press(I::KeyCode::Enter);
        CHECK_MESSAGE(setup.Binding("status") == "開始できるのは部屋主だけです", "[pvp-room-start-refused] " << setup.Binding("status"));
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1600);
        while (std::chrono::steady_clock::now() < until) {
            setup.Frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK_MESSAGE(setup.Binding("status") == "開始できるのは部屋主だけです", "[pvp-room-said-stays] " << setup.Binding("status"));
        CHECK_MESSAGE(!Asked(gateway, "POST", "/v1/rooms/5/start"), "[pvp-room-start-refused] the Gateway was asked");
    }
    TempDirectory directory;
    Setup setup(1, 4, 0, LobbyFirst(), directory.Utf8());
    setup.Load();
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("notice").find("接続できません") != std::string::npos; }, 2000),
                    "[pvp-gw-unreachable] " << setup.Binding("notice"));
}

// The whole line, run by the services' Go test TestTheRealClientPlaysThroughTheGateway (CTest
// object_fps_pvp.gateway_client, label network): the Gateway at GYO_PVP_GATEWAY with real Matches behind it, this
// Client over UDP. Without the variable the case says so and ends.
TEST_CASE("client: the loading screen covers the game until it is loaded and drawn; then the Match is told it is ready") {
    Setup setup;
    setup.Frame();  // the first frame enters the loading page
    REQUIRE_MESSAGE(setup.Root() != 0, "[pvp-loading-first] the loading page's root is there");
    const auto* overlay = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
    CHECK_MESSAGE(overlay->canvas == "loading", "[pvp-loading-first] the first frame shows nothing of the game");
    double last = 0.0;
    bool monotonic = true;
    bool sawProgress = false;
    // The drawing as the program reports it: a frame shown each frame, no draw skipped.
    std::uint64_t shown = 0;
    REQUIRE_MESSAGE(FramesUntil(setup, [&] {
                        setup.Client().SetResource(PC::DrawnFrames{true, ++shown, 0});
                        const PC::ClientStatus status = PC::StatusOf(setup.Client());
                        monotonic = monotonic && status.progress >= last;
                        sawProgress = sawProgress || (status.progress > 0.0 && status.progress < 1.0);
                        last = status.progress;
                        if (!status.ready) {
                            const auto* root = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
                            if (root == nullptr || root->canvas != "loading") return true;
                        }
                        return status.ready;
                    }),
                    "[pvp-loading-ready] the game was loaded, drawn and its own player came");
    CHECK_MESSAGE(PC::StatusOf(setup.Client()).ready, "[pvp-loading-covers] the loading screen stayed until ready");
    CHECK(monotonic);
    CHECK_MESSAGE(sawProgress, "[pvp-loading-progress] the bar moved through the loading");
    CHECK(last == 1.0);
    // The input says so: the Match's player of this Client is ready.
    REQUIRE_MESSAGE(FramesUntil(setup, [&] {
                        const K::EntityId own = PlayerOf(GameWorld(*setup.match), 1);
                        return own != 0 && GameWorld(*setup.match).Get<PW::PlayerInput>(own) != nullptr &&
                               GameWorld(*setup.match).Get<PW::PlayerInput>(own)->ready == 1;
                    }),
                    "[pvp-loading-told] the Match's player of this Client says it is ready");
    // A Match played from the first step: the screen is gone.
    setup.Frames(2);
    const auto* after = setup.Root() != 0 ? setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root()) : nullptr;
    CHECK_MESSAGE((after == nullptr || after->canvas != "loading"), "[pvp-loading-gone]");
    // The game shows when it begins, without the pause (the player's report: it paused as soon as it began, the pointer
    // being not yet the game's); its first click takes the pointer.
    setup.Frames(5);
    CHECK_FALSE(PC::StatusOf(setup.Client()).captured);
    const auto* later = setup.Root() != 0 ? setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root()) : nullptr;
    CHECK_MESSAGE((later == nullptr || later->canvas != "pause"), "[pvp-play-not-paused] no pause when the game begins");
}

TEST_CASE("client: gateway: a read of the room asked before the player got ready does not turn the button back") {
    TempDirectory directory;
    FakeGateway gateway;
    Setup setup(1, 4, 0, PC::SceneChange{.scene = PC::Scene::Room, .room = 3}, directory.Utf8(), gateway.Address());
    setup.Load();
    REQUIRE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備する" && Asked(gateway, "GET", "/v1/rooms/3"); }));
    // A read of the room goes out and is held: the player gets ready meanwhile.
    gateway.Hold("GET", "/v1/rooms/3");
    const auto begin = std::chrono::steady_clock::now();
    while (gateway.Held() == 0 && std::chrono::steady_clock::now() - begin < std::chrono::seconds(5)) {
        setup.Frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(gateway.Held() == 1);
    setup.Press(I::KeyCode::Enter);
    REQUIRE(setup.Binding("ready") == "準備をやめる");
    // The held read answers (the room as it was: not ready), then the ready call goes and answers.
    gateway.Release();
    bool turnedBack = false;
    for (int i = 0; i < 200 && !Asked(gateway, "POST", "/v1/rooms/3/ready"); ++i) {
        setup.Frame();
        turnedBack = turnedBack || setup.Binding("ready") == "準備する";
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    for (int i = 0; i < 20; ++i) {
        setup.Frame();
        turnedBack = turnedBack || setup.Binding("ready") == "準備する";
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(Asked(gateway, "POST", "/v1/rooms/3/ready"));
    CHECK_MESSAGE(!turnedBack, "[pvp-room-ready-not-undone] the read asked before the ready turned the button back");
    CHECK(setup.Binding("ready") == "準備をやめる");
}

TEST_CASE("client: gateway: two presses of ready before an answer end as the second asks; a refusal of an action stays") {
    TempDirectory directory;
    FakeGateway gateway;
    Setup setup(1, 4, 0, PC::SceneChange{.scene = PC::Scene::Room, .room = 3}, directory.Utf8(), gateway.Address());
    setup.Load();
    REQUIRE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備する" && Asked(gateway, "GET", "/v1/rooms/3"); }));
    // The room is read every 0.75 s of the Client's time (a step a frame, whatever the machine's speed): the windows
    // here count the reads the Gateway saw, not the wall clock. Reads after the request at `from` (an index of the
    // requests seen): one is asked only once the calls before it were answered and seen by the room.
    const auto readsAfter = [&gateway](const std::size_t from) {
        const std::vector<FakeGateway::Seen> seen = gateway.Requests();
        std::size_t reads = 0;
        for (std::size_t i = from; i < seen.size(); ++i) reads += seen[i].method == "GET" && seen[i].target == "/v1/rooms/3" ? 1 : 0;
        return reads;
    };
    const auto lastOf = [&gateway](const std::string& method, const std::string& target) {
        const std::vector<FakeGateway::Seen> seen = gateway.Requests();
        for (std::size_t i = seen.size(); i > 0; --i) {
            if (seen[i - 1].method == method && seen[i - 1].target == target) return i;
        }
        return std::size_t{0};
    };
    const auto runUntil = [&setup](const std::function<bool()>& done, const std::function<void()>& each) {
        for (int i = 0; i < 2000 && !done(); ++i) {
            setup.Frame();
            each();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return done();
    };
    // Ready, then not ready again, before the first answer: the first answer (ready) does not turn the button.
    gateway.Hold("POST", "/v1/rooms/3/ready");
    setup.Press(I::KeyCode::Enter);
    REQUIRE(FramesUntil(setup, [&] { return gateway.Held() == 1; }));
    setup.Press(I::KeyCode::Enter);
    REQUIRE(setup.Binding("ready") == "準備する");
    gateway.Release();
    bool turned = false;
    // Both ready calls answered, and two reads after them (the room's word on the readiness, read and shown).
    REQUIRE(runUntil([&] { return readsAfter(lastOf("POST", "/v1/rooms/3/ready")) >= 2; },
                     [&] { turned = turned || setup.Binding("ready") != "準備する"; }));
    CHECK_MESSAGE(!turned, "[pvp-room-ready-last-asked] the first answer turned the button");
    // The owner starts while not ready: the Gateway refuses (not_all_ready), and the word stays through the reads.
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE(FramesUntil(setup, [&] { return Asked(gateway, "POST", "/v1/rooms/3/start"); }));
    // Three reads after the start: the refusal and at least two reads were answered.
    REQUIRE(runUntil([&] { return readsAfter(lastOf("POST", "/v1/rooms/3/start")) >= 3; }, [] {}));
    CHECK_MESSAGE(setup.Binding("status") == "全員の準備がまだです", "[pvp-room-refusal-stays] " << setup.Binding("status"));
    // A read that goes wrong after it is the newer word: it takes the line.
    gateway.FailRoomReads(true);
    const std::size_t failing = gateway.Requests().size();
    // Two reads seen after the reads went wrong: the first of them (refused) was answered.
    REQUIRE(runUntil([&] { return readsAfter(failing) >= 2; }, [] {}));
    CHECK_MESSAGE(setup.Binding("status") == "Gateway: busy", "[pvp-room-problem-shown] " << setup.Binding("status"));
}

TEST_CASE("client: gateway: a started room whose connect cannot be read says so, over the word on the start") {
    TempDirectory directory;
    FakeGateway gateway;
    gateway.BadConnect(true);
    Setup setup(1, 4, 0, PC::SceneChange{.scene = PC::Scene::Room, .room = 3}, directory.Utf8(), gateway.Address());
    setup.Load();
    REQUIRE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備する" && Asked(gateway, "GET", "/v1/rooms/3"); }));
    setup.Press(I::KeyCode::Enter);
    REQUIRE(FramesUntil(setup, [&] { return Asked(gateway, "POST", "/v1/rooms/3/ready"); }));
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE(FramesUntil(setup, [&] { return Asked(gateway, "POST", "/v1/rooms/3/start"); }));
    CHECK_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("status") == "Gateway の接続先を読めません: not-an-address"; }),
                  "[pvp-room-connect-unreadable] " << setup.Binding("status"));
    CHECK(setup.Scene().scene == PC::Scene::Room);
}

TEST_CASE("client: gateway: each exchange is kept for the log: what was asked, its answer, how long it waited and took") {
    FakeGateway gateway;
    PC::GatewayClient client(gateway.Address(), PC::GatewaySignIn{"device", {}});
    client.Ask("list", PC::ListRoomsRequest());
    client.Ask("record", PC::SaveRequest());
    // The client's time is what each Update is given: one step per Update. An exchange answered within the Update
    // that started it took no step, so the list is held for a known number of Updates: it took at least those.
    constexpr std::int64_t kStep = 1'000'000;
    constexpr int kHeldUpdates = 20;
    gateway.Hold("GET", "/v1/rooms");
    std::vector<PC::GatewayResult> results;
    std::int64_t now = 0;
    int heldUpdates = 0;
    const auto begin = std::chrono::steady_clock::now();
    while (results.size() < 2 && std::chrono::steady_clock::now() - begin < std::chrono::seconds(5)) {
        if (auto result = client.Update(now)) results.push_back(std::move(*result));
        now += kStep;
        if (gateway.Held() == 1 && ++heldUpdates == kHeldUpdates) gateway.Release();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(heldUpdates >= kHeldUpdates);
    REQUIRE(results.size() == 2);
    const std::vector<PC::GatewayCallRecord> calls = client.TakeCalls();
    REQUIRE_MESSAGE(calls.size() == 3, "[pvp-gw-calls-kept] the session's, the list's and the record's: " << calls.size());
    CHECK((calls[0].session && calls[0].tag == "list" && calls[0].method == "POST" && calls[0].target == "/v1/session" && calls[0].status == 200));
    CHECK((!calls[1].session && calls[1].tag == "list" && calls[1].method == "GET" && calls[1].target == "/v1/rooms" && calls[1].status == 200));
    CHECK((calls[2].tag == "record" && calls[2].target == "/v1/saves/me" && calls[2].status == 200));
    // The list went at once (its session first: they share its wait); the record waited behind both.
    CHECK((calls[0].waited == 0 && calls[1].waited == 0));
    CHECK_MESSAGE(calls[2].waited >= calls[0].took + calls[1].took, "[pvp-gw-calls-waited] " << calls[2].waited);
    CHECK_MESSAGE(calls[1].took >= kHeldUpdates * kStep, "[pvp-gw-calls-took] " << calls[1].took);
    CHECK_MESSAGE(client.TakeCalls().empty(), "a take empties them");
}

TEST_CASE("client: gateway: a game that ends on its own goes back to the lobby without leaving the room, and does not reconnect") {
    TempDirectory directory;
    FakeGateway gateway;
    // The Match refuses (no seat): the connection ends without the player asking.
    PC::SceneChange game = GameScene();
    game.room = 3;
    Setup setup(1, 0, 0, game, directory.Utf8(), gateway.Address());
    REQUIRE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Lobby; }));
    setup.Frames(60);
    CHECK_MESSAGE(!Asked(gateway, "POST", "/v1/rooms/3/leave"), "[pvp-gw-end-keeps-room] the room is kept: the player may come back");
    CHECK_MESSAGE(setup.client->Buffer().Find<N::NetService>()->Client() == nullptr, "no connection is made again");
}

TEST_CASE("client: a Match that closes the connection sends the game back to the lobby; it does not connect again") {
    Setup setup;
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).playing; }));
    // The Match closes this Client's connection (as when the same user's newer connection takes the player over).
    K::World& match = GameWorld(*setup.match);
    std::optional<std::uint32_t> connection;
    for (const K::EntityId id : match.Entities()) {
        const auto* peer = match.Get<N::Peer>(id);
        if (peer != nullptr && peer->userId == 1) connection = peer->connectionId;
    }
    REQUIRE(connection.has_value());
    setup.match->Buffer().Find<N::NetService>()->Server()->Close(*connection);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Lobby; }), "[pvp-closed-lobby]");
    CHECK_MESSAGE(setup.Scene().status.find("閉じました") != std::string::npos, setup.Scene().status);
    setup.Frames(60);
    CHECK_MESSAGE(setup.client->Buffer().Find<N::NetService>()->Client() == nullptr, "[pvp-closed-no-reconnect] no new connection");
    bool back = false;
    for (const K::EntityId id : match.Entities()) {
        const auto* peer = match.Get<N::Peer>(id);
        back = back || (peer != nullptr && peer->userId == 1);
    }
    CHECK_FALSE(back);
}

TEST_CASE("client: gateway: a room whose match ended sends the room back to the lobby, which says so") {
    TempDirectory directory;
    FakeGateway gateway;
    Setup setup(1, 4, 0, PC::SceneChange{.scene = PC::Scene::Room, .room = 3}, directory.Utf8(), gateway.Address());
    REQUIRE(FramesUntil(setup, [&] { return Asked(gateway, "GET", "/v1/rooms/3"); }));
    gateway.EndMatch();
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Lobby; }), "[pvp-gw-room-gone]");
    CHECK(setup.Scene().status == "対戦は終わりました");
}

TEST_CASE("client: gateway: the error codes in the player's words; a call without an answer times out; the device id is kept") {
    const auto text = [](std::string code, const int status = 409) {
        PC::GatewayResult result;
        result.status = status;
        result.errorCode = std::move(code);
        return PC::GatewayErrorText(result, "1.2.3.4:5");
    };
    CHECK(text("unreachable", 0) == "Gateway（1.2.3.4:5）に接続できません");
    CHECK(text("timeout", 0) == "Gateway（1.2.3.4:5）から応答がありません");
    CHECK(text("room_full") == "部屋が満員です");
    CHECK(text("room_started") == "その部屋はもう始まっています");
    CHECK(text("room_starting") == "その部屋はもう始まっています");
    CHECK(text("room_not_found", 404) == "部屋がありません");
    CHECK(text("not_member", 403) == "この部屋のメンバーではありません");
    CHECK(text("match_ended", 410) == "対戦は終わりました");
    CHECK(text("not_owner", 403) == "開始できるのは部屋主だけです");
    CHECK(text("not_all_ready") == "全員の準備がまだです");
    CHECK(text("no_match_available", 503) == "対戦を始められませんでした。もう一度開始してください");
    CHECK(text("unknown_map", 400) == "Gateway がこのマップかモードを知りません");
    CHECK(text("", 500).find("HTTP 500") != std::string::npos);
    CHECK(text("key_failed", 500) == "Gateway: key_failed");
    CHECK(text("no_device", 0).find("この端末の ID") == 0);

    // A Gateway that takes the call and never answers: the call times out (the clock is the caller's).
    GYO::Test::LoopbackListener silent;
    REQUIRE(silent.Ok());
    PC::GatewayClient client(silent.Address(), PC::GatewaySignIn{"device", {}}, {}, 1'000'000'000);
    client.Ask("list", PC::ListRoomsRequest());
    std::optional<PC::GatewayResult> result;
    std::thread accept([&silent] { (void)silent.Accept(std::chrono::seconds(5)); });
    for (int i = 0; i < 200 && !result; ++i) {
        result = client.Update(static_cast<std::int64_t>(i) * 20'000'000);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    accept.join();
    REQUIRE_MESSAGE(result.has_value(), "[pvp-gw-timeout] the call never ended");
    CHECK_MESSAGE(result->errorCode == "timeout", "[pvp-gw-timeout] " << result->errorCode << " " << result->detail);
    // Kept for the log: the session's exchange (the first of the call) that had no answer.
    const std::vector<PC::GatewayCallRecord> calls = client.TakeCalls();
    REQUIRE_MESSAGE(calls.size() == 1, "[pvp-gw-calls-timeout] " << calls.size());
    CHECK_MESSAGE((calls[0].session && calls[0].status == 0 && calls[0].errorCode == "timeout" && calls[0].took >= 1'000'000'000),
                  "[pvp-gw-calls-timeout] " << calls[0].errorCode << " " << calls[0].took);

    // The device id kept in its file is the one used again.
    TempDirectory directory;
    const std::string path = directory.Utf8() + "/device-id";
    REQUIRE(static_cast<bool>(PC::WriteLocalText(path, "kept-device")));
    std::string error;
    CHECK_MESSAGE(PC::LoadDeviceId(path, error) == "kept-device", "[pvp-gw-device-kept] " << error);
}

TEST_CASE("client: the loading screen waits for the Match's start; it is judged on the arena the Match plays") {
    {
        // The Match waits for users 1 and 2; the other Client (user 2) never says it is ready.
        Setup setup(1, 4, 0, std::nullopt, {}, std::nullopt, {1, 2});
        REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).ready; }));
        setup.Frames(10);
        CHECK_FALSE(PC::StatusOf(setup.Client()).started);
        REQUIRE_MESSAGE(setup.Root() != 0, "[pvp-loading-waits-start] the loading page's root is there");
        const auto* root = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
        CHECK_MESSAGE(root->canvas == "loading", "[pvp-loading-waits-start] ready, but the match has not started: still covered");
        CHECK(setup.Binding("loading") == "ほかのプレイヤーを待っています…");
    }
    // The Match plays arena 1 and is slow: the Client loads arena 0 (its first) before the Match says which; when the
    // Match comes, the game is built again on arena 1, and is ready only once arena 1 is shown.
    Setup setup(1, 4, 1);
    setup.holdMatch = true;
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).progress >= 0.8; }));
    setup.holdMatch = false;
    REQUIRE(FramesUntil(setup, [&] { return setup.Scene().map == ShippedContent()->arenas[1].id; }));
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).ready; }));
    const object_fps_pvp::data::ArenaMap shownMap = ShippedMapOf(ShippedContent()->arenas[1]);
    const object_fps_pvp::data::ArenaMap otherMap = ShippedMapOf(ShippedContent()->arenas[0]);
    bool shown = false;
    bool other = false;
    for (const K::EntityId id : setup.Client().Entities()) {
        const auto* mesh = setup.Client().Get<R::Mesh3d>(id);
        if (mesh == nullptr) continue;
        shown = shown || ShowsPalette(*mesh, shownMap);
        other = other || (ShowsPalette(*mesh, otherMap) && !ShowsPalette(*mesh, shownMap));
    }
    CHECK_MESSAGE((shown && !other), "[pvp-loading-arena] when ready, the Match's arena is shown (and the first one no more)");
}

TEST_CASE("client: the pause comes when the window loses its focus, or the pointer is given back with Tab") {
    Setup setup;
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).started && PC::StatusOf(setup.Client()).ready; }));
    setup.Frames(3);  // the loading page leaves (a request, applied next frame)
    const auto canvas = [&setup] {
        const auto* root = setup.Root() != 0 ? setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root()) : nullptr;
        return root != nullptr ? root->canvas : std::string();
    };
    REQUIRE(canvas().empty());
    // The window loses its focus before the game took the pointer: the pause.
    auto* window = setup.Client().ResourceMut<W::WindowState>();
    window->known = true;
    window->physicalSize = {1280, 720};
    window->focused = false;
    setup.Frames(2);
    CHECK_MESSAGE(canvas() == "pause", "[pvp-pause-focus] " << canvas());
    // Back in focus, a click takes the pointer: the game again; Tab gives it back: the pause.
    setup.Client().ResourceMut<W::WindowState>()->focused = true;
    setup.Frame();
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.state = I::ButtonState::Pressed;
    click.position = {640.0F, 360.0F};
    setup.Next().buttons.push_back(click);
    setup.Frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    setup.Frames(2);
    REQUIRE(PC::StatusOf(setup.Client()).captured);
    REQUIRE(canvas() != "pause");
    setup.Press(I::KeyCode::Tab);
    setup.Frames(2);
    CHECK_MESSAGE(canvas() == "pause", "[pvp-pause-tab] " << canvas());
}

TEST_CASE("client: without its own player from the Match the loading screen does not end") {
    Setup setup;
    setup.holdMatch = true;  // nothing comes from the Match
    REQUIRE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).progress >= 0.85; }));
    setup.Frames(20);
    CHECK_MESSAGE(!PC::StatusOf(setup.Client()).ready, "[pvp-loading-needs-player] the assets are done, the own player is not there");
}

namespace {

// The ResizeWindow and SetWindowMode Messages the Frame World sent (read where sdl_window would apply them).
std::vector<GYO::Engine::Math::Vec2i> resized;
std::vector<W::WindowMode> modes;
K::SystemResult KeepResizes(K::MessageReader<W::ResizeWindow> sent, K::MessageReader<W::SetWindowMode> mode) {
    for (const W::ResizeWindow* resize : sent.Read()) resized.push_back(resize->size);
    for (const W::SetWindowMode* set : mode.Read()) modes.push_back(set->mode);
    return {};
}

} // namespace

TEST_CASE("client: settings: the sizes offered in turn, applied to the window and saved; the next start reads them") {
    std::string error;
    PC::ClientSettings chosen{{1920, 1080}, W::WindowMode::BorderlessFullscreen, false};
    const auto decoded = PC::DecodeSettings(PC::EncodeSettings(chosen), error);
    REQUIRE_MESSAGE(decoded.has_value(), error);
    CHECK_MESSAGE(*decoded == chosen, "[pvp-settings-roundtrip]");
    CHECK_FALSE_MESSAGE(PC::DecodeSettings("{\"version\":1,\"resolution\":[1000,700],\"windowMode\":\"windowed\",\"vsync\":true}", error).has_value(),
                        "[pvp-settings-fixed-sizes] a size not offered is not read");
    CHECK_FALSE(PC::DecodeSettings("{\"version\":2,\"resolution\":[1280,720],\"windowMode\":\"windowed\",\"vsync\":true}", error).has_value());
    CHECK(PC::NextResolution({2560, 1440}) == GYO::Engine::Math::Vec2i{1280, 720});
    TempDirectory directory;
    const std::string path = directory.Utf8() + "/settings.json";
    REQUIRE(static_cast<bool>(PC::WriteLocalText(path, "not json")));
    const PC::LoadedSettings bad = PC::LoadSettings(path);
    CHECK_MESSAGE((bad.settings == PC::ClientSettings{} && !bad.problem.empty()), "[pvp-settings-bad-file] the defaults, and why");

    // The menu's settings: next size twice, borderless, VSync off, apply.
    Setup setup(1, 4, 0, PC::SceneChange{}, directory.Utf8());
    PC::ClientStart* start = setup.client->Buffer().Find<PC::ClientStart>();
    REQUIRE(start != nullptr);
    resized.clear();
    modes.clear();
    // (the Setup has no settings file: the test's directory is given here, as the program gives its own)
    setup.Client().ResourceMut<PC::SceneRequest>()->next = PC::SceneChange{};
    start->settingsPath = path;
    setup.Frame();
    // Inserted into the menu's Frame World (a replacement resets what was inserted).
    REQUIRE(static_cast<bool>(setup.client->InsertSystems("Frame", "test", [](K::Registrar& r) {
        K::SystemOptions keep;
        keep.name = "test.KeepResizes";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::last);
        keep.mounts.push_back(std::move(mount));
        r.AddSystem<&KeepResizes>(std::move(keep));
    })));
    setup.Load();
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // 設定
    setup.Load();                     // the page's request is applied next frame; its root is laid out then
    CHECK_MESSAGE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "settings", "[pvp-settings-shown]");
    CHECK(setup.Binding("resolution") == "解像度: 1280 × 720");
    setup.Press(I::KeyCode::Enter);
    setup.Press(I::KeyCode::Enter);  // 1600, then 1920
    CHECK(setup.Binding("resolution") == "解像度: 1920 × 1080");
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // borderless
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // VSync off
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // apply
    setup.Frames(1);
    REQUIRE_MESSAGE(resized.size() == 1, "[pvp-settings-resize] the window is asked for the new size once");
    CHECK(resized.front() == GYO::Engine::Math::Vec2i{1920, 1080});
    REQUIRE_MESSAGE(modes.size() == 1, "[pvp-settings-mode] the window is asked for the mode");
    CHECK(modes.front() == W::WindowMode::BorderlessFullscreen);
    setup.Frames(1);  // the Render World's Extract copies VSync
    CHECK_MESSAGE(setup.client->World("Render").GetWorld().Resource<R::Presentation>()->presentMode == R::PresentMode::AutoNoVsync,
                  "[pvp-settings-vsync] VSync off reaches render's Presentation");
    CHECK_MESSAGE(setup.Binding("settings_status").find("保存しました") != std::string::npos, setup.Binding("settings_status"));
    const PC::LoadedSettings saved = PC::LoadSettings(path);
    CHECK_MESSAGE((saved.problem.empty() && saved.settings == chosen), "[pvp-settings-saved] " << saved.problem);
    // Carried to the next scene: the lobby and back keep them.
    setup.Client().ResourceMut<PC::SceneRequest>()->next = PC::SceneChange{.scene = PC::Scene::Lobby};
    setup.Frame();
    CHECK_MESSAGE(*setup.Client().Resource<PC::ClientSettings>() == chosen, "[pvp-settings-carried]");
}

TEST_CASE("client: settings: the start's settings are the first scene's; what is not applied is dropped when the page is left") {
    PC::ClientSettings saved{{2560, 1440}, W::WindowMode::Windowed, false};
    Setup setup(1, 4, 0, PC::SceneChange{}, {}, std::nullopt, {}, 0, [&saved](PC::ClientStart& start) { start.settings = saved; });
    setup.Frames(2);
    CHECK_MESSAGE(*setup.Client().Resource<PC::ClientSettings>() == saved, "[pvp-settings-start] the settings the program read");
    CHECK_MESSAGE(setup.client->World("Render").GetWorld().Resource<R::Presentation>()->presentMode == R::PresentMode::AutoNoVsync,
                  "[pvp-settings-start-vsync] ... their VSync from the first frames");
    setup.Load();
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // 設定
    setup.Load();
    CHECK(setup.Binding("resolution") == "解像度: 2560 × 1440");
    setup.Press(I::KeyCode::Enter);  // 1280 (round), not applied
    CHECK(setup.Binding("resolution") == "解像度: 1280 × 720");
    for (int i = 0; i < 4; ++i) setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // 戻る
    setup.Load();
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // 設定 again
    setup.Load();
    CHECK_MESSAGE(setup.Binding("resolution") == "解像度: 2560 × 1440", "[pvp-settings-dropped] " << setup.Binding("resolution"));
}

TEST_CASE("client: settings: the resolution is physical pixels: the window is given the logical size at its scale factor") {
    CHECK(PC::WindowSizeFor({1920, 1080}, 1.25F) == GYO::Engine::Math::Vec2i{1536, 864});
    CHECK(PC::WindowSizeFor({2560, 1440}, 2.0F) == GYO::Engine::Math::Vec2i{1280, 720});
    CHECK(PC::WindowSizeFor({1280, 720}, 0.0F) == GYO::Engine::Math::Vec2i{1280, 720});
    PC::ClientSettings saved{{1920, 1080}, W::WindowMode::Windowed, true};
    Setup setup(1, 4, 0, PC::SceneChange{}, {}, std::nullopt, {}, 0, [&saved](PC::ClientStart& start) { start.settings = saved; });
    REQUIRE(static_cast<bool>(setup.client->InsertSystems("Frame", "test", [](K::Registrar& r) {
        K::SystemOptions keep;
        keep.name = "test.KeepResizes";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::last);
        keep.mounts.push_back(std::move(mount));
        r.AddSystem<&KeepResizes>(std::move(keep));
    })));
    resized.clear();
    setup.Frames(3);
    CHECK_MESSAGE(resized.empty(), "a window at scale 1 is left as it opened");
    // The window says it is at 125 % (Windows' display scale): it is asked for the logical size of 1920 × 1080 pixels.
    auto* window = setup.Client().ResourceMut<W::WindowState>();
    window->known = true;
    window->scaleFactor = 1.25F;
    window->physicalSize = {1920, 1080};
    window->focused = true;
    // (the Setup's pointer, at the middle of a 1280 x 720 window, would rest on another button of this larger one)
    setup.Next().moves.push_back(I::CursorMoved{{1.0F, 1.0F}, {}});
    setup.Frames(2);
    REQUIRE_MESSAGE(resized.size() == 1, "[pvp-settings-physical] once, when the scale is said");
    CHECK_MESSAGE((resized.front() == GYO::Engine::Math::Vec2i{1536, 864}), "[pvp-settings-physical] " << resized.front().x);
    // Applied from the settings page: at the same scale.
    setup.Load();
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);  // 設定
    setup.Load();
    for (int i = 0; i < 3; ++i) setup.Press(I::KeyCode::Tab);
    resized.clear();
    setup.Press(I::KeyCode::Enter);  // apply
    setup.Frames(1);
    REQUIRE_MESSAGE(resized.size() == 1, setup.Binding("settings_status"));
    CHECK_MESSAGE((resized.front() == GYO::Engine::Math::Vec2i{1536, 864}), "[pvp-settings-apply-physical] " << resized.front().x);
    // The window goes to a screen at 200 %: asked again, for that scale.
    resized.clear();
    setup.Client().ResourceMut<W::WindowState>()->scaleFactor = 2.0F;
    setup.Frames(2);
    REQUIRE_MESSAGE(resized.size() == 1, "[pvp-settings-rescaled] once more, at the new scale");
    CHECK_MESSAGE((resized.front() == GYO::Engine::Math::Vec2i{960, 540}), "[pvp-settings-rescaled] " << resized.front().x);
}

TEST_CASE("client: single player: the room is the multiplayer's: the map chosen there is played; the mode is the first installed") {
    TempDirectory directory;
    Setup setup(1, 4, 0, PC::SceneChange{}, directory.Utf8());
    setup.Load();
    setup.Press(I::KeyCode::Enter);  // シングルプレイ
    REQUIRE(setup.Scene().scene == PC::Scene::SoloRoom);
    setup.Load();
    CHECK_MESSAGE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "solo_room", "[pvp-solo-room-shared]");
    // The first installed mode (modes/modes.json), by its name (mode-config declaration, M1).
    CHECK_MESSAGE(setup.Binding("mode") == "モード: 核心 PvP（固定）", "[pvp-solo-mode] " << setup.Binding("mode"));
    const auto& arenas = ShippedContent()->arenas;
    REQUIRE(arenas.size() >= 2);
    CHECK(setup.Binding("map") == "マップ: " + arenas[0].id);
    // The map (the second button), then the start (the first: the focus goes round).
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    CHECK_MESSAGE(setup.Binding("map") == "マップ: " + arenas[1].id, "[pvp-solo-map-next] " << setup.Binding("map"));
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(setup.Scene().scene == PC::Scene::SoloGame, "[pvp-solo-start] " << setup.Binding("status"));
    CHECK_MESSAGE(setup.Scene().map == arenas[1].id, "[pvp-solo-map] " << setup.Scene().map);
    CHECK_MESSAGE(setup.Scene().mode == "core_pvp", "[pvp-solo-mode-played] " << setup.Scene().mode);
    CHECK_MESSAGE(setup.Solo().Resource<PW::GameRules>()->arena.id == arenas[1].id, "[pvp-solo-map-played]");
    // 中止 on the loading screen (its focused button): back to the room, on the same map.
    for (int i = 0; i < 400; ++i) {
        const auto* state = setup.Client().Get<GYO::Engine::Ui::UiRootState>(setup.Root());
        if (state != nullptr && state->runtime) break;
        setup.Frames(1);
    }
    REQUIRE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "loading");
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::SoloRoom; }), "[pvp-solo-abort]");
    setup.Load();
    CHECK_MESSAGE(setup.Binding("map") == "マップ: " + arenas[1].id, "[pvp-solo-map-kept] " << setup.Binding("map"));
}

namespace {

// The labels over the heads (overhead-ui declaration, section 5 C1-C3): the name and the bar pieces of a thing's label.
struct LabelParts final {
    K::EntityId name = 0;
    K::EntityId bar = 0;
};
LabelParts PartsOf(K::World& world, const K::EntityId target) {
    LabelParts parts;
    for (const K::EntityId id : world.Entities()) {
        const auto* part = world.Get<PC::OverheadPart>(id);
        if (part == nullptr || part->target != target) continue;
        (part->bar ? parts.bar : parts.name) = id;
    }
    return parts;
}

bool NameShows(K::World& world, const K::EntityId part) {
    const auto* sprite = world.Get<R::Sprite>(part);
    return sprite != nullptr && sprite->color.a == 255 &&
           std::holds_alternative<std::shared_ptr<const GYO::Engine::Image::ImageData>>(sprite->image);
}

// Where this frame's cameras put the anchor on the HUD (no walls), as ShowOverheads places it.
std::optional<GYO::Engine::Math::Vec2> Expected(K::World& world, const GYO::Engine::Math::Vec3 anchor) {
    namespace C = GYO::Engine::Camera;
    K::EntityId eye = 0, flat = 0;
    for (const K::EntityId id : world.Entities()) {
        const auto* camera = world.Get<C::Camera>(id);
        if (camera == nullptr) continue;
        if (world.Get<C::Camera3d>(id) != nullptr && camera->order == 0) eye = id;
        if (world.Get<C::Camera2d>(id) != nullptr) flat = id;
    }
    REQUIRE(eye != 0);
    REQUIRE(flat != 0);
    const PC::OverheadPlacement placed = PC::PlaceOverhead(*world.Get<C::ComputedCamera>(eye), *world.Get<T::GlobalTransform>(eye),
                                                           *world.Get<C::ComputedCamera>(flat), *world.Get<T::GlobalTransform>(flat), anchor, nullptr);
    if (const auto* at = std::get_if<GYO::Engine::Math::Vec2>(&placed)) return *at;
    return std::nullopt;
}

// The world camera's forward on the ground.
GYO::Engine::Math::Vec3 GroundForward(K::World& world) {
    for (const K::EntityId id : world.Entities()) {
        const auto* camera = world.Get<GYO::Engine::Camera::Camera>(id);
        if (camera == nullptr || camera->order != 0 || world.Get<GYO::Engine::Camera::Camera3d>(id) == nullptr) continue;
        GYO::Engine::Math::Vec3 forward = GYO::Engine::Math::Rotate(world.Get<T::Transform>(id)->rotation, {0.0F, 0.0F, 1.0F});
        forward.y = 0.0F;
        return forward * (1.0F / GYO::Engine::Math::Length(forward));
    }
    FAIL("no world camera");
    return {};
}

// The own player takes the pointer (a click), so the mouse turns the view.
void Capture(Setup& setup) {
    I::MouseButtonInput click;
    click.button = I::MouseButton::Left;
    click.state = I::ButtonState::Pressed;
    click.position = {640.0F, 360.0F};
    setup.Next().buttons.push_back(click);
    setup.Frame();
    click.state = I::ButtonState::Released;
    setup.Next().buttons.push_back(click);
    setup.Frame();
    REQUIRE(PC::StatusOf(setup.Client()).captured);
}

// The other player stood a few metres before the own player's eye (at the Match: the Clients follow it).
K::EntityId RivalBefore(Setup& setup, const float metres) {
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    const K::EntityId rival = PlayerOf(GameWorld(*setup.match), 2);
    REQUIRE(own != 0);
    REQUIRE(rival != 0);
    const GYO::Engine::Math::Vec3 forward = GroundForward(setup.Client());
    K::World& match = GameWorld(*setup.match);
    const GYO::Engine::Math::Vec3 feet = match.Get<PW::PlayerBody>(own)->feet;
    match.GetMut<PW::PlayerBody>(rival)->feet = feet + forward * metres;
    setup.Frames(90);
    return rival;
}
} // namespace

TEST_CASE("client: overhead: the other player's name stands over its head where the camera sees it, with no bar; the own has none") {
    Setup setup;
    setup.Shown();
    Capture(setup);
    const K::EntityId rival = RivalBefore(setup, 4.0F);
    K::World& client = setup.Client();
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    CHECK_MESSAGE(client.Get<PC::Overhead>(own) == nullptr, "[pvp-overhead-not-own]");
    const auto* label = client.Get<PC::Overhead>(rival);
    REQUIRE_MESSAGE(label != nullptr, "[pvp-overhead-player] the other player has a label");
    const auto* name = client.Get<PW::PlayerName>(rival);
    CHECK_MESSAGE(label->label == PC::PlayerLabel(2, name != nullptr ? name->text : std::string_view{}), "[pvp-overhead-player-name] " << label->label);
    CHECK_MESSAGE(!label->health.has_value(), "[pvp-overhead-player-no-bar] a player has no health bar");
    // Over the head: above the body's top.
    CHECK(label->anchor.y > client.Get<PW::PlayerBody>(rival)->feet.y + 1.6F);
    const LabelParts parts = PartsOf(client, rival);
    REQUIRE(parts.name != 0);
    CHECK(parts.bar == 0);
    REQUIRE_MESSAGE(NameShows(client, parts.name), "[pvp-overhead-shown] the name shows");
    const auto expected = Expected(client, label->anchor);
    REQUIRE(expected.has_value());
    const auto& at = client.Get<T::GlobalTransform>(parts.name)->matrix.values;
    CHECK_MESSAGE(std::abs(at[12] - expected->x) <= 1.0F, "[pvp-overhead-at] " << at[12] << " vs " << expected->x);
    CHECK(std::abs(at[13] - expected->y) <= 1.0F);
    // The view turns: in the same frame the name follows this frame's camera.
    const float before = at[12];
    setup.Next().motions.push_back(I::MouseMotion{{60.0F, 0.0F}, {}});
    setup.Frame();
    const auto turned = Expected(client, client.Get<PC::Overhead>(rival)->anchor);
    REQUIRE(turned.has_value());
    const auto& now = client.Get<T::GlobalTransform>(parts.name)->matrix.values;
    CHECK_MESSAGE(std::abs(now[12] - turned->x) <= 1.0F, "[pvp-overhead-same-frame] " << now[12] << " vs " << turned->x);
    CHECK(std::abs(now[12] - before) > 5.0F);
}

TEST_CASE("client: overhead: the other player hit many times still has no health bar") {
    Setup setup;
    setup.Shown();
    Capture(setup);
    const K::EntityId rival = RivalBefore(setup, 4.0F);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
    for (int shot = 0; shot < 3; ++shot) {
        K::World& match = GameWorld(*setup.match);
        match.GetMut<PW::PlayerCombat>(rival)->hp -= 20'000;
        PW::ShotRecord record;
        record.tick = TickOf(match);
        record.kind = sim::ShotHitKind::Player;
        record.target = rival;
        record.part = sim::HitPart::Body;
        record.damage = 20'000;
        *match.GetMut<PW::ShotRecord>(own) = record;
        setup.Frames(10);
    }
    REQUIRE(setup.Client().Get<PW::PlayerCombat>(rival)->hp < 100'000);
    const auto* label = setup.Client().Get<PC::Overhead>(rival);
    REQUIRE(label != nullptr);
    CHECK_MESSAGE(!label->health.has_value(), "[pvp-overhead-player-no-bar-hit]");
    CHECK(PartsOf(setup.Client(), rival).bar == 0);
}

TEST_CASE("client: overhead: turned away the name hides; the other player gone, its label goes") {
    Setup setup;
    setup.Shown();
    Capture(setup);
    const K::EntityId rival = RivalBefore(setup, 4.0F);
    K::World& client = setup.Client();
    const LabelParts parts = PartsOf(client, rival);
    REQUIRE(parts.name != 0);
    REQUIRE(NameShows(client, parts.name));
    // Half a turn: the other player is behind the camera.
    setup.Next().motions.push_back(I::MouseMotion{{std::numbers::pi_v<float> / 0.0025F, 0.0F}, {}});
    setup.Frames(2);
    CHECK_MESSAGE(!NameShows(client, parts.name), "[pvp-overhead-hidden-behind]");
    CHECK(client.Get<PC::Overhead>(rival) != nullptr);
    // Back again: shown.
    setup.Next().motions.push_back(I::MouseMotion{{-std::numbers::pi_v<float> / 0.0025F, 0.0F}, {}});
    setup.Frames(2);
    CHECK(NameShows(client, parts.name));
    // The other Client closes: its label's pieces go with it.
    auto* service = setup.other->Buffer().Find<N::NetService>();
    REQUIRE(service != nullptr);
    service->Close();
    service->Flush(service->Now(), N::Stamp{});
    setup.other.reset();
    setup.Frames(30);
    CHECK_FALSE(client.Alive(rival));
    CHECK_MESSAGE(PartsOf(client, rival).name == 0, "[pvp-overhead-gone] the name's piece went with the player");
}

namespace {

// A whole Client (the program's Worlds, headless) against a real Gateway: the "network-room:" cases (their Gateway is
// the services' own, games/object_fps_pvp/services/stack/client_test.go).
struct NetworkClient final {
    TempDirectory directory;
    std::unique_ptr<GYO::Engine::Threads::WorkerPool> pool;
    std::unique_ptr<WA::WorkerPoolExecutor> executor;
    std::unique_ptr<O::Product> product;

    explicit NetworkClient(const GYO::Engine::OS::SocketAddress gateway, PC::SceneChange first = LobbyFirst()) {
        auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "network assets", .threadCount = 2});
        REQUIRE(static_cast<bool>(started));
        pool = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(started).value());
        executor = std::make_unique<WA::WorkerPoolExecutor>(*pool);
        std::string error;
        auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, error);
        REQUIRE_MESSAGE(presentation.has_value(), error);
        PC::ClientOptions options{ShippedContent(), std::make_shared<const PC::PresentationContent>(std::move(*presentation)), 0, 0, nullptr};
        O::ProductDescription description;
        description.name = "pvp-client";
        N::NetProductOptions net;
        net.role = N::NetRole::Client;
        description.options.Set(net);
        PC::ClientStart start;
        start.first = std::move(first);
        start.dataDirectory = directory.Utf8();
        start.gateway = gateway;
        description.options.Set(start);
        O::WorldDescription frame = PC::ClientFrameWorld(options, false);
        frame.build = [inner = std::move(frame.build)](O::WorldSetup& setup) {
            inner(setup);
            setup.Registrar().Resource<Pending>({.name = "test.Pending"});
            K::SystemOptions write;
            write.name = "test.WriteEvents";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(O::DefaultSchedules::preUpdate);
            mount.before = {std::string(I::kInputSet), std::string(W::kWindowSet)};
            write.mounts.push_back(std::move(mount));
            setup.Registrar().AddSystem<&WriteEvents>(std::move(write));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                Pending first;
                first.focus.push_back(W::WindowFocused{true, {}});
                first.sizes.push_back(W::WindowResized{{1280, 720}, 1.0F, {}});
                w.SetResource(std::move(first));
                return {};
            });
        };
        description.worlds.push_back(PC::ClientSoloWorld(options));
        description.worlds.push_back(PC::ClientSimWorld(options));
        description.worlds.push_back(std::move(frame));
        description.worlds.push_back(PC::ClientRenderWorld(false));
        description.configure = [](O::ProductSetup& setup) { PC::ConfigureClient(setup); };
        WA::AssetOptions assets;
        assets.contentRoot = PVP_CONTENT_ROOT;
        assets.executor = executor.get();
        description.options.Set(assets);
        auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        product = std::move(built).value();
    }
    ~NetworkClient() {
        product.reset();
        executor.reset();
        pool.reset();
    }

    K::World& Frame() { return product->World("Frame").GetWorld(); }
    const PC::SceneChange& Scene() { return product->Buffer().Find<PC::SceneDirector>()->Current(); }
    // One frame, and the scene it asks for (between frames, as the program does).
    void Step() {
        auto ran = product->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
        if (PC::SceneChangeWanted(*product)) REQUIRE(static_cast<bool>(PC::ChangeScene(*product)));
    }
    std::string Binding(const std::string_view id) {
        for (const K::EntityId e : Frame().Entities()) {
            const auto* bindings = Frame().Get<GYO::Engine::Ui::UiBindings>(e);
            if (bindings == nullptr) continue;
            const auto* value = bindings->Find(id);
            const auto* text = value != nullptr ? std::get_if<std::string>(value) : nullptr;
            if (text != nullptr) return *text;
        }
        return {};
    }
    std::vector<std::string> Lines(const std::string_view id) {
        std::vector<std::string> lines;
        for (const K::EntityId e : Frame().Entities()) {
            const auto* bindings = Frame().Get<GYO::Engine::Ui::UiBindings>(e);
            const auto* value = bindings != nullptr ? bindings->Find(id) : nullptr;
            const auto* list = value != nullptr ? std::get_if<GYO::Engine::Ui::UiList>(value) : nullptr;
            if (list == nullptr) continue;
            for (const auto& item : list->items) {
                const auto found = item.fields.find("line");
                const auto* text = found != item.fields.end() ? std::get_if<std::string>(&found->second) : nullptr;
                if (text != nullptr) lines.push_back(*text);
            }
        }
        return lines;
    }
    bool Loaded() {
        for (const K::EntityId e : Frame().Entities()) {
            const auto* state = Frame().Get<GYO::Engine::Ui::UiRootState>(e);
            if (state != nullptr && state->runtime) return true;
        }
        return false;
    }
    void Key(const I::KeyCode code) { Frame().ResourceMut<Pending>()->keys.push_back(::Key(code)); }
    void Focus(const bool focused) { Frame().ResourceMut<Pending>()->focus.push_back(W::WindowFocused{focused, {}}); }
    // The pointer to (x, y) of the 1280 x 720 window, and the left button pressed (released: false) there.
    void Mouse(const float x, const float y, const bool pressed) {
        Pending& pending = *Frame().ResourceMut<Pending>();
        pending.moves.push_back(I::CursorMoved{{x, y}, {}});
        I::MouseButtonInput button;
        button.button = I::MouseButton::Left;
        button.state = pressed ? I::ButtonState::Pressed : I::ButtonState::Released;
        button.position = {x, y};
        pending.buttons.push_back(button);
    }
};

// A frame of a Client, every 10 ms of real time, until done or the time is up.
template <class Done>
bool RunUntil(NetworkClient& client, const Done& done, const double seconds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < until) {
        if (done()) return true;
        client.Step();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return done();
}

// A key, and two frames for it to be read.
void Press(NetworkClient& client, const I::KeyCode code) {
    client.Key(code);
    client.Step();
    client.Step();
}

// Both Clients a frame each, every 10 ms of real time, until done or the time is up; the time it took.
template <class Done>
std::optional<double> RunBoth(NetworkClient& a, NetworkClient& b, const Done& done, const double seconds) {
    const auto begin = std::chrono::steady_clock::now();
    const auto until = begin + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < until) {
        if (done()) return std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        a.Step();
        b.Step();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (done()) return seconds;
    return std::nullopt;
}

} // namespace

TEST_CASE("network: through the Gateway: a room made, readied and started, played in the Match, evacuated; the save is read") {
    const char* address = std::getenv("GYO_PVP_GATEWAY");
    if (address == nullptr) {
        MESSAGE("GYO_PVP_GATEWAY is not set: nothing to run against");
        return;
    }
    const auto gateway = PC::ParseMatchAddress(address);
    REQUIRE(gateway.has_value());
    NetworkClient client(*gateway);
    const auto loaded = [&] { return client.Loaded(); };
    REQUIRE_MESSAGE(RunUntil(client, [&] { return client.Binding("record").find("戦績：") == 0 && client.Binding("record") != "戦績：—"; }, 20),
                    "the lobby read the record: " << client.Binding("record") << " / " << client.Binding("notice"));
    const std::string before = client.Binding("record");
    REQUIRE(RunUntil(client, loaded, 20));
    for (int i = 0; i < 5; ++i) Press(client, I::KeyCode::Tab);
    Press(client, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunUntil(client, [&] { return client.Scene().scene == PC::Scene::Room; }, 20), client.Binding("status"));
    REQUIRE(RunUntil(client, loaded, 20));
    Press(client, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunUntil(client, [&] { return client.Binding("ready") == "準備をやめる"; }, 20), client.Binding("status"));
    Press(client, I::KeyCode::Tab);
    Press(client, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunUntil(client, [&] { return client.Scene().scene == PC::Scene::MultiGame; }, 40), "started: " << client.Binding("status"));
    N::NetService& net = *client.product->Buffer().Find<N::NetService>();
    REQUIRE_MESSAGE(RunUntil(client, [&] { return PC::StatusOf(client.Frame()).playing; }, 40),
                    "[pvp-gw-e2e-playing] the own player came from the Match: " << PC::StatusOf(client.Frame()).endReason << " / scene "
                        << static_cast<int>(client.Scene().scene) << " (" << client.Scene().status << ") / session " << (net.Client() != nullptr)
                        << " / link failures " << net.LinkFailures() << " / target "
                        << HasResource<N::NetConnectTarget>(client.product->World("Sim").GetWorld()) << " / connected "
                        << PC::StatusOf(client.Frame()).connected);
    // The match starts (this Client was the room's one member, and it is ready): the loading screen goes.
    REQUIRE_MESSAGE(RunUntil(client, [&] { return PC::StatusOf(client.Frame()).started; }, 20), "[pvp-gw-e2e-started]");
    // Evacuate (Esc: the pause; Enter on 撤退する): the room is left, the connection closes, the Match writes the save.
    for (int i = 0; i < 3; ++i) client.Step();
    Press(client, I::KeyCode::Escape);
    for (int i = 0; i < 3; ++i) client.Step();
    Press(client, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunUntil(client, [&] { return client.Scene().scene == PC::Scene::Lobby; }, 5), "[pvp-gw-e2e-evacuated]");
    CHECK_MESSAGE(RunUntil(client, [&] { return client.Binding("record").find("出撃 1 回") != std::string::npos; }, 40),
                  "[pvp-gw-e2e-saved] the record after one game: " << client.Binding("record") << " (before: " << before << ")");
}

TEST_CASE("network-room: A plays alone on the big map, then B joins A's room; A gets ready at once, and stays ready") {
    const char* address = std::getenv("GYO_PVP_GATEWAY");
    if (address == nullptr) {
        MESSAGE("GYO_PVP_GATEWAY is not set: nothing to run against");
        return;
    }
    const auto gateway = PC::ParseMatchAddress(address);
    REQUIRE(gateway.has_value());
    // As the player did: A from the menu, B in the lobby.
    NetworkClient a(*gateway, PC::SceneChange{});
    NetworkClient b(*gateway);
    const auto press = [&](NetworkClient& who, const I::KeyCode code) {
        who.Key(code);
        REQUIRE(RunBoth(a, b, [] { return false; }, 0.03) == std::nullopt);
    };
    // A plays a single player game on the 250 m map first (the menu's first button; the room's map, the fourth
    // installed arena; 開始), evacuates (Esc, 撤退する) and goes to the lobby (the menu's second button).
    REQUIRE(RunBoth(a, b, [&] { return a.Loaded(); }, 20).has_value());
    press(a, I::KeyCode::Enter);
    REQUIRE(RunBoth(a, b, [&] { return a.Scene().scene == PC::Scene::SoloRoom && a.Loaded(); }, 20).has_value());
    press(a, I::KeyCode::Tab);
    for (int i = 0; i < 3; ++i) press(a, I::KeyCode::Enter);
    REQUIRE(a.Binding("map") == "マップ: kiriyu_extraction_250m");
    press(a, I::KeyCode::Tab);
    press(a, I::KeyCode::Tab);
    press(a, I::KeyCode::Enter);
    REQUIRE(a.Scene().scene == PC::Scene::SoloGame);
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return PC::StatusOf(a.Frame()).started && PC::StatusOf(a.Frame()).ready; }, 60).has_value(),
                    "the single player game began");
    REQUIRE(!RunBoth(a, b, [] { return false; }, 1).has_value());
    press(a, I::KeyCode::Escape);
    REQUIRE(RunBoth(a, b, [&] { return a.Loaded(); }, 5).has_value());
    press(a, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return a.Scene().scene == PC::Scene::Menu && a.Loaded(); }, 20).has_value(), a.Binding("status"));
    press(a, I::KeyCode::Tab);
    press(a, I::KeyCode::Enter);
    REQUIRE(RunBoth(a, b, [&] { return a.Scene().scene == PC::Scene::Lobby && a.Loaded() && b.Loaded() && a.Binding("notice").empty(); }, 20)
                .has_value());
    // A makes a room (the sixth button: join, prev, next, mode, map, create).
    for (int i = 0; i < 5; ++i) press(a, I::KeyCode::Tab);
    press(a, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return a.Scene().scene == PC::Scene::Room && a.Loaded(); }, 20).has_value(), a.Binding("status"));
    // B joins it (the first button, the room picked: the only one).
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return b.Binding("empty").empty() && !b.Lines("rooms").empty(); }, 20).has_value(), b.Binding("notice"));
    press(b, I::KeyCode::Enter);
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return b.Scene().scene == PC::Scene::Room && b.Loaded(); }, 20).has_value(), b.Binding("status"));
    // A sees B: then A gets ready at once.
    REQUIRE_MESSAGE(RunBoth(a, b, [&] { return a.Lines("members").size() == 2; }, 20).has_value(), a.Binding("status"));
    // As the player does: clicks on 準備する (the room canvas's first button, at 160..440 x 584..636), several times,
    // each a press and a release a few frames apart; each time A's choice reaches the room and stays.
    const auto aReady = [&] {
        for (const std::string& line : b.Lines("members")) {
            if (line.find("（部屋主）") != std::string::npos) return line.find("準備完了") != std::string::npos;
        }
        return false;
    };
    for (int round = 0; round < 6; ++round) {
        const bool want = round % 2 == 0;
        // From the press (the UI acts on it): A's ready call, as A kept it, and the room as B reads it.
        (void)a.Frame().ResourceMut<PC::GatewayClient>()->TakeCalls();
        // The player comes to A's window from B's.
        b.Focus(false);
        a.Focus(true);
        a.Mouse(300.0F, 610.0F, true);
        std::optional<std::int64_t> readyCall;
        const auto sent = [&] {
            for (const PC::GatewayCallRecord& call : a.Frame().ResourceMut<PC::GatewayClient>()->TakeCalls()) {
                if (call.tag == "ready" && !call.session) {
                    CHECK_MESSAGE(call.status == 200, "[pvp-room-ready-quick] round " << round << ": " << call.status << " " << call.errorCode);
                    readyCall = call.waited + call.took;
                }
            }
            return readyCall.has_value();
        };
        REQUIRE_MESSAGE(RunBoth(a, b, sent, 5).has_value(), "[pvp-room-ready-quick] round " << round << ": A's ready call was not answered");
        MESSAGE("round " << round << ": A's ready call answered " << static_cast<double>(*readyCall) / 1e9 << " s after it was asked");
        CHECK_MESSAGE(*readyCall < 500'000'000, "[pvp-room-ready-quick] round " << round);
        a.Mouse(300.0F, 610.0F, false);
        const auto took = RunBoth(a, b, [&] { return a.Binding("ready") == (want ? "準備をやめる" : "準備する") && aReady() == want; }, 10);
        REQUIRE_MESSAGE(took.has_value(), "[pvp-room-ready-quick] round " << round << ": A's choice never reached the room: A shows '"
                                                                          << a.Binding("ready") << "', status '" << a.Binding("status") << "'");
        bool turned = false;
        RunBoth(a, b, [&] { turned = turned || a.Binding("ready") != (want ? "準備をやめる" : "準備する") || aReady() != want; return false; }, 2);
        CHECK_MESSAGE(!turned, "[pvp-room-ready-stays] round " << round << ": A's choice turned back");
        // ... and goes back to B's.
        a.Focus(false);
        b.Focus(true);
    }
}
