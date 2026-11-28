// The Client: one Product for the whole program (game-flow declaration 3.1; client/Scenes.hpp), its Worlds replaced
// when the scene changes, the window open throughout: "Solo" (the single player's game, no net, on its own thread),
// "Sim" (the game's client World, on its own thread; it plays only in a game connected to a Match), "Frame" (sdl_window: the scene the player sees -- the menu, the lobby, the game),
// "Render" (extracts from it, sdl_gpu) and sdl_audio for the sound. sdl_window's runner changes the scene between two
// frames (SDLWindowBetweenFrames) and runs until the window closes (the menu's Quit, or closing it).
//
//   gyo_object_fps_pvp                         the menu first
//   gyo_object_fps_pvp --connect A.B.C.D:P     starts in a game connected to the Match at that address
//   gyo_object_fps_pvp --user N                with --connect: the user id (default: a random one; through the
//                                              Gateway the user id is the Gateway's)
//   gyo_object_fps_pvp --frames N              closes itself after N frames (the tests): the menu, or the game with
//                                              --connect; a frame must have been shown
//   gyo_object_fps_pvp --content DIR           reads the content from DIR (default: next to the executable)
//   gyo_object_fps_pvp --gateway A.B.C.D:P     the Gateway multiplayer talks to (default 127.0.0.1:7700)
//   gyo_object_fps_pvp --profile NAME          keeps this player's files (the single player's save) under NAME in
//                                              the user's data directory (default: "default"): the single player's
//                                              save and the device id the Gateway knows this player by; two Clients
//                                              on one machine take two names
//   gyo_object_fps_pvp --verbose               logs the connection, the own player and the drawing driver
//
// Controls: click (or Tab) to take the mouse; W A S D to move, mouse to look, Space to jump, left Ctrl to crouch,
// left click to fire, R to reload; Tab or Esc gives the mouse back.

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/client/LocalFiles.hpp"
#include "object_fps_pvp/client/Scenes.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/base/Log.hpp"
#include "engine/input/Input.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/ui/UiRuntime.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/mesh_render/MeshRenderModule.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/NetService.hpp"
#include "engine/net/Replication.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/Files.hpp"
#include "engine/os/Machine.hpp"
#include "engine/os/SocketAddress.hpp"
#include "engine/os/Process.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"
#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;
namespace I = GYO::Engine::Input;
namespace PC = object_fps_pvp::client;
namespace PW = object_fps_pvp::world;

void Log(const GYO::Engine::Base::LogLevel level, const std::string& text) { GYO::Engine::Base::Log(level, GYO_GAME_ID, text); }

// --verbose: the connection and the own player as they change (the game scene's ClientView), the arena the game
// plays (the Frame World's rules) and when its map is ready.
struct VerboseWatch final {
    bool connected = false;
    bool playing = false;
    std::string arena;
    bool mapReady = false;
    std::chrono::steady_clock::time_point since = std::chrono::steady_clock::now();
};
void HashValue(K::Hasher&, const VerboseWatch&) {}

K::SystemResult WatchArena(K::Res<PW::GameRules> rules, K::Res<PW::MapLoad> map, K::Res<PC::ClientView> view, K::ResMut<VerboseWatch> seen) {
    if (rules->arena.id != seen.Read().arena) {
        Log(GYO::Engine::Base::LogLevel::Info, "the game plays arena " + rules->arena.id);
        seen->arena = rules->arena.id;
    }
    // Ready: this World's picture and the Sim (or Solo) World's walls.
    if (!seen.Read().mapReady && map->stage == PW::MapStage::Done && view->mapLoad.stage == PW::MapStage::Done) {
        seen->mapReady = true;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - seen.Read().since);
        Log(GYO::Engine::Base::LogLevel::Info, "the map is ready (" + std::to_string(map->total) + " regions, " + std::to_string(ms.count()) + " ms)");
    }
    return {};
}

K::SystemResult WatchConnection(K::Res<PC::ClientView> view, K::ResMut<VerboseWatch> seen) {
    const bool connected = view->connected;
    const bool playing = view->ownPlayer != 0;
    if (connected != seen.Read().connected) Log(GYO::Engine::Base::LogLevel::Info, connected ? "connected" : "not connected");
    if (playing != seen.Read().playing) Log(GYO::Engine::Base::LogLevel::Info, playing ? "playing" : "not playing");
    if (connected != seen.Read().connected || playing != seen.Read().playing) {
        seen->connected = connected;
        seen->playing = playing;
    }
    return {};
}

