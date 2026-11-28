// The 2D pass on a GPU (docs/architecture/target/v2/render.zh-Hant.md section 2, sdl.zh-Hant.md "sdl_gpu"; label gpu).
// sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back (GPUDevice::Capture, the same drawing
// as on the window); each pixel is compared with its exact colour (master pitfall 18: compare the colour, not "something
// was drawn"). Only a blend of a translucent colour may differ by one step (the GPU's rounding). The frames case runs
// the Plugin in a Product on a shown window (at least one frame must reach it). Tags "[gp-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/Presentation.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/transform/Transform.hpp"
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
    // What the backend got done (render-pipeline declaration 14.13).
    R::RenderFrameReceipt receipt;
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
    auto captured = Device().Capture(asks, {}, commands, kSize, picture.tally, picture.receipt);
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
    Picture p = Draw(commands, &textures);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kRed), "[gp-tex-top-left] texture row 0 is on top, pixel 0 on the left");
    CHECK_MESSAGE(p.All(4, 0, 8, 4, kGreen), "[gp-tex-top-right] top right");
    CHECK_MESSAGE(p.All(0, 4, 4, 8, kWhite), "[gp-tex-bottom-left] bottom left");
    CHECK_MESSAGE(p.All(4, 4, 8, 8, kYellow), "[gp-tex-bottom-right] bottom right");
    CHECK_MESSAGE(Device().TextureCount() == 1, "[gp-tex-made] the texture has a GPU texture");

    // The next frame: the texture stays; an update replaces its region; the vertex colour multiplies.
    textures.EndFrame(commands, p.receipt);
    textures.Update(id, R::TextureRegion{1, 0, 1, 1}, {0, 0, 255, 255});
    R::RenderCommands next;
    R::Pass2D tinted = Cleared(kBlue);
    Quad(tinted, Rect{0, 0, 8, 8}, R::Color8{255, 0, 255, 255}, id);
    next.passes.push_back(std::move(tinted));
    Picture q = Draw(next, &textures);
    CHECK_MESSAGE(q.All(0, 0, 4, 4, kRed), "[gp-tex-kept] the texture is kept between frames (red times magenta is red)");
    CHECK_MESSAGE(q.All(4, 0, 8, 4, kBlue), "[gp-tex-update] the updated region is blue now");
    CHECK_MESSAGE(q.All(0, 4, 4, 8, R::Color8{255, 0, 255, 255}), "[gp-tex-tint] white times magenta is magenta");

    // Let go: a draw naming it is skipped and counted.
    textures.EndFrame(next, q.receipt);
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
    Picture p = Draw(commands, &textures);
    CHECK_MESSAGE(p.All(0, 0, 2, 8, R::Color8{0, 0, 0, 255}), "[gp-linear-left] left of the first texel's centre: black (clamped)");
    CHECK_MESSAGE(p.All(6, 0, 8, 8, kWhite), "[gp-linear-right] right of the last texel's centre: white");
    // Pixel 3: texel position 0.375, so 0.375 of the way to white (95.6); the GPU's filter precision may differ by 2.
    const R::Color8 between = p.At(3, 4);
    CHECK_MESSAGE((Diff(between.r, 96) <= 2 && between.r == between.g && between.g == between.b), "[gp-linear-between] blended between them");
    textures.EndFrame(commands, p.receipt);
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
    elsewhere.target = R::TextureId{7};  // no picture for target 7
    commands.passes.push_back(std::move(elsewhere));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kRed), "[gp-view-scale] two view units are four pixels");
    CHECK_MESSAGE(p.All(4, 0, 8, 4, kBlue), "[gp-pass-load] the second pass keeps the first pass's picture");
    CHECK_MESSAGE(p.All(6, 6, 8, 8, kGreen), "[gp-pass-second] the second pass draws in pixels (no view)");
    CHECK_MESSAGE(p.tally.unmappedPasses == 1, "[gp-unmapped] a pass on a target with no picture is counted, not drawn");
}

