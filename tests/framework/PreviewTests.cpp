// The offscreen 3D preview (docs/framework/preview.md, Design Invariants 1-4):
// PV1 to PV4 need a GPU (the offscreen device beside the window's SDL
// renderer), PV5 and PV6 run on SDL's dummy video driver and software
// renderer (tests/framework/CMakeLists.txt).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/Preview.hpp"
#include "gyo/framework/PreviewCamera.hpp"
#include "gyo/framework/PreviewTexture.hpp"

#include "engine/base/Error.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

using namespace Gyo::Tools::Framework;
namespace Render = Engine::Render;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Render::Backend::Sdl::SdlRenderer;

namespace {

std::unique_ptr<SdlPlatform> Platform(const char* title) {
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = title;
    options.width = 128;
    options.height = 96;
    options.resizable = false;
    auto platform = SdlPlatform::Create(options);
    REQUIRE_MESSAGE(platform, Engine::Base::Describe(platform.error()));
    return std::move(platform.value());
}

std::unique_ptr<SdlRenderer> WindowRenderer(SdlPlatform& platform) {
    auto renderer = SdlRenderer::Create(platform, {false});
    REQUIRE_MESSAGE(renderer, Engine::Base::Describe(renderer.error()));
    return std::move(renderer.value());
}

PreviewOptions Options(std::uint32_t width, std::uint32_t height) {
    PreviewOptions options;
    options.shaderBundle = GYO_TEST_SHADER_BUNDLE;
    options.width = width;
    options.height = height;
    return options;
}

std::unique_ptr<OffscreenPreview> Preview(SdlPlatform& platform, std::uint32_t width, std::uint32_t height) {
    auto preview = OffscreenPreview::Create(platform, Options(width, height));
    REQUIRE_MESSAGE(preview, Engine::Base::Describe(preview.error()));
    return std::move(preview.value());
}

std::array<int, 4> Pixel(const Render::TextureReadback& frame, std::uint32_t x, std::uint32_t y) {
    const std::size_t offset = static_cast<std::size_t>(y) * frame.rowPitch + static_cast<std::size_t>(x) * 4U;
    return {std::to_integer<int>(frame.bytes[offset]), std::to_integer<int>(frame.bytes[offset + 1]),
            std::to_integer<int>(frame.bytes[offset + 2]), std::to_integer<int>(frame.bytes[offset + 3])};
}

bool IsRed(std::array<int, 4> pixel) { return pixel[0] > 200 && pixel[1] < 40 && pixel[2] < 40; }

bool MiddleIsRed(const Render::TextureReadback& frame) { return IsRed(Pixel(frame, frame.width / 2U, frame.height / 2U)); }

// A red square of side 2 in the XY plane at z = 0, both sides drawn.
struct Square final {
    Render::MeshHandle mesh{};
    Render::IRenderDevice* device{};
    explicit Square(Render::IRenderDevice& on) : device(&on) {
        const std::vector<Render::Vertex3D> vertices{{{-1, -1, 0}, {0, 1}}, {{1, -1, 0}, {1, 1}}, {{1, 1, 0}, {1, 0}}, {{-1, 1, 0}, {0, 0}}};
        const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};
        auto created = device->CreateMesh({vertices, indices});
        REQUIRE_MESSAGE(created, Engine::Base::Describe(created.error()));
        mesh = created.value();
    }
    ~Square() { static_cast<void>(device->ReleaseMesh(mesh)); }
    void Submit(Render::RenderQueue& queue) const {
        Render::MeshSubmission submission;
        submission.mesh = mesh;
        submission.material.tint = {1.0F, 0.0F, 0.0F, 1.0F};
        submission.doubleSided = true;
        auto submitted = queue.Submit(submission);
        REQUIRE_MESSAGE(submitted, Engine::Base::Describe(submitted.error()));
    }
};

const Render::Color kGrey{0.18F, 0.19F, 0.21F, 1.0F};

Render::PerspectiveCamera3D Facing() {
    OrbitCamera camera;
    camera.pitchRadians = 0.0F;
    camera.distance = 4.0F;
    return camera.Camera();
}

Render::PerspectiveCamera3D TurnedAway() {
    OrbitCamera camera;
    camera.pitchRadians = 0.0F;
    camera.distance = 4.0F;
    camera.yawRadians = std::numbers::pi_v<float>;
    camera.target = {0, 0, -8};
    return camera.Camera();
}

