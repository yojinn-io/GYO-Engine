// A product with a window (sdl_window): the default Modules (input, window among them), the sdl_window Plugin and
// one System of the game in Update. The game reads input's state, writes window's requests, and closes the window
// on Escape (or after --frames N). sdl_window's runner pumps the OS events before each frame and ends when the close
// was applied. The window is made from window's WindowSettings in the product options.
//
//   gyo_example_sdl_window              a 640x360 window; arrows move nothing, Escape closes, the title follows the cursor
//   gyo_example_sdl_window --frames N   closes itself after N frames (CI runs it on SDL's dummy driver)

#include "engine/base/Log.hpp"
#include "engine/input/Input.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/window/Window.hpp"
#include "plugins/sdl_window/SdlWindowPlugin.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::Window;

// The example's own Resource: frames left before it closes itself (-1: never).
struct FramesLeft {
    std::int64_t value = -1;
};
void HashValue(K::Hasher&, const FramesLeft&) {}

K::SystemResult Play(K::Res<I::ButtonInput<I::KeyCode>> keys, K::Res<I::CursorPosition> cursor,
                     K::ResMut<W::WindowRequest> window, K::ResMut<FramesLeft> left) {
    if (keys->JustPressed(I::KeyCode::Escape)) window->close = true;
    if (left.Read().value > 0 && --left->value == 0) window->close = true;
    if (cursor->position) {
        const std::string title = "GYO sdl_window example (" + std::to_string(std::lround(cursor->position->x)) + ", " +
                                  std::to_string(std::lround(cursor->position->y)) + ")";
        if (window.Read().title != title) window->title = title;
    }
    return {};
}

O::ProductDescription Describe(std::int64_t frames) {
    O::WorldDescription world;
    world.name = "Main";
    world.build = [frames](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Resource<FramesLeft>({.name = "FramesLeft"});
        K::SystemOptions play;
        play.name = "Play";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        play.mounts.push_back(std::move(mount));
        r.AddSystem<&Play>(std::move(play));
        setup.Prepare([frames](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(FramesLeft{frames});
            return {};
        });
    };
    O::ProductDescription description;
    description.name = "sdl_window_example";
    W::WindowSettings settings;
    settings.title = "GYO sdl_window example";
    settings.size = {640, 360};
    description.options.Set(settings);
    GYO::Plugins::SdlWindow::SdlWindowOptions options;
    // Nothing draws yet (batch R2a waits for the display): do not spin faster than about 60 frames a second.
    options.minFrameInterval = std::chrono::milliseconds(16);
    description.options.Set(options);
    description.worlds.push_back(std::move(world));
    return description;
}

void LogError(const std::string& text) {
    ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Error, "example", text);
}

} // namespace

int main(int argc, char** argv) {
    std::int64_t frames = -1;
    if (argc > 2 && std::string_view(argv[1]) == "--frames") frames = std::stoll(argv[2]);

    auto built = O::Product::Build(Describe(frames), O::MakeRegisteredUnits());
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        LogError("the Product did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail));
        return 1;
    }
    O::RunnerExit exit;
    {
        std::unique_ptr<O::Product> product = std::move(built).value();
        exit = product->Run();
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        LogError(exit.reason);
        return 1;
    }
    return 0;
}
