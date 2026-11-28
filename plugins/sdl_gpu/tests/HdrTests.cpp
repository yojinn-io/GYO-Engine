// A Hdr camera's picture and its tonemapping pass on a GPU (render-pipeline declaration section 16, revision 16.8, A4;
// label gpu, ctest sdl_gpu.hdr). sdl_gpu's adapter draws RenderCommands on an 8 by 8 picture and reads it back
// (GPUDevice::Capture): light written on a Rgba16Float render target goes through the built-in tonemapping shader and
// is compared, pixel by pixel, with the tests' CPU reference of the same curves (TonemappingReference.hpp), within one
// byte; the colour rule of a linear target for 2D, 3D and clears; and cameras composed by render's ComposePasses. The
// light is written by a test shader of the tests' own (shaders/test_constant.frag.hlsl). Tags "[gh-...]" are the
// mutants' expectations.
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
#include "engine/camera/Camera.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"

#include "GPUDevice.hpp"
#include "TestShaders.hpp"
#include "TonemappingReference.hpp"

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
R::Pass3D Scene(const std::optional<R::TextureId> target, const R::Color8 colour, const R::TextureId texture = {}) {
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
    draw.texture = texture;
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
            commands.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, curve.method, R::BlendMode::Opaque));
            const Picture p = c.Frame(commands);
            const R::Color8 expected = GYO::Test::Tonemapping::TonemapToBytes(curve.method, {light[0], light[1], light[2]}, light[3]);
            CHECK_MESSAGE(p.tally.skippedDraws == 0, std::string(curve.tag) << " drawn: " << p.tally.lastRefusal);
            CHECK_MESSAGE(p.Near(0, 0, 8, 8, expected), std::string(curve.tag) << " light " << light[0] << ", " << light[1] << ", " << light[2]);
        }
    }
}

TEST_CASE("sdl_gpu hdr: the tonemapping pass reads the picture at its own pixels, inside its viewport only") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    R::RenderCommands commands;
    // The picture: blue light 1 and red light 1 over x 0..6 (bytes 255 and 0 decode to themselves).
    R::Pass2D into = On(picture, R::Color8{0, 0, 255, 255});
    Quad2D(into, Rect{0, 0, 6, 8}, R::Color8{255, 0, 0, 255});
    commands.passes.push_back(std::move(into));
    commands.passes.push_back(On(std::nullopt, R::Color8{0, 255, 0, 255}));
    // Over the right half: pixel x shows the picture's x (stretched over the viewport, x 6 would show the picture's 5).
    commands.passes.push_back(R::TonemappingPass(picture, std::nullopt, R::PixelRect{4, 0, 4, 8}, Method::None, R::BlendMode::Opaque));
    const Picture p = c.Frame(commands);
    CHECK_MESSAGE(p.Near(0, 0, 4, 8, R::Color8{0, 255, 0, 255}), "[gh-viewport-only] outside its viewport the target is as it was");
    CHECK_MESSAGE((p.Near(4, 0, 6, 8, R::Color8{255, 0, 0, 255}) && p.Near(6, 0, 8, 8, R::Color8{0, 0, 255, 255})),
                  "[gh-pixel-mapped] each pixel shows the picture's pixel under it, not the picture stretched over the viewport");
}

