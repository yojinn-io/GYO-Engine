// The Client: one Product with the game's client World ("Game", with sdl_window) and the World that draws it
// ("Render", with sdl_gpu), sdl_audio for the sound; run by sdl_window's runner until the window closes.
//
//   gyo_object_fps_pvp                         connects to a Match on 127.0.0.1:40000
//   gyo_object_fps_pvp --connect A.B.C.D:P     connects to the Match at that address (the lobby's stand-in: PV12)
//   gyo_object_fps_pvp --user N                connects with user id N (default: a random one)
//   gyo_object_fps_pvp --frames N              closes itself after N frames (the tests)
//   gyo_object_fps_pvp --content DIR           reads the content from DIR (default: next to the executable)
//   gyo_object_fps_pvp --verbose               logs the connection, the own player and the drawing driver
//
// Controls: click (or Tab) to take the mouse; W A S D to move, mouse to look, Space to jump, left Ctrl to crouch,
// left click to fire, R to reload; Tab or Esc gives the mouse back.

#include "object_fps_pvp/client/ClientWorld.hpp"

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

std::optional<GYO::Engine::OS::DatagramAddress> ParseAddress(const std::string_view text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) return std::nullopt;
    std::uint16_t port = 0;
    if (!ParseNumber(text.substr(colon + 1), port) || port == 0) return std::nullopt;
    auto address = GYO::Engine::OS::DatagramAddress::Parse(text.substr(0, colon), port);
    if (!address) return std::nullopt;
    return *address;
}

} // namespace

int main(int argc, char** argv) {
    GYO::Engine::OS::DatagramAddress server = GYO::Engine::OS::DatagramAddress::Loopback(40000);
    std::uint64_t userId = 0;
    std::int64_t frames = -1;
    std::optional<std::string> contentRoot;
    bool verbose = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const bool hasValue = index + 1 < argc;
        if (argument == "--connect" && hasValue) {
            const auto address = ParseAddress(argv[++index]);
            if (!address) {
                Log(GYO::Engine::Base::LogLevel::Error, "--connect takes an IPv4 address and a port, A.B.C.D:P");
                return 2;
            }
            server = *address;
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
    auto pool = GYO::Engine::Threads::WorkerPool::Start({.name = "object_fps_pvp assets", .threadCount = 2});
    if (!pool) {
        Log(GYO::Engine::Base::LogLevel::Error, "the asset threads did not start: " + GYO::Engine::Base::Describe(pool.error()));
        return 1;
    }
    WA::WorkerPoolExecutor executor(*pool);

    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    N::NetProductOptions net;
    net.role = N::NetRole::Client;
    description.options.Set(net);
    PC::ClientOptions options{sharedGame, std::make_shared<const PC::PresentationContent>(std::move(*presentation)), 0, userId};
    O::WorldDescription gameWorld = PC::ClientGameWorld(options);
    // Whom it connects to (net-connect-target: a client World's NetConnectTarget, put by its prepare function).
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
    W::WindowSettings window;
    window.title = GYO_GAME_DISPLAY_NAME;
    window.size = {1280, 720};
    window.resizable = false;
    description.options.Set(window);
    GYO::Plugins::SDLWindow::SDLWindowOptions sdl;
    sdl.minFrameInterval = std::chrono::milliseconds(4);
    description.options.Set(sdl);
    WA::AssetOptions assets;
    assets.contentRoot = *contentRoot;
    assets.executor = &executor;
    description.options.Set(assets);

    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        std::string text = "the game did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail);
        for (const O::UnitReportedError& unit : report.unitErrors) text += "\n" + unit.unit + ": " + unit.code + ": " + unit.message;
        Log(GYO::Engine::Base::LogLevel::Error, text);
        return 1;
    }
    O::RunnerExit exit;
    bool mismatch = false;
    {
        std::unique_ptr<O::Product> product = std::move(built).value();
        if (const auto* output = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLAudio::AudioOutput>()) {
            if (const auto* failure = output->Failure()) {
                Log(GYO::Engine::Base::LogLevel::Warning, "no sound: " + GYO::Engine::Base::Describe(*failure));
            }
        }
        const auto* gpu = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
        if (gpu != nullptr && verbose) Log(GYO::Engine::Base::LogLevel::Info, "drawing on the " + gpu->DriverName() + " GPU driver");
        if (verbose) Log(GYO::Engine::Base::LogLevel::Info, "connecting to " + server.ToString() + " as user " + std::to_string(userId));
        exit = product->Run();
        // The window closed: tell the Match now (the disconnect packets, sent at once), so it takes the player off
        // without waiting for the timeout (lightyear's netcode client sends its disconnect packets the same way).
        if (auto* service = product->Buffer().Find<N::NetService>()) {
            K::World& world = product->World("Game").GetWorld();
            N::Stamp stamp;
            if (const auto* tick = world.Resource<GYO::Engine::WorldTime::Tick>()) stamp.tick = static_cast<std::uint32_t>(tick->value);
            if (const auto* replica = world.Resource<N::NetReplicaState>()) stamp.worldGeneration = replica->generation.value_or(0);
            service->Close();
            service->Flush(service->Now(), stamp);
        }
        if (const auto* state = product->World("Game").GetWorld().Resource<ProgramState>()) mismatch = !state->shown.mismatch.empty();
        const bool drawingFailed =
            gpu != nullptr && (gpu->FramesFailed() > 0 || gpu->SkippedDraws() > 0 || gpu->RefusedTextureAsks() > 0);
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
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        Log(GYO::Engine::Base::LogLevel::Error, exit.reason);
        return 1;
    }
    return mismatch ? 1 : 0;
}