// --verbose, every scene: what the player did and what the program did for it, to tell from the log where a moment
// went -- the frames that took long (and in which scene), the window's focus and mode, the events the backend dropped (a full
// queue: every button is let go), the mouse presses and releases that reached the Frame World with what the input
// state made of them (just pressed, the cursor known: what the UI needs to take a click; not in a game, where a click
// is a shot), the UI actions they made, the UI documents laid out (and laid out again: a new runtime), and each call
// to the Gateway (how long it waited behind the others and how long it took).
constexpr std::int64_t kSlowFrame = 100'000'000;

struct FrameWatch final {
    std::string scene;
    bool game = false;
    // The roots seen with their runtime: the entity, the canvas and the document the runtime was made for.
    struct Seen final {
        K::EntityId root{};
        std::string canvas;
        const void* document = nullptr;
    };
    std::vector<Seen> laidOut;
};
void HashValue(K::Hasher&, const FrameWatch&) {}

using WatchedRoots = K::Query<K::Read<GYO::Engine::Ui::UiRoot>, K::Read<GYO::Engine::Ui::UiRootState>>;
using WatchedBindings = K::Query<K::Read<GYO::Engine::Ui::UiBindings>>;

// What the cursor is on as world_ui hit tests a press: the topmost interactive element of each root that takes input
// (world_ui gives a press to every such root), in that root's viewport's physical pixels; "nothing" when none is
// there, or the cursor is not known. The bindings are this frame's as the scenes left them (world_ui laid the press
// out before them: a list that changed in the frame may differ).
std::string HitAt(WatchedRoots& roots, WatchedBindings& bindings, const std::optional<GYO::Engine::Math::Vec2>& cursor) {
    if (!cursor) return "nothing (the cursor is not known)";
    std::string hits;
    roots.ForEach([&](const K::EntityId id, const GYO::Engine::Ui::UiRoot& root, const GYO::Engine::Ui::UiRootState& state) {
        if (!state.runtime || !root.acceptsInput) return;
        const auto* values = bindings.Get<GYO::Engine::Ui::UiBindings>(id);
        const auto layout = state.runtime->EvaluateLayout(values != nullptr ? values->Table() : GYO::Engine::Ui::UiBindingTable{}, state.viewport);
        if (!layout) return;
        const GYO::Engine::Math::Vec2 point{cursor->x * state.scaleFactor - state.viewportPosition.x,
                                            cursor->y * state.scaleFactor - state.viewportPosition.y};
        const auto* element = GYO::Engine::Ui::HitTestUiLayout(*layout, point, true);
        if (element == nullptr) return;
        hits += (hits.empty() ? "" : ", ") + std::string("'") + element->id + "' of canvas '" + root.canvas + "'";
    });
    return hits.empty() ? "nothing" : hits;
}

std::string Milliseconds(const std::int64_t nanoseconds) {
    const std::int64_t tenths = nanoseconds / 100'000;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " ms";
}