TEST_CASE("sdl_gpu hdr: the unlit 3D pass and a clear on a picture, through no curve, are their pixels on the target") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    // A clear and a colour with bytes above the decoding's linear toe (40: where a power of 2.2 differs most) and one in
    // it (10): as drawn straight on the target (a camera without Hdr) ...
    constexpr R::Color8 kClear{128, 64, 32, 255};
    constexpr R::Color8 kColour{200, 40, 10, 255};
    R::RenderCommands direct;
    direct.passes.push_back(On(std::nullopt, kClear));
    direct.passes.push_back(Scene(std::nullopt, kColour));
    const Picture straight = c.Frame(direct);
    REQUIRE_MESSAGE((straight.Near(0, 0, 4, 8, kColour) && straight.Near(4, 0, 8, 8, kClear)), "[gh-straight] the camera without Hdr");
    // ... and through its picture with no curve.
    R::RenderCommands hdr;
    hdr.passes.push_back(On(picture, kClear));
    hdr.passes.push_back(Scene(picture, kColour));
    hdr.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, Method::None, R::BlendMode::Opaque));
    const Picture through = c.Frame(hdr);
    CHECK_MESSAGE(through.tally.skippedDraws == 0, "[gh-unlit-drawn] " << through.tally.lastRefusal);
    CHECK_MESSAGE(through.Near(0, 0, 4, 8, kColour), "[gh-unlit-linear] the unlit colour stands on the picture for its light");
    CHECK_MESSAGE(through.Near(4, 0, 8, 8, kClear), "[gh-clear-decoded] the clear colour stands on the picture for its light");
}

TEST_CASE("sdl_gpu hdr: on a picture every colour is its light: a 2D pass's, an sRGB texture's once, a bytes texture's decoded") {
    HdrCase c;
    const R::TextureId picture = c.HdrPicture();
    // A 2D pass of a colour above the decoding's toe on the picture, through no curve: its bytes again (decoded on the
    // picture, encoded by the tonemapping).
    constexpr R::Color8 kColour{128, 64, 32, 255};
    R::RenderCommands flat;
    R::Pass2D into = On(picture, R::Color8{0, 0, 0, 255});
    Quad2D(into, Rect{0, 0, 8, 8}, kColour);
    flat.passes.push_back(std::move(into));
    flat.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, Method::None, R::BlendMode::Opaque));
    const Picture p = c.Frame(flat);
    CHECK_MESSAGE(p.Near(0, 0, 8, 8, kColour), "[gh-2d-linear] a 2D pass's colour stands on the picture for its light");
    // The same bytes in an sRGB texture (sampled as linear light) and in a bytes texture, each times white in a 3D pass.
    const std::vector<std::uint8_t> pixel{188, 188, 188, 255};
    const R::TextureId srgb = c.textures.Create(R::TextureDesc{1, 1, R::TextureFilter::Nearest, R::TextureFormat::Rgba8UnormSrgb}, pixel);
    const R::TextureId bytes = c.textures.Create(R::TextureDesc{1, 1, R::TextureFilter::Nearest, R::TextureFormat::Rgba8Unorm}, pixel);
    c.live.push_back(srgb);
    c.live.push_back(bytes);
    for (const auto& [texture, tag] : {std::pair{srgb, "[gh-srgb-texture-once] an sRGB texture's texel is not decoded twice"},
                                       std::pair{bytes, "[gh-bytes-texture-decoded] a bytes texture's texel is decoded"}}) {
        R::RenderCommands solid;
        solid.passes.push_back(On(picture, R::Color8{0, 0, 0, 255}));
        solid.passes.push_back(Scene(picture, R::Color8{255, 255, 255, 255}, texture));
        solid.passes.push_back(R::TonemappingPass(picture, std::nullopt, std::nullopt, Method::None, R::BlendMode::Opaque));
        const Picture q = c.Frame(solid);
        CHECK_MESSAGE(q.tally.skippedDraws == 0, std::string(tag) << " drawn: " << q.tally.lastRefusal);
        CHECK_MESSAGE(q.Near(0, 0, 4, 8, R::Color8{188, 188, 188, 255}), std::string(tag));
    }
}

namespace {

// A camera of the composing cases on the 8 by 8 main target: its viewport, clear, Hdr picture, order.
R::ExtractedCamera ComposedCamera(const std::int32_t order, const GYO::Engine::Camera::Viewport viewport, const std::optional<R::Color8> clear,
                                  const std::optional<R::TextureId> hdr, const bool is3d = true) {
    R::ExtractedCamera camera;
    camera.entity = static_cast<std::uint64_t>(order + 1);
    camera.order = order;
    camera.computed.valid = true;
    camera.computed.viewport = viewport;
    camera.clear = clear;
    camera.hdr = hdr;
    camera.is3d = is3d;
    camera.is2d = !is3d;
    camera.tonemapping = Method::None;
    return camera;
}

GYO::Engine::Camera::Viewport Viewport(const int x, const int width) { return GYO::Engine::Camera::Viewport{{x, 0}, {width, 8}}; }

constexpr R::Color8 kRed{255, 0, 0, 255};
constexpr R::Color8 kGreen{0, 255, 0, 255};
constexpr R::Color8 kBlue{0, 0, 255, 255};
constexpr R::Color8 kWhite{255, 255, 255, 255};

} // namespace

