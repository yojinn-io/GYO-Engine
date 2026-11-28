// A Hdr camera's picture and its tonemapping pass on a GPU (render-pipeline declaration section 16, A4; label gpu,
// ctest sdl_gpu.hdr). sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back
// (GPUDevice::Capture): light written on a Rgba16Float render target goes through the built-in tonemapping shader and
// is compared, pixel by pixel, with render's CPU side of the same curves (TonemapToBytes), within one byte. The light
// is written by a test shader of the tests' own (shaders/test_constant.frag.hlsl). Tags "[gh-...]" are the mutants'
// expectations.
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render/Tonemapping.hpp"

#include "GPUDevice.hpp"
#include "TestShaders.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

namespace G = GYO::Plugins::SDLGPU;
namespace M = GYO::Engine::Math;
namespace R = GYO::Engine::Render;
using GYO::Engine::Math::Rect;
using Method = R::Tonemapping::Method;

namespace {

constexpr R::TargetSize kSize{8, 8};

G::GPUDevice& Device() {
    static std::unique_ptr<G::GPUDevice> device = [] {
        auto made = G::GPUDevice::Create(nullptr, true, G::TestShaders());  // SDL's validation on
        REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
        return std::move(*made);
    }();
    return *device;
}

struct Picture {
    G::CapturedImage image;
    G::FrameTally tally;
    [[nodiscard]] R::Color8 At(const std::uint32_t x, const std::uint32_t y) const {
        const std::size_t i = (static_cast<std::size_t>(y) * image.size.width + x) * 4;
        return R::Color8{image.pixels[i], image.pixels[i + 1], image.pixels[i + 2], image.pixels[i + 3]};
    }
    // Every pixel of [x0, x1) x [y0, y1) within one byte of colour in each channel.
    [[nodiscard]] bool Near(const std::uint32_t x0, const std::uint32_t y0, const std::uint32_t x1, const std::uint32_t y1,
                            const R::Color8 colour) const {
        const auto off = [](const std::uint8_t a, const std::uint8_t b) { return std::abs(static_cast<int>(a) - static_cast<int>(b)) > 1; };
        for (std::uint32_t y = y0; y < y1; ++y) {
            for (std::uint32_t x = x0; x < x1; ++x) {
                const R::Color8 c = At(x, y);
                if (off(c.r, colour.r) || off(c.g, colour.g) || off(c.b, colour.b) || off(c.a, colour.a)) {
                    MESSAGE("pixel (" << x << ", " << y << ") is " << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a)
                                      << ", not " << int(colour.r) << "," << int(colour.g) << "," << int(colour.b) << "," << int(colour.a));
                    return false;
                }
            }
        }
        return true;
    }
};

// One case's targets on the shared device, let go at its end (as PhaseTests' PhaseCase).
struct HdrCase {
    R::RenderTextures textures;
    std::vector<R::TextureId> live;

    R::TextureId Target(const R::RenderTargetDesc& desc) { return live.emplace_back(textures.CreateTarget(desc)); }
    // A Hdr camera's picture: linear light, the size of the main target, read at its own pixels.
    R::TextureId HdrPicture() { return Target(R::RenderTargetDesc{kSize, R::RenderTargetFormat::Rgba16Float, R::TextureFilter::Nearest}); }
    Picture Frame(const R::RenderCommands& commands) {
        Picture picture;
        R::RenderFrameReceipt receipt;
        auto captured = Device().Capture(textures.Pending(), {}, commands, kSize, picture.tally, receipt);
        REQUIRE_MESSAGE(static_cast<bool>(captured), (captured ? std::string() : captured.error().message));
        picture.image = std::move(*captured);
        textures.EndFrame(commands, receipt);
        return picture;
    }
    ~HdrCase() {
        for (auto it = live.rbegin(); it != live.rend(); ++it) textures.Release(*it);
        G::FrameTally tally;
        R::RenderFrameReceipt receipt;
        static_cast<void>(Device().Capture(textures.Pending(), {}, R::RenderCommands{}, kSize, tally, receipt));
    }
};

R::Pass2D On(const std::optional<R::TextureId> target, const R::Color8 clear) {
    R::Pass2D pass;
    pass.target = target;
    pass.clear = clear;
    return pass;
}

// Writes the light (r, g, b, a) at every pixel of target (the test shader, its parameters one float4).
R::PassFullscreen Light(const R::TextureId target, const std::array<float, 4> light) {
    R::PassFullscreen pass;
    pass.target = target;
    pass.shader = "test_constant.frag";
    pass.parameters.resize(sizeof(light));
    std::memcpy(pass.parameters.data(), light.data(), sizeof(light));
    return pass;
}

// A 2D rectangle in pixels (y down), opaque.
void Quad2D(R::Pass2D& pass, const Rect r, const R::Color8 colour) {
    const auto base = static_cast<std::uint32_t>(pass.vertices.size());
    pass.vertices.push_back({{r.x, r.y}, {0.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y}, {1.0F, 0.0F}, colour});
    pass.vertices.push_back({{r.x + r.width, r.y + r.height}, {1.0F, 1.0F}, colour});
    pass.vertices.push_back({{r.x, r.y + r.height}, {0.0F, 1.0F}, colour});
    const auto first = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(base + i);
    pass.draws.push_back(R::Draw2D{R::TextureId{}, R::BlendMode::Opaque, std::nullopt, first, 6, 0});
}

