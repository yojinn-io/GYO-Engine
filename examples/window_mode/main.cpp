// A settings screen's three switches without the screen (window-mode-vsync declaration, section 5: what a machine
// checks): the window's mode (window's SetWindowMode), its Windowed size (ResizeWindow, four fixed sizes) and VSync
// (render's Presentation), on a 1280x720 picture letterboxed into whatever the window is (camera's Letterbox). The
// title says what window's state and sdl_gpu report. One World with the default Modules, render_world, sdl_window and
// sdl_gpu; the game's one System writes the requests and the frame's 2D pass.
//
//   gyo_example_window_mode [--fullscreen]   F fullscreen, W windowed, 1-4 1280x720 / 1600x900 / 1920x1080 /
//                                            2560x1440, V VSync on/off, Escape closes
//   gyo_example_window_mode --frames N       goes through the switches by itself and closes after N frames; exit
//                                            code 1 when no frame was shown or anything failed (ctest, label gpu)
//
// The bar that runs across the picture shows tearing when VSync is off (and IMMEDIATE was chosen).

#include "engine/base/Log.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/input/Input.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/Presentation.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/window/Window.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"
#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace C = GYO::Engine::Camera;
namespace G = GYO::Plugins::SDLGPU;
namespace I = GYO::Engine::Input;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace W = GYO::Engine::Window;

constexpr std::array<GYO::Engine::Math::Vec2i, 4> kSizes{{{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}}};
constexpr float kViewWidth = 1280.0F;
constexpr float kViewHeight = 720.0F;

// The example's own state: frames left before it closes itself (-1: never), the frame count, sdl_gpu's status (put
// by main after the Product was built) and the frame rate measured over the last second.
struct Demo {
    std::int64_t framesLeft = -1;
    std::int64_t frame = 0;
    const G::SDLGPUStatus* gpu = nullptr;
    std::chrono::steady_clock::time_point since{};
    int framesSince = 0;
    double fps = 0.0;
    // What was printed last (a line each time the mode, the size or the swapchain's mode changes).
    std::string printed;
};
void HashValue(K::Hasher&, const Demo&) {}

void Quad(R::Pass2D& pass, const GYO::Engine::Math::Rect rect, const R::Color8 colour) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    const float x0 = rect.x;
    const float y0 = rect.y;
    const float x1 = rect.x + rect.width;
    const float y1 = rect.y + rect.height;
    pass.vertices.push_back({{x0, y0}, {0, 0}, colour});
    pass.vertices.push_back({{x1, y0}, {1, 0}, colour});
    pass.vertices.push_back({{x1, y1}, {1, 1}, colour});
    pass.vertices.push_back({{x0, y1}, {0, 1}, colour});
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    R::Draw2D draw;
    draw.indexOffset = first;
    draw.indexCount = 6;
    pass.draws.push_back(draw);
}

const char* ModeName(const W::WindowMode mode) {
    return mode == W::WindowMode::BorderlessFullscreen ? "BorderlessFullscreen" : "Windowed";
}

// --frames: the switches by themselves, 60 frames apart (macOS's fullscreen is an animation of about a second).
void Script(const std::int64_t frame, K::MessageWriter<W::SetWindowMode>& modes, K::MessageWriter<W::ResizeWindow>& sizes,
            R::Presentation& presentation) {
    switch (frame) {
    case 10: modes.Send(W::SetWindowMode{W::WindowMode::BorderlessFullscreen}); break;
    case 70: sizes.Send(W::ResizeWindow{kSizes[1]}); break;
    case 80: presentation.presentMode = R::PresentMode::AutoNoVsync; break;
    case 100: presentation.presentMode = R::PresentMode::AutoVsync; break;
    case 120: modes.Send(W::SetWindowMode{W::WindowMode::Windowed}); break;
    default: break;
    }
}

