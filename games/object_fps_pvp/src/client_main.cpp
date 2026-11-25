// The Client: first the lobby (its own Product: a window with the game's gyo.ui document, no net) where the player
// gives the Match's address; then the Client's Product (the game's client World "Game" with sdl_window, the World
// that draws it "Render" with sdl_gpu, sdl_audio for the sound) connected to that address, run by sdl_window's runner
// until its window closes. When the connection ends (refused, timed out, closed by the Match) the lobby comes back
// with the reason; closing a window, or the lobby's Quit, ends the program.
//
//   gyo_object_fps_pvp                         the lobby first
//   gyo_object_fps_pvp --connect A.B.C.D:P     no lobby the first time: connects to the Match at that address
//   gyo_object_fps_pvp --user N                connects with user id N (default: a random one)
//   gyo_object_fps_pvp --frames N              runs one Product for N frames and ends (the tests): the lobby, or the
//                                              Client with --connect
//   gyo_object_fps_pvp --content DIR           reads the content from DIR (default: next to the executable)
//   gyo_object_fps_pvp --verbose               logs the connection, the own player and the drawing driver
//
// Controls: click (or Tab) to take the mouse; W A S D to move, mouse to look, Space to jump, left Ctrl to crouch,
// left click to fire, R to reload; Tab or Esc gives the mouse back.

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/client/LobbyWorld.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/base/Log.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/NetService.hpp"
#include "engine/net/Replication.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/DatagramSocket.hpp"
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

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;
namespace PC = object_fps_pvp::client;
namespace PW = object_fps_pvp::world;

void Log(const GYO::Engine::Base::LogLevel level, const std::string& text) { GYO::Engine::Base::Log(level, GYO_GAME_ID, text); }

// Frames left before the program closes itself (-1: never); a content mismatch closes it at once.
struct ProgramState final {
    std::int64_t framesLeft = -1;
    bool verbose = false;
    PC::ClientStatus shown;
};
void HashValue(K::Hasher&, const ProgramState&) {}

K::SystemResult WatchProgram(K::ExclusiveWorld<> world) {
    auto state = world.Read<ProgramState>([](const ProgramState& s) { return s; });
    if (!state || !state->has_value()) return {};
    ProgramState next = **state;
    bool close = false;
    if (next.framesLeft > 0 && --next.framesLeft == 0) close = true;
    auto mismatch = world.Read<PW::ContentMismatch>([](const PW::ContentMismatch& m) { return m.reason; });
    if (mismatch && mismatch->has_value() && !(*mismatch)->empty() && next.shown.mismatch.empty()) {
        Log(GYO::Engine::Base::LogLevel::Error, "the Match's content differs from this Client's: " + **mismatch);
        next.shown.mismatch = **mismatch;
        close = true;
    }
    auto link = world.Read<N::NetWorldLink>([](const N::NetWorldLink& l) { return l.clientPeer; });
    const bool connected = link && link->has_value() && **link != 0;
    if (connected != next.shown.connected && next.verbose) Log(GYO::Engine::Base::LogLevel::Info, connected ? "connected" : "not connected");
    next.shown.connected = connected;
    // The own player is there once the Match's player with this Client's user id has reached it.
    auto user = world.Read<PW::LocalUser>([](const PW::LocalUser& u) { return u.userId; });
    bool playing = false;
    if (user && user->has_value()) {
        for (const K::EntityId id : world.Entities()) {
            auto owner = world.Read<PW::Owner>(id, [](const PW::Owner& o) { return o.userId; });
            if (owner && owner->has_value() && **owner == **user) playing = true;
        }
    }
    if (playing != next.shown.playing && next.verbose) Log(GYO::Engine::Base::LogLevel::Info, playing ? "playing" : "not playing");
    next.shown.playing = playing;
    auto set = world.SetResource(std::move(next));
    if (!set) return GYO::Engine::Base::Err(std::move(set).error());
    if (close) {
        auto modified = world.Modify<W::WindowRequest>([](W::WindowRequest& request) { request.close = true; });
        if (!modified) return GYO::Engine::Base::Err(std::move(modified).error());
    }
    return {};
}

// The lobby with --frames: closes its window after the frames.
struct FrameBudget final {
    std::int64_t framesLeft = -1;
};
void HashValue(K::Hasher&, const FrameBudget&) {}