// A 3D pass of one unlit rectangle over world x 0..4 (the left half) at depth 0.5, world x 0..8 and y 0..8 to clip.
R::Pass3D Scene(const std::optional<R::TextureId> target, const R::Color8 colour) {
    R::Pass3D pass;
    pass.target = target;
    pass.clipFromWorld = M::Multiply(M::MakeTranslation({-1.0F, -1.0F, 0.0F}), M::MakeScale({0.25F, 0.25F, 1.0F}));
    pass.vertices = {{{0, 0, 0.5F}, {0, 0, -1}, {0, 1}}, {{4, 0, 0.5F}, {0, 0, -1}, {1, 1}}, {{4, 8, 0.5F}, {0, 0, -1}, {1, 0}},
                     {{0, 8, 0.5F}, {0, 0, -1}, {0, 0}}};
    pass.indices = {0, 1, 2, 0, 2, 3};
    pass.instances.push_back(R::Instance3D{{}, colour});
    R::Draw3D draw;
    draw.indexCount = 6;
    draw.instanceCount = 1;
    pass.draws.push_back(draw);
    return pass;
}

struct Curve {
    Method method;
    const char* tag;
};

} // namespace

TEST_CASE("sdl_gpu hdr: light past 1 on a Rgba16Float picture comes out of each curve as the CPU's bytes, within one") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    // 4.0 grey (the acceptance's stimulus), a bright saturated colour, a dark one; all exact in 16-bit floats.
    constexpr std::array<std::array<float, 4>, 3> kLight{{{4.0F, 4.0F, 4.0F, 1.0F}, {4.0F, 1.0F, 0.25F, 1.0F}, {0.5F, 0.25F, 0.125F, 1.0F}}};
    constexpr std::array<Curve, 6> kCurves{{{Method::None, "[gh-curve-none]"},
                                            {Method::Reinhard, "[gh-curve-reinhard]"},
                                            {Method::ReinhardLuminance, "[gh-curve-reinhard-luminance]"},
                                            {Method::AcesFitted, "[gh-curve-aces]"},
                                            {Method::SomewhatBoringDisplayTransform, "[gh-curve-boring]"},
                                            {Method::KhronosPbrNeutral, "[gh-curve-pbr-neutral]"}}};
    for (const Curve& curve : kCurves) {
        for (const std::array<float, 4>& light : kLight) {
            R::RenderCommands commands;
            commands.passes.push_back(Light(picture, light));
            commands.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, curve.method));
            const Picture p = c.Frame(commands);
            const R::Color8 expected = R::TonemapToBytes(curve.method, {light[0], light[1], light[2]}, light[3]);
            CHECK_MESSAGE(p.tally.skippedDraws == 0, curve.tag, " drawn: ", p.tally.lastRefusal);
            CHECK_MESSAGE(p.Near(0, 0, 8, 8, expected), curve.tag, " light ", light[0], ", ", light[1], ", ", light[2]);
        }
    }
}

TEST_CASE("sdl_gpu hdr: the tonemapping pass reads the picture at its own pixels, inside its viewport only") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    R::RenderCommands commands;
    // The picture: blue light 1 (the clear's bytes decoded), red light 1 over x 0..6 (a 2D pass writes its bytes / 255).
    R::Pass2D into = On(picture, R::Color8{0, 0, 255, 255});
    Quad2D(into, Rect{0, 0, 6, 8}, R::Color8{255, 0, 0, 255});
    commands.passes.push_back(std::move(into));
    commands.passes.push_back(On(std::nullopt, R::Color8{0, 255, 0, 255}));
    // Over the right half: pixel x shows the picture's x (stretched over the viewport, x 6 would show the picture's 5).
    commands.passes.push_back(R::TonemappingPass(picture, std::nullopt, R::PixelRect{4, 0, 4, 8}, Method::None));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE(p.Near(0, 0, 4, 8, R::Color8{0, 255, 0, 255}), "[gh-viewport-only] outside its viewport the target is as it was");
    CHECK_MESSAGE((p.Near(4, 0, 6, 8, R::Color8{255, 0, 0, 255}) && p.Near(6, 0, 8, 8, R::Color8{0, 0, 255, 255})),
                  "[gh-pixel-mapped] each pixel shows the picture's pixel under it, not the picture stretched over the viewport");
}

TEST_CASE("sdl_gpu hdr: the unlit 3D pass and a clear on a picture, through no curve, are their pixels on the target") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    // An sRGB-grey clear and a colour of every byte range: as drawn straight on the target (a camera without Hdr) ...
    constexpr R::Color8 kClear{128, 64, 32, 255};
    constexpr R::Color8 kColour{200, 100, 10, 255};
    R::RenderCommands direct;
    direct.passes.push_back(On(std::nullopt, kClear));
    direct.passes.push_back(Scene(std::nullopt, kColour));
    const Picture straight = c.Frame(direct);
    REQUIRE_MESSAGE((straight.Near(0, 0, 4, 8, kColour) && straight.Near(4, 0, 8, 8, kClear)), "[gh-straight] the camera without Hdr");
    // ... and through its picture with no curve.
    R::RenderCommands hdr;
    hdr.passes.push_back(On(picture, kClear));
    hdr.passes.push_back(Scene(picture, kColour));
    hdr.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, Method::None));
    const Picture through = c.Frame(hdr);
    CHECK_MESSAGE(through.tally.skippedDraws == 0, "[gh-unlit-drawn] ", through.tally.lastRefusal);
    CHECK_MESSAGE(through.Near(0, 0, 4, 8, kColour), "[gh-unlit-linear] the unlit colour stands on the picture for its light");
    CHECK_MESSAGE(through.Near(4, 0, 8, 8, kClear), "[gh-clear-decoded] the clear colour stands on the picture for its light");
}

} // namespace GYO::Plugins::SDLGPU