TEST_CASE("sdl_gpu pass2d: a viewport takes the view; the clear covers the whole target (letterbox bars)") {
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(R::Color8{0, 0, 0, 255});
    pass.viewport = R::PixelRect{0, 2, 8, 4};  // a 2:1 picture in an 8 by 8 target: bars of 2 rows above and below
    pass.view = Rect{0, 0, 4, 2};              // one view unit is two pixels of the viewport
    Quad(pass, Rect{0, 0, 4, 2}, kRed);        // fills the view
    // View y 0.5 to 1 is row 3 of the target only when the view is stretched over the viewport (over the whole
    // target it would be rows 2 and 3).
    Quad(pass, Rect{0, 0.5F, 1, 0.5F}, kYellow);
    // A clip of view y 0.5 to 1 on the viewport is row 3 only (on the whole target, rows 2 and 3).
    Quad(pass, Rect{2, 0, 2, 2}, kGreen, {}, R::BlendMode::Alpha, Rect{3, 0.5F, 1, 0.5F});
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 8, 2, R::Color8{0, 0, 0, 255}), "[gp-viewport-bar-top] above the viewport: the clear colour");
    CHECK_MESSAGE(p.All(0, 6, 8, 8, R::Color8{0, 0, 0, 255}), "[gp-viewport-bar-bottom] below the viewport: the clear colour");
    CHECK_MESSAGE(p.All(0, 2, 6, 3, kRed), "[gp-viewport-fill] the viewport's first row is the view's top");
    CHECK_MESSAGE(p.All(6, 2, 8, 3, kRed), "[gp-viewport-clip-above] the clip lets nothing through above view y 0.5");
    CHECK_MESSAGE(p.All(0, 3, 2, 4, kYellow), "[gp-viewport-fill] view y 0.5 is row 3");
    CHECK_MESSAGE(p.All(6, 3, 8, 4, kGreen), "[gp-viewport-clip] a clip is measured in the viewport");
    CHECK_MESSAGE(p.All(2, 3, 6, 4, kRed), "[gp-viewport-clip-rest] outside the clip the draw below");
    CHECK_MESSAGE(p.All(0, 4, 8, 6, kRed), "[gp-viewport-rest] the rest of the viewport");
}