K::SystemResult WatchFrame(K::Res<WT::RealTime> time, K::MessageReader<W::WindowFocused> focus, K::MessageReader<W::WindowModeChanged> modes,
                           K::MessageReader<I::InputEventsDropped> dropped,
                           K::MessageReader<I::MouseButtonInput> buttons, K::Res<I::ButtonInput<I::MouseButton>> pressed,
                           K::Res<I::CursorPosition> cursor, K::MessageReader<GYO::Engine::Ui::UiAction> actions,
                           WatchedRoots roots, WatchedBindings bindings, K::ResMut<PC::GatewayClient> gateway, K::ResMut<FrameWatch> watch) {
    const std::string at = " (" + watch.Read().scene + ")";
    if (time->delta > kSlowFrame) Log(GYO::Engine::Base::LogLevel::Info, "slow frame: " + Milliseconds(time->delta) + at);
    for (const W::WindowFocused* event : focus.Read()) {
        Log(GYO::Engine::Base::LogLevel::Info, std::string("window: ") + (event->focused ? "focused" : "not focused") + at);
    }
    for (const W::WindowModeChanged* event : modes.Read()) {
        Log(GYO::Engine::Base::LogLevel::Info, std::string("window: ") + (event->mode == W::WindowMode::BorderlessFullscreen ? "fullscreen" : "windowed") + at);
    }
    for (const I::InputEventsDropped* event : dropped.Read()) {
        Log(GYO::Engine::Base::LogLevel::Info, "input: " + std::to_string(event->count) + " events dropped (a full queue)" + at);
    }
    for (const I::MouseButtonInput* button : buttons.Read()) {
        if (watch.Read().game || button->button != I::MouseButton::Left) continue;
        const bool down = button->state == I::ButtonState::Pressed;
        std::string line = std::string("mouse: ") + (down ? "pressed" : "released") + " at " + std::to_string(static_cast<int>(button->position.x)) +
                           "," + std::to_string(static_cast<int>(button->position.y));
        if (down) {
            line += std::string(", just pressed ") + (pressed->JustPressed(I::MouseButton::Left) ? "yes" : "no") + ", cursor " +
                    (cursor->position ? "known" : "unknown") + ", hits " + HitAt(roots, bindings, cursor->position);
        }
        Log(GYO::Engine::Base::LogLevel::Info, line + at);
    }
    for (const GYO::Engine::Ui::UiAction* action : actions.Read()) Log(GYO::Engine::Base::LogLevel::Info, "ui: " + action->action + at);
    // A root seen with its runtime last frame and not now (taken away, or its runtime dropped) is forgotten: when it
    // comes back, its new runtime is told again.
    std::vector<FrameWatch::Seen> now;
    roots.ForEach([&](const K::EntityId id, const GYO::Engine::Ui::UiRoot& root, const GYO::Engine::Ui::UiRootState& state) {
        if (!state.runtime) return;
        const void* document = state.runtime->Document().get();
        const auto seen = std::find_if(watch.Read().laidOut.begin(), watch.Read().laidOut.end(),
                                       [&](const FrameWatch::Seen& s) { return s.root == id && s.canvas == root.canvas; });
        if (seen == watch.Read().laidOut.end()) {
            Log(GYO::Engine::Base::LogLevel::Info, "ui: canvas '" + root.canvas + "' laid out" + at);
        } else if (seen->document != document) {
            Log(GYO::Engine::Base::LogLevel::Info, "ui: canvas '" + root.canvas + "' laid out again (a new document)" + at);
        }
        now.push_back(FrameWatch::Seen{id, root.canvas, document});
    });
    if (now.size() != watch.Read().laidOut.size() ||
        !std::equal(now.begin(), now.end(), watch.Read().laidOut.begin(),
                    [](const FrameWatch::Seen& a, const FrameWatch::Seen& b) { return a.root == b.root && a.canvas == b.canvas && a.document == b.document; })) {
        watch->laidOut = std::move(now);
    }
    for (const PC::GatewayCallRecord& call : gateway->TakeCalls()) {
        const std::string answer = call.status != 0 ? std::to_string(call.status) + (call.errorCode.empty() ? "" : " " + call.errorCode) : call.errorCode;
        Log(GYO::Engine::Base::LogLevel::Info, "gateway: " + call.method + " " + call.target + " (" + call.tag + (call.session ? ", its session" : "") +
                                                   ") -> " + answer + " in " + Milliseconds(call.took) + ", waited " + Milliseconds(call.waited) + at);
    }
    return {};
}

// --verbose: the main loop's heartbeat. The loop marks where it is as each part of a frame begins -- the Frame World
// (its exchange in, the first thing of its round), the Render World up to the drawing (its exchange in, before its
// extract), sdl_gpu's GPUSubmit (the uploads, the acquire, the draw, the present), a scene change (between frames),
// and the rest between two frames (the window's event pump, the wait) -- and a watcher on its own thread tells when
// one part lasts over a second: while it lasts (a loop that never comes back leaves its line), and about how long it
// lasted. A frame whose parts each take under a second is not told here (slow frame tells it after).
struct Heartbeat final {
    enum class Part : std::uint8_t { Starting, FrameWorld, RenderWorld, Drawing, SceneChange, BetweenFrames };
    std::atomic<Part> part{Part::Starting};
    std::atomic<std::int64_t> since{0};  // steady clock, nanoseconds; 0: not marked yet

    static std::int64_t Now() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    // The time first: a watcher that reads the new part reads its time too (sequentially consistent).
    void Mark(const Part at) {
        since.store(Now());
        part.store(at);
    }
};
struct HeartbeatRef final {
    std::shared_ptr<Heartbeat> beat;
};
void HashValue(K::Hasher&, const HeartbeatRef&) {}

const char* PartName(const Heartbeat::Part part) {
    switch (part) {
    case Heartbeat::Part::Starting: return "the start";
    case Heartbeat::Part::FrameWorld: return "the Frame World";
    case Heartbeat::Part::RenderWorld: return "the Render World before the drawing";
    case Heartbeat::Part::Drawing: return "sdl_gpu's GPUSubmit (uploads, acquire, draw, present)";
    case Heartbeat::Part::SceneChange: return "a scene change";
    case Heartbeat::Part::BetweenFrames: return "between frames (the window's event pump, the wait)";
    }
    return "?";
}

template <Heartbeat::Part At>
K::SystemResult Beat(K::Res<HeartbeatRef> heartbeat) {
    heartbeat->beat->Mark(At);
    return {};
}

