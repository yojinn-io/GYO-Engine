// The 2D pass on a GPU (docs/architecture/target/v2/render.zh-Hant.md section 2, sdl.zh-Hant.md "sdl_gpu"; label gpu).
// sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back (GPUDevice::Capture, the same drawing
// as on the window); each pixel is compared with its exact colour (master pitfall 18: compare the colour, not "something
// was drawn"). Only a blend of a translucent colour may differ by one step (the GPU's rounding). The frames case runs
// the Plugin in a Product on a hidden window. Tags "[gp-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"
#include "plugins/sdl_window/SDLWindowPlugin.hpp"

#include "GPUDevice.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

namespace G = GYO::Plugins::SDLGPU;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace RW = GYO::Engine::RenderWorld;
using GYO::Engine::Math::Rect;

namespace {

constexpr R::TargetSize kSize{8, 8};
constexpr R::Color8 kBlue{0, 0, 255, 255};
constexpr R::Color8 kRed{255, 0, 0, 255};
constexpr R::Color8 kGreen{0, 255, 0, 255};
constexpr R::Color8 kWhite{255, 255, 255, 255};
constexpr R::Color8 kYellow{255, 255, 0, 255};

// One device for every case (making one per case is slow on Lavapipe).
G::GPUDevice& Device() {
    static std::unique_ptr<G::GPUDevice> device = [] {
        auto made = G::GPUDevice::Create(nullptr, true);  // SDL's validation on
        REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
        return std::move(*made);
    }();
    return *device;
}

// A rectangle of two triangles, uv (0, 0) at its top-left and (1, 1) at its bottom-right.
void Quad(R::Pass2D& pass, const Rect r, const R::Color8 colour, const R::TextureId texture = {},
          const R::BlendMode blend = R::BlendMode::Alpha, const std::optional<Rect> clip = std::nullopt) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    pass.vertices.push_back({{r.x, r.y}, {0.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y}, {1.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y + r.height}, {1.0F, 1.0F}, colour});
    pass.vertices.push_back({{r.x, r.y + r.height}, {0.0F, 1.0F}, colour});
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    pass.draws.push_back(R::Draw2D{texture, blend, clip, first, 6, 0});
}

R::Pass2D Cleared(const R::Color8 colour) {
    R::Pass2D pass;
    pass.clear = colour;
    return pass;
}

struct Picture {
    G::CapturedImage image;
    G::FrameTally tally;
    [[nodiscard]] R::Color8 At(const std::uint32_t x, const std::uint32_t y) const {
        const std::size_t i = (static_cast<std::size_t>(y) * image.size.width + x) * 4;
        return R::Color8{image.pixels[i], image.pixels[i + 1], image.pixels[i + 2], image.pixels[i + 3]};
    }
    // Every pixel of [x0, x1) x [y0, y1) is exactly this colour.
    [[nodiscard]] bool All(const std::uint32_t x0, const std::uint32_t y0, const std::uint32_t x1, const std::uint32_t y1,
                           const R::Color8 colour) const {
        for (std::uint32_t y = y0; y < y1; ++y) {
            for (std::uint32_t x = x0; x < x1; ++x) {
                if (!(At(x, y) == colour)) {
                    MESSAGE("pixel (" << x << ", " << y << ") is " << int(At(x, y).r) << "," << int(At(x, y).g) << "," << int(At(x, y).b)
                                      << "," << int(At(x, y).a));
                    return false;
                }
            }
        }
        return true;
    }
};

Picture Draw(const R::RenderCommands& commands, const R::RenderTextures* textures = nullptr) {
    Picture picture;
    const std::span<const R::TextureOp> asks = textures != nullptr ? textures->Pending() : std::span<const R::TextureOp>{};
    auto captured = Device().Capture(asks, commands, kSize, picture.tally);
    REQUIRE_MESSAGE(static_cast<bool>(captured), (captured ? std::string() : captured.error().message));
    picture.image = std::move(*captured);
    REQUIRE(picture.image.pixels.size() == std::size_t{kSize.width} * kSize.height * 4);
    return picture;
}

