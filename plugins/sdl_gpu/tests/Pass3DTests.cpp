// The 3D pass on a GPU (docs/architecture/target/v2/render.zh-Hant.md section 8, sdl.zh-Hant.md "sdl_gpu"; label gpu).
// As Pass2DTests.cpp: sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back
// (GPUDevice::Capture), and each pixel is compared with its exact colour. The 3D passes see the world through an
// orthographic clipFromWorld built by hand (math.md: y up, clip depth 0 near and 1 far): world x 0..8 and y 0..8 are
// the picture, one unit a pixel, world y 0 at the bottom row; world z is the depth. Tags "[g3-...]" are the mutants'
// expectations.
#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/input/InputModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderMeshes.hpp"
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
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace RW = GYO::Engine::RenderWorld;
using GYO::Engine::Math::Rect;

namespace {

constexpr R::TargetSize kSize{8, 8};
constexpr R::Color8 kNone{0, 0, 0, 0};
constexpr R::Color8 kBlue{0, 0, 255, 255};
constexpr R::Color8 kRed{255, 0, 0, 255};
constexpr R::Color8 kGreen{0, 255, 0, 255};
constexpr R::Color8 kWhite{255, 255, 255, 255};
constexpr R::Color8 kYellow{255, 255, 0, 255};
constexpr R::Color8 kMagenta{255, 0, 255, 255};

// One device for every case of this file (making one per case is slow on Lavapipe).
G::GPUDevice& Device() {
    static std::unique_ptr<G::GPUDevice> device = [] {
        auto made = G::GPUDevice::Create(nullptr, true);  // SDL's validation on
        REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
        return std::move(*made);
    }();
    return *device;
}

// World x 0..8 and y 0..8 to clip -1..1 (y up), z kept as the depth.
M::Matrix4 Ortho() { return M::Multiply(M::MakeTranslation({-1.0F, -1.0F, 0.0F}), M::MakeScale({0.25F, 0.25F, 1.0F})); }

// A rectangle of world x0..x1, y0..y1 at depth z: its four corners counter-clockwise on the screen from the bottom
// left, uv (0, 0) at the top-left corner and (1, 1) at the bottom-right one.
std::vector<R::Vertex3D> QuadVertices(const float x0, const float y0, const float x1, const float y1, const float z) {
    return {{{x0, y0, z}, {0, 0, -1}, {0, 1}}, {{x1, y0, z}, {0, 0, -1}, {1, 1}}, {{x1, y1, z}, {0, 0, -1}, {1, 0}},
            {{x0, y1, z}, {0, 0, -1}, {0, 0}}};
}

// Adds the rectangle to the pass's own geometry and a draw of it; returns the draw to adjust.
R::Draw3D& Quad(R::Pass3D& pass, const float x0, const float y0, const float x1, const float y1, const float z, const R::Color8 colour) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    for (const R::Vertex3D& v : QuadVertices(x0, y0, x1, y1, z)) pass.vertices.push_back(v);
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    R::Draw3D draw;
    draw.indexOffset = first;
    draw.indexCount = 6;
    draw.color = colour;
    pass.draws.push_back(draw);
    return pass.draws.back();
}

R::Pass3D Scene() {
    R::Pass3D pass;
    pass.clipFromWorld = Ortho();
    return pass;
}

R::Pass2D Cleared(const R::Color8 colour) {
    R::Pass2D pass;
    pass.clear = colour;
    return pass;
}

// A 2D rectangle in pixels (y down), as Pass2DTests.cpp's.
void Quad2D(R::Pass2D& pass, const Rect r, const R::Color8 colour) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    pass.vertices.push_back({{r.x, r.y}, {0.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y}, {1.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y + r.height}, {1.0F, 1.0F}, colour});
    pass.vertices.push_back({{r.x, r.y + r.height}, {0.0F, 1.0F}, colour});
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    pass.draws.push_back(R::Draw2D{{}, R::BlendMode::Alpha, std::nullopt, first, 6, 0});
}