TEST_CASE("sdl_gpu hdr: composed, a Hdr camera keeps what other cameras drew where it draws nothing") {
    HdrCase c;
    // A Hdr world camera (blue, red on the left half), then a 2D UI camera without Hdr drawing white on the top two rows.
    {
        const R::TextureId picture = c.HdrPicture();
        R::ExtractedCameras cameras;
        cameras.cameras = {ComposedCamera(0, Viewport(0, 8), kBlue, picture), ComposedCamera(1, Viewport(0, 8), std::nullopt, std::nullopt, false)};
        R::CameraPasses passes;
        passes.byCamera.resize(2);
        passes.byCamera[0][R::CameraPhase::Opaque].push_back(Scene(R::PhaseTarget(cameras.cameras[0], R::CameraPhase::Opaque), kRed));
        R::Pass2D ui;
        ui.target = R::PhaseTarget(cameras.cameras[1], R::CameraPhase::Overlay);
        Quad2D(ui, Rect{0, 0, 8, 2}, kWhite);
        passes.byCamera[1][R::CameraPhase::Overlay].push_back(std::move(ui));
        R::RenderCommands commands;
        R::ComposePasses(cameras, passes, commands);
        const Picture p = c.Frame(commands);
        CHECK_MESSAGE((p.Near(0, 0, 8, 2, kWhite) && p.Near(0, 2, 4, 8, kRed) && p.Near(4, 2, 8, 8, kBlue)),
                      "[gh-ui-kept] the UI camera after a Hdr camera keeps its pixels, the Hdr camera's around it");
    }
    // Two Hdr cameras side by side, red on the left half, green on the right: neither clears the other's.
    {
        const R::TextureId left = c.HdrPicture();
        const R::TextureId right = c.HdrPicture();
        R::ExtractedCameras cameras;
        cameras.cameras = {ComposedCamera(0, Viewport(0, 4), kRed, left), ComposedCamera(1, Viewport(4, 4), kGreen, right)};
        R::CameraPasses passes;
        passes.byCamera.resize(2);
        R::RenderCommands commands;
        R::ComposePasses(cameras, passes, commands);
        const Picture p = c.Frame(commands);
        CHECK_MESSAGE((p.Near(0, 0, 4, 8, kRed) && p.Near(4, 0, 8, 8, kGreen)), "[gh-split-kept] two Hdr cameras on halves keep each other's half");
    }
    // A background camera (green), then a Hdr camera over the whole target without a colour, red on the left half.
    {
        const R::TextureId picture = c.HdrPicture();
        R::ExtractedCameras cameras;
        cameras.cameras = {ComposedCamera(0, Viewport(0, 8), kGreen, std::nullopt, false), ComposedCamera(1, Viewport(0, 8), std::nullopt, picture)};
        R::CameraPasses passes;
        passes.byCamera.resize(2);
        passes.byCamera[1][R::CameraPhase::Opaque].push_back(Scene(R::PhaseTarget(cameras.cameras[1], R::CameraPhase::Opaque), kRed));
        R::RenderCommands commands;
        R::ComposePasses(cameras, passes, commands);
        const Picture p = c.Frame(commands);
        CHECK_MESSAGE((p.Near(0, 0, 4, 8, kRed) && p.Near(4, 0, 8, 8, kGreen)),
                      "[gh-blend-over] a Hdr camera without a colour draws over the background, which stays where it draws nothing");
    }
}

} // namespace GYO::Plugins::SDLGPU