Render::TextureReadback Frame(OffscreenPreview& preview, std::uint32_t width, std::uint32_t height,
                              const Render::PerspectiveCamera3D& camera, const Square* square) {
    auto queue = preview.BeginFrame(width, height, camera, kGrey);
    REQUIRE_MESSAGE(queue, Engine::Base::Describe(queue.error()));
    if (square) square->Submit(*queue.value());
    auto frame = preview.EndFrame();
    REQUIRE_MESSAGE(frame, Engine::Base::Describe(frame.error()));
    return std::move(frame.value());
}

// The frame handed out is the device's last frame, byte for byte.
void RequireDevicesFrame(OffscreenPreview& preview, const Render::TextureReadback& frame) {
    auto device = preview.Device().ReadFrame();
    REQUIRE_MESSAGE(device, Engine::Base::Describe(device.error()));
    CHECK(device.value().width == frame.width);
    CHECK(device.value().height == frame.height);
    CHECK(device.value().rowPitch == frame.rowPitch);
    CHECK_MESSAGE(device.value().bytes == frame.bytes, "the preview handed out another frame than the device's last one");
}

// The window's texture holds the frame: drawn 1:1 by the SDL renderer and
// read back, every pixel's colour matches.
bool TextureHolds(SdlRenderer& renderer, const PreviewTexture& texture, const Render::TextureReadback& frame) {
    SDL_Renderer* native = Engine::Render::Backend::Sdl::NativeRenderer(renderer);
    SDL_Texture* image = Engine::Render::Backend::Sdl::NativeTexture(renderer, texture.handle);
    if (image == nullptr) return false;
    SDL_SetTextureBlendMode(image, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(image, SDL_SCALEMODE_NEAREST);
    if (!renderer.Clear({0, 0, 0, 255})) return false;
    const SDL_FRect area{0, 0, static_cast<float>(frame.width), static_cast<float>(frame.height)};
    if (!SDL_RenderTexture(native, image, nullptr, &area)) return false;
    const SDL_Rect read{0, 0, static_cast<int>(frame.width), static_cast<int>(frame.height)};
    SDL_Surface* surface = SDL_RenderReadPixels(native, &read);
    if (surface == nullptr) return false;
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (rgba == nullptr) return false;
    bool same = true;
    for (std::uint32_t y = 0; y < frame.height && same; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * rgba->pitch;
        for (std::uint32_t x = 0; x < frame.width && same; ++x) {
            const auto expected = Pixel(frame, x, y);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                if (row[x * 4U + channel] != expected[channel]) same = false;
            }
        }
    }
    SDL_DestroySurface(rgba);
    return same;
}

// A frame nobody drew: width x height, rows of rowPitch bytes, pixel (x, y) = (x * 40, y * 60, 7, 255).
Render::TextureReadback MadeFrame(std::uint32_t width, std::uint32_t height, std::uint32_t rowPitch) {
    Render::TextureReadback frame;
    frame.width = width;
    frame.height = height;
    frame.rowPitch = rowPitch;
    frame.format = Render::TextureFormat::Rgba8SRgb;
    frame.bytes.assign(static_cast<std::size_t>(rowPitch) * height, std::byte{0});
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t at = static_cast<std::size_t>(y) * rowPitch + static_cast<std::size_t>(x) * 4U;
            frame.bytes[at] = static_cast<std::byte>(x * 40U);
            frame.bytes[at + 1] = static_cast<std::byte>(y * 60U);
            frame.bytes[at + 2] = std::byte{7};
            frame.bytes[at + 3] = std::byte{255};
        }
    }
    return frame;
}

} // namespace

TEST_CASE("PV1: Create fails as StartFailed with Engine's cause when the bundle is not there, and starts when it is") {
    auto platform = Platform("framework preview PV1");
    auto renderer = WindowRenderer(*platform);
    PreviewOptions missing = Options(32, 32);
    missing.shaderBundle = std::filesystem::path(GYO_TEST_SHADER_BUNDLE) / "no-such-bundle";
    auto failed = OffscreenPreview::Create(*platform, missing);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code == PreviewErrorCode::StartFailed);
    CHECK_MESSAGE(failed.error().message.find("shader bundle") != std::string::npos,
                  "the failure does not name the shader bundle: " << failed.error().message);
    CHECK_MESSAGE(!failed.error().detail.empty(), "Engine's cause is missing from the detail");
    auto started = OffscreenPreview::Create(*platform, Options(32, 32));
    CHECK_MESSAGE(started, Engine::Base::Describe(started.error()));
}