struct Picture {
    G::CapturedImage image;
    G::FrameTally tally;
    [[nodiscard]] R::Color8 At(const std::uint32_t x, const std::uint32_t y) const {
        const std::size_t i = (static_cast<std::size_t>(y) * image.size.width + x) * 4;
        return R::Color8{image.pixels[i], image.pixels[i + 1], image.pixels[i + 2], image.pixels[i + 3]};
    }
    // Every pixel of columns [x0, x1) and rows [y0, y1) (row 0 on top) is exactly this colour.
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

Picture Draw(const R::RenderCommands& commands, const R::RenderTextures* textures = nullptr, const R::RenderMeshes* meshes = nullptr) {
    Picture picture;
    const std::span<const R::TextureOp> asks = textures != nullptr ? textures->Pending() : std::span<const R::TextureOp>{};
    const std::span<const R::MeshOp> meshAsks = meshes != nullptr ? meshes->Pending() : std::span<const R::MeshOp>{};
    auto captured = Device().Capture(asks, meshAsks, commands, kSize, picture.tally);
    REQUIRE_MESSAGE(static_cast<bool>(captured), (captured ? std::string() : captured.error().message));
    picture.image = std::move(*captured);
    REQUIRE(picture.image.pixels.size() == std::size_t{kSize.width} * kSize.height * 4);
    return picture;
}

int Diff(const std::uint8_t a, const int b) { return std::abs(static_cast<int>(a) - b); }

} // namespace

TEST_CASE("sdl_gpu pass3d: a quad covers exactly its pixels, y up; the target's first pass starts transparent black") {
    R::RenderCommands commands;
    commands.passes.push_back(Cleared(kBlue));
    R::Pass3D pass = Scene();
    Quad(pass, 0, 0, 4, 2, 0.5F, kRed);  // world y 0..2: the bottom two rows
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 6, 4, 8, kRed), "[g3-quad-inside] the quad's pixels are its colour");
    CHECK_MESSAGE(p.All(0, 0, 4, 6, kBlue), "[g3-quad-y-up] above it the 2D pass's clear (world y up is up on the screen)");
    CHECK_MESSAGE(p.All(4, 0, 8, 8, kBlue), "[g3-quad-right] right of it the 2D pass's clear");

    R::RenderCommands alone;
    R::Pass3D only = Scene();
    Quad(only, 0, 0, 4, 2, 0.5F, kRed);
    alone.passes.push_back(std::move(only));
    const Picture q = Draw(alone);
    CHECK_MESSAGE(q.All(0, 6, 4, 8, kRed), "[g3-first-drawn] a 3D pass first on the target draws");
    CHECK_MESSAGE(q.All(4, 0, 8, 8, kNone), "[g3-first-clears] and the rest of the target is transparent black");
}

TEST_CASE("sdl_gpu pass3d: depth: the nearer hides the farther, equal depth draws, no write, no test") {
    {
        // A nearer quad drawn first hides a farther one drawn second; the same depth again draws (LESS_OR_EQUAL).
        R::RenderCommands commands;
        R::Pass3D pass = Scene();
        Quad(pass, 0, 0, 8, 8, 0.2F, kGreen);
        Quad(pass, 0, 0, 4, 8, 0.8F, kRed);
        Quad(pass, 4, 0, 8, 8, 0.2F, kYellow);
        commands.passes.push_back(std::move(pass));
        const Picture p = Draw(commands);
        CHECK_MESSAGE(p.All(0, 0, 4, 8, kGreen), "[g3-depth-hides] the farther quad drawn later is hidden");
        CHECK_MESSAGE(p.All(4, 0, 8, 8, kYellow), "[g3-depth-equal] a quad at the same depth drawn later draws");
    }
    {
        // depthWrite false: a later farther quad still draws over it.
        R::RenderCommands commands;
        R::Pass3D pass = Scene();
        Quad(pass, 0, 0, 8, 8, 0.2F, kRed).depthWrite = false;
        Quad(pass, 0, 0, 4, 8, 0.8F, kGreen);
        commands.passes.push_back(std::move(pass));
        const Picture p = Draw(commands);
        CHECK_MESSAGE(p.All(0, 0, 4, 8, kGreen), "[g3-no-write] a quad that writes no depth hides nothing drawn after it");
        CHECK_MESSAGE(p.All(4, 0, 8, 8, kRed), "[g3-no-write-drawn] it is drawn itself");
    }
    {
        // depthTest false: drawn over a nearer quad, and writes no depth (a later quad behind it but in front of the
        // first one draws).
        R::RenderCommands commands;
        R::Pass3D pass = Scene();
        Quad(pass, 0, 0, 8, 8, 0.5F, kGreen);
        Quad(pass, 0, 0, 4, 8, 0.8F, kRed).depthTest = false;
        Quad(pass, 4, 0, 8, 8, 0.1F, kYellow).depthTest = false;
        Quad(pass, 4, 0, 8, 8, 0.3F, kBlue);
        commands.passes.push_back(std::move(pass));
        const Picture p = Draw(commands);
        CHECK_MESSAGE(p.All(0, 0, 4, 8, kRed), "[g3-no-test-over] a quad that does not test draws over a nearer one");
        CHECK_MESSAGE(p.All(4, 0, 8, 8, kBlue), "[g3-no-test-no-write] and writes no depth");
    }
}