int Diff(const std::uint8_t a, const int b) { return std::abs(static_cast<int>(a) - b); }

} // namespace

TEST_CASE("sdl_gpu pass2d: a pass clears its target to its colour; no clear is transparent black") {
    R::RenderCommands commands;
    commands.passes.push_back(Cleared(R::Color8{10, 20, 30, 255}));
    CHECK_MESSAGE(Draw(commands).All(0, 0, 8, 8, R::Color8{10, 20, 30, 255}), "[gp-clear] every pixel is the clear colour");
    R::RenderCommands none;
    none.passes.push_back(R::Pass2D{});
    CHECK_MESSAGE(Draw(none).All(0, 0, 8, 8, R::Color8{0, 0, 0, 0}), "[gp-clear-default] the first pass without a clear is transparent black");
    CHECK_MESSAGE(Draw(R::RenderCommands{}).All(0, 0, 8, 8, R::Color8{0, 0, 0, 0}), "[gp-clear-no-pass] no pass at all: transparent black");
}

TEST_CASE("sdl_gpu pass2d: a plain colour covers exactly its pixels, y down from the top-left") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 4, 2}, kRed);
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 4, 2, kRed), "[gp-plain-inside] the rectangle's pixels are its colour");
    CHECK_MESSAGE(p.All(4, 0, 8, 8, kBlue), "[gp-plain-right] right of it the clear colour");
    CHECK_MESSAGE(p.All(0, 2, 4, 8, kBlue), "[gp-plain-below] below it the clear colour (y down)");
}

TEST_CASE("sdl_gpu pass2d: a texture is sampled top row first, nearest pixel, times the vertex colour") {
    R::RenderTextures textures;
    // 2 by 2: red, green on the top row; white, yellow on the bottom row.
    const R::TextureId id = textures.Create(R::TextureDesc{2, 2, R::TextureFilter::Nearest},
                                            {255, 0, 0, 255, 0, 255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 255});
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 8, 8}, kWhite, id);
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands, &textures);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kRed), "[gp-tex-top-left] texture row 0 is on top, pixel 0 on the left");
    CHECK_MESSAGE(p.All(4, 0, 8, 4, kGreen), "[gp-tex-top-right] top right");
    CHECK_MESSAGE(p.All(0, 4, 4, 8, kWhite), "[gp-tex-bottom-left] bottom left");
    CHECK_MESSAGE(p.All(4, 4, 8, 8, kYellow), "[gp-tex-bottom-right] bottom right");
    CHECK_MESSAGE(Device().TextureCount() == 1, "[gp-tex-made] the texture has a GPU texture");

    // The next frame: the texture stays; an update replaces its region; the vertex colour multiplies.
    textures.ClearPending();
    textures.Update(id, R::TextureRegion{1, 0, 1, 1}, {0, 0, 255, 255});
    R::RenderCommands next;
    R::Pass2D tinted = Cleared(kBlue);
    Quad(tinted, Rect{0, 0, 8, 8}, R::Color8{255, 0, 255, 255}, id);
    next.passes.push_back(std::move(tinted));
    const Picture q = Draw(next, &textures);
    CHECK_MESSAGE(q.All(0, 0, 4, 4, kRed), "[gp-tex-kept] the texture is kept between frames (red times magenta is red)");
    CHECK_MESSAGE(q.All(4, 0, 8, 4, kBlue), "[gp-tex-update] the updated region is blue now");
    CHECK_MESSAGE(q.All(0, 4, 4, 8, R::Color8{255, 0, 255, 255}), "[gp-tex-tint] white times magenta is magenta");

    // Let go: a draw naming it is skipped and counted.
    textures.ClearPending();
    textures.Release(id);
    R::RenderCommands gone;
    R::Pass2D after = Cleared(kBlue);
    Quad(after, Rect{0, 0, 8, 8}, kWhite, id);
    gone.passes.push_back(std::move(after));
    const Picture r = Draw(gone, &textures);
    CHECK_MESSAGE(r.All(0, 0, 8, 8, kBlue), "[gp-tex-released-not-drawn] a released texture draws nothing");
    CHECK_MESSAGE(r.tally.skippedDraws == 1, "[gp-tex-released-counted] the skipped draw is counted");
    CHECK_MESSAGE(Device().TextureCount() == 0, "[gp-tex-released-gone] its GPU texture is gone");
}

