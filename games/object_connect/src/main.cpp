// Object_Connect's program (the original Core/Application and WinMain): the
// Product with the default Modules and render_world, the sdl_window, sdl_audio and sdl_gpu Plugins and one World with
// the game, run by sdl_window's runner until the game closes the window.
//
//   gyo_object_connect               the game in a 1280 x 720 window
//   gyo_object_connect --frames N    closes itself after N frames once the
//                                    content has loaded (CI runs it headless)
//   gyo_object_connect --verbose     logs the screen, the cursor, the drag and
//                                    the warps as they change
//   gyo_object_connect --hidden      the window is made hidden (the tests)

#include "object_connect/runtime/GameSound.hpp"
#include "object_connect/runtime/RuntimeWorld.hpp"
#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Resources.hpp"

#include "gyo/GameConfig.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/base/Log.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
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
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;

void LogError(const std::string& text) {
    ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Error, GYO_GAME_ID, text);
}

// Frames left before the program closes itself (-1: never). Counted only after
// generation, so a run with --frames always plays frames with the content.
struct FramesLeft {
    std::int64_t value = -1;
};
void HashValue(K::Hasher&, const FramesLeft&) {}

K::SystemResult CountFrames(K::ResMut<FramesLeft> left, K::ResMut<W::WindowRequest> window) {
    if (left.Read().value > 0 && --left->value == 0) {
        window->close = true;
    }
    return {};
}

void LogInfo(const std::string& text) {
    ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Info, GYO_GAME_ID, text);
}

// --verbose: what the game decided, one line each, while nothing is drawn yet (the screen, the cursor, warps).
struct Shown {
    std::optional<object_connect::GameScreen> screen;
    object_connect::runtime::CursorAppearance cursor{};
    bool drag = false;
};
void HashValue(K::Hasher&, const Shown&) {}

constexpr const char* kIconNames[] = {"Normal", "Interactive", "Holding"};

K::SystemResult LogDecisions(K::Res<object_connect::world::ScreenState> screen,
                             K::Res<object_connect::world::ScreenRequest> request,
                             K::Res<object_connect::runtime::CursorAppearance> cursor,
                             K::Res<object_connect::world::Session> session,
                             K::MessageReader<W::WarpCursor> warps, K::ResMut<Shown> shown) {
    // The screen the frame ends on (a request made this frame is applied next frame).
    const std::optional<object_connect::GameScreen> current = object_connect::world::EffectiveScreen(*screen, *request);
    if (current != shown.Read().screen && current) {
        LogInfo(std::string("screen ") +
                std::string(GYO::Engine::StateMachine::StateMachineTraits<object_connect::GameScreen>::values
                                [static_cast<std::size_t>(*current)]
                                    .name));
        shown->screen = current;
    }
    if (cursor->icon != shown.Read().cursor.icon || cursor->custom != shown.Read().cursor.custom) {
        LogInfo(std::string("cursor ") + kIconNames[static_cast<std::size_t>(cursor->icon)] +
                (cursor->custom ? " (the game's)" : " (the system's)"));
        shown->cursor = *cursor;
    }
    const bool drag = session->board && session->board->IsDragging();
    if (drag != shown.Read().drag) {
        LogInfo(drag ? "drag started" : (session->board && session->board->IsSolved() ? "drag ended: solved" : "drag ended"));
        shown->drag = drag;
    }
    for (const W::WarpCursor* warp : warps.Read()) {
        LogInfo("warp the cursor to (" + std::to_string(warp->position.x) + ", " + std::to_string(warp->position.y) +
                ")");
    }
    return {};
}

// The content root next to the executable: bin/assets/<game> (GyoContent.cmake stages it there).
std::optional<std::string> ContentRoot() {
    auto executable = GYO::Engine::OS::Process::ExecutablePath();
    if (!executable) {
        return std::nullopt;
    }
    const std::string& path = *executable;
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return std::nullopt;
    }
    return path.substr(0, slash + 1) + GYO_GAME_ASSETS;
}

