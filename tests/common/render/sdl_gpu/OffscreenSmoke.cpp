// The offscreen SDL_GPU device (v8-engine-render-offscreen, judgments R1-R4 in
// docs/tools/plans/v8/model-viewer.md): it never claims the window, draws each
// frame into its own RGBA8 sRGB texture and reads back the frame just drawn.
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/base/Error.hpp"
#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/RenderQueue.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

namespace {

using namespace Engine::Render;
using Device = Backend::SdlGpu::SdlGpuRenderDevice;
using Rgba = std::array<int, 4>;

template <class Result>
bool ExpectError(const Result& result, RenderErrorCode code, const char* name) {
    if (!result && result.error().code == code) return true;
    std::cerr << name << " did not return the expected error\n";
    return false;
}

template <class Result>
bool Expect(const Result& result, const char* name) {
    if (result) return true;
    std::cerr << name << ": " << Engine::Base::Describe(result.error()) << '\n';
    return false;
}

float SrgbToLinear(int byte) {
    const double c = byte / 255.0;
    return static_cast<float>(c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4));
}

Rgba PixelAt(const TextureReadback& frame, std::uint32_t x, std::uint32_t y) {
    const std::size_t offset = static_cast<std::size_t>(y) * frame.rowPitch + static_cast<std::size_t>(x) * 4U;
    Rgba pixel{};
    for (std::size_t channel = 0; channel < pixel.size(); ++channel)
        pixel[channel] = static_cast<int>(std::to_integer<std::uint8_t>(frame.bytes[offset + channel]));
    return pixel;
}

// Clears one frame to the colour whose sRGB encoding is `expected` (alpha is
// linear) and reads it back.
std::optional<TextureReadback> ClearFrame(Device& device, Rgba expected) {
    auto acquired = device.AcquireFrame();
    if (!Expect(acquired, "offscreen acquire") || !acquired.value()) {
        std::cerr << "offscreen device returned no frame\n";
        return std::nullopt;
    }
    PreparedPass pass;
    pass.colorLoad = AttachmentLoad::Clear;
    pass.clearColor = {SrgbToLinear(expected[0]), SrgbToLinear(expected[1]), SrgbToLinear(expected[2]),
                       static_cast<float>(expected[3]) / 255.0F};
    PreparedFrame frame;
    frame.passes.push_back(pass);
    auto submitted = device.SubmitFrame(*acquired.value(), frame);
    if (!Expect(submitted, "offscreen submit")) return std::nullopt;
    if (submitted.value() != PresentStatus::Presented) {
        std::cerr << "offscreen submit did not report Presented\n";
        return std::nullopt;
    }
    auto read = device.ReadFrame();
    if (!Expect(read, "offscreen readback")) return std::nullopt;
    return std::move(read.value());
}

// R2: every pixel is the cleared colour's sRGB encoding, exactly.
bool CheckCleared(const TextureReadback& frame, std::uint32_t width, std::uint32_t height, Rgba expected,
                  const char* label) {
    if (frame.width != width || frame.height != height || frame.format != TextureFormat::Rgba8SRgb ||
        frame.rowPitch < width * 4U || frame.bytes.size() < static_cast<std::size_t>(frame.rowPitch) * height) {
        std::cerr << label << ": readback is " << frame.width << 'x' << frame.height << " pitch " << frame.rowPitch
                  << ", expected " << width << 'x' << height << " RGBA8 sRGB\n";
        return false;
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Rgba pixel = PixelAt(frame, x, y);
            if (pixel != expected) {
                std::cerr << label << ": pixel " << x << ',' << y << " is " << pixel[0] << ',' << pixel[1] << ','
                          << pixel[2] << ',' << pixel[3] << ", expected " << expected[0] << ',' << expected[1]
                          << ',' << expected[2] << ',' << expected[3] << '\n';
                return false;
            }
        }
    }
    return true;
}

Backend::SdlGpu::SdlGpuOffscreenOptions OffscreenOptions(const std::string& driver, ShaderFormatMask formats,
                                                         std::uint32_t width, std::uint32_t height) {
    Backend::SdlGpu::SdlGpuOffscreenOptions options;
    options.width = width;
    options.height = height;
    options.driver = driver;
    options.availableShaderFormats = formats;
    options.debugMode = true;
    return options;
}