TEST_CASE("sdl_gpu pass3d: cull Back leaves out triangles clockwise on the screen; None keeps both") {
    // Left: counter-clockwise on the screen (world y up), covering pixel (0, 7). Right: clockwise, covering (4, 7).
    const auto scene = [](const R::CullMode cull) {
        R::Pass3D pass = Scene();
        pass.vertices = {{{0, 0, 0.5F}, {}, {}}, {{4, 0, 0.5F}, {}, {}}, {{0, 4, 0.5F}, {}, {}},
                         {{4, 0, 0.5F}, {}, {}}, {{4, 4, 0.5F}, {}, {}}, {{8, 0, 0.5F}, {}, {}}};
        pass.indices = {0, 1, 2, 3, 4, 5};
        R::Draw3D draw;
        draw.indexCount = 6;
        draw.color = kRed;
        draw.cull = cull;
        pass.draws.push_back(draw);
        R::RenderCommands commands;
        commands.passes.push_back(Cleared(kBlue));
        commands.passes.push_back(std::move(pass));
        return commands;
    };
    const Picture back = Draw(scene(R::CullMode::Back));
    CHECK_MESSAGE(back.At(0, 7) == kRed, "[g3-cull-back-front-kept] Back keeps the counter-clockwise triangle");
    CHECK_MESSAGE(back.At(4, 7) == kBlue, "[g3-cull-back-hides] Back leaves out the clockwise triangle");
    const Picture none = Draw(scene(R::CullMode::None));
    CHECK_MESSAGE((none.At(0, 7) == kRed && none.At(4, 7) == kRed), "[g3-cull-none-both] None keeps both");
}

TEST_CASE("sdl_gpu pass3d: a retained mesh and the pass's own vertices draw; offsets and world apply; a released mesh is skipped") {
    R::RenderMeshes meshes;
    // Two quads: x 0..2 and x 2..4, indices 0..5 and 6..11.
    std::vector<R::Vertex3D> vertices = QuadVertices(0, 0, 2, 8, 0.5F);
    for (const R::Vertex3D& v : QuadVertices(2, 0, 4, 8, 0.5F)) vertices.push_back(v);
    const R::MeshId id = meshes.Create(std::move(vertices), {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7});
    const auto scene = [&] {
        R::Pass3D pass = Scene();
        // The mesh's second quad moved 4 to the right: x 6..8.
        R::Draw3D mesh;
        mesh.mesh = id;
        mesh.indexOffset = 6;
        mesh.indexCount = 6;
        mesh.world = M::MakeTranslation({4.0F, 0.0F, 0.0F});
        mesh.color = kRed;
        pass.draws.push_back(mesh);
        // Own vertices: x 0..2 (vertices 0..3) and x 2..4 (vertices 4..7); the draw names the first's indices and
        // vertexOffset 4: x 2..4.
        Quad(pass, 0, 0, 2, 8, 0.5F, kGreen).vertexOffset = 4;
        for (const R::Vertex3D& v : QuadVertices(2, 0, 4, 8, 0.5F)) pass.vertices.push_back(v);
        R::RenderCommands commands;
        commands.passes.push_back(Cleared(kBlue));
        commands.passes.push_back(std::move(pass));
        return commands;
    };
    const Picture p = Draw(scene(), nullptr, &meshes);
    CHECK_MESSAGE(p.All(6, 0, 8, 8, kRed), "[g3-mesh-world] the mesh's quad, placed by the draw's world");
    CHECK_MESSAGE(p.All(4, 0, 6, 8, kBlue), "[g3-mesh-index-offset] indexOffset picks the mesh's second quad only");
    CHECK_MESSAGE(p.All(2, 0, 4, 8, kGreen), "[g3-own-drawn] the pass's own vertices, vertexOffset applied");
    CHECK_MESSAGE(p.All(0, 0, 2, 8, kBlue), "[g3-vertex-offset] vertexOffset moves the indices to other vertices");
    CHECK_MESSAGE(p.tally.skippedDraws == 0, "[g3-mesh-none-skipped] nothing skipped");
    CHECK_MESSAGE(Device().MeshCount() == 1, "[g3-mesh-made] the mesh has GPU buffers");

    // The next frame: the mesh is kept.
    meshes.ClearPending();
    const Picture q = Draw(scene(), nullptr, &meshes);
    CHECK_MESSAGE(q.All(6, 0, 8, 8, kRed), "[g3-mesh-kept] the mesh is kept between frames");

    // Let go: a draw naming it is skipped and counted.
    meshes.ClearPending();
    meshes.Release(id);
    const Picture r = Draw(scene(), nullptr, &meshes);
    CHECK_MESSAGE(r.All(4, 0, 8, 8, kBlue), "[g3-mesh-released-not-drawn] a released mesh draws nothing");
    CHECK_MESSAGE(r.All(2, 0, 4, 8, kGreen), "[g3-mesh-released-others] the other draws still draw");
    CHECK_MESSAGE(r.tally.skippedDraws == 1, "[g3-mesh-released-counted] the skipped draw is counted");
    CHECK_MESSAGE(Device().MeshCount() == 0, "[g3-mesh-released-gone] its GPU buffers are gone");
}