// The beat at the start of a World's round: its exchange in runs before its schedules.
void BeatOnExchangeIn(O::WorldSetup& setup, const std::shared_ptr<Heartbeat>& heartbeat, const Heartbeat::Part at) {
    setup.ExchangeIn([heartbeat, at](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        heartbeat->Mark(at);
        return {};
    });
}

// A Beat<At> system named name in the frame schedule's stage (before the systems named in before).
template <Heartbeat::Part At>
void AddBeat(K::Registrar& r, std::string name, const std::string_view stage, std::vector<std::string> before) {
    K::SystemOptions beat;
    beat.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(stage);
    mount.before = std::move(before);
    beat.mounts.push_back(std::move(mount));
    r.AddSystem<&Beat<At>>(std::move(beat));
}

// The Render World's beats in its frame schedule: before GPUSubmit (in the Submit stage) and after it (Cleanup).
void BeatAroundDrawing(O::WorldSetup& setup, const std::shared_ptr<Heartbeat>& heartbeat) {
    K::Registrar& r = setup.Registrar();
    r.Resource<HeartbeatRef>({.name = "HeartbeatRef"});
    AddBeat<Heartbeat::Part::Drawing>(r, "object_fps_pvp.Heartbeat.Drawing", GYO::Engine::RenderWorld::kSubmitStage, {"GPUSubmit"});
    AddBeat<Heartbeat::Part::BetweenFrames>(r, "object_fps_pvp.Heartbeat.BetweenFrames", GYO::Engine::RenderWorld::kCleanupStage, {});
    setup.Prepare([heartbeat](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(HeartbeatRef{heartbeat});
        return {};
    });
}

// The watcher: every 200 ms; a part over a second is told at once, again every five seconds, and when it ends (its
// length as the watcher saw it: up to 200 ms more).
class HeartbeatWatch final {
public:
    explicit HeartbeatWatch(std::shared_ptr<Heartbeat> beat) : beat_(std::move(beat)), thread_([this] { Run(); }) {
        beat_->Mark(Heartbeat::Part::Starting);
    }
    ~HeartbeatWatch() {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        thread_.join();
    }
    HeartbeatWatch(const HeartbeatWatch&) = delete;
    HeartbeatWatch& operator=(const HeartbeatWatch&) = delete;

private:
    void Run() {
        constexpr std::int64_t kStuck = 1'000'000'000;
        constexpr std::int64_t kAgain = 5'000'000'000;
        std::int64_t stuckSince = -1;  // the since of the part told as stuck
        Heartbeat::Part stuckPart = Heartbeat::Part::Starting;
        std::int64_t told = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        while (!wake_.wait_for(lock, std::chrono::milliseconds(200), [this] { return stop_; })) {
            const Heartbeat::Part part = beat_->part.load();
            const std::int64_t since = beat_->since.load();
            const std::int64_t now = Heartbeat::Now();
            if (stuckSince >= 0 && since != stuckSince) {
                Log(GYO::Engine::Base::LogLevel::Info, std::string("heartbeat: going again after about ") + Seconds(since - stuckSince) + " in " +
                                                           PartName(stuckPart));
                stuckSince = -1;
            }
            if (since == 0 || now - since < kStuck) continue;
            if (stuckSince != since) {
                stuckSince = since;
                stuckPart = part;
                told = now;
                Log(GYO::Engine::Base::LogLevel::Info, std::string("heartbeat: one part of the frame for ") + Seconds(now - since) + ", in " + PartName(part));
            } else if (now - told >= kAgain) {
                told = now;
                Log(GYO::Engine::Base::LogLevel::Info, std::string("heartbeat: still in one part after ") + Seconds(now - since) + ", in " + PartName(part));
            }
        }
    }
    static std::string Seconds(const std::int64_t nanoseconds) {
        const std::int64_t tenths = nanoseconds / 100'000'000;
        return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " s";
    }

    std::shared_ptr<Heartbeat> beat_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
    std::thread thread_;
};

std::string SceneName(const PC::Scene scene) {
    switch (scene) {
    case PC::Scene::Menu: return "menu";
    case PC::Scene::Splash: return "start screen";
    case PC::Scene::SoloRoom: return "single player room";
    case PC::Scene::SoloGame: return "single player game";
    case PC::Scene::Lobby: return "lobby";
    case PC::Scene::Room: return "room";
    case PC::Scene::MultiGame: return "multiplayer game";
    }
    return "?";
}

// Threads for one kind of loading work (the map's region files, its regions' pictures): the machine's hardware
// threads, between least and most (std::thread::hardware_concurrency may say 0: not known).
std::size_t LoadingThreads(const std::size_t least, const std::size_t most) {
    const unsigned known = std::thread::hardware_concurrency();
    return std::clamp<std::size_t>(known == 0 ? least : known, least, most);
}