K::SystemResult CountFrames(K::ResMut<FrameBudget> budget, K::ResMut<W::WindowRequest> request) {
    if (budget.Read().framesLeft > 0 && --budget->framesLeft == 0) request->close = true;
    return {};
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

// What one run of the Client's Product ended with.
struct ClientRun final {
    int exitCode = 0;          // nonzero: the program ends with it
    bool windowClosed = false; // the player closed the window (the program ends)
    bool mismatch = false;     // the Match's content differs from this Client's
    std::string endReason;     // the connection ended: back to the lobby with this
};

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

// The common Product settings: the window, sdl_window's frame interval, the asset store.
void SetCommonOptions(O::ProductDescription& description, const std::string& contentRoot, WA::WorkerPoolExecutor& executor) {
    W::WindowSettings window;
    window.title = GYO_GAME_DISPLAY_NAME;
    window.size = {1280, 720};
    window.resizable = false;
    description.options.Set(window);
    GYO::Plugins::SDLWindow::SDLWindowOptions sdl;
    sdl.minFrameInterval = std::chrono::milliseconds(4);
    description.options.Set(sdl);
    WA::AssetOptions assets;
    assets.contentRoot = contentRoot;
    assets.executor = &executor;
    description.options.Set(assets);
}

// The lobby: its own Product until the player submits an address, quits or closes the window.
std::optional<PC::LobbyOutcome> RunLobby(const std::string& address, const std::string& status, const std::string& contentRoot,
                                         WA::WorkerPoolExecutor& executor, const std::int64_t frames, int& exitCode) {
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    O::WorldDescription lobby = PC::LobbyGameWorld({address, status});
    if (frames > 0) {
        lobby.build = [inner = std::move(lobby.build), frames](O::WorldSetup& setup) {
            inner(setup);
            setup.Registrar().Resource<FrameBudget>({.name = "FrameBudget"});
            K::SystemOptions count;
            count.name = "CountFrames";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(O::DefaultSchedules::update);
            count.mounts.push_back(std::move(mount));
            setup.Registrar().AddSystem<&CountFrames>(std::move(count));
            setup.Prepare([frames](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(FrameBudget{frames});
                return {};
            });
        };
    }
    description.worlds.push_back(std::move(lobby));
    description.worlds.push_back(PC::LobbyRenderWorld());
    description.configure = [](O::ProductSetup& setup) { PC::ConfigureLobby(setup); };
    SetCommonOptions(description, contentRoot, executor);
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        LogStartupReport(built.error(), "the lobby");
        exitCode = 1;
        return std::nullopt;
    }
    std::unique_ptr<O::Product> product = std::move(built).value();
    const O::RunnerExit exit = product->Run();
    if (exit.code == O::RunnerExitCode::Failure) {
        Log(GYO::Engine::Base::LogLevel::Error, exit.reason);
        exitCode = 1;
        return std::nullopt;
    }
    const auto* result = product->World("Game").GetWorld().Resource<PC::LobbyOutcome>();
    if (result == nullptr) {
        exitCode = 1;
        return std::nullopt;
    }
    // A lobby that was never shown is a blank window: say why (ui page section 9 point 8).
    if (!result->failure.empty()) Log(GYO::Engine::Base::LogLevel::Error, result->failure);
    exitCode = CheckDrawing(*product, frames, exit);
    if (frames > 0 && exitCode == 0 && !result->shown) {
        Log(GYO::Engine::Base::LogLevel::Error, "the lobby was not shown in " + std::to_string(frames) + " frames");
        exitCode = 1;
    }
    return *result;
}

