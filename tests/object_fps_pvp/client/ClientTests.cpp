// The Client (client/ClientWorld.hpp) without a window or a GPU: its Game World with the Client's Systems, its
// Render World without sdl_gpu (what it would draw is read at the Submit stage), a Match and a second plain Client
// over memory links, the content read from assets/object_fps_pvp through the asset Module. The input and window
// Messages are written by a test System where sdl_window writes them.
#include <doctest/doctest.h>

#include "../world/WorldTestSupport.hpp"

#include "object_fps_pvp/client/ClientWorld.hpp"

#include "engine/audio/Mixer.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/model/Animation.hpp"
#include "engine/net/NetService.hpp"
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
};
void HashValue(K::Hasher&, const Pending&) {}

K::SystemResult WriteEvents(K::ResMut<Pending> pending, K::MessageWriter<I::KeyboardInput> keys,
                            K::MessageWriter<I::MouseButtonInput> buttons, K::MessageWriter<I::CursorMoved> moves,
                            K::MessageWriter<I::MouseMotion> motions, K::MessageWriter<W::WindowFocused> focus,
                            K::MessageWriter<W::WindowResized> sizes) {
    Pending& p = pending.Get();
    for (auto& m : p.keys) keys.Send(m);
    for (auto& m : p.buttons) buttons.Send(m);
    for (auto& m : p.moves) moves.Send(m);
    for (auto& m : p.motions) motions.Send(m);
    for (auto& m : p.focus) focus.Send(m);
    for (auto& m : p.sizes) sizes.Send(m);
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

    explicit Setup(const int perStep = 1) : framesPerStep(perStep) {
        auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "test assets", .threadCount = 2});
        REQUIRE(static_cast<bool>(started));
        pool = std::make_unique<GYO::Engine::Threads::WorkerPool>(std::move(started).value());
        executor = std::make_unique<WA::WorkerPoolExecutor>(*pool);
        auto reader = [c = clock] { return *c; };
        match = BuildMatch(ShippedContent(), std::make_shared<HubLink>(std::vector<N::MemoryLink>{first.first, second.first}),
                           std::make_shared<PW::Seats>(), reader);
        other = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(second.second), 2, reader);

        std::string error;
        auto presentation = PC::LoadPresentation(PVP_CONTENT_ROOT, *ShippedContent(), error);
        REQUIRE_MESSAGE(presentation.has_value(), error);
        PC::ClientOptions options{ShippedContent(), std::make_shared<const PC::PresentationContent>(std::move(*presentation)), 0, 1};
        O::WorldDescription game = Stepped(PC::ClientGameWorld(options, false), kStep / framesPerStep);
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
        net.serverEndpoint = N::MemoryLink::EndpointA();
        net.userId = 1;
        net.clock = reader;
        description.options.Set(net);
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
    K::World& Client() { return GameWorld(*client); }
    Pending& Next() { return *Client().ResourceMut<Pending>(); }
    void Frame() {
        const bool step = frame++ % framesPerStep == 0;
        if (step) REQUIRE(static_cast<bool>(match->RunFrame()));
        auto ran = client->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
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

I::KeyboardInput Key(const I::KeyCode code, const I::ButtonState state = I::ButtonState::Pressed) {
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
    const K::EntityId own = PW::OwnPlayer(setup.Client());
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
    const K::EntityId own = PW::OwnPlayer(setup.Client());
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
    CHECK(PlayerOf(setup.Client(), 2) == 0);
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
    const K::EntityId own = PW::OwnPlayer(setup.Client());
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