TEST_CASE("PV2: every frame is its own: what was submitted, this frame's readback, the device's last frame") {
    auto platform = Platform("framework preview PV2");
    auto renderer = WindowRenderer(*platform);
    auto preview = Preview(*platform, 61, 45);
    Square square(preview->Device());
    const auto first = Frame(*preview, 61, 45, Facing(), &square);
    CHECK_MESSAGE(MiddleIsRed(first), "the square is not at the middle of the frame");
    RequireDevicesFrame(*preview, first);
    // The same camera, nothing submitted: the queue of the frame before is gone.
    const auto second = Frame(*preview, 61, 45, Facing(), nullptr);
    CHECK_MESSAGE(!MiddleIsRed(second), "a frame without a submission still shows the frame before");
    RequireDevicesFrame(*preview, second);
    CHECK(first.bytes != second.bytes);
}

TEST_CASE("PV3: the frame has the size BeginFrame gives, frame by frame") {
    auto platform = Platform("framework preview PV3");
    auto renderer = WindowRenderer(*platform);
    auto preview = Preview(*platform, 61, 45);
    Square square(preview->Device());
    const auto small = Frame(*preview, 40, 30, Facing(), &square);
    CHECK(small.width == 40);
    CHECK(small.height == 30);
    CHECK(small.rowPitch >= 160);
    CHECK(small.bytes.size() >= static_cast<std::size_t>(small.rowPitch) * 29U + 160U);
    CHECK(MiddleIsRed(small));
    const auto large = Frame(*preview, 61, 45, Facing(), &square);
    CHECK(large.width == 61);
    CHECK(large.height == 45);
    CHECK(MiddleIsRed(large));
    auto refused = preview->BeginFrame(0, 30, Facing(), kGrey);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == PreviewErrorCode::DrawFailed);
}

TEST_CASE("PV4: the frame is seen from the camera BeginFrame gives") {
    auto platform = Platform("framework preview PV4");
    auto renderer = WindowRenderer(*platform);
    auto preview = Preview(*platform, 61, 45);
    Square square(preview->Device());
    CHECK_MESSAGE(MiddleIsRed(Frame(*preview, 61, 45, Facing(), &square)), "facing the square, the middle is not red");
    CHECK_MESSAGE(!MiddleIsRed(Frame(*preview, 61, 45, TurnedAway(), &square)), "turned away, the middle still shows the square");
    CHECK_MESSAGE(MiddleIsRed(Frame(*preview, 61, 45, Facing(), &square)), "facing it again, the middle is not red");
}

TEST_CASE("PV5: UploadFrame makes a texture of the frame's size, keeps it while the size stays, replaces it when it changes, and uploads the bytes") {
    auto platform = Platform("framework preview PV5");
    auto renderer = WindowRenderer(*platform);
    PreviewTexture texture;
    const auto first = MadeFrame(4, 3, 20);
    REQUIRE(UploadFrame(*renderer, texture, first));
    REQUIRE(texture.handle);
    CHECK(texture.width == 4);
    CHECK(texture.height == 3);
    CHECK_MESSAGE(TextureHolds(*renderer, texture, first), "the texture does not hold the first frame");
    const auto firstHandle = texture.handle;
    // The same size: the same texture, the new bytes.
    auto second = MadeFrame(4, 3, 16);
    second.bytes[0] = std::byte{250};
    REQUIRE(UploadFrame(*renderer, texture, second));
    CHECK_MESSAGE(texture.handle == firstHandle, "a frame of the same size made a new texture");
    CHECK_MESSAGE(TextureHolds(*renderer, texture, second), "the texture does not hold the second frame");
    // Another size: the old texture is released, a new one of the new size made.
    const auto third = MadeFrame(2, 2, 8);
    REQUIRE(UploadFrame(*renderer, texture, third));
    CHECK(texture.handle != firstHandle);
    CHECK(texture.width == 2);
    CHECK(texture.height == 2);
    CHECK_MESSAGE(Engine::Render::Backend::Sdl::NativeTexture(*renderer, firstHandle) == nullptr,
                  "the texture of the old size was not released");
    CHECK_MESSAGE(TextureHolds(*renderer, texture, third), "the texture does not hold the third frame");
    // A frame the renderer refuses (bytes too short) is UploadFailed, texture kept.
    auto broken = MadeFrame(2, 2, 8);
    broken.bytes.resize(8);
    auto refused = UploadFrame(*renderer, texture, broken);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == PreviewErrorCode::UploadFailed);
    CHECK(texture.handle);
    // A frame in another format than RGBA8 sRGB is UploadFailed, texture kept.
    auto linear = MadeFrame(2, 2, 8);
    linear.format = Render::TextureFormat::Rgba16Float;
    auto wrongFormat = UploadFrame(*renderer, texture, linear);
    REQUIRE_FALSE_MESSAGE(wrongFormat, "a frame that is not RGBA8 sRGB was uploaded");
    CHECK(wrongFormat.error().code == PreviewErrorCode::UploadFailed);
    CHECK_MESSAGE(TextureHolds(*renderer, texture, third), "a refused frame changed the texture");
    REQUIRE(renderer->ReleaseTexture(texture.handle));
}