TEST_CASE("sdl_gpu pass2d: the 2D pipeline's pass of a letterboxed camera, end to end") {
    // A 2D camera showing 4 x 2 world units in the 8 x 8 picture: viewport (0, 2) 8 x 4, two pixels a unit.
    namespace Cam = GYO::Engine::Camera;
    Cam::OrthographicProjection o = Cam::OrthographicProjection::Default2d();
    o.scalingMode = Cam::ScalingFixed{4.0F, 2.0F};
    const Cam::TargetSize size{{8, 8}, 1.0F};
    const Cam::Letterbox box{4.0F, 2.0F};
    R::ExtractedCamera camera;
    camera.computed = Cam::Compute(Cam::Camera{}, Cam::Projection{o}, &size, &box);
    camera.view = Cam::ViewOf(camera.computed, GYO::Engine::Transform::GlobalTransform{});  // at the origin
    camera.is2d = true;
    camera.clear = R::Color8{0, 0, 0, 255};
    // A red square over world x -2..0 (z 0) and a green one over -0.5..1.5 (z 1, in front), y up.
    const auto item = [](std::uint64_t entity, float x, float z, R::Color8 colour) {
        R::Extracted2dItem i;
        i.entity = entity;
        GYO::Engine::Transform::Transform t;
        t.translation = {x, 0.0F, z};
        i.world = GYO::Engine::Transform::ToMatrix(t);
        i.vertices = {{{-1, 1}, {}, colour}, {{1, 1}, {}, colour}, {{1, -1}, {}, colour}, {{-1, -1}, {}, colour}};
        i.indices = {0, 1, 2, 0, 2, 3};
        return i;
    };
    const std::vector<R::Extracted2dItem> items{item(2, 0.5F, 1.0F, kGreen), item(1, -1.0F, 0.0F, kRed)};
    std::optional<R::Pass2D> pass = R::Queue2d(camera, items);
    REQUIRE(pass.has_value());
    // As camera_render composes a frame: the camera's clear, then its pass.
    R::ExtractedCameras cameras;
    cameras.cameras.push_back(camera);
    R::CameraPasses slots;
    slots.byCamera.resize(1);
    slots.byCamera[0][R::CameraPhase::Transparent].push_back(std::move(*pass));
    R::RenderCommands commands;
    R::ComposePasses(cameras, slots, commands);
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 8, 2, R::Color8{0, 0, 0, 255}), "[gp-pipe-bar-top] the bar above");
    CHECK_MESSAGE(p.All(0, 6, 8, 8, R::Color8{0, 0, 0, 255}), "[gp-pipe-bar-bottom] the bar below");
    CHECK_MESSAGE(p.All(0, 2, 3, 6, kRed), "[gp-pipe-red] world x -2..-0.5 is pixels 0..3");
    CHECK_MESSAGE(p.All(3, 2, 7, 6, kGreen), "[gp-pipe-green] the nearer square over the farther one");
    CHECK_MESSAGE(p.All(7, 2, 8, 6, R::Color8{0, 0, 0, 255}), "[gp-pipe-rest] nothing past x 1.5");
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
    // Shown: a hidden window gives no picture (every frame skipped, which is not an error), and this case checks
    // that frames reach the window (Xvfb on L1, a desktop elsewhere).
    settings.hidden = false;
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
    // The window shows up a few frames after it is made (the display maps it): frames until one is shown, at most 120,
    // pumping the OS events as a runner does.
    GYO::Plugins::SDLWindow::SDLWindowPump* pump = product.Buffer().Find<GYO::Plugins::SDLWindow::SDLWindowPump>();
    REQUIRE(pump != nullptr);
    int kFrames = 0;
    while (kFrames < 4 || (status->FramesShown() == 0 && kFrames < 120)) {
        pump->Pump();
        auto ran = product.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message));
        ++kFrames;
    }
    CHECK_MESSAGE(status->FramesShown() >= 1, "[gp-frames-shown] a frame reached the window (" << status->FramesSkipped() << " skipped)");
    CHECK_MESSAGE(status->FramesFailed() == 0, "[gp-frames-none-failed] no frame failed: " << status->LastFailure());
    CHECK_MESSAGE(status->FramesShown() + status->FramesSkipped() == kFrames, "[gp-frames-each] each frame is shown or skipped");
    CHECK_MESSAGE(status->RefusedTextureAsks() == 0, "[gp-frames-asks] the texture ask was applied");
    CHECK_MESSAGE(status->SkippedDraws() == 0, "[gp-frames-draws] no draw skipped");
    CHECK_MESSAGE(!status->DriverName().empty(), "[gp-frames-driver] SDL names its driver");
    CHECK_MESSAGE(status->TextureCount() == 1, "[gp-frames-texture] the texture asked for in the first frame has a GPU texture");

    // VSync off and on again from render's Presentation (window-mode-vsync declaration, section 4.3).
    CHECK_MESSAGE(status->ActivePresentMode() == G::SwapchainPresentMode::Vsync, "[gp-present-default] the swapchain starts with VSYNC");
    K::World& world = product.World("Main").GetWorld();
    world.ResourceMut<R::Presentation>()->presentMode = R::PresentMode::AutoNoVsync;
    pump->Pump();
    REQUIRE(static_cast<bool>(product.RunFrame()));
    const G::SwapchainPresentMode off = status->ActivePresentMode();
    MESSAGE("VSync off on " << status->DriverName() << ": " << std::string(G::ToString(off)));
    CHECK_MESSAGE(status->RefusedPresentModes() == 0, "[gp-present-none-refused] " << status->LastFailure());
    world.ResourceMut<R::Presentation>()->presentMode = R::PresentMode::AutoVsync;
    pump->Pump();
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->ActivePresentMode() == G::SwapchainPresentMode::Vsync, "[gp-present-back] VSync on again is VSYNC");
    CHECK_MESSAGE(status->FramesFailed() == 0, "[gp-present-frames] no frame failed across the changes: " << status->LastFailure());
    pump->Pump();
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->PresentModeSets() == 2, "[gp-present-only-on-change] set once per change, not per frame: " << status->PresentModeSets());
    // An environment premise, not the contract: a driver with VSYNC only keeps it for AutoNoVsync, but every driver
    // these tests run on (Metal, Vulkan on Lavapipe under Xvfb) has IMMEDIATE or MAILBOX, so here it must have changed.
    CHECK_MESSAGE(off != G::SwapchainPresentMode::Vsync,
                  "[gp-present-off] VSync off left VSYNC (" << std::string(G::ToString(off)) << "; the environment premise above)");

    // A replacement that fails before the swap: the old World goes on drawing with its textures.
    O::ReplaceRequest failing;
    failing.extra = [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "refused by the test", {}, "test"));
    };
    REQUIRE_FALSE(static_cast<bool>(product.ReplaceWorld("Main", std::move(failing))));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->TextureCount() == 1, "[gp-world-failed-keeps] a failed replacement forgets no texture");
    // A replacement that succeeds: the new World's ids start again at 1, so the old World's textures go; its
    // Presentation is the default (nothing carried over), so VSync is on again.
    world.ResourceMut<R::Presentation>()->presentMode = R::PresentMode::AutoNoVsync;
    REQUIRE(static_cast<bool>(product.RunFrame()));
    REQUIRE(static_cast<bool>(product.ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->ActivePresentMode() == G::SwapchainPresentMode::Vsync,
                  "[gp-present-new-world] a new World's default Presentation is applied (carry it over to keep VSync off)");
    CHECK_MESSAGE(status->TextureCount() == 0, "[gp-world-new-forgets] the new World's first frame lets the old World's textures go");
    CHECK_MESSAGE(status->FramesFailed() == 0, "[gp-world-frames] no frame failed");
}