// R1, as the viewer uses it: the window already has the SDL 2D renderer. The
// offscreen device is created, draws, reads back and goes away, and the 2D
// renderer keeps clearing and presenting the window.
bool CheckBesideSdlRenderer(Engine::Platform::Sdl::SdlPlatform& platform, const std::string& driver,
                            ShaderFormatMask formats) {
    auto renderer = Backend::Sdl::SdlRenderer::Create(platform, {false});
    if (!Expect(renderer, "SDL 2D renderer")) return false;
    {
        auto device = Device::CreateOffscreen(platform, OffscreenOptions(driver, formats, 32, 24));
        if (!Expect(device, "offscreen device beside the SDL 2D renderer")) return false;
        const auto frame = ClearFrame(*device.value(), {0, 64, 188, 255});
        if (!frame || !CheckCleared(*frame, 32, 24, {0, 64, 188, 255}, "frame beside the SDL 2D renderer")) return false;
    }
    for (int frame = 0; frame < 3; ++frame) {
        if (!Expect(renderer.value()->Clear({10, 20, 30, 255}), "SDL 2D renderer clear after the offscreen device") ||
            !Expect(renderer.value()->Present(), "SDL 2D renderer present after the offscreen device")) return false;
        static_cast<void>(platform.PumpEvents());
    }
    return true;
}

// R1: while an offscreen device lives, the window is still unclaimed, so a
// windowed device can claim it. SDL refuses a second claim of one window.
bool CheckWindowUnclaimed(Engine::Platform::Sdl::SdlPlatform& platform, Device& offscreen, const std::string& driver,
                          ShaderFormatMask formats) {
    Backend::SdlGpu::SdlGpuOptions options;
    options.driver = driver;
    options.availableShaderFormats = formats;
    options.vsync = false;
    options.debugMode = true;
    auto windowed = Device::Create(platform, options);
    if (!Expect(windowed, "windowed device while an offscreen device lives")) return false;
    return ExpectError(windowed.value()->ReadFrame(), RenderErrorCode::UnsupportedOperation, "windowed ReadFrame") &&
           ExpectError(windowed.value()->SetFrameSize(8, 8), RenderErrorCode::UnsupportedOperation,
                       "windowed SetFrameSize") &&
           ExpectError(offscreen.ReadFrame(), RenderErrorCode::InvalidArgument, "ReadFrame before any frame");
}

// R2 over two frames: each readback is the frame just submitted.
bool CheckConsecutiveFrames(Device& device, std::uint32_t width, std::uint32_t height) {
    constexpr Rgba first{0, 64, 188, 255};
    constexpr Rgba second{255, 188, 64, 128};
    const auto one = ClearFrame(device, first);
    if (!one || !CheckCleared(*one, width, height, first, "first frame")) return false;
    const auto two = ClearFrame(device, second);
    return two && CheckCleared(*two, width, height, second, "second frame");
}

// R4: a new size reaches the next frame and its readback; nothing is read or
// resized during a frame, and an abandoned frame is not read back.
bool CheckResizeAndFrameRules(Device& device) {
    if (!ExpectError(device.SetFrameSize(0, 5), RenderErrorCode::InvalidArgument, "zero frame width") ||
        !Expect(device.SetFrameSize(37, 23), "resize")) return false;
    if (!ExpectError(device.ReadFrame(), RenderErrorCode::InvalidArgument, "ReadFrame of the old size")) return false;
    const auto resized = ClearFrame(device, {255, 0, 0, 255});
    if (!resized || !CheckCleared(*resized, 37, 23, {255, 0, 0, 255}, "resized frame")) return false;

    auto acquired = device.AcquireFrame();
    if (!Expect(acquired, "acquire for the frame rules") || !acquired.value()) return false;
    if (acquired.value()->width != 37 || acquired.value()->height != 23 ||
        acquired.value()->colorFormat != TextureFormat::Rgba8SRgb) {
        std::cerr << "acquired offscreen frame does not describe the frame target\n";
        return false;
    }
    if (!ExpectError(device.ReadFrame(), RenderErrorCode::InvalidArgument, "ReadFrame during a frame") ||
        !ExpectError(device.SetFrameSize(16, 16), RenderErrorCode::InvalidArgument, "resize during a frame")) return false;
    device.AbandonFrame(*acquired.value());
    if (!ExpectError(device.ReadFrame(), RenderErrorCode::InvalidArgument, "ReadFrame after an abandoned frame"))
        return false;

    // A submitted frame that stores nothing into the frame target is not read back.
    const auto stored = ClearFrame(device, {255, 0, 0, 255});
    if (!stored) return false;
    acquired = device.AcquireFrame();
    if (!Expect(acquired, "acquire for an empty frame") || !acquired.value() ||
        !Expect(device.SubmitFrame(*acquired.value(), {}), "submit an empty frame")) return false;
    return ExpectError(device.ReadFrame(), RenderErrorCode::InvalidArgument, "ReadFrame after a frame that stored nothing");
}

enum class Nearer { Red, Green, Neither };

Nearer Classify(Rgba pixel) {
    if (pixel[0] > 100 && pixel[1] < pixel[0] / 2 && pixel[2] < pixel[0] / 2) return Nearer::Red;
    if (pixel[1] > 100 && pixel[0] < pixel[1] / 2 && pixel[2] < pixel[1] / 2) return Nearer::Green;
    return Nearer::Neither;
}