K::SystemResult Play(K::Res<I::ButtonInput<I::KeyCode>> keys, K::Res<W::WindowState> state, K::ResMut<W::WindowRequest> window,
                     K::ResMut<R::Presentation> presentation, K::ResMut<R::RenderCommands> commands, K::ResMut<Demo> demo,
                     K::MessageWriter<W::SetWindowMode> modes, K::MessageWriter<W::ResizeWindow> sizes) {
    Demo& d = demo.Get();
    ++d.frame;
    if (keys->JustPressed(I::KeyCode::Escape)) window->close = true;
    if (d.framesLeft > 0 && --d.framesLeft == 0) window->close = true;
    if (keys->JustPressed(I::KeyCode::F)) modes.Send(W::SetWindowMode{W::WindowMode::BorderlessFullscreen});
    if (keys->JustPressed(I::KeyCode::W)) modes.Send(W::SetWindowMode{W::WindowMode::Windowed});
    constexpr std::array<I::KeyCode, 4> digits{I::KeyCode::Digit1, I::KeyCode::Digit2, I::KeyCode::Digit3, I::KeyCode::Digit4};
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (keys->JustPressed(digits[i])) sizes.Send(W::ResizeWindow{kSizes[i]});
    }
    if (keys->JustPressed(I::KeyCode::V)) {
        R::PresentMode& mode = presentation->presentMode;
        mode = mode == R::PresentMode::AutoVsync ? R::PresentMode::AutoNoVsync : R::PresentMode::AutoVsync;
    }
    if (d.framesLeft >= 0) Script(d.frame, modes, sizes, presentation.Get());

    // The frame rate, once a second.
    const auto now = std::chrono::steady_clock::now();
    ++d.framesSince;
    if (now - d.since >= std::chrono::seconds(1)) {
        d.fps = d.framesSince / std::chrono::duration<double>(now - d.since).count();
        d.framesSince = 0;
        d.since = now;
    }
    if (!state->known) return {};
    const std::string title = std::string("window_mode: ") + ModeName(state->mode) + " " + std::to_string(state->physicalSize.x) + "x" +
                              std::to_string(state->physicalSize.y) + " px (scale " + std::to_string(state->scaleFactor).substr(0, 4) +
                              ") | Presentation " + R::ToString(presentation.Read().presentMode) + " -> swapchain " +
                              (d.gpu != nullptr ? G::ToString(d.gpu->ActivePresentMode()) : "?") + " | " +
                              std::to_string(static_cast<int>(d.fps + 0.5)) + " fps";
    if (window.Read().title != title) window->title = title;
    const std::string line = std::string(ModeName(state->mode)) + " " + std::to_string(state->physicalSize.x) + "x" +
                             std::to_string(state->physicalSize.y) + " px, Presentation " + R::ToString(presentation.Read().presentMode) +
                             ", swapchain " + (d.gpu != nullptr ? G::ToString(d.gpu->ActivePresentMode()) : "?");
    if (line != d.printed) {
        d.printed = line;
        ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Info, "window_mode", "frame " + std::to_string(d.frame) + ": " + line);
    }

    // The 1280x720 picture, letterboxed into the window (camera's Letterbox, as the 2D pipeline does).
    const C::TargetSize target{state->physicalSize, state->scaleFactor};
    const C::Letterbox letterbox{kViewWidth, kViewHeight};
    const C::ComputedCamera camera = C::Compute(C::Camera{}, C::Projection{C::OrthographicProjection::Default2d()}, &target, &letterbox);
    if (!camera.valid) return {};
    R::Pass2D pass;
    pass.clear = R::Color8{0, 0, 0, 255};  // the bars
    pass.viewport = R::PixelRect{static_cast<std::uint32_t>(camera.viewport.position.x), static_cast<std::uint32_t>(camera.viewport.position.y),
                                 static_cast<std::uint32_t>(camera.viewport.size.x), static_cast<std::uint32_t>(camera.viewport.size.y)};
    pass.view = GYO::Engine::Math::Rect{0, 0, kViewWidth, kViewHeight};
    Quad(pass, {0, 0, kViewWidth, kViewHeight}, R::Color8{40, 60, 90, 255});
    // A frame 8 view units in from the picture's edges: all four must show (nothing cut by the letterbox).
    Quad(pass, {8, 8, kViewWidth - 16, 4}, R::Color8{255, 200, 0, 255});
    Quad(pass, {8, kViewHeight - 12, kViewWidth - 16, 4}, R::Color8{255, 200, 0, 255});
    Quad(pass, {8, 8, 4, kViewHeight - 16}, R::Color8{255, 200, 0, 255});
    Quad(pass, {kViewWidth - 12, 8, 4, kViewHeight - 16}, R::Color8{255, 200, 0, 255});
    // The running bar: 1280 view units in 64 frames.
    const float x = static_cast<float>((d.frame * 20) % static_cast<std::int64_t>(kViewWidth));
    Quad(pass, {x, 0, 24, kViewHeight}, R::Color8{240, 240, 240, 255});
    commands->passes.push_back(std::move(pass));
    return {};
}