// The frame-time budget declared in render.zh-Hant.md section 2 (batch R2a): measured, not judged (the GPU and the
// driver decide it; Lavapipe is far slower than any player's GPU). It prints the mean of Capture (asks, uploads, the
// pass and the read back, waiting for the GPU) of a frame of a common 2D game: 1280 by 720, 2,000 quads, one pass.
TEST_CASE("sdl_gpu budget: a common 2D game's frame") {
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
    R::RenderFrameReceipt receipt;
    static_cast<void>(device.Capture({}, {}, commands, R::TargetSize{1280, 720}, tally, receipt));
    constexpr int kRuns = 20;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRuns; ++i) REQUIRE(static_cast<bool>(device.Capture({}, {}, commands, R::TargetSize{1280, 720}, tally, receipt)));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / kRuns;
    MESSAGE("[gp-budget] driver " << device.DriverName() << ": " << ms << " ms a frame (budget 8 ms, render.zh-Hant.md section 2)");
    CHECK(ms > 0.0);

    // The same frame through the 2D pipeline (render.zh-Hant.md section 7.5): 2,000 extracted items, one camera,
    // Queue2d (sorting, the vertices through the camera, batching) and then Capture.
    namespace Cam = GYO::Engine::Camera;
    Cam::OrthographicProjection o = Cam::OrthographicProjection::Default2d();
    o.scalingMode = Cam::ScalingFixed{1280.0F, 720.0F};
    const Cam::TargetSize size{{1280, 720}, 1.0F};
    R::ExtractedCamera camera;
    camera.computed = Cam::Compute(Cam::Camera{}, Cam::Projection{o}, &size);
    camera.view = Cam::ViewOf(camera.computed, GYO::Engine::Transform::GlobalTransform{});  // at the origin
    camera.is2d = true;
    camera.clear = kBlue;
    std::vector<R::Extracted2dItem> items(2000);
    for (std::size_t i = 0; i < items.size(); ++i) {
        GYO::Engine::Transform::Transform t;
        t.translation = {static_cast<float>((i * 37) % 1240) - 620.0F, static_cast<float>((i * 53) % 680) - 340.0F,
                         static_cast<float>(i % 7)};
        items[i].entity = i + 1;
        items[i].world = GYO::Engine::Transform::ToMatrix(t);
        const R::Color8 c{255, 0, 0, 128};
        items[i].vertices = {{{-20, 20}, {}, c}, {{20, 20}, {}, c}, {{20, -20}, {}, c}, {{-20, -20}, {}, c}};
        items[i].indices = {0, 1, 2, 0, 2, 3};
    }
    const auto pipeStart = std::chrono::steady_clock::now();
    for (int i = 0; i < kRuns; ++i) {
        std::optional<R::Pass2D> queued = R::Queue2d(camera, items);
        REQUIRE(queued.has_value());
        R::RenderCommands piped;
        piped.passes.push_back(std::move(*queued));
        REQUIRE(static_cast<bool>(device.Capture({}, {}, piped, R::TargetSize{1280, 720}, tally, receipt)));
    }
    const double pipeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pipeStart).count() / kRuns;
    MESSAGE("[gp-budget-pipeline] driver " << device.DriverName() << ": " << pipeMs
                                           << " ms a frame through Queue2d (budget 8 ms, render.zh-Hant.md section 7.5)");
    CHECK(pipeMs > 0.0);
}

