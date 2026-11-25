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
#include "object_fps_pvp/client/Scenes.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/base/Log.hpp"
#include "engine/input/Input.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/ui/UiRuntime.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/NetService.hpp"
#include "engine/net/Replication.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/Files.hpp"
#include "engine/os/SocketAddress.hpp"
#include "engine/os/Process.hpp"
#include "engine/threads/WorkerPool.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/WorkerPoolExecutor.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"
#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
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

// --verbose: the connection and the own player as they change (the game scene's ClientView), and the arena the game
// plays (the Frame World's rules: the room's map at first, then the Match's).
struct VerboseWatch final {
    bool connected = false;
    bool playing = false;
    std::string arena;
};
void HashValue(K::Hasher&, const VerboseWatch&) {}

K::SystemResult WatchArena(K::Res<PW::GameRules> rules, K::ResMut<VerboseWatch> seen) {
    if (rules->arena.id == seen.Read().arena) return {};
    Log(GYO::Engine::Base::LogLevel::Info, "the game plays arena " + rules->arena.id);
    seen->arena = rules->arena.id;
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

using WatchedRoots = K::Query<K::Read<GYO::Engine::Ui::UiRoot>, K::Read<GYO::Engine::Ui::UiRootState>, K::Read<GYO::Engine::Ui::UiBindings>>;

// What a press at (x, y) of the window lands on: the topmost interactive element of a root that takes input, as
// world_ui hit tests it (its viewport's physical pixels); "nothing" when no element is there.
std::string HitAt(WatchedRoots& roots, const GYO::Engine::Math::Vec2 at) {
    std::string hit = "nothing";
    roots.ForEach([&](K::EntityId, const GYO::Engine::Ui::UiRoot& root, const GYO::Engine::Ui::UiRootState& state, const GYO::Engine::Ui::UiBindings& bindings) {
        if (!state.runtime || !root.acceptsInput) return;
        const auto layout = state.runtime->EvaluateLayout(bindings.Table(), state.viewport);
        if (!layout) return;
        const GYO::Engine::Math::Vec2 point{at.x * state.scaleFactor - state.viewportPosition.x, at.y * state.scaleFactor - state.viewportPosition.y};
        if (const auto* element = GYO::Engine::Ui::HitTestUiLayout(*layout, point, true)) hit = "'" + element->id + "' of canvas '" + root.canvas + "'";
    });
    return hit;
}

std::string Milliseconds(const std::int64_t nanoseconds) {
    const std::int64_t tenths = nanoseconds / 100'000;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " ms";
}

K::SystemResult WatchFrame(K::Res<WT::RealTime> time, K::MessageReader<W::WindowFocused> focus, K::MessageReader<W::WindowModeChanged> modes,
                           K::MessageReader<I::InputEventsDropped> dropped,
                           K::MessageReader<I::MouseButtonInput> buttons, K::Res<I::ButtonInput<I::MouseButton>> pressed,
                           K::Res<I::CursorPosition> cursor, K::MessageReader<GYO::Engine::Ui::UiAction> actions,
                           WatchedRoots roots, K::ResMut<PC::GatewayClient> gateway, K::ResMut<FrameWatch> watch) {
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
                    (cursor->position ? "known" : "unknown") + ", hits " + HitAt(roots, button->position);
        }
        Log(GYO::Engine::Base::LogLevel::Info, line + at);
    }
    for (const GYO::Engine::Ui::UiAction* action : actions.Read()) Log(GYO::Engine::Base::LogLevel::Info, "ui: " + action->action + at);
    roots.ForEach([&](const K::EntityId id, const GYO::Engine::Ui::UiRoot& root, const GYO::Engine::Ui::UiRootState& state,
                      const GYO::Engine::Ui::UiBindings&) {
        if (!state.runtime) return;
        const void* document = state.runtime->Document().get();
        for (FrameWatch::Seen& seen : watch->laidOut) {
            if (seen.root != id || seen.canvas != root.canvas) continue;
            if (seen.document == document) return;
            Log(GYO::Engine::Base::LogLevel::Info, "ui: canvas '" + root.canvas + "' laid out again (a new document)" + at);
            seen.document = document;
            return;
        }
        Log(GYO::Engine::Base::LogLevel::Info, "ui: canvas '" + root.canvas + "' laid out" + at);
        watch->laidOut.push_back(FrameWatch::Seen{id, root.canvas, document});
    });
    for (const PC::GatewayCallRecord& call : gateway->TakeCalls()) {
        const std::string answer = call.status != 0 ? std::to_string(call.status) + (call.errorCode.empty() ? "" : " " + call.errorCode) : call.errorCode;
        Log(GYO::Engine::Base::LogLevel::Info, "gateway: " + call.method + " " + call.target + " (" + call.tag + (call.session ? ", its session" : "") +
                                                   ") -> " + answer + " in " + Milliseconds(call.took) + ", waited " + Milliseconds(call.waited) + at);
    }
    return {};
}

