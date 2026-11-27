// Full-screen and depth-only passes on a GPU (render-pipeline declaration section 14, A2; label gpu, ctest sdl_gpu.phases
// and sdl_gpu.unwritten). sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back
// (GPUDevice::Capture), each pixel compared with its colour; the full-screen passes' shaders are the tests' own
// (shaders/, TestShaders.hpp). Tags "[gf-...]", "[gd-...]" and "[gt-unwritten-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

#include "GPUDevice.hpp"
#include "RenderContracts.hpp"
#include "TestShaders.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

namespace G = GYO::Plugins::SDLGPU;
namespace R = GYO::Engine::Render;
using GYO::Engine::Math::Rect;

namespace {

constexpr R::TargetSize kSize{8, 8};
constexpr R::Color8 kBlue{0, 0, 255, 255};
constexpr R::Color8 kRed{255, 0, 0, 255};
constexpr R::Color8 kWhite{255, 255, 255, 255};
constexpr R::Color8 kYellow{255, 255, 0, 255};
constexpr R::Color8 kCyan{0, 255, 255, 255};

// One device for every case, carrying the tests' own shaders.
G::GPUDevice& Device() {
    static std::unique_ptr<G::GPUDevice> device = [] {
        auto made = G::GPUDevice::Create(nullptr, true, G::TestShaders());  // SDL's validation on
        REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
        return std::move(*made);
    }();
    return *device;
}

void Quad(R::Pass2D& pass, const Rect r, const R::Color8 colour, const R::TextureId texture = {}) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    pass.vertices.push_back({{r.x, r.y}, {0.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y}, {1.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y + r.height}, {1.0F, 1.0F}, colour});
    pass.vertices.push_back({{r.x, r.y + r.height}, {0.0F, 1.0F}, colour});
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    pass.draws.push_back(R::Draw2D{texture, R::BlendMode::Opaque, std::nullopt, first, 6, 0});
}

R::Pass2D On(const std::optional<R::TextureId> target, const R::Color8 clear) {
    R::Pass2D pass;
    pass.target = target;
    pass.clear = clear;
    return pass;
}

// A rectangle of clip space (y up) at depth z, one instance, in a Pass3D or a PassDepth.
template <class Pass>
void ClipQuad(Pass& pass, const float x0, const float x1, const float z, const R::Color8 colour = kWhite) {
    pass.vertices = {{{x0, -1, z}, {}, {}}, {{x1, -1, z}, {}, {}}, {{x1, 1, z}, {}, {}}, {{x0, 1, z}, {}, {}}};
    pass.indices = {0, 1, 2, 0, 2, 3};
    pass.instances.push_back(R::Instance3D{{}, colour});
    // One draw of the pass's own six indices and its one instance (a Draw3D or a MeshDraw).
    pass.draws.emplace_back();
    pass.draws.back().indexCount = 6;
    pass.draws.back().instanceCount = 1;
}

R::PassFullscreen Fullscreen(const std::optional<R::TextureId> target, std::string shader, std::vector<R::TextureId> inputs,
                             std::vector<float> lanes = {}) {
    R::PassFullscreen pass;
    pass.target = target;
    pass.shader = std::move(shader);
    pass.inputs = std::move(inputs);
    pass.parameters.resize(lanes.size() * sizeof(float));
    if (!lanes.empty()) std::memcpy(pass.parameters.data(), lanes.data(), pass.parameters.size());
    return pass;
}

struct Picture {
    G::CapturedImage image;
    G::FrameTally tally;
    [[nodiscard]] R::Color8 At(const std::uint32_t x, const std::uint32_t y) const {
        const std::size_t i = (static_cast<std::size_t>(y) * image.size.width + x) * 4;
        return R::Color8{image.pixels[i], image.pixels[i + 1], image.pixels[i + 2], image.pixels[i + 3]};
    }
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
    // Every pixel's red channel of [x0, x1) x [y0, y1) is within one step of red.
    [[nodiscard]] bool Red(const std::uint32_t x0, const std::uint32_t y0, const std::uint32_t x1, const std::uint32_t y1, const int red) const {
        for (std::uint32_t y = y0; y < y1; ++y) {
            for (std::uint32_t x = x0; x < x1; ++x) {
                if (std::abs(static_cast<int>(At(x, y).r) - red) > 1) {
                    MESSAGE("pixel (" << x << ", " << y << ") has red " << int(At(x, y).r) << ", not " << red);
                    return false;
                }
            }
        }
        return true;
    }
};

// One case's textures on the shared device, let go at its end (as Pass2DTests' TargetCase).
struct PhaseCase {
    R::RenderTextures textures;
    std::vector<R::TextureId> live;