O::ProductDescription Describe(const std::string& contentRoot, WA::WorkerPoolExecutor& executor,
                               const std::int64_t frames, const bool verbose, const bool hidden) {
    O::WorldDescription world;
    world.name = "Main";
    world.build = [frames, verbose](O::WorldSetup& setup) {
        object_connect::runtime::BuildRuntime(setup);
        K::Registrar& r = setup.Registrar();
        r.Resource<FramesLeft>({.name = "FramesLeft"});
        K::SystemOptions count;
        count.name = "CountFrames";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        mount.conditions = {O::AfterGenerating()};
        count.mounts.push_back(std::move(mount));
        r.AddSystem<&CountFrames>(std::move(count));
        if (verbose) {
            r.Resource<Shown>({.name = "Shown"});
            K::SystemOptions log;
            log.name = "LogDecisions";
            K::Mount last;
            last.schedule = std::string(O::DefaultSchedules::frame);
            last.stage = std::string(O::DefaultSchedules::last);
            last.conditions = {O::AfterGenerating()};
            log.mounts.push_back(std::move(last));
            r.AddSystem<&LogDecisions>(std::move(log));
        }
        setup.Prepare([frames, verbose](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(FramesLeft{frames});
            if (verbose) {
                w.SetResource(Shown{});
            }
            return {};
        });
    };

    O::ProductDescription description;
    description.name = GYO_GAME_ID;
    description.worlds.push_back(std::move(world));
    // The game's loaders join the Product's store and the game's submitter joins sdl_audio's mixer; a refusal
    // stops the build before any World is made.
    description.configure = [](O::ProductSetup& setup) { object_connect::runtime::ConfigureProduct(setup); };

    // The original's window: the 1280 x 720 back buffer, not resizable (G6).
    W::WindowSettings window;
    window.title = GYO_GAME_DISPLAY_NAME;
    window.size = {1280, 720};
    window.resizable = false;
    window.hidden = hidden;
    description.options.Set(window);

    // Presenting waits for the display; a minimized window gives no picture and does not wait, so a small floor
    // keeps the loop from spinning then.
    GYO::Plugins::SDLWindow::SDLWindowOptions sdl;
    sdl.minFrameInterval = std::chrono::milliseconds(4);
    description.options.Set(sdl);

    WA::AssetOptions assets;
    assets.contentRoot = contentRoot;
    assets.executor = &executor;
    description.options.Set(assets);

    return description;
}

} // namespace

int main(int argc, char** argv) {
    std::int64_t frames = -1;
    bool verbose = false;
    bool hidden = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--frames" && index + 1 < argc) {
            const std::string_view value(argv[++index]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || frames <= 0) {
                LogError("--frames takes a positive whole number, not '" + std::string(value) + "'");
                return 2;
            }
        } else if (argument == "--verbose") {
            verbose = true;
        } else if (argument == "--hidden") {
            hidden = true;
        } else {
            LogError("unknown argument '" + std::string(argument) + "' (--frames N, --verbose, --hidden)");
            return 2;
        }
    }

    const std::optional<std::string> contentRoot = ContentRoot();
    if (!contentRoot) {
        LogError("the executable's directory is unknown, so the content cannot be found");
        return 1;
    }
    auto pool = GYO::Engine::Threads::WorkerPool::Start({.name = "object_connect assets", .threadCount = 2});
    if (!pool) {
        LogError("the asset threads did not start: " + GYO::Engine::Base::Describe(pool.error()));
        return 1;
    }
    WA::WorkerPoolExecutor executor(*pool);

    auto built = O::Product::Build(Describe(*contentRoot, executor, frames, verbose, hidden), O::MakeRegisteredUnits());
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        std::string text = "the game did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail);
        for (const O::UnitReportedError& unit : report.unitErrors) {
            text += "\n" + unit.unit + ": " + unit.code + ": " + unit.message;
        }
        LogError(text);
        return 1;
    }
    O::RunnerExit exit;
    {
        // The Product and its Worlds go before the pool they submit jobs to.
        std::unique_ptr<O::Product> product = std::move(built).value();
        // A game without a sound device still plays (the original disabled its audio with a warning).
        if (const auto* output = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLAudio::AudioOutput>()) {
            if (const auto* failure = output->Failure()) {
                ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Warning, GYO_GAME_ID,
                                         "no sound: " + GYO::Engine::Base::Describe(*failure));
            } else if (verbose) {
                LogInfo("sound on the " + output->DriverName() + " driver");
            }
        }
        const auto* gpu = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLGPU::SDLGPUStatus>();
        if (gpu != nullptr && verbose) {
            LogInfo("drawing on the " + gpu->DriverName() + " GPU driver");
        }
        exit = product->Run();
        // What did not draw is told, not fatal (sdl_gpu): one line when frames or draws failed. A run with --frames
        // (the tests) fails then, and when no frame reached the window at all.
        const bool drawingFailed =
            gpu != nullptr && (gpu->FramesFailed() > 0 || gpu->SkippedDraws() > 0 || gpu->RefusedTextureAsks() > 0);
        if (drawingFailed) {
            ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Warning, GYO_GAME_ID,
                                     "drawing: " + std::to_string(gpu->FramesFailed()) + " frames failed, " +
                                         std::to_string(gpu->SkippedDraws()) + " draws skipped, " +
                                         std::to_string(gpu->RefusedTextureAsks()) + " textures refused; last: " +
                                         gpu->LastFailure());
        }
        if (frames > 0 && exit.code != O::RunnerExitCode::Failure) {
            if (gpu == nullptr || gpu->FramesShown() == 0) {
                LogError("no frame was shown (" + std::to_string(gpu != nullptr ? gpu->FramesSkipped() : 0) +
                         " skipped)");
                return 1;
            }
            if (drawingFailed) {
                return 1;
            }
        }
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        LogError(exit.reason);
        return 1;
    }
    return 0;
}