// ---- Render targets and texture formats (render-pipeline declaration section 13, A1; label gpu, ctest sdl_gpu.targets)

namespace {

// One case's textures on the shared device: what the case leaves live is let go at its end, so the next case starts
// from a device without textures (TextureCount 0).
struct TargetCase {
    R::RenderTextures textures;
    std::vector<R::TextureId> live;

    R::TextureId Target(const R::RenderTargetDesc& desc) { return live.emplace_back(textures.CreateTarget(desc)); }
    R::TextureId Texture(const R::TextureDesc& desc, std::vector<std::uint8_t> pixels) {
        return live.emplace_back(textures.Create(desc, std::move(pixels)));
    }
    void Release(const R::TextureId id) {
        textures.Release(id);
        live.erase(std::find(live.begin(), live.end(), id));
    }
    // A frame on a main target of size: this case's asks (targets that follow the main target take its size, as
    // camera_render gives it), then they are cleared (as render_world's Cleanup).
    Picture Frame(const R::RenderCommands& commands, const R::TargetSize size = kSize) {
        textures.SetMainSize(size);
        Picture picture;
        R::RenderFrameReceipt receipt;
        auto captured = Device().Capture(textures.Pending(), {}, commands, size, picture.tally, receipt);
        REQUIRE_MESSAGE(static_cast<bool>(captured), (captured ? std::string() : captured.error().message));
        picture.image = std::move(*captured);
        // As render_world's Cleanup: the frame ends with the backend's receipt (kept in the picture as it was written).
        picture.receipt = receipt;
        textures.EndFrame(commands, receipt);
        return picture;
    }
    ~TargetCase() {
        for (auto it = live.rbegin(); it != live.rend(); ++it) textures.Release(*it);
        G::FrameTally tally;
        R::RenderFrameReceipt receipt;
        static_cast<void>(Device().Capture(textures.Pending(), {}, R::RenderCommands{}, kSize, tally, receipt));
    }
};

R::RenderTargetDesc Fixed(const std::uint32_t width, const std::uint32_t height) {
    return R::RenderTargetDesc{R::TargetSize{width, height}, R::RenderTargetFormat::Rgba8Unorm, R::TextureFilter::Nearest};
}

// The whole of a target sampled onto the main target's (0, 0) .. (width, height), nearest pixel.
R::Pass2D SampleTarget(const R::TextureId target, const float width, const float height) {
    R::Pass2D pass = Cleared(kWhite);
    Quad(pass, Rect{0, 0, width, height}, kWhite, target, R::BlendMode::Opaque);
    return pass;
}

R::Pass2D On(const R::TextureId target, const std::optional<R::Color8> clear = std::nullopt) {
    R::Pass2D pass;
    pass.target = target;
    pass.clear = clear;
    return pass;
}

} // namespace