    R::TextureId Target(const R::RenderTargetDesc& desc) { return live.emplace_back(textures.CreateTarget(desc)); }
    Picture Frame(const R::RenderCommands& commands) {
        Picture picture;
        auto captured = Device().Capture(textures.Pending(), {}, commands, kSize, picture.tally);
        REQUIRE_MESSAGE(static_cast<bool>(captured), (captured ? std::string() : captured.error().message));
        picture.image = std::move(*captured);
        // As render_world's Cleanup: the frame's account applied, then the asks cleared.
        textures.Apply(R::AssumedReport(commands, textures.Pending()));
        textures.ClearPending();
        return picture;
    }
    ~PhaseCase() {
        for (auto it = live.rbegin(); it != live.rend(); ++it) textures.Release(*it);
        G::FrameTally tally;
        static_cast<void>(Device().Capture(textures.Pending(), {}, R::RenderCommands{}, kSize, tally));
    }
};

R::RenderTargetDesc Fixed(const R::RenderTargetFormat format = R::RenderTargetFormat::Rgba8Unorm) {
    return R::RenderTargetDesc{kSize, format, R::TextureFilter::Nearest};
}

} // namespace

TEST_CASE("sdl_gpu phases: two full-screen passes in a chain: the first inverts a target, the second shifts the result") {
    PhaseCase c;
    const R::TextureId source = c.Target(Fixed());
    const R::TextureId inverted = c.Target(Fixed());
    R::RenderCommands commands;
    // The source: red over x 0..2 of the top half, blue elsewhere.
    R::Pass2D into = On(source, kBlue);
    Quad(into, Rect{0, 0, 2, 4}, kRed);
    commands.passes.push_back(std::move(into));
    // Inverted: red is cyan, blue is yellow. Then onto the main target, moved right by 2 pixels (0.25 in uv): a pixel
    // at x shows x - 2 (the left edge repeated).
    commands.passes.push_back(Fullscreen(inverted, "test_invert.frag", {source}));
    commands.passes.push_back(Fullscreen(std::nullopt, "test_shift.frag", {inverted}, {0.25F, 0.0F, 0.0F, 0.0F}));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE(p.tally.skippedDraws == 0, "[gf-drawn] " << p.tally.lastRefusal);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kCyan), "[gf-chain] the inverted red, shifted right by 2 pixels, still at the top");
    CHECK_MESSAGE((p.All(4, 0, 8, 4, kYellow) && p.All(0, 4, 8, 8, kYellow)), "[gf-chain] the inverted blue around it");

    // The next frame retains the inverted target (no pass draws on it): its last picture is sampled.
    R::RenderCommands next;
    next.retained.push_back(inverted);
    next.passes.push_back(Fullscreen(std::nullopt, "test_shift.frag", {inverted}, {0.0F, 0.0F, 0.0F, 0.0F}));
    const Picture q = c.Frame(next);
    CHECK_MESSAGE((q.All(0, 0, 2, 4, kCyan) && q.All(2, 0, 8, 4, kYellow) && q.All(0, 4, 8, 8, kYellow)),
                  "[gf-reused-sampled] the inverted picture of the last frame, not moved");

    // One full-screen pass reading what a 2D pass drew (the chain's two passes would turn a wrong uv round twice): the
    // source's red is at the top, so uv runs down the target as a 2D pass's does. The source is reused.
    R::RenderCommands once;
    once.retained.push_back(source);
    once.passes.push_back(Fullscreen(std::nullopt, "test_invert.frag", {source}));
    const Picture r = c.Frame(once);
    CHECK_MESSAGE((r.All(0, 0, 2, 4, kCyan) && r.All(0, 4, 2, 8, kYellow)), "[gf-uv-down] uv (0, 0) is the target's top-left");
}

TEST_CASE("sdl_gpu phases: a full-screen pass whose shader the device does not carry draws nothing and is counted") {
    PhaseCase c;
    R::RenderCommands commands;
    commands.passes.push_back(On(std::nullopt, kBlue));
    commands.passes.push_back(Fullscreen(std::nullopt, "no_such.frag", {}));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE((p.tally.skippedDraws == 1 && p.All(0, 0, 8, 8, kBlue)), "[gf-unknown-counted] nothing drawn, one skipped draw");
}