// The Client's Product connected to server, until its window closes (by the player, or when the connection ended).
ClientRun RunClient(const GYO::Engine::OS::DatagramAddress& server, const std::uint64_t userId,
                    const std::shared_ptr<const PW::GameContent>& game, const std::shared_ptr<const PC::PresentationContent>& presentation,
                    const std::string& contentRoot, WA::WorkerPoolExecutor& executor, const std::int64_t frames, const bool verbose) {
    ClientRun run;
    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    description.options.Set(net);
    PC::ClientOptions options{game, presentation, 0, userId};
    O::WorldDescription gameWorld = PC::ClientGameWorld(options);
    // Whom it connects to (net-connect-target: a client World's NetConnectTarget, put by its prepare function; the
    // lobby has none, so it connects to nothing).
    gameWorld.prepare.push_back([target = N::NetConnectTarget{server, userId, {}}](K::World& w) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(target);
        return {};
    });
    gameWorld.build = [inner = std::move(gameWorld.build), frames, verbose](O::WorldSetup& setup) {
        inner(setup);
        setup.Registrar().Resource<ProgramState>({.name = "ProgramState"});
        K::SystemOptions watch;
        watch.name = "WatchProgram";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        watch.mounts.push_back(std::move(mount));
        setup.Registrar().AddSystem<&WatchProgram>(std::move(watch));
        setup.Prepare([frames, verbose](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(ProgramState{frames, verbose, {}});
            return {};
        });
    };
    description.worlds.push_back(std::move(gameWorld));
    description.worlds.push_back(PC::ClientRenderWorld());
    description.configure = [](O::ProductSetup& setup) { PC::ConfigureClient(setup); };
    SetCommonOptions(description, contentRoot, executor);

    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        LogStartupReport(built.error(), "the game");
        run.exitCode = 1;
        return run;
    }
    std::unique_ptr<O::Product> product = std::move(built).value();
    if (const auto* output = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLAudio::AudioOutput>()) {
        if (const auto* failure = output->Failure()) {
            Log(GYO::Engine::Base::LogLevel::Warning, "no sound: " + GYO::Engine::Base::Describe(*failure));
        }
    }
    const auto* gpu = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
    if (gpu != nullptr && verbose) Log(GYO::Engine::Base::LogLevel::Info, "drawing on the " + gpu->DriverName() + " GPU driver");
    if (verbose) Log(GYO::Engine::Base::LogLevel::Info, "connecting to " + server.ToString() + " as user " + std::to_string(userId));
    const O::RunnerExit exit = product->Run();
    K::World& world = product->World("Game").GetWorld();
    const PC::ClientStatus status = PC::StatusOf(world);
    // The window closed: tell the Match now (the disconnect packets, sent at once), so it takes the player off
    // without waiting for the timeout (lightyear's netcode client sends its disconnect packets the same way).
    if (auto* service = product->Buffer().Find<N::NetService>()) {
        N::Stamp stamp;
        if (const auto* tick = world.Resource<GYO::Engine::WorldTime::Tick>()) stamp.tick = static_cast<std::uint32_t>(tick->value);
        if (const auto* replica = world.Resource<N::NetReplicaState>()) stamp.worldGeneration = replica->generation.value_or(0);
        service->Close();
        service->Flush(service->Now(), stamp);
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        Log(GYO::Engine::Base::LogLevel::Error, exit.reason);
        run.exitCode = 1;
        return run;
    }
    run.exitCode = CheckDrawing(*product, frames, exit);
    if (!status.mismatch.empty()) {
        run.mismatch = true;
        run.endReason = "Match の内容がこの Client と違います: " + status.mismatch;
    } else if (status.ended) {
        if (verbose) Log(GYO::Engine::Base::LogLevel::Info, "the connection ended: " + status.endReason);
        run.endReason = status.endReason;
    } else {
        run.windowClosed = true;
    }
    return run;
}

int main(int argc, char** argv) {
    std::optional<GYO::Engine::OS::DatagramAddress> connect;
    std::uint64_t userId = 0;
    std::int64_t frames = -1;
    std::optional<std::string> contentRoot;
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
        } else if (argument == "--verbose") {
            verbose = true;
        } else {
            Log(GYO::Engine::Base::LogLevel::Error, "unknown argument '" + std::string(argument) +
                                                        "' (--connect A.B.C.D:P, --user N, --frames N, --content DIR, --verbose)");
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

    // The lobby, then the Client with the address it gave; back to the lobby when the connection ends. With --frames
    // one Product runs and the program ends (the lobby, or the Client with --connect).
    std::string address = connect ? connect->ToString() : "127.0.0.1:40000";
    std::string status;
    std::optional<GYO::Engine::OS::DatagramAddress> server = connect;
    while (true) {
        if (!server) {
            int exitCode = 0;
            const auto outcome = RunLobby(address, status, *contentRoot, executor, frames, exitCode);
            if (!outcome || exitCode != 0) return exitCode != 0 ? exitCode : 1;
            if (frames > 0 || !outcome->server) return 0;  // --frames, Quit or the window closed
            server = outcome->server;
            address = outcome->address;
        }
        const ClientRun run = RunClient(*server, userId, sharedGame, sharedPresentation, *contentRoot, executor, frames, verbose);
        if (run.exitCode != 0) return run.exitCode;
        if (frames > 0) return run.mismatch ? 1 : 0;  // a test's run: a content mismatch fails it, as before the lobby
        if (run.windowClosed) return 0;
        status = run.endReason;
        server.reset();
    }
}