TEST_CASE("sdl_gpu pass2d: a Linear texture is blended between its pixels") {
    R::RenderTextures textures;
    // 2 by 1: black, white. Over 8 pixels, pixel x samples u = (x + 0.5) / 8, texel position 2u - 0.5 (clamped).
    const R::TextureId id = textures.Create(R::TextureDesc{2, 1, R::TextureFilter::Linear}, {0, 0, 0, 255, 255, 255, 255, 255});
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 8, 8}, kWhite, id);
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands, &textures);
    CHECK_MESSAGE(p.All(0, 0, 2, 8, R::Color8{0, 0, 0, 255}), "[gp-linear-left] left of the first texel's centre: black (clamped)");
    CHECK_MESSAGE(p.All(6, 0, 8, 8, kWhite), "[gp-linear-right] right of the last texel's centre: white");
    // Pixel 3: texel position 0.375, so 0.375 of the way to white (95.6); the GPU's filter precision may differ by 2.
    const R::Color8 between = p.At(3, 4);
    CHECK_MESSAGE((Diff(between.r, 96) <= 2 && between.r == between.g && between.g == between.b), "[gp-linear-between] blended between them");
    textures.ClearPending();
    textures.Release(id);
    static_cast<void>(Draw(R::RenderCommands{}, &textures));
}

TEST_CASE("sdl_gpu pass2d: a clip lets only its pixels through") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 8, 8}, kRed, {}, R::BlendMode::Alpha, Rect{2, 3, 4, 2});
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(2, 3, 6, 5, kRed), "[gp-clip-inside] inside the clip the colour");
    CHECK_MESSAGE(p.All(0, 0, 8, 3, kBlue), "[gp-clip-above] above it untouched");
    CHECK_MESSAGE(p.All(0, 5, 8, 8, kBlue), "[gp-clip-below] below it untouched");
    CHECK_MESSAGE((p.All(0, 3, 2, 5, kBlue) && p.All(6, 3, 8, 5, kBlue)), "[gp-clip-sides] beside it untouched");
}

TEST_CASE("sdl_gpu pass2d: Alpha blends straight alpha over the target; Opaque replaces it") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 4, 8}, R::Color8{255, 0, 0, 128}, {}, R::BlendMode::Alpha);
    Quad(pass, Rect{4, 0, 4, 8}, R::Color8{255, 0, 0, 128}, {}, R::BlendMode::Opaque);
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    // colour = src * a + dst * (1 - a), a = 128/255; alpha = a + 1 * (1 - a) = 1.
    const R::Color8 blended = p.At(1, 1);
    CHECK_MESSAGE((Diff(blended.r, 128) <= 1 && blended.g == 0 && Diff(blended.b, 127) <= 1 && blended.a == 255),
                  "[gp-blend-alpha] red at half alpha over blue");
    CHECK_MESSAGE(p.All(0, 0, 4, 8, blended), "[gp-blend-alpha-even] the same everywhere");
    CHECK_MESSAGE(p.All(4, 0, 8, 8, R::Color8{255, 0, 0, 128}), "[gp-blend-opaque] Opaque writes the colour and its alpha as they are");
}

TEST_CASE("sdl_gpu pass2d: draws lie in their order; offsets pick each draw's vertices and indices") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 6, 6}, kRed);
    Quad(pass, Rect{2, 2, 6, 6}, kGreen);
    // A third draw that reuses the first quad's indices with vertexOffset 8: the vertices of a third quad.
    pass.vertices.push_back({{0, 6}, {0, 0}, kYellow});
    pass.vertices.push_back({{2, 6}, {1, 0}, kYellow});
    pass.vertices.push_back({{2, 8}, {1, 1}, kYellow});
    pass.vertices.push_back({{0, 8}, {0, 1}, kYellow});
    pass.draws.push_back(R::Draw2D{{}, R::BlendMode::Alpha, std::nullopt, 0, 6, 8});
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 2, 2, kRed), "[gp-order-first] the first draw where nothing covers it");
    CHECK_MESSAGE(p.All(2, 2, 6, 6, kGreen), "[gp-order-later-over] the later draw lies over the earlier one");
    CHECK_MESSAGE(p.All(0, 6, 2, 8, kYellow), "[gp-vertex-offset] vertexOffset moves the indices to other vertices");
}