TEST_CASE("sdl_gpu pass3d: a texture times the draw's colour, top row on top; a missing texture is skipped") {
    R::RenderTextures textures;
    // 2 by 2: red, green on the top row; white, yellow on the bottom row.
    const R::TextureId id = textures.Create(R::TextureDesc{2, 2, R::TextureFilter::Nearest},
                                            {255, 0, 0, 255, 0, 255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 255});
    R::RenderCommands commands;
    R::Pass3D pass = Scene();
    Quad(pass, 0, 0, 8, 8, 0.5F, kMagenta).texture = id;
    Quad(pass, 0, 0, 8, 8, 0.4F, kWhite).texture = R::TextureId{999};
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands, &textures);
    CHECK_MESSAGE(p.All(0, 0, 4, 4, kRed), "[g3-tex-top-left] red times magenta, texture row 0 on top");
    CHECK_MESSAGE(p.All(4, 0, 8, 4, R::Color8{0, 0, 0, 255}), "[g3-tex-top-right] green times magenta is black");
    CHECK_MESSAGE(p.All(0, 4, 4, 8, kMagenta), "[g3-tex-tint] white times magenta is magenta");
    CHECK_MESSAGE(p.All(4, 4, 8, 8, kRed), "[g3-tex-bottom-right] yellow times magenta is red");
    CHECK_MESSAGE(p.tally.skippedDraws == 1, "[g3-tex-missing-counted] a draw naming a texture with no GPU texture is skipped and counted");
    textures.ClearPending();
    textures.Release(id);
    static_cast<void>(Draw(R::RenderCommands{}, &textures));
}

TEST_CASE("sdl_gpu pass3d: Alpha blends straight alpha over the target; Opaque replaces it") {
    R::RenderCommands commands;
    commands.passes.push_back(Cleared(kBlue));
    R::Pass3D pass = Scene();
    Quad(pass, 4, 0, 8, 8, 0.5F, R::Color8{255, 0, 0, 128});  // Opaque, the default
    Quad(pass, 0, 0, 4, 8, 0.5F, R::Color8{255, 0, 0, 128}).blend = R::BlendMode::Alpha;
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    const R::Color8 blended = p.At(1, 1);
    CHECK_MESSAGE((Diff(blended.r, 128) <= 1 && blended.g == 0 && Diff(blended.b, 127) <= 1 && blended.a == 255),
                  "[g3-blend-alpha] red at half alpha over blue");
    CHECK_MESSAGE(p.All(4, 0, 8, 8, R::Color8{255, 0, 0, 128}), "[g3-blend-opaque] Opaque writes the colour and its alpha as they are");
}

TEST_CASE("sdl_gpu pass3d: each pass starts with cleared depth and keeps the colour") {
    R::RenderCommands commands;
    R::Pass3D first = Scene();
    Quad(first, 0, 0, 8, 8, 0.2F, kGreen);
    commands.passes.push_back(std::move(first));
    R::Pass3D second = Scene();
    Quad(second, 4, 0, 8, 8, 0.8F, kRed);  // behind the first pass's quad
    commands.passes.push_back(std::move(second));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(4, 0, 8, 8, kRed), "[g3-pass-depth-cleared] a later pass's quad behind the first pass's is drawn");
    CHECK_MESSAGE(p.All(0, 0, 4, 8, kGreen), "[g3-pass-keeps-colour] a later 3D pass does not clear the colour");
}