O::ProductDescription Describe(const std::int64_t frames, const bool fullscreen) {
    O::WorldDescription world;
    world.name = "Main";
    world.build = [frames](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Resource<Demo>({.name = "Demo"});
        K::SystemOptions play;
        play.name = "Play";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        play.mounts.push_back(std::move(mount));
        r.AddSystem<&Play>(std::move(play));
        setup.Prepare([frames](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            Demo demo;
            demo.framesLeft = frames;
            demo.since = std::chrono::steady_clock::now();
            w.SetResource(std::move(demo));
            return {};
        });
    };
    O::ProductDescription description;
    description.name = "window_mode_example";
    W::WindowSettings settings;
    settings.title = "window_mode";
    settings.size = kSizes[0];
    settings.mode = fullscreen ? W::WindowMode::BorderlessFullscreen : W::WindowMode::Windowed;
    description.options.Set(settings);
    description.worlds.push_back(std::move(world));
    return description;
}

void LogError(const std::string& text) { ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Error, "example", text); }

} // namespace

int main(int argc, char** argv) {
    std::int64_t frames = -1;
    bool fullscreen = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--frames" && i + 1 < argc) {
            frames = std::stoll(argv[++i]);
        } else if (argument == "--fullscreen") {
            fullscreen = true;
        } else {
            LogError("unknown argument '" + std::string(argument) + "' (--frames N, --fullscreen)");
            return 2;
        }
    }
    auto built = O::Product::Build(Describe(frames, fullscreen), O::MakeRegisteredUnits());
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        std::string text = "the Product did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail);
        for (const O::UnitReportedError& unit : report.unitErrors) text += "\n" + unit.unit + ": " + unit.code + ": " + unit.message;
        LogError(text);
        return 1;
    }
    std::unique_ptr<O::Product> product = std::move(built).value();
    const auto* gpu = std::as_const(product->Buffer()).Find<G::SDLGPUStatus>();
    const auto* pump = std::as_const(product->Buffer()).Find<GYO::Plugins::SDLWindow::SDLWindowPump>();
    product->World("Main").GetWorld().ResourceMut<Demo>()->gpu = gpu;
    const O::RunnerExit exit = product->Run();
    if (exit.code == O::RunnerExitCode::Failure) {
        LogError(exit.reason);
        return 1;
    }
    if (gpu != nullptr && (gpu->FramesFailed() > 0 || gpu->RefusedPresentModes() > 0)) {
        LogError("drawing: " + std::to_string(gpu->FramesFailed()) + " frames failed, " + std::to_string(gpu->RefusedPresentModes()) +
                 " present modes refused; last: " + gpu->LastFailure());
        if (frames > 0) return 1;
    }
    if (pump != nullptr && pump->RefusedRequests() > 0) {
        LogError("window: " + std::to_string(pump->RefusedRequests()) + " requests refused; last: " + pump->LastRefusal());
        if (frames > 0) return 1;
    }
    if (gpu != nullptr) {
        ::GYO::Engine::Base::Log(::GYO::Engine::Base::LogLevel::Info, "window_mode",
                                 "driver " + gpu->DriverName() + ": " + std::to_string(gpu->FramesShown()) + " frames shown, " +
                                     std::to_string(gpu->FramesSkipped()) + " skipped, present mode set " +
                                     std::to_string(gpu->PresentModeSets()) + " times");
    }
    if (frames > 0 && (gpu == nullptr || gpu->FramesShown() == 0)) {
        LogError("no frame was shown");
        return 1;
    }
    return 0;
}