TEST_CASE("sdl_gpu phases: a depth-only pass writes a Depth target; a 3D pass tests against it; a 2D pass samples it") {
    PhaseCase c;
    const R::TextureId depth = c.Target(Fixed(R::RenderTargetFormat::Depth));
    const R::TextureId colour = c.Target(Fixed());
    R::RenderCommands commands;
    // The prepass: the left half of clip space at depth 0.25; the rest stays at the clear, 1.
    R::PassDepth prepass;
    prepass.target = depth;
    ClipQuad(prepass, -1.0F, 0.0F, 0.25F);
    commands.passes.push_back(std::move(prepass));
    // The scene: a yellow quad over clip x -1..0.5 (pixels 0..6) at depth 0.5, tested against the prepass's depth (kept,
    // not cleared) and writing its own.
    commands.passes.push_back(On(colour, kBlue));
    R::Pass3D scene;
    scene.target = colour;
    scene.depth = depth;
    scene.clearDepth = false;
    ClipQuad(scene, -1.0F, 0.5F, 0.5F, kYellow);
    commands.passes.push_back(std::move(scene));
    // The main target: the scene above, the depth below (its first channel as red).
    R::Pass2D main = On(std::nullopt, kWhite);
    Quad(main, Rect{0, 0, 8, 4}, kWhite, colour);
    Quad(main, Rect{0, 4, 8, 4}, kWhite, depth);
    commands.passes.push_back(std::move(main));
    const Picture p = c.Frame(commands);
    REQUIRE_MESSAGE((p.tally.refusedAsks == 0 && p.tally.skippedDraws == 0 && p.tally.unmappedPasses == 0), "[gd-made] " << p.tally.lastRefusal);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kBlue), "[gd-tested] the scene's quad behind the prepass's depth is hidden");
    CHECK_MESSAGE((p.All(4, 0, 6, 4, kYellow) && p.All(6, 0, 8, 4, kBlue)), "[gd-tested-in-front] and drawn where the prepass left the clear");
    CHECK_MESSAGE(p.Red(0, 4, 4, 8, 64), "[gd-sampled] the depth the prepass wrote, 0.25 (64 of 255)");
    CHECK_MESSAGE(p.Red(4, 4, 6, 8, 128), "[gd-sampled-scene] the depth the 3D pass wrote into it, 0.5");
    CHECK_MESSAGE(p.Red(6, 4, 8, 8, 255), "[gd-sampled-clear] the cleared depth, 1");

    // A 3D pass whose draw samples the depth target it tests and writes reads and writes one texture at once: skipped.
    R::RenderCommands loop;
    R::Pass3D own;
    own.target = colour;
    own.depth = depth;
    ClipQuad(own, -1.0F, 1.0F, 0.5F);
    own.draws.back().texture = depth;
    loop.passes.push_back(std::move(own));
    const Picture q = c.Frame(loop);
    CHECK_MESSAGE(q.tally.skippedDraws == 1, "[gd-own-depth-skipped] the draw sampling its pass's depth target is skipped and counted");
}

TEST_CASE("sdl_gpu phases: a 3D pass on the main target, its depth target following the main target at the data side's size, the picture another: draws skipped, counted") {
    PhaseCase c;
    // The data side's window is 16 x 16 a frame before the window's picture (8 x 8) is.
    c.textures.SetMainSize(R::TargetSize{16, 16});
    const R::TextureId depth = c.Target(R::RenderTargetDesc{R::FollowTarget{}, R::RenderTargetFormat::Depth});
    R::RenderCommands commands;
    commands.passes.push_back(On(std::nullopt, kBlue));
    R::Pass3D scene;
    scene.depth = depth;
    ClipQuad(scene, -1.0F, 1.0F, 0.5F, kYellow);
    commands.passes.push_back(std::move(scene));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE((p.tally.skippedDraws == 1 && p.All(0, 0, 8, 8, kBlue)), "[gd-depth-size-skipped] its draws skipped and counted, not a Programmer Error");
}

TEST_CASE("sdl_gpu contracts: a frame whose sampled target is drawn this frame, then retained with its picture, passes the backend's checks") {
    // No GPU: the checks are the frame's data (RP-3; a 3D pass's depth target of its target's size).
    R::RenderTextures textures;
    const R::TextureId target = textures.CreateTarget(Fixed());
    R::RenderCommands drawn;
    drawn.passes.push_back(On(target, kBlue));
    R::Pass2D main = On(std::nullopt, kWhite);
    Quad(main, Rect{0, 0, 8, 8}, kWhite, target);
    drawn.passes.push_back(main);
    G::RequireRenderContracts(drawn, textures);
    textures.Apply(R::AssumedReport(drawn, textures.Pending()));
    textures.ClearPending();
    R::RenderCommands retained;
    retained.retained.push_back(target);
    retained.passes.push_back(std::move(main));
    G::RequireRenderContracts(retained, textures);
    CHECK_MESSAGE(textures.HasPicture(target), "[gt-contracts-kept] the target drawn on the frame before keeps its picture");
}

TEST_CASE("sdl_gpu unwritten: sampling a render target neither drawn on nor retained with a picture this frame is a Programmer Error (RP-3)" * doctest::skip()) {
    // The assertion fails in what the backend checks before it draws: the handler tells it and ends the process (ctest
    // sdl_gpu.unwritten passes on the tag). No GPU: the check is the frame's data.
    GYO::Engine::Base::SetAssertionHandler([](const GYO::Engine::Base::AssertionFailure& failure) {
        std::printf("[gt-unwritten-asserted] %s\n", failure.expression);
        std::fflush(stdout);
        std::_Exit(0);
    });
    R::RenderTextures textures;
    const R::TextureId target = textures.CreateTarget(Fixed());
    R::RenderCommands commands;
    R::Pass2D main = On(std::nullopt, kWhite);
    Quad(main, Rect{0, 0, 8, 8}, kWhite, target);
    commands.passes.push_back(std::move(main));
    // Retained, but it has no picture (made and never drawn on): nothing stands for this frame's.
    commands.retained.push_back(target);
    G::RequireRenderContracts(commands, textures);
    GYO::Engine::Base::SetAssertionHandler(nullptr);
    FAIL("[gt-unwritten-not-asserted] the target neither drawn on nor retained with a picture was sampled without a Programmer Error");
}

} // namespace GYO::Plugins::SDLGPU