std::optional<std::string> ContentRoot() {
    auto executable = GYO::Engine::OS::Process::ExecutablePath();
    if (!executable) return std::nullopt;
    const std::string& path = *executable;
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return std::nullopt;
    return path.substr(0, slash + 1) + GYO_GAME_ASSETS;
}

template <class T>
bool ParseNumber(const std::string_view text, T& value) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

} // namespace

void LogStartupReport(const O::ProductStartupReport& report, const std::string& what) {
    std::string text = what + " did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail);
    for (const O::UnitReportedError& unit : report.unitErrors) text += "\n" + unit.unit + ": " + unit.code + ": " + unit.message;
    Log(GYO::Engine::Base::LogLevel::Error, text);
}

// The drawing's health after a run; with --frames, a frame must have been shown and none failed.
int CheckDrawing(O::Product& product, const std::int64_t frames, const O::RunnerExit& exit) {
    const auto* gpu = std::as_const(product.Buffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
    const bool drawingFailed = gpu != nullptr && (gpu->FramesFailed() > 0 || gpu->SkippedDraws() > 0 || gpu->RefusedTextureAsks() > 0);
    if (drawingFailed) {
        Log(GYO::Engine::Base::LogLevel::Warning, "drawing: " + std::to_string(gpu->FramesFailed()) + " frames failed, " +
                                                      std::to_string(gpu->SkippedDraws()) + " draws skipped, " +
                                                      std::to_string(gpu->RefusedTextureAsks()) + " textures refused; last: " +
                                                      gpu->LastFailure());
    }
    if (frames > 0 && exit.code != O::RunnerExitCode::Failure) {
        if (gpu == nullptr || gpu->FramesShown() == 0) {
            Log(GYO::Engine::Base::LogLevel::Error, "no frame was shown");
            return 1;
        }
        if (drawingFailed) return 1;
    }
    return 0;
}

// The Client's Product from its first scene, until its window closes.
int RunProduct(PC::ClientStart start, const std::uint64_t userId, const std::shared_ptr<const PW::GameContent>& game,
               const std::shared_ptr<const PC::PresentationContent>& presentation, const std::string& contentRoot,
               WA::WorkerPoolExecutor& executor, WA::WorkerPoolExecutor& compute, const bool verbose) {
    const std::int64_t frames = start.frames;
    const GYO::Engine::Math::Vec2i resolution = start.settings.resolution;
    const W::WindowMode windowMode = start.settings.window;
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    description.options.Set(net);
    description.options.Set(std::move(start));
    PC::ClientOptions options{game, presentation, 0, userId, &compute};
    O::WorldDescription frameWorld = PC::ClientFrameWorld(options);
    // The loading screen's warm-up reads what sdl_gpu has shown (DrawnFrames), between the Frame World's rounds.
    frameWorld.build = [inner = std::move(frameWorld.build)](O::WorldSetup& setup) {
        inner(setup);
        const auto* gpu = std::as_const(setup.ProductBuffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
        if (gpu == nullptr) return;
        const auto* meshes = std::as_const(setup.ProductBuffer()).Find<GYO::Engine::MeshRender::MeshRenderStatus>();
        setup.ExchangeIn([gpu, meshes](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            const PC::DrawnFrames now{true, gpu->FramesShown(), gpu->SkippedDraws(), meshes != nullptr ? meshes->MeshesWaiting() : 0};
            const auto* drawn = w.Resource<PC::DrawnFrames>();
            if (drawn == nullptr || drawn->shown != now.shown || drawn->skipped != now.skipped || drawn->waiting != now.waiting || !drawn->known) {
                w.SetResource(now);
            }
            return {};
        });
    };
    const auto heartbeat = std::make_shared<Heartbeat>();
    if (verbose) {
        frameWorld.build = [inner = std::move(frameWorld.build), heartbeat](O::WorldSetup& setup) {
            inner(setup);
            const auto* director = setup.ProductBuffer().Find<PC::SceneDirector>();
            if (director == nullptr) return;
            const PC::SceneChange& scene = director->Current();
            const auto last = [](std::string name) {
                K::SystemOptions watch;
                watch.name = std::move(name);
                K::Mount mount;
                mount.schedule = std::string(O::DefaultSchedules::frame);
                mount.stage = std::string(O::DefaultSchedules::last);
                watch.mounts.push_back(std::move(mount));
                return watch;
            };
            Log(GYO::Engine::Base::LogLevel::Info, "scene: " + SceneName(scene.scene));
            BeatOnExchangeIn(setup, heartbeat, Heartbeat::Part::FrameWorld);
            setup.Registrar().Resource<FrameWatch>({.name = "FrameWatch"});
            setup.Registrar().AddSystem<&WatchFrame>(last("WatchFrame"));
            const bool game = scene.scene == PC::Scene::SoloGame || scene.scene == PC::Scene::MultiGame;
            setup.Prepare([name = SceneName(scene.scene), game](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(FrameWatch{name, game, {}});
                return {};
            });
            const bool multi = scene.scene == PC::Scene::MultiGame;
            if (!multi && scene.scene != PC::Scene::SoloGame) return;
            Log(GYO::Engine::Base::LogLevel::Info, std::string(multi ? "multiplayer" : "single player") + " game: map '" + scene.map +
                                                       "' chosen" + (multi ? " in room " + std::to_string(scene.room) : std::string()));
            setup.Registrar().Resource<VerboseWatch>({.name = "VerboseWatch"});
            setup.Registrar().AddSystem<&WatchArena>(last("WatchArena"));
            if (multi) setup.Registrar().AddSystem<&WatchConnection>(last("WatchConnection"));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(VerboseWatch{});
                return {};
            });
        };
    }
    description.worlds.push_back(PC::ClientSoloWorld(options));
    description.worlds.push_back(PC::ClientSimWorld(options));
    description.worlds.push_back(std::move(frameWorld));
    O::WorldDescription renderWorld = PC::ClientRenderWorld();
    if (verbose) {
        renderWorld.build = [inner = std::move(renderWorld.build), heartbeat](O::WorldSetup& setup) {
            inner(setup);
            BeatOnExchangeIn(setup, heartbeat, Heartbeat::Part::RenderWorld);
            BeatAroundDrawing(setup, heartbeat);
        };
    }
    description.worlds.push_back(std::move(renderWorld));
    description.configure = [](O::ProductSetup& setup) { PC::ConfigureClient(setup); };

    W::WindowSettings window;
    window.title = GYO_GAME_DISPLAY_NAME;
    // The size and the mode the player's settings give (the size is physical pixels: the window opens at it as logical
    // ones and is resized once it says its scale factor, Scenes.cpp FitWindow; their VSync: the Render World's
    // Presentation, ClientWorld).
    window.size = resolution;
    window.mode = windowMode;
    if (verbose) {
        Log(GYO::Engine::Base::LogLevel::Info, std::string("window: starts ") + (windowMode == W::WindowMode::BorderlessFullscreen ? "fullscreen" : "windowed") +
                                                   " at " + std::to_string(resolution.x) + "x" + std::to_string(resolution.y));
    }
    window.resizable = false;
    description.options.Set(window);
    GYO::Plugins::SDLWindow::SDLWindowOptions sdl;
    sdl.minFrameInterval = std::chrono::milliseconds(4);
    // The Sim World on its own thread (live-frame-sim-thread declaration 4.3): while the OS holds the window (a
    // drag, a resize) the steps and the packets go on, so the Match does not see this Client go quiet.
    sdl.ownThreadWorlds = {"Solo", "Sim"};
    // The scene changes, between two frames (client/Scenes.hpp).
    sdl.betweenFrames.wanted = [](O::Product& product) { return PC::SceneChangeWanted(product); };
    sdl.betweenFrames.run = [heartbeat, verbose](O::Product& product) {
        if (verbose) heartbeat->Mark(Heartbeat::Part::SceneChange);
        auto changed = PC::ChangeScene(product);
        if (verbose) heartbeat->Mark(Heartbeat::Part::BetweenFrames);
        return changed;
    };
    description.options.Set(sdl);
    WA::AssetOptions assets;
    assets.contentRoot = contentRoot;
    assets.executor = &executor;
    description.options.Set(assets);

    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        LogStartupReport(built.error(), "the game");
        return 1;
    }
    std::unique_ptr<O::Product> product = std::move(built).value();
    if (const auto* output = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLAudio::AudioOutput>()) {
        if (const auto* failure = output->Failure()) {
            Log(GYO::Engine::Base::LogLevel::Warning, "no sound: " + GYO::Engine::Base::Describe(*failure));
        }
    }
    const auto* gpu = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
    if (gpu != nullptr && verbose) Log(GYO::Engine::Base::LogLevel::Info, "drawing on the " + gpu->DriverName() + " GPU driver");
    std::optional<HeartbeatWatch> watch;
    if (verbose) watch.emplace(heartbeat);
    const O::RunnerExit exit = product->Run();
    watch.reset();
    // The window closed: tell the Match now (the disconnect packets, sent at once), so it takes the player off
    // without waiting for the timeout (lightyear's netcode client sends its disconnect packets the same way).
    if (auto* service = product->Buffer().Find<N::NetService>(); service != nullptr && product->HasWorld("Sim")) {
        K::World& world = product->World("Sim").GetWorld();
        N::Stamp stamp;
        if (const auto* tick = world.Resource<GYO::Engine::WorldTime::Tick>()) stamp.tick = static_cast<std::uint32_t>(tick->value);
        if (const auto* replica = world.Resource<N::NetReplicaState>()) stamp.worldGeneration = replica->generation.value_or(0);
        service->Close();
        service->Flush(service->Now(), stamp);
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        Log(GYO::Engine::Base::LogLevel::Error, exit.reason);
        return 1;
    }
    const PC::FrameNotes notes = [&] {
        const auto* found = std::as_const(product->World("Frame").GetWorld()).Resource<PC::FrameNotes>();
        return found != nullptr ? *found : PC::FrameNotes{};
    }();
    // A scene whose document was never shown is a blank window: say why (ui page section 9 point 8).
    if (!notes.failure.empty()) Log(GYO::Engine::Base::LogLevel::Error, notes.failure);
    if (!notes.mismatch.empty()) Log(GYO::Engine::Base::LogLevel::Error, "the Match's content differs from this Client's: " + notes.mismatch);
    if (!notes.map.empty()) Log(GYO::Engine::Base::LogLevel::Error, "the map did not load: " + notes.map);
    if (verbose && !notes.endReason.empty()) Log(GYO::Engine::Base::LogLevel::Info, "the last connection ended: " + notes.endReason);
    int code = CheckDrawing(*product, frames, exit);
    if (frames > 0 && code == 0) {
        // A test's run: a content mismatch fails it; a scene with a document must have shown it.
        if (!notes.mismatch.empty() || !notes.map.empty()) code = 1;
        const auto* director = std::as_const(product->Buffer()).Find<PC::SceneDirector>();
        const bool documentScene = director != nullptr && director->Current().scene != PC::Scene::MultiGame;
        if (documentScene && !notes.shown) {
            Log(GYO::Engine::Base::LogLevel::Error, "the scene's document was not shown in " + std::to_string(frames) + " frames");
            code = 1;
        }
    }
    return code;
}