TEST_CASE("sdl_gpu targets: a pass draws into an 8 x 8 target and a later pass samples it onto the main target") {
    TargetCase c;
    const R::TextureId target = c.Target(Fixed(8, 8));
    R::RenderCommands commands;
    // Into the target: blue, a red left half, a green bottom-right quarter.
    R::Pass2D into = On(target, kBlue);
    Quad(into, Rect{0, 0, 4, 8}, kRed);
    Quad(into, Rect{4, 4, 4, 4}, kGreen);
    commands.passes.push_back(std::move(into));
    // The main target shows the target, flipped left to right by its uv (so its left half lands on the right).
    R::Pass2D main = Cleared(kWhite);
    main.vertices = {{{0, 0}, {1, 0}, kWhite}, {{8, 0}, {0, 0}, kWhite}, {{8, 8}, {0, 1}, kWhite}, {{0, 8}, {1, 1}, kWhite}};
    main.indices = {0, 1, 2, 0, 2, 3};
    main.draws.push_back(R::Draw2D{target, R::BlendMode::Opaque, std::nullopt, 0, 6, 0});
    commands.passes.push_back(std::move(main));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE(p.tally.unmappedPasses == 0, "[gt-target-mapped] a pass on a target RenderTextures made is drawn");
    CHECK_MESSAGE(p.tally.refusedAsks == 0, "[gt-target-made] " << p.tally.lastRefusal);
    CHECK_MESSAGE(p.All(4, 0, 8, 8, kRed), "[gt-target-sampled] the target's red half, sampled and flipped onto the right");
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kBlue), "[gt-target-sampled] the target's blue, top left");
    CHECK_MESSAGE(p.All(0, 4, 4, 8, kGreen), "[gt-target-sampled] the target's green quarter, bottom left");

    // A later frame: a 3D pass draws into the target too (its own depth, the target's size), then the main pass.
    R::RenderCommands next;
    next.passes.push_back(On(target, kBlue));
    R::Pass3D scene;
    scene.target = target;
    scene.vertices = {{{-1, -1, 0.5F}, {}, {}}, {{1, -1, 0.5F}, {}, {}}, {{1, 1, 0.5F}, {}, {}}, {{-1, 1, 0.5F}, {}, {}}};
    scene.indices = {0, 1, 2, 0, 2, 3};
    scene.instances.push_back(R::Instance3D{{}, kYellow});
    scene.draws.push_back(R::Draw3D{{R::MeshId{}, 0, 6, 0, 0, 1}});
    next.passes.push_back(std::move(scene));
    next.passes.push_back(SampleTarget(target, 8, 8));
    const Picture q = c.Frame(next);
    CHECK_MESSAGE(q.All(0, 0, 8, 8, kYellow), "[gt-target-3d] a 3D pass on the target covers it (clip space is the target's)");
    CHECK_MESSAGE(q.tally.skippedDraws == 0, "[gt-target-3d] " << q.tally.lastRefusal);

    // Let go: a pass on it is counted as unmapped and not drawn; its GPU texture is gone.
    c.Release(target);
    R::RenderCommands gone;
    gone.passes.push_back(On(target, kRed));
    gone.passes.push_back(Cleared(kGreen));
    const Picture r = c.Frame(gone);
    CHECK_MESSAGE((r.tally.unmappedPasses == 1 && r.All(0, 0, 8, 8, kGreen)), "[gt-target-released-unmapped] a pass on a released target");
    CHECK_MESSAGE(Device().TextureCount() == 0, "[gt-target-released] the target's GPU texture is gone");
}