TEST_CASE("sdl_gpu pass2d: a view is stretched over the target; later passes keep what is there unless they clear") {
    R::RenderCommands commands;
    R::Pass2D first = Cleared(kBlue);
    first.view = Rect{0, 0, 4, 4};  // one view unit is two pixels
    Quad(first, Rect{0, 0, 2, 2}, kRed);
    commands.passes.push_back(std::move(first));
    R::Pass2D second;  // no clear: draws over the first pass's picture
    Quad(second, Rect{6, 6, 2, 2}, kGreen);
    commands.passes.push_back(std::move(second));
    R::Pass2D elsewhere = Cleared(kWhite);
    elsewhere.target = 7;  // no picture for target 7
    commands.passes.push_back(std::move(elsewhere));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kRed), "[gp-view-scale] two view units are four pixels");
    CHECK_MESSAGE(p.All(4, 0, 8, 4, kBlue), "[gp-pass-load] the second pass keeps the first pass's picture");
    CHECK_MESSAGE(p.All(6, 6, 8, 8, kGreen), "[gp-pass-second] the second pass draws in pixels (no view)");
    CHECK_MESSAGE(p.tally.unmappedPasses == 1, "[gp-unmapped] a pass on a target with no picture is counted, not drawn");
}

namespace {

int frameProduced = 0;

K::SystemResult ProduceFrame(K::ResMut<R::RenderCommands> commands, K::ResMut<R::RenderTextures> textures) {
    R::Pass2D pass = Cleared(kBlue);
    if (frameProduced == 0) {
        const R::TextureId id = textures.Get().Create(R::TextureDesc{1, 1}, {255, 255, 255, 255});
        Quad(pass, Rect{0, 0, 16, 16}, kRed, id);
    }
    commands.Get().passes.push_back(std::move(pass));
    ++frameProduced;
    return {};
}

O::RegisterRow Row(std::string name, O::UnitKind kind, bool library, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = kind;
    row.libraryOnly = library;
    row.defaultOn = kind == O::UnitKind::Module;
    row.depends = std::move(depends);
    return row;
}

} // namespace