std::string SceneName(const PC::Scene scene) {
    switch (scene) {
    case PC::Scene::Menu: return "menu";
    case PC::Scene::SoloRoom: return "single player room";
    case PC::Scene::SoloGame: return "single player game";
    case PC::Scene::Lobby: return "lobby";
    case PC::Scene::Room: return "room";
    case PC::Scene::MultiGame: return "multiplayer game";
    }
    return "?";
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
               WA::WorkerPoolExecutor& executor, const bool verbose) {
    const std::int64_t frames = start.frames;
    const GYO::Engine::Math::Vec2i resolution = start.settings.resolution;
    const W::WindowMode windowMode = start.settings.window;
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    description.options.Set(net);
    description.options.Set(std::move(start));
    PC::ClientOptions options{game, presentation, 0, userId};
    O::WorldDescription frameWorld = PC::ClientFrameWorld(options);
    // The loading screen's warm-up reads what sdl_gpu has shown (DrawnFrames), between the Frame World's rounds.
    frameWorld.build = [inner = std::move(frameWorld.build)](O::WorldSetup& setup) {
        inner(setup);
        const auto* gpu = std::as_const(setup.ProductBuffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
        if (gpu == nullptr) return;
        setup.ExchangeIn([gpu](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            const PC::DrawnFrames now{true, gpu->FramesShown(), gpu->SkippedDraws()};
            const auto* drawn = w.Resource<PC::DrawnFrames>();
            if (drawn == nullptr || drawn->shown != now.shown || drawn->skipped != now.skipped || !drawn->known) w.SetResource(now);
            return {};
        });
    };
    if (verbose) {
        frameWorld.build = [inner = std::move(frameWorld.build)](O::WorldSetup& setup) {
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
    description.worlds.push_back(PC::ClientRenderWorld());
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
    sdl.betweenFrames.run = [](O::Product& product) { return PC::ChangeScene(product); };
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
    const O::RunnerExit exit = product->Run();
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
    if (verbose && !notes.endReason.empty()) Log(GYO::Engine::Base::LogLevel::Info, "the last connection ended: " + notes.endReason);
    int code = CheckDrawing(*product, frames, exit);
    if (frames > 0 && code == 0) {
        // A test's run: a content mismatch fails it; a scene with a document must have shown it.
        if (!notes.mismatch.empty()) code = 1;
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
    auto presentation = PC::LoadPresentation(*contentRoot, *sharedGame, error);
    if (!presentation) {
        Log(GYO::Engine::Base::LogLevel::Error, "the content did not load: " + error);
        return 1;
    }
    const auto sharedPresentation = std::make_shared<const PC::PresentationContent>(std::move(*presentation));
    auto pool = GYO::Engine::Threads::WorkerPool::Start({.name = "object_fps_pvp assets", .threadCount = 2});
    if (!pool) {
        Log(GYO::Engine::Base::LogLevel::Error, "the asset threads did not start: " + GYO::Engine::Base::Describe(pool.error()));
        return 1;
    }
    WA::WorkerPoolExecutor executor(*pool);

    PC::ClientStart start;
    start.frames = frames;
    start.gateway = gateway;
    // This player's files: <the user's data directory>/<profile>. None found: nothing is saved (the scenes say so).
    if (auto data = GYO::Engine::OS::Files::UserDataDirectory("GYO", GYO_GAME_ID)) {
        start.dataDirectory = *data + "/" + profile;
        // The settings are the machine's, not the profile's.
        start.settingsPath = *data + "/settings.json";
        PC::LoadedSettings loaded = PC::LoadSettings(start.settingsPath);
        if (!loaded.problem.empty()) Log(GYO::Engine::Base::LogLevel::Warning, "the settings were not read, the defaults are used: " + loaded.problem);
        start.settings = loaded.settings;
    } else {
        Log(GYO::Engine::Base::LogLevel::Warning, "no user data directory, nothing will be saved: " + GYO::Engine::Base::Describe(data.error()));
    }
    if (connect) {
        start.first = PC::SceneChange{.scene = PC::Scene::MultiGame, .connect = N::NetConnectTarget{*connect, userId, {}}};
    }
    return RunProduct(std::move(start), userId, sharedGame, sharedPresentation, *contentRoot, executor, verbose);
}