int main(int argc, char** argv) {
    std::optional<GYO::Engine::OS::SocketAddress> connect;
    std::uint64_t userId = 0;
    std::int64_t frames = -1;
    std::optional<std::string> contentRoot;
    std::string profile = "default";
    GYO::Engine::OS::SocketAddress gateway = GYO::Engine::OS::SocketAddress::Loopback(7700);
    bool verbose = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const bool hasValue = index + 1 < argc;
        if (argument == "--connect" && hasValue) {
            const auto address = PC::ParseMatchAddress(argv[++index]);
            if (!address) {
                Log(GYO::Engine::Base::LogLevel::Error, "--connect takes an IPv4 address and a port, A.B.C.D:P");
                return 2;
            }
            connect = *address;
        } else if (argument == "--user" && hasValue) {
            if (!ParseNumber(argv[++index], userId) || userId == 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--user takes a positive whole number");
                return 2;
            }
        } else if (argument == "--frames" && hasValue) {
            if (!ParseNumber(argv[++index], frames) || frames <= 0) {
                Log(GYO::Engine::Base::LogLevel::Error, "--frames takes a positive whole number");
                return 2;
            }
        } else if (argument == "--content" && hasValue) {
            contentRoot = argv[++index];
        } else if (argument == "--gateway" && hasValue) {
            const auto address = PC::ParseMatchAddress(argv[++index]);
            if (!address) {
                Log(GYO::Engine::Base::LogLevel::Error, "--gateway takes an IPv4 address and a port, A.B.C.D:P");
                return 2;
            }
            gateway = *address;
        } else if (argument == "--profile" && hasValue) {
            profile = argv[++index];
            if (profile.empty() || profile == "." || profile == ".." || profile.find_first_of("/\\") != std::string::npos) {
                Log(GYO::Engine::Base::LogLevel::Error, "--profile takes a name without / or \\");
                return 2;
            }
        } else if (argument == "--verbose") {
            verbose = true;
        } else {
            Log(GYO::Engine::Base::LogLevel::Error, "unknown argument '" + std::string(argument) +
                                                        "' (--connect A.B.C.D:P, --user N, --frames N, --content DIR, --gateway A.B.C.D:P, --profile NAME, --verbose)");
            return 2;
        }
    }
    if (userId == 0) {
        std::random_device device;
        userId = (std::uint64_t{device()} << 32 | device()) | 1;
    }
    if (!contentRoot) contentRoot = ContentRoot();
    if (!contentRoot) {
        Log(GYO::Engine::Base::LogLevel::Error, "the executable's directory is unknown, so the content cannot be found");
        return 1;
    }
    std::string error;
    auto game = PW::LoadGameContent(*contentRoot, error);
    if (!game) {
        Log(GYO::Engine::Base::LogLevel::Error, "the content did not load: " + error);
        return 1;
    }
    auto sharedGame = std::make_shared<const PW::GameContent>(std::move(*game));
    auto presentation = PC::LoadPresentation(*contentRoot, error);
    if (!presentation) {
        Log(GYO::Engine::Base::LogLevel::Error, "the content did not load: " + error);
        return 1;
    }
    const auto sharedPresentation = std::make_shared<const PC::PresentationContent>(std::move(*presentation));
    // The asset files (a map's regions each a job) on up to 8 threads, the regions' pictures on up to 4 (one job per
    // region; big-map declaration 17.8).
    auto pool = GYO::Engine::Threads::WorkerPool::Start({.name = "object_fps_pvp assets", .threadCount = LoadingThreads(2, 8)});
    if (!pool) {
        Log(GYO::Engine::Base::LogLevel::Error, "the asset threads did not start: " + GYO::Engine::Base::Describe(pool.error()));
        return 1;
    }
    WA::WorkerPoolExecutor executor(*pool);
    // The map's walls and pictures are made on their own thread, not behind the files the asset threads read (Bevy
    // keeps IoTaskPool and AsyncComputeTaskPool apart: bevy_tasks/src/usages.rs).
    auto computePool = GYO::Engine::Threads::WorkerPool::Start({.name = "object_fps_pvp compute", .threadCount = LoadingThreads(1, 4)});
    if (!computePool) {
        Log(GYO::Engine::Base::LogLevel::Error, "the compute thread did not start: " + GYO::Engine::Base::Describe(computePool.error()));
        return 1;
    }
    WA::WorkerPoolExecutor compute(*computePool);

    PC::ClientStart start;
    start.frames = frames;
    start.gateway = gateway;
    // This player's files: <the place>/<profile>, the settings in <the place> (the machine's, not the profile's). The
    // place is the user's data directory, or beside the program when a file named "portable" is there (PC::DataPlace).
    // None found: nothing is saved (the scenes say so).
    std::optional<std::string> executable;
    if (auto path = GYO::Engine::OS::Process::ExecutablePath()) {
        executable = std::move(*path);
    } else {
        Log(GYO::Engine::Base::LogLevel::Warning, "the program's path is not known, no portable mode: " + GYO::Engine::Base::Describe(path.error()));
    }
    std::optional<std::string> userData;
    if (auto data = GYO::Engine::OS::Files::UserDataDirectory("GYO", GYO_GAME_ID)) {
        userData = std::move(*data);
    } else if (!executable) {
        Log(GYO::Engine::Base::LogLevel::Warning, "no user data directory: " + GYO::Engine::Base::Describe(data.error()));
    }
    const auto exists = [](const std::string& path) {
        const auto found = GYO::Engine::OS::Files::Exists(path);
        return found && *found;
    };
    if (const auto place = PC::ChooseDataPlace(executable, exists, userData)) {
        Log(GYO::Engine::Base::LogLevel::Info, std::string(place->portable ? "portable mode: the files are beside the program, in "
                                                                           : "the files are in the user's data directory, ") +
                                                   place->directory);
        start.dataDirectory = place->directory + "/" + profile;
        start.settingsPath = place->directory + "/settings.json";
        PC::LoadedSettings loaded = PC::LoadSettings(start.settingsPath);
        if (!loaded.problem.empty()) Log(GYO::Engine::Base::LogLevel::Warning, "the settings were not read, the defaults are used: " + loaded.problem);
        start.settings = loaded.settings;
    } else {
        Log(GYO::Engine::Base::LogLevel::Warning, "no place for the files, nothing will be saved");
    }
    // The name this player signs in with: the settings' name, or the machine's host name (display-name declaration
    // 4.1). Not logged: a host name often holds the person's own name (its section 6).
    std::optional<std::string> hostName;
    if (!start.settings.name) {
        if (auto host = GYO::Engine::OS::Machine::HostName()) {
            hostName = std::move(*host);
        } else {
            Log(GYO::Engine::Base::LogLevel::Warning, "the host name is not known, no name is sent: " + GYO::Engine::Base::Describe(host.error()));
        }
    }
    start.name = PC::PlayerNameCandidate(start.settings, hostName);
    if (connect) {
        start.first = PC::SceneChange{.scene = PC::Scene::MultiGame, .connect = N::NetConnectTarget{*connect, userId, {}}};
    } else {
        start.first = PC::SceneChange{.scene = PC::Scene::Splash};
    }
    return RunProduct(std::move(start), userId, sharedGame, sharedPresentation, *contentRoot, executor, compute, verbose);
}