TEST_CASE("sdl_gpu pass3d: a pass with clearDepth false keeps the depth the pass before it left") {
    R::RenderCommands commands;
    R::Pass3D first = Scene();
    Quad(first, 0, 0, 8, 8, 0.2F, kGreen);
    commands.passes.push_back(std::move(first));
    R::Pass3D second = Scene();
    second.clearDepth = false;
    Quad(second, 4, 0, 8, 8, 0.8F, kRed);  // behind the first pass's quad: hidden by the depth it kept
    Quad(second, 0, 0, 4, 8, 0.1F, kBlue);  // in front: drawn
    commands.passes.push_back(std::move(second));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(4, 0, 8, 8, kGreen), "[g3-pass-depth-kept] the kept depth hides the later pass's farther quad");
    CHECK_MESSAGE(p.All(0, 0, 4, 8, kBlue), "[g3-pass-depth-kept-near] a nearer one is drawn");
}

TEST_CASE("sdl_gpu pass3d: a viewport takes the clip space") {
    R::RenderCommands commands;
    commands.passes.push_back(Cleared(kBlue));
    R::Pass3D pass = Scene();
    pass.viewport = R::PixelRect{0, 2, 8, 4};  // rows 2..5
    Quad(pass, 0, 0, 8, 8, 0.5F, kRed);    // the whole clip space
    Quad(pass, 0, 0, 8, 4, 0.4F, kGreen);  // its lower half
    commands.passes.push_back(std::move(pass));
    const Picture p = Draw(commands);
    CHECK_MESSAGE(p.All(0, 0, 8, 2, kBlue), "[g3-viewport-above] above the viewport nothing is drawn");
    CHECK_MESSAGE(p.All(0, 6, 8, 8, kBlue), "[g3-viewport-below] below the viewport nothing is drawn");
    CHECK_MESSAGE(p.All(0, 2, 8, 4, kRed), "[g3-viewport-top] the clip space's upper half is the viewport's upper half");
    CHECK_MESSAGE(p.All(0, 4, 8, 6, kGreen), "[g3-viewport-bottom] its lower half the viewport's lower half");
}

TEST_CASE("sdl_gpu pass3d: drawn among 2D passes in their order; a pass on another target is counted") {
    R::RenderMeshes meshes;
    const R::MeshId whole = meshes.Create(QuadVertices(0, 0, 8, 8, 0.5F), {0, 1, 2, 0, 2, 3});
    R::RenderCommands commands;
    R::Pass2D before = Cleared(kBlue);
    Quad2D(before, Rect{0, 0, 2, 2}, kYellow);
    commands.passes.push_back(std::move(before));
    R::Pass3D scene = Scene();
    Quad(scene, 4, 0, 8, 8, 0.5F, kRed);
    commands.passes.push_back(std::move(scene));
    R::Pass2D after;  // no clear: over the 3D pass's picture
    Quad2D(after, Rect{6, 6, 2, 2}, kGreen);
    commands.passes.push_back(std::move(after));
    R::Pass3D elsewhere = Scene();
    elsewhere.target = 7;  // no picture for target 7
    R::Draw3D draw;
    draw.mesh = whole;
    draw.indexCount = 6;
    draw.color = kWhite;
    elsewhere.draws.push_back(draw);
    commands.passes.push_back(std::move(elsewhere));
    const Picture p = Draw(commands, nullptr, &meshes);
    CHECK_MESSAGE(p.All(0, 0, 2, 2, kYellow), "[g3-mixed-2d-before] the 2D pass before");
    CHECK_MESSAGE(p.All(4, 0, 8, 6, kRed), "[g3-mixed-3d] the 3D pass over it");
    CHECK_MESSAGE(p.All(6, 6, 8, 8, kGreen), "[g3-then-2d-over] the 2D pass after draws over the 3D pass");
    CHECK_MESSAGE((p.All(2, 0, 4, 8, kBlue) && p.All(0, 2, 2, 8, kBlue) && p.All(4, 6, 6, 8, kRed)),
                  "[g3-unmapped-not-drawn] the rest as drawn: the pass on target 7 is not drawn here");
    CHECK_MESSAGE(p.tally.unmappedPasses == 1, "[g3-unmapped] it is counted");
    CHECK_MESSAGE(p.tally.skippedDraws == 0, "[g3-mixed-none-skipped] nothing skipped");

    // A 3D pass first, then a 2D pass without a clear: the 2D pass keeps the 3D pass's picture.
    meshes.ClearPending();
    R::RenderCommands next;
    R::Pass3D first = Scene();
    Quad(first, 0, 0, 4, 8, 0.5F, kRed);
    next.passes.push_back(std::move(first));
    R::Pass2D over;
    Quad2D(over, Rect{6, 0, 2, 2}, kGreen);
    next.passes.push_back(std::move(over));
    const Picture q = Draw(next, nullptr, &meshes);
    CHECK_MESSAGE(q.All(0, 0, 4, 8, kRed), "[g3-then-2d-keeps] a 2D pass after the first 3D pass does not clear it");
    CHECK_MESSAGE(q.All(6, 0, 8, 2, kGreen), "[g3-then-2d-drawn] and draws");
    CHECK_MESSAGE(q.All(4, 2, 8, 8, kNone), "[g3-then-2d-rest] the rest is the first pass's transparent black");
    meshes.ClearPending();
    meshes.Release(whole);
    static_cast<void>(Draw(R::RenderCommands{}, nullptr, &meshes));
}