// R3, through Renderer as the viewer draws: a red quad facing the camera and a
// green quad turned 45 degrees about the vertical cross each other, so each is
// nearer on one side. Seen from behind, the nearer quad at each world side
// swaps. Without a depth test the quad drawn last covers both sample points.
bool CheckDepth(Device& device, Renderer& renderer) {
    if (!Expect(device.SetFrameSize(96, 64), "resize for the depth check")) return false;
    const MeshData quad = MakeUnitQuadXY();
    const auto mesh = device.CreateMesh(quad.View());
    if (!Expect(mesh, "quad mesh")) return false;
    const auto sides = [&](PerspectiveCamera3D camera) -> std::optional<std::array<Nearer, 2>> {
        RenderQueue queue;
        queue.SetCamera(camera);
        MeshSubmission red;
        red.mesh = mesh.value();
        red.transform.scale = {2, 2, 1};
        red.material.tint = {1, 0, 0, 1};
        red.doubleSided = true;
        MeshSubmission green = red;
        green.transform.rotationRadians = {0, std::numbers::pi_v<float> / 4.0F, 0};
        green.material.tint = {0, 1, 0, 1};
        if (!queue.Submit(red) || !queue.Submit(green)) return std::nullopt;
        const auto rendered = renderer.Render(queue);
        if (!Expect(rendered, "depth frame")) return std::nullopt;
        const auto frame = device.ReadFrame();
        if (!Expect(frame, "depth readback")) return std::nullopt;
        const std::uint32_t y = frame.value().height / 2U;
        const std::uint32_t offset = frame.value().width / 8U;
        return std::array<Nearer, 2>{Classify(PixelAt(frame.value(), frame.value().width / 2U - offset, y)),
                                     Classify(PixelAt(frame.value(), frame.value().width / 2U + offset, y))};
    };
    const auto front = sides({{0, 0, -2}});
    const auto back = sides({{0, 0, 2}, {0, std::numbers::pi_v<float>, 0}});
    if (!front || !back) return false;
    // Engine's view space is left-handed with +Z forward, and the turn maps +x
    // to (cos, 0, -sin): the green quad is nearer on world +x. From the front
    // that is screen right; the camera turned about the vertical sees world +x
    // on its left, where the red quad is now nearer. So both views read red,
    // green from left to right, and only the nearer quad gives that. A reversed
    // depth order reads green, red; no depth test reads one colour.
    constexpr std::array<Nearer, 2> expected{Nearer::Red, Nearer::Green};
    if (*front != expected || *back != expected) {
        std::cerr << "crossing quads were not depth tested: front " << static_cast<int>((*front)[0]) << ','
                  << static_cast<int>((*front)[1]) << " back " << static_cast<int>((*back)[0]) << ','
                  << static_cast<int>((*back)[1]) << '\n';
        return false;
    }
    return Expect(device.ReleaseMesh(mesh.value()), "release quad mesh");
}

} // namespace

int main(int argc, char** argv) {
    std::string requestedDriver = "auto";
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) != "--gpu-driver" || index + 1 >= argc) {
            std::cerr << "usage: gyo_sdl_gpu_offscreen_smoke [--gpu-driver auto|d3d12|vulkan|metal]\n";
            return 2;
        }
        requestedDriver = argv[++index];
    }
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = "GYO offscreen SDL_GPU smoke";
    options.width = 160;
    options.height = 120;
    options.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(options);
    if (!Expect(platform, "platform")) return 1;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = GYO_TEST_SHADER_BUNDLE;
    ShaderLibrary library;
    auto bundle = library.AppendBundle(source, Engine::Asset::Resolver::AssetPathResolver(resolverOptions));
    if (!Expect(bundle, "shader bundle")) return 1;
    const ShaderFormatMask formats = library.CompleteFormats();

    if (!CheckBesideSdlRenderer(*platform.value(), requestedDriver, formats)) return 1;

    if (!ExpectError(Device::CreateOffscreen(*platform.value(), OffscreenOptions(requestedDriver, formats, 0, 8)),
                     RenderErrorCode::InvalidArgument, "zero offscreen size")) return 1;
    // An odd width exercises padded readback rows.
    auto device = Device::CreateOffscreen(*platform.value(), OffscreenOptions(requestedDriver, formats, 41, 29));
    if (!Expect(device, "offscreen device")) return 1;
    std::cout << "offscreen SDL_GPU requested=" << requestedDriver << " actual=" << device.value()->GetInfo().driver
              << '\n';
    if (!CheckWindowUnclaimed(*platform.value(), *device.value(), requestedDriver, formats) ||
        !CheckConsecutiveFrames(*device.value(), 41, 29) || !CheckResizeAndFrameRules(*device.value())) return 1;

    Renderer renderer;
    if (!Expect(renderer.Initialize(*device.value(), library), "renderer on the offscreen device")) return 1;
    if (!CheckDepth(*device.value(), renderer)) return 1;
    renderer.Reset();
    std::cout << "offscreen SDL_GPU window, clear, consecutive frames, resize, frame rules and depth passed\n";
    return 0;
}