TEST_CASE("PV6: OrbitCamera gives Engine's camera: behind the target along its forward, pitch down, clips from the distance") {
    OrbitCamera orbit;
    orbit.target = {1.0F, 2.0F, 3.0F};
    orbit.pitchRadians = 0.0F;
    orbit.yawRadians = 0.0F;
    orbit.distance = 4.0F;
    const auto straight = orbit.Camera();
    CHECK(straight.position.x == doctest::Approx(1.0F));
    CHECK(straight.position.y == doctest::Approx(2.0F));
    CHECK(straight.position.z == doctest::Approx(-1.0F)); // 3 - 4, behind along +Z forward
    CHECK(straight.rotationRadians.x == doctest::Approx(0.0F));
    CHECK(straight.rotationRadians.y == doctest::Approx(0.0F));
    CHECK(straight.verticalFieldOfViewRadians == doctest::Approx(orbit.verticalFieldOfViewRadians));
    CHECK(straight.nearClip == doctest::Approx(0.04F));
    CHECK(straight.farClip == doctest::Approx(40.0F));

    orbit.pitchRadians = 0.5F;
    const auto down = orbit.Camera();
    CHECK_MESSAGE(down.position.y > orbit.target.y, "a positive pitch does not look down (the camera is not above the target)");
    CHECK(down.position.y == doctest::Approx(2.0F + 4.0F * std::sin(0.5F)));
    CHECK(down.position.z == doctest::Approx(3.0F - 4.0F * std::cos(0.5F)));
    CHECK(down.rotationRadians.x == doctest::Approx(0.5F));

    orbit.pitchRadians = 0.0F;
    orbit.yawRadians = std::numbers::pi_v<float> / 2.0F;
    const auto side = orbit.Camera();
    CHECK(side.position.x == doctest::Approx(1.0F - 4.0F).epsilon(0.001));
    CHECK(side.position.z == doctest::Approx(3.0F).epsilon(0.001));
    CHECK(side.rotationRadians.y == doctest::Approx(orbit.yawRadians));

    // Yaw and pitch together: Engine's camera turns Y then X (Z, Y, X Euler angles), so the camera sits behind the
    // target along (sin yaw cos pitch, -sin pitch, cos yaw cos pitch).
    orbit.yawRadians = 0.7F;
    orbit.pitchRadians = 0.4F;
    const auto both = orbit.Camera();
    CHECK_MESSAGE(both.position.x == doctest::Approx(1.0F - 4.0F * std::sin(0.7F) * std::cos(0.4F)).epsilon(0.001),
                  "yaw and pitch are not applied in Engine's order");
    CHECK(both.position.y == doctest::Approx(2.0F + 4.0F * std::sin(0.4F)).epsilon(0.001));
    CHECK(both.position.z == doctest::Approx(3.0F - 4.0F * std::cos(0.7F) * std::cos(0.4F)).epsilon(0.001));
    CHECK(both.rotationRadians.x == doctest::Approx(0.4F));
    CHECK(both.rotationRadians.y == doctest::Approx(0.7F));
    orbit.pitchRadians = 0.0F;

    orbit.distance = 0.01F;
    const auto close = orbit.Camera();
    CHECK(close.nearClip == doctest::Approx(0.001F));
    CHECK(close.farClip == doctest::Approx(1.0F));
    orbit.distance = 200.0F;
    const auto far = orbit.Camera();
    CHECK(far.nearClip == doctest::Approx(2.0F));
    CHECK(far.farClip == doctest::Approx(2000.0F));
}