namespace {

int meshFrames = 0;
R::MeshId frameMesh{};

K::SystemResult ProduceMeshFrame(K::ResMut<R::RenderCommands> commands, K::ResMut<R::RenderMeshes> meshes) {
    R::Pass3D pass = Scene();
    if (meshFrames == 0) frameMesh = meshes.Get().Create(QuadVertices(0, 0, 8, 8, 0.5F), {0, 1, 2, 0, 2, 3});
    if (meshFrames < 4) {
        R::Draw3D draw;
        draw.mesh = frameMesh;
        draw.indexCount = 6;
        draw.color = kRed;
        pass.draws.push_back(draw);
    }
    commands.Get().passes.push_back(std::move(pass));
    ++meshFrames;
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

TEST_CASE("sdl_gpu pass3d: GPUSubmit draws 3D frames on the window; a new World lets the old World's meshes go") {
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
    d.name = "draws3d";
    GYO::Engine::Window::WindowSettings settings;
    settings.title = "sdl_gpu 3D test";
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
        o.name = "TestProduceMeshFrame";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::update);
        o.mounts.push_back(std::move(m));
        setup.Registrar().AddSystem<&ProduceMeshFrame>(std::move(o));
    };
    d.worlds.push_back(std::move(w));
    meshFrames = 0;
    auto built = O::Product::Build(std::move(d), std::move(units));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    O::Product& product = **built;
    const G::SDLGPUStatus* status = product.Buffer().Find<G::SDLGPUStatus>();
    REQUIRE(status != nullptr);
    constexpr int kFrames = 4;
    for (int i = 0; i < kFrames; ++i) {
        auto ran = product.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message));
    }
    CHECK_MESSAGE(status->FramesFailed() == 0, "[g3-frames-none-failed] no frame failed: " << status->LastFailure());
    CHECK_MESSAGE(status->FramesShown() + status->FramesSkipped() == kFrames, "[g3-frames-each] each frame is shown or skipped");
    CHECK_MESSAGE(status->RefusedMeshAsks() == 0, "[g3-frames-asks] the mesh ask was applied");
    CHECK_MESSAGE(status->SkippedDraws() == 0, "[g3-frames-draws] no draw skipped");
    CHECK_MESSAGE(status->MeshCount() == 1, "[g3-frames-mesh] the mesh asked for in the first frame has GPU buffers");

    // A replacement that fails before the swap: the old World goes on drawing with its meshes.
    O::ReplaceRequest failing;
    failing.extra = [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "refused by the test", {}, "test"));
    };
    REQUIRE_FALSE(static_cast<bool>(product.ReplaceWorld("Main", std::move(failing))));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->MeshCount() == 1, "[g3-world-failed-keeps] a failed replacement forgets no mesh");
    // A replacement that succeeds: the new World's ids start again at 1, so the old World's meshes go.
    REQUIRE(static_cast<bool>(product.ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(static_cast<bool>(product.RunFrame()));
    CHECK_MESSAGE(status->MeshCount() == 0, "[g3-world-new-forgets] the new World's first frame lets the old World's meshes go");
    CHECK_MESSAGE(status->FramesFailed() == 0, "[g3-world-frames] no frame failed");
}

} // namespace GYO::Plugins::SDLGPU
