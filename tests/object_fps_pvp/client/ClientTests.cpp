// The Client (client/ClientWorld.hpp) without a window or a GPU: its Sim World (the client World), its Frame World
// with the Client's Systems (both on the test's thread here: the frame runs Sim, then Frame), its Render World without sdl_gpu (what it would draw is read at the Submit stage), a Match and a second plain Client
// over memory links, the content read from assets/object_fps_pvp through the asset Module. The input and window
// Messages are written by a test System where sdl_window writes them.
#include <doctest/doctest.h>

#include "../world/WorldTestSupport.hpp"

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/client/Gateway.hpp"
#include "object_fps_pvp/client/LocalFiles.hpp"
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
#include "engine/render/RenderCommands.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"

#include <atomic>
#include <cstdlib>
#include <cctype>
#include <mutex>
#include <filesystem>
#include <random>
#include <chrono>
#include <thread>
#include <variant>

using namespace object_fps_pvp::test;
namespace PC = object_fps_pvp::client;
namespace I = GYO::Engine::Input;
namespace W = GYO::Engine::Window;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Transform;
namespace WA = GYO::Engine::WorldAsset;

namespace {

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

    std::shared_ptr<const PC::PresentationContent> presentation;

    // matchArena: the arena the Match plays (the Client starts with arena 0 and follows it). first: the Client's first
    // scene (none: the game, connected to the Match over the memory link).
    explicit Setup(const int perStep = 1, const std::uint32_t seats = 4, const std::size_t matchArena = 0,
                   std::optional<PC::SceneChange> firstScene = std::nullopt, std::string dataDirectory = {},
                   std::optional<GYO::Engine::OS::DatagramAddress> gateway = std::nullopt)
        : framesPerStep(perStep) {
        auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "test assets", .threadCount = 2});
        REQUIRE(static_cast<bool>(started));
        pool = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(started).value());
        executor = std::make_unique<WA::WorkerPoolExecutor>(*pool);
        auto reader = [c = clock] { return *c; };
        match = BuildMatch(ShippedContent(), std::make_shared<HubLink>(std::vector<N::MemoryLink>{first.first, second.first}),
                           SeatsOf(seats), reader, matchArena);
        other = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(second.second), 2, reader);

        std::string error;
        auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, *ShippedContent(), error);
        REQUIRE_MESSAGE(presentation.has_value(), error);
        this->presentation = std::make_shared<const PC::PresentationContent>(std::move(*presentation));
        PC::ClientOptions options{ShippedContent(), this->presentation, 0, 1};
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
        render.build = [](O::WorldSetup& setup) {
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
        start.gateway = gateway.value_or(GYO::Engine::OS::DatagramAddress::Loopback(1));
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
    void Frame() {
        const bool step = frame++ % framesPerStep == 0;
        if (step) REQUIRE(static_cast<bool>(match->RunFrame()));
        auto ran = client->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
        // The scene changes between two frames, as sdl_window's runner does (SDLWindowBetweenFrames).
        if (PC::SceneChangeWanted(*client)) {
            auto changed = PC::ChangeScene(*client);
            REQUIRE_MESSAGE(static_cast<bool>(changed), (changed ? std::string() : changed.error().message + "\n" + changed.error().detail));
        }
        if (step && other) REQUIRE(static_cast<bool>(other->RunFrame()));
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
    setup.Frames(480);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
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
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
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
    // W held from here on: every step's input walks forward while the controls are fresh.
    setup.Next().keys.push_back(Key(I::KeyCode::W));
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
    setup.Frames(10);
    CHECK(PC::StatusOf(setup.Client()).connected);
    CHECK(PlayerOf(GameWorld(*setup.match), 1) != 0);
}

// The Sim World lent: the Frame World runs several frames between two of its rounds, and the presses of those frames
// add up in the exchange (a click in an early frame is not overwritten by the later frames' controls).
TEST_CASE("client: with the Sim World lent, a click among several frames between two of its rounds fires once") {
    Setup setup;
    setup.other.reset();
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const K::EntityId own = PW::OwnPlayer(setup.Sim());
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

// The Match plays another arena than the one the Client starts with: the Frame World follows the Sim World's.
TEST_CASE("client: the Frame World follows the arena the Match plays") {
    Setup setup(1, 4, 1);
    setup.Frames(120);
    REQUIRE(PC::StatusOf(setup.Client()).playing);
    const PW::InstalledArena& played = ShippedContent()->arenas[1];
    REQUIRE(setup.Sim().Resource<PW::GameRules>()->arena.id == played.content.id);
    CHECK_MESSAGE(setup.Client().Resource<PW::GameRules>()->arena.id == played.content.id, "[pvp-frame-arena] the Frame World's rules");
    CHECK(setup.Client().Resource<PW::GameRules>()->wallsDigest == played.wallsDigest);
    bool drawn = false;
    for (const K::EntityId id : setup.Client().Entities()) {
        const auto* mesh = setup.Client().Get<R::Mesh3d>(id);
        if (mesh == nullptr) continue;
        const auto* image = std::get_if<std::shared_ptr<const GYO::Engine::Image::ImageData>>(&mesh->image);
        if (image != nullptr && *image == setup.presentation->arenaPalettes[1]) drawn = true;
    }
    CHECK_MESSAGE(drawn, "[pvp-frame-arena] the played arena's picture is drawn");
}

TEST_CASE("client: the other player's stand-in goes when it leaves; closing tells the Match at once") {
    Setup setup;
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
    setup.Frames(20);
    CHECK_MESSAGE(PlayerOf(GameWorld(*setup.match), 2) == 0, "the Match took the player off without waiting for the timeout");
    CHECK_MESSAGE(PlayerOf(setup.Client(), 2) == 0, "[pvp-frame-mirror] the Frame World's copy went with the Sim World's player");
    CHECK(BodiesAt(setup.Client(), at) == 0);
}

TEST_CASE("client: the first-person weapon's idle pose sits before the viewmodel camera, low on the right") {
    std::string error;
    const auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, *ShippedContent(), error);
    REQUIRE_MESSAGE(presentation.has_value(), error);
    const PC::ViewmodelDefinition& viewmodel = presentation->viewmodel;
    const auto& model = *viewmodel.model;
    std::vector<GYO::Engine::Model::LocalTransform> local(model.nodes.size());
    GYO::Engine::Model::SampleClip(model.nodes, model.clips[viewmodel.idle], 0.0F, GYO::Engine::Model::Wrap::Loop, local);
    std::vector<Math::Matrix4> nodes(local.size());
    GYO::Engine::Model::ComputeModelSpace(model.nodes, local, nodes);
    const Math::Matrix4 toCamera = T::ToMatrix(PC::ViewmodelPlacement(viewmodel));
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

TEST_CASE("client: the menu goes to the lobby and back; the Render World stays and is told of each new Frame World") {
    Setup setup(1, 4, 0, PC::SceneChange{});
    setup.Load();
    REQUIRE(setup.Scene().scene == PC::Scene::Menu);
    CHECK(setup.Binding("status") == "モードを選んでください");
    CHECK_MESSAGE(!HasResource<PW::GameRules>(setup.Sim()), "[pvp-menu-sim-empty] the menu's Sim World plays nothing");
    CHECK_MESSAGE(setup.client->Buffer().Find<N::NetService>()->Client() == nullptr, "[pvp-menu-no-connection]");
    const K::World* render = &setup.client->World("Render").GetWorld();
    const std::uint64_t swaps = render->Resource<O::ExtractSourceSwaps>()->count;
    // Settings is not there yet: the status says so.
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    CHECK(setup.Scene().scene == PC::Scene::Menu);
    CHECK(setup.Binding("status") == "準備中です");
    // Back to multiplayer (Shift+Tab), Enter: the lobby.
    I::KeyboardInput back = Key(I::KeyCode::Tab);
    back.modifiers.Add(I::KeyModifier::Shift);
    setup.Next().keys.push_back(back);
    setup.Frames(1);
    setup.Press(I::KeyCode::Enter);
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
        for (int i = 0; i < 60 && !PC::StatusOf(setup.Client()).playing; ++i) {
            setup.Frame();
            socketless = socketless && noSocket();
        }
        REQUIRE_MESSAGE(PC::StatusOf(setup.Client()).playing, "[pvp-solo-playing] the own player reached the Frame World");
        // The loading screen until the game is loaded and drawn; then the Local World starts.
        CHECK_MESSAGE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas == "loading", "[pvp-loading-shown]");
        REQUIRE_MESSAGE(FramesUntil(setup, [&] { return PC::StatusOf(setup.Client()).started; }), "[pvp-solo-started]");
        CHECK(PC::StatusOf(setup.Client()).ready);
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
        const auto* overlay = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
        REQUIRE(overlay != nullptr);
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
    CHECK_MESSAGE(setup.Notes().failure.empty(), "[pvp-solo-documents] every scene's document was shown: " << setup.Notes().failure);
}

TEST_CASE("client: leaving the game closes the connection; a new game connects again and sees nothing of the last") {
    Setup setup;
    for (int i = 0; i < 120 && !PC::StatusOf(setup.Client()).playing; ++i) setup.Frame();
    REQUIRE(PC::StatusOf(setup.Client()).playing);
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
    for (int i = 0; i < 120 && !PC::StatusOf(setup.Client()).playing; ++i) setup.Frame();
    CHECK_MESSAGE(PC::StatusOf(setup.Client()).playing, "[pvp-scene-reconnects] the new game is played");
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
    };

    FakeGateway() {
        REQUIRE(listener_.Ok());
        thread_ = std::thread([this] { Serve(); });
    }
    ~FakeGateway() {
        stop_ = true;
        thread_.join();
    }
    [[nodiscard]] GYO::Engine::OS::DatagramAddress Address() const { return listener_.Address(); }
    [[nodiscard]] std::vector<Seen> Requests() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return seen_;
    }
    // The open room of another user the lobby lists (and the user can join).
    void OfferRoom(const std::uint64_t id) {
        const std::lock_guard<std::mutex> lock(mutex_);
        other_ = id;
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
                           (started_ ? "started" : "open") + "\",\"owner\":7,\"members\":[{\"userId\":7,\"ready\":" + (ready_ ? "true" : "false") + "}]";
        if (withConnect && started_) json += ",\"connect\":{\"server\":\"127.0.0.1:47000\",\"userId\":7,\"token\":\"00ff\"}";
        return json + "}";
    }
    std::pair<int, std::string> Route(const std::string& method, const std::string& target, const std::string& body) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (method == "POST" && target == "/v1/session") return {200, "{\"userId\":7,\"sessionToken\":\"t" + std::to_string(++sessions_) + "\",\"expiresAt\":99}"};
        if (method == "GET" && target == "/v1/rooms") {
            if (std::exchange(expire_, false)) return {401, "{\"error\":\"session_expired\"}"};
            std::string rooms;
            if (other_ != 0) rooms = "{\"id\":" + std::to_string(other_) + ",\"mode\":\"survival\",\"map\":\"pvp_corners_v1\",\"players\":2,\"capacity\":4,\"state\":\"open\"}";
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
            return {200, "{\"id\":" + std::to_string(other_) + ",\"mode\":\"survival\",\"map\":\"pvp_corners_v1\",\"players\":3,\"capacity\":4,\"state\":\"open\",\"owner\":9,\"members\":[]}"};
        }
        if (target.starts_with("/v1/rooms/" + std::to_string(other_)) && other_ != 0 && method == "GET") {
            return {200, "{\"id\":" + std::to_string(other_) + ",\"mode\":\"survival\",\"map\":\"pvp_corners_v1\",\"players\":3,\"capacity\":4,\"state\":\"open\",\"owner\":9,\"members\":[{\"userId\":9,\"ready\":true},{\"userId\":7,\"ready\":false}]}"};
        }
        if (method == "GET" && target == "/v1/rooms/3") return {200, RoomJson(true)};
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
                seen_.push_back({method, target, Header(*request, "authorization"), Header(*request, "idempotency-key")});
            }
            const auto [status, answer] = Route(method, target, body);
            (void)listener_.Send(Answer(status, answer));
            listener_.CloseAccepted();
        }
    }

    GYO::Test::LoopbackListener listener_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::vector<Seen> seen_;
    std::uint64_t other_ = 0;
    bool expire_ = false;
    int sessions_ = 0;
    std::string mode_ = "coop";
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