TEST_CASE("sdl_gpu frames: GPUSubmit draws each frame of a Product on the window, nothing fails") {
    O::RegisteredUnits units;
    units.rows.push_back(Row("input", O::UnitKind::Module, false));
    units.modules.push_back({"input", std::make_unique<GYO::Engine::Input::InputModule>()});
    units.rows.push_back(Row("window", O::UnitKind::Module, false));
    units.modules.push_back({"window", std::make_unique<GYO::Engine::Window::WindowModule>()});
    units.rows.push_back(Row("render", O::UnitKind::Module, true));
    units.rows.push_back(Row("render_world", O::UnitKind::Module, false, {"render"}));
    units.modules.push_back({"render_world", std::make_unique<RW::RenderWorldModule>()});
    units.rows.push_back(Row("sdl", O::UnitKind::Plugin, true));
    units.rows.push_back(Row("sdl_window", O::UnitKind::Plugin, false, {"sdl", "input", "window"}));
    units.plugins.push_back({"sdl_window", std::make_unique<GYO::Plugins::SDLWindow::SDLWindowPlugin>()});
    units.rows.push_back(Row("sdl_gpu", O::UnitKind::Plugin, false, {"sdl", "render", "render_world"}));
    units.plugins.push_back({"sdl_gpu", std::make_unique<G::SDLGPUPlugin>()});
    O::ProductDescription d;
    d.name = "draws";
    GYO::Engine::Window::WindowSettings settings;
    settings.title = "sdl_gpu test";
    settings.size = {64, 48};
    settings.hidden = true;
    d.options.Set(settings);
    GYO::Plugins::SDLWindow::SDLWindowOptions windowOptions;
    windowOptions.installRunner = false;
    d.options.Set(windowOptions);
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& setup) {
        K::SystemOptions o;
        o.name = "TestProduceFrame";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::update);
        o.mounts.push_back(std::move(m));
        setup.Registrar().AddSystem<&ProduceFrame>(std::move(o));
    };
    d.worlds.push_back(std::move(w));
    frameProduced = 0;
    auto built = O::Product::Build(std::move(d), std::move(units));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    O::Product& product = **built;
    const G::SDLGPUStatus* status = product.Buffer().Find<G::SDLGPUStatus>();
    REQUIRE(status != nullptr);
    MESSAGE("GPU driver: " << status->DriverName());
    constexpr int kFrames = 4;
    for (int i = 0; i < kFrames; ++i) {
        auto ran = product.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message));
    }
    CHECK_MESSAGE(status->FramesFailed() == 0, "[gp-frames-none-failed] no frame failed: " << status->LastFailure());
    CHECK_MESSAGE(status->FramesShown() + status->FramesSkipped() == kFrames, "[gp-frames-each] each frame is shown or skipped");
    CHECK_MESSAGE(status->RefusedTextureAsks() == 0, "[gp-frames-asks] the texture ask was applied");
    CHECK_MESSAGE(status->SkippedDraws() == 0, "[gp-frames-draws] no draw skipped");
    CHECK_MESSAGE(!status->DriverName().empty(), "[gp-frames-driver] SDL names its driver");
    CHECK_MESSAGE(status->TextureCount() == 1, "[gp-frames-texture] the texture asked for in the first frame has a GPU texture");

    // A replacement that fails before the swap: the old World goes on drawing with its textures.
    O::ReplaceRequest failing;
    failing.extra = [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "refused by the test", {}, "test"));
    };
    REQUIRE_FALSE(static_cast<bool>(product.ReplaceWorld("Main", std::move(failing))));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->TextureCount() == 1, "[gp-world-failed-keeps] a failed replacement forgets no texture");
    // A replacement that succeeds: the new World's ids start again at 1, so the old World's textures go.
    REQUIRE(static_cast<bool>(product.ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->TextureCount() == 0, "[gp-world-new-forgets] the new World's first frame lets the old World's textures go");
    CHECK_MESSAGE(status->FramesFailed() == 0, "[gp-world-frames] no frame failed");
}

// The frame-time budget declared in render.zh-Hant.md section 2 (batch R2a): measured, not judged (the GPU and the
// driver decide it; Lavapipe is far slower than any player's GPU). It prints the mean of Capture (asks, uploads, the
// pass and the read back, waiting for the GPU) of an Object_Connect-sized frame: 1280 by 720, 2,000 quads, one pass.
TEST_CASE("sdl_gpu budget: an Object_Connect-sized 2D frame") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    pass.view = Rect{0, 0, 1280, 720};
    for (int i = 0; i < 2000; ++i) {
        const float x = static_cast<float>((i * 37) % 1240);
        const float y = static_cast<float>((i * 53) % 680);
        Quad(pass, Rect{x, y, 40, 40}, R::Color8{255, 0, 0, 128});
    }
    commands.passes.push_back(std::move(pass));
    // A device as a product has it: without SDL's validation.
    auto made = G::GPUDevice::Create(nullptr, false);
    REQUIRE(static_cast<bool>(made));
    G::GPUDevice& device = **made;
    G::FrameTally tally;
    static_cast<void>(device.Capture({}, commands, R::TargetSize{1280, 720}, tally));
    constexpr int kRuns = 20;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRuns; ++i) REQUIRE(static_cast<bool>(device.Capture({}, commands, R::TargetSize{1280, 720}, tally)));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kRuns;
    MESSAGE("[gp-budget] driver " << device.DriverName() << ": " << ms << " ms a frame (budget 8 ms, render.zh-Hant.md section 2)");
    CHECK(ms > 0.0);
}

} // namespace GYO::Plugins::SDLGPU