TEST_CASE("sdl_gpu targets: an sRGB texture is sampled as linear values; a Unorm one as stored") {
    TargetCase c;
    // The same bytes in both: 188 and 128 (sRGB-encoded 0.5029 and 0.2158 in linear), alpha 255.
    const std::vector<std::uint8_t> bytes{188, 188, 188, 255, 128, 128, 128, 255};
    const R::TextureId srgb = c.Texture(R::TextureDesc{2, 1, R::TextureFilter::Nearest, R::TextureFormat::Rgba8UnormSrgb}, bytes);
    const R::TextureId stored = c.Texture(R::TextureDesc{2, 1, R::TextureFilter::Nearest, R::TextureFormat::Rgba8Unorm}, bytes);
    R::RenderCommands commands;
    R::Pass2D pass = Cleared(kBlue);
    Quad(pass, Rect{0, 0, 8, 4}, kWhite, srgb);
    Quad(pass, Rect{0, 4, 8, 4}, kWhite, stored);
    commands.passes.push_back(std::move(pass));
    const Picture p = c.Frame(commands);
    REQUIRE_MESSAGE(p.tally.refusedAsks == 0, p.tally.lastRefusal);
    // The main target is Unorm: what the shader read is written as it is (0.5029 * 255 = 128.2, 0.2158 * 255 = 55.0).
    const R::Color8 decoded = p.At(1, 1);
    const R::Color8 decodedDark = p.At(6, 1);
    CHECK_MESSAGE((Diff(decoded.r, 128) <= 1 && decoded.r == decoded.g && decoded.g == decoded.b && decoded.a == 255),
                  "[gt-srgb-decoded] sRGB 188 sampled as " << int(decoded.r));
    CHECK_MESSAGE(Diff(decodedDark.r, 55) <= 1, "[gt-srgb-decoded] sRGB 128 sampled as " << int(decodedDark.r));
    CHECK_MESSAGE((p.All(0, 4, 4, 8, R::Color8{188, 188, 188, 255}) && p.All(4, 4, 8, 8, R::Color8{128, 128, 128, 255})),
                  "[gt-unorm-stored] a Unorm texture is sampled as stored");
}