std::string ListLine(Setup& setup, const std::size_t row) {
    const auto* bindings = setup.Client().Get<GYO::Engine::Ui::UiBindings>(setup.Root());
    if (bindings == nullptr) return {};
    const auto* value = bindings->Find("rooms");
    const auto* list = value != nullptr ? std::get_if<GYO::Engine::Ui::UiList>(value) : nullptr;
    if (list == nullptr || row >= list->items.size()) return {};
    const auto found = list->items[row].fields.find("line");
    const auto* text = found != list->items[row].fields.end() ? std::get_if<std::string>(&found->second) : nullptr;
    return text != nullptr ? *text : std::string();
}

} // namespace

TEST_CASE("client: gateway: the requests carry the session's token, an idempotency key on each change, and the answers read") {
    std::optional<PC::GatewaySession> session = PC::GatewaySession{7, "tok", 99};
    const auto create = PC::ToHttp(PC::CreateRoomRequest("coop", "pvp_training_v1", "k1"), "127.0.0.1:7700", session);
    CHECK(create.method == "POST");
    CHECK(create.target == "/v1/rooms");
    CHECK(create.host == "127.0.0.1:7700");
    CHECK(create.body == "{\"mode\":\"coop\",\"map\":\"pvp_training_v1\"}");
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
    const auto room = PC::ParseRoom("{\"id\":3,\"mode\":\"coop\",\"map\":\"m\",\"players\":1,\"capacity\":4,\"state\":\"started\",\"owner\":7,"
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
    // Make a room: the creation is the sixth of the focus order (join, prev, next, mode, map, create).
    for (int i = 0; i < 5; ++i) setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::Room; }), "[pvp-gw-create] " << setup.Binding("status"));
    CHECK(setup.Scene().room == 3);
    CHECK(Asked(gateway, "POST", "/v1/rooms"));
    setup.Load();
    // Ready (the first button), then start (the second): the game, connected where the started room says.
    REQUIRE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備する"; }));
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Binding("ready") == "準備をやめる"; }), "[pvp-gw-ready] " << setup.Binding("status"));
    setup.Press(I::KeyCode::Tab);
    setup.Press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(FramesUntil(setup, [&] { return setup.Scene().scene == PC::Scene::MultiGame; }), "[pvp-gw-start] " << setup.Binding("status"));
    REQUIRE(setup.Scene().connect.has_value());
    const auto* server = std::get_if<GYO::Engine::OS::DatagramAddress>(&setup.Scene().connect->server);
    REQUIRE(server != nullptr);
    CHECK_MESSAGE(server->ToString() == "127.0.0.1:47000", "[pvp-gw-connect]");
    CHECK(setup.Scene().connect->userId == 7);
    CHECK(setup.Scene().connect->token == std::vector<std::byte>{std::byte{0x00}, std::byte{0xff}});
    CHECK(setup.Scene().room == 3);
    CHECK_MESSAGE(HasResource<N::NetConnectTarget>(setup.Sim()), "[pvp-scene-sim-target] the game's Sim World has the target");
    CHECK_MESSAGE(setup.Sim().Resource<PW::LocalUser>()->userId == 7, "[pvp-gw-user] the player is the Gateway's user");
    CHECK(setup.Client().Resource<PW::LocalUser>()->userId == 7);
    // Every call after the session's carried its token; every change its own key.
    for (const FakeGateway::Seen& seen : gateway.Requests()) {
        if (seen.target == "/v1/session") continue;
        CHECK_MESSAGE(seen.authorization == "Bearer t1", "[pvp-gw-token] " << seen.method << " " << seen.target);
        if (seen.method == "POST") CHECK_MESSAGE(!seen.key.empty(), "[pvp-gw-keys] " << seen.target);
    }
    // Back in the lobby (as the evacuation asks), the room is left first.
    setup.Client().ResourceMut<PC::SceneRequest>()->next = PC::SceneChange{.scene = PC::Scene::Lobby, .status = "撤退しました", .room = 3};
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
TEST_CASE("network: through the Gateway: a room made, readied and started, played in the Match, evacuated; the save is read") {
    const char* address = std::getenv("GYO_PVP_GATEWAY");
    if (address == nullptr) {
        MESSAGE("GYO_PVP_GATEWAY is not set: nothing to run against");
        return;
    }
    const auto gateway = PC::ParseMatchAddress(address);
    REQUIRE(gateway.has_value());
    TempDirectory directory;
    auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "network assets", .threadCount = 2});
    REQUIRE(static_cast<bool>(started));
    auto pool = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(started).value());
    WA::WorkerPoolExecutor executor(*pool);
    std::string error;
    auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, *ShippedContent(), error);
    REQUIRE_MESSAGE(presentation.has_value(), error);
    PC::ClientOptions options{ShippedContent(), std::make_shared<const PC::PresentationContent>(std::move(*presentation)), 0, 0};
    O::ProductDescription description;
    description.name = "pvp-client";
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    description.options.Set(net);
    PC::ClientStart start;
    start.first = LobbyFirst();
    start.dataDirectory = directory.Utf8();
    start.gateway = *gateway;
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
    assets.executor = &executor;
    description.options.Set(assets);
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    std::unique_ptr<O::Product> client = std::move(built).value();

    const auto frameWorld = [&]() -> K::World& { return client->World("Frame").GetWorld(); };
    const auto scene = [&]() -> const PC::SceneChange& { return client->Buffer().Find<PC::SceneDirector>()->Current(); };
    const auto binding = [&](const std::string_view id) -> std::string {
        for (const K::EntityId e : frameWorld().Entities()) {
            const auto* bindings = frameWorld().Get<GYO::Engine::Ui::UiBindings>(e);
            if (bindings == nullptr) continue;
            const auto* value = bindings->Find(id);
            const auto* text = value != nullptr ? std::get_if<std::string>(value) : nullptr;
            if (text != nullptr) return *text;
        }
        return {};
    };
    // Real time: a frame every 10 ms, the scene changes between frames.
    const auto runUntil = [&](const auto& done, const int seconds) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < until) {
            if (done()) return true;
            auto ran = client->RunFrame();
            REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
            if (PC::SceneChangeWanted(*client)) REQUIRE(static_cast<bool>(PC::ChangeScene(*client)));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return done();
    };
    // The scene's document is laid out (keys before that reach nothing).
    const auto loaded = [&] {
        for (const K::EntityId e : frameWorld().Entities()) {
            const auto* state = frameWorld().Get<GYO::Engine::Ui::UiRootState>(e);
            if (state != nullptr && state->runtime) return true;
        }
        return false;
    };
    const auto press = [&](const I::KeyCode code) {
        frameWorld().ResourceMut<Pending>()->keys.push_back(Key(code));
        for (int i = 0; i < 2; ++i) {
            REQUIRE(static_cast<bool>(client->RunFrame()));
            if (PC::SceneChangeWanted(*client)) REQUIRE(static_cast<bool>(PC::ChangeScene(*client)));
        }
    };
    REQUIRE_MESSAGE(runUntil([&] { return binding("record").find("戦績：") == 0 && binding("record") != "戦績：—"; }, 20),
                    "the lobby read the record: " << binding("record") << " / " << binding("notice"));
    const std::string before = binding("record");
    REQUIRE(runUntil(loaded, 20));
    for (int i = 0; i < 5; ++i) press(I::KeyCode::Tab);
    press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(runUntil([&] { return scene().scene == PC::Scene::Room; }, 20), binding("status"));
    REQUIRE(runUntil(loaded, 20));
    press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(runUntil([&] { return binding("ready") == "準備をやめる"; }, 20), binding("status"));
    press(I::KeyCode::Tab);
    press(I::KeyCode::Enter);
    REQUIRE_MESSAGE(runUntil([&] { return scene().scene == PC::Scene::MultiGame; }, 40), "started: " << binding("status"));
    REQUIRE_MESSAGE(runUntil([&] { return PC::StatusOf(frameWorld()).playing; }, 40),
                    "[pvp-gw-e2e-playing] the own player came from the Match: " << PC::StatusOf(frameWorld()).endReason);
    // Evacuate (the pause's button, as the evacuation asks): the lobby leaves the room; the Match writes the save.
    frameWorld().ResourceMut<PC::SceneRequest>()->next = PC::SceneChange{.scene = PC::Scene::Lobby, .status = "撤退しました", .room = scene().room};
    REQUIRE(runUntil([&] { return scene().scene == PC::Scene::Lobby; }, 5));
    CHECK_MESSAGE(runUntil([&] { return binding("record").find("出撃 1 回") != std::string::npos; }, 40),
                  "[pvp-gw-e2e-saved] the record after one game: " << binding("record") << " (before: " << before << ")");
}

TEST_CASE("client: the loading screen covers the game until it is loaded and drawn; then the Match is told it is ready") {
    Setup setup;
    const auto* overlay = setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root());
    REQUIRE(overlay != nullptr);
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
    REQUIRE(FramesUntil(setup, [&] {
        const K::EntityId own = PlayerOf(GameWorld(*setup.match), 1);
        return own != 0 && GameWorld(*setup.match).Get<PW::PlayerInput>(own) != nullptr &&
               GameWorld(*setup.match).Get<PW::PlayerInput>(own)->ready == 1;
    }));
    // A Match played from the first step: the screen is gone.
    setup.Frames(2);
    CHECK_MESSAGE(setup.Client().Get<GYO::Engine::Ui::UiRoot>(setup.Root())->canvas != "loading", "[pvp-loading-gone]");
}