TEST_CASE("sdl_gpu targets: a target follows the main target's size, or another target's, again when it changes") {
    TargetCase c;
    const R::TextureId target = c.Target(R::RenderTargetDesc{R::FollowTarget{}, R::RenderTargetFormat::Rgba8Unorm, R::TextureFilter::Nearest});
    // Into the target: blue, a red square of 4 x 4 pixels at its top-left (no view: one unit a pixel).
    const auto frame = [&](const R::TargetSize size) {
        R::RenderCommands commands;
        R::Pass2D into = On(target, kBlue);
        Quad(into, Rect{0, 0, 4, 4}, kRed);
        commands.passes.push_back(std::move(into));
        commands.passes.push_back(SampleTarget(target, static_cast<float>(size.width), static_cast<float>(size.height)));
        return commands;
    };
    const Picture small = c.Frame(frame(kSize), kSize);
    REQUIRE_MESSAGE(small.tally.refusedAsks == 0, small.tally.lastRefusal);
    CHECK_MESSAGE((small.All(0, 0, 4, 4, kRed) && small.All(4, 0, 8, 8, kBlue) && small.All(0, 4, 4, 8, kBlue)),
                  "[gt-target-follows] at 8 x 8 the square is 4 x 4 pixels");
    const R::TargetSize large{16, 16};
    const Picture big = c.Frame(frame(large), large);
    // Had the target kept 8 x 8, its square would be stretched to 8 x 8 pixels of the main target.
    CHECK_MESSAGE((big.All(0, 0, 4, 4, kRed) && big.All(4, 0, 16, 16, kBlue) && big.All(0, 4, 4, 16, kBlue)),
                  "[gt-target-follows] at 16 x 16 the target is 16 x 16 too: the square stays 4 x 4 pixels");
    CHECK_MESSAGE(Device().TextureCount() == 1, "[gt-target-follows] one texture for the target, made again, not added");

    // A target following a fixed 4 x 4 one: sampled over 8 x 8, each of its pixels is 2 x 2 (it is 4 x 4 too).
    const R::TextureId fixed = c.Target(Fixed(4, 4));
    const R::TextureId follower = c.Target(R::RenderTargetDesc{R::FollowTarget{fixed}, R::RenderTargetFormat::Rgba8Unorm, R::TextureFilter::Nearest});
    R::RenderCommands commands;
    commands.passes.push_back(On(fixed, kBlue));
    R::Pass2D into = On(follower, kBlue);
    Quad(into, Rect{0, 0, 1, 1}, kRed);  // one pixel of the follower
    commands.passes.push_back(std::move(into));
    commands.passes.push_back(SampleTarget(follower, 8, 8));
    const Picture other = c.Frame(commands);
    CHECK_MESSAGE((other.All(0, 0, 2, 2, kRed) && other.All(2, 0, 8, 8, kBlue) && other.All(0, 2, 2, 8, kBlue)),
                  "[gt-target-follows-other] the follower is the followed target's 4 x 4");
}

TEST_CASE("sdl_gpu targets: a target keeps its picture from frame to frame; it is transparent black when made") {
    TargetCase c;
    const R::TextureId target = c.Target(Fixed(8, 8));
    // Frame 1: the target's first pass has no clear: the target was just made, so it starts transparent black.
    R::RenderCommands first;
    R::Pass2D left = On(target);
    Quad(left, Rect{0, 0, 4, 8}, kRed);
    first.passes.push_back(std::move(left));
    first.passes.push_back(SampleTarget(target, 8, 8));
    const Picture p = c.Frame(first);
    CHECK_MESSAGE((p.All(0, 0, 4, 8, kRed) && p.All(4, 0, 8, 8, R::Color8{0, 0, 0, 0})), "[gt-target-made-clear] a new target is transparent black");
    // Frame 2: a pass with no clear draws green on the right half over what frame 1 left (Bevy's LoadOp::Load).
    R::RenderCommands second;
    R::Pass2D right = On(target);
    Quad(right, Rect{4, 0, 4, 8}, kGreen);
    second.passes.push_back(std::move(right));
    second.passes.push_back(SampleTarget(target, 8, 8));
    const Picture q = c.Frame(second);
    CHECK_MESSAGE((q.All(0, 0, 4, 8, kRed) && q.All(4, 0, 8, 8, kGreen)), "[gt-target-kept] the red half of frame 1 is still there");
}

TEST_CASE("sdl_gpu targets: a draw that samples the target its pass draws on is skipped and counted") {
    TargetCase c;
    const R::TextureId target = c.Target(Fixed(8, 8));
    R::RenderCommands commands;
    R::Pass2D into = On(target, kBlue);
    Quad(into, Rect{0, 0, 8, 8}, kWhite, target, R::BlendMode::Opaque);  // reads and writes the target at once
    Quad(into, Rect{0, 0, 4, 4}, kRed);
    commands.passes.push_back(std::move(into));
    commands.passes.push_back(SampleTarget(target, 8, 8));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE(p.tally.skippedDraws == 1, "[gt-feedback-skipped] the draw sampling its own target is skipped and counted");
    CHECK_MESSAGE((p.All(0, 0, 4, 4, kRed) && p.All(4, 0, 8, 8, kBlue)), "[gt-feedback-rest] the other draws are drawn");
}

} // namespace GYO::Plugins::SDLGPU
