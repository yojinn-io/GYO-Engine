// The SDL 2D renderer's streaming textures (v8-engine-sdl-renderer-texture,
// judgment T1 in docs/tools/plans/v8/model-viewer.md): create, upload and
// release; a wrong size or byte count is refused with nothing read or written.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using namespace Engine::Render::Backend::Sdl;

namespace {

struct Window final {
    std::unique_ptr<Engine::Platform::Sdl::SdlPlatform> platform;
    std::unique_ptr<SdlRenderer> renderer;
};

Window Open() {
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = "sdl renderer texture test";
    options.width = 16;
    options.height = 16;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(options);
    REQUIRE(platform);
    auto renderer = SdlRenderer::Create(*platform.value(), {false});
    REQUIRE(renderer);
    return {std::move(platform.value()), std::move(renderer.value())};
}

// Rows of rowPitch bytes; pixel (x, y) is (10x + 1, 10y + 2, 7, 255), padding 0xEE.
std::vector<std::byte> Pixels(std::uint32_t width, std::uint32_t height, std::uint32_t rowPitch) {
    std::vector<std::byte> bytes(static_cast<std::size_t>(rowPitch) * height, std::byte{0xEE});
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::byte* pixel = bytes.data() + static_cast<std::size_t>(y) * rowPitch + x * 4U;
            pixel[0] = static_cast<std::byte>(10 * x + 1);
            pixel[1] = static_cast<std::byte>(10 * y + 2);
            pixel[2] = std::byte{7};
            pixel[3] = std::byte{255};
        }
    }
    return bytes;
}

// Draws the texture at its own size in the top-left corner and reads it back.
void CheckDrawn(SdlRenderer& renderer, SdlTextureHandle texture, std::uint32_t width, std::uint32_t height) {
    SDL_Renderer* native = NativeRenderer(renderer);
    SDL_Texture* image = NativeTexture(renderer, texture);
    REQUIRE(image != nullptr);
    REQUIRE(renderer.Clear({0, 0, 0, 255}));
    const SDL_FRect area{0, 0, static_cast<float>(width), static_cast<float>(height)};
    REQUIRE(SDL_RenderTexture(native, image, nullptr, &area));
    const SDL_Rect read{0, 0, static_cast<int>(width), static_cast<int>(height)};
    SDL_Surface* surface = SDL_RenderReadPixels(native, &read);
    REQUIRE(surface != nullptr);
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    REQUIRE(rgba != nullptr);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto* pixel = static_cast<const std::uint8_t*>(rgba->pixels) + y * rgba->pitch + x * 4U;
            CAPTURE(x);
            CAPTURE(y);
            CHECK(pixel[0] == 10 * x + 1);
            CHECK(pixel[1] == 10 * y + 2);
            CHECK(pixel[2] == 7);
        }
    }
    SDL_DestroySurface(rgba);
}

} // namespace

TEST_CASE("T1: a texture is created, filled from padded rows and drawn as uploaded") {
    Window window = Open();
    auto texture = window.renderer->CreateStreamingTexture(3, 2);
    REQUIRE(texture);
    CHECK(static_cast<bool>(texture.value()));

    const auto tight = Pixels(3, 2, 12);
    REQUIRE(window.renderer->UploadTexture(texture.value(), tight, 12));
    CheckDrawn(*window.renderer, texture.value(), 3, 2);

    // A readback's rows are padded; the last row needs only width * 4 bytes.
    const auto padded = Pixels(3, 2, 256);
    REQUIRE(window.renderer->UploadTexture(texture.value(), std::span(padded).first(256 + 12), 256));
    CheckDrawn(*window.renderer, texture.value(), 3, 2);
    CHECK(window.renderer->ReleaseTexture(texture.value()));
}

TEST_CASE("T1: a wrong size, pitch or byte count is refused and leaves the texture as it was") {
    Window window = Open();
    CHECK(window.renderer->CreateStreamingTexture(0, 4).error().code == SdlRendererErrorCode::InvalidArgument);
    CHECK(window.renderer->CreateStreamingTexture(4, 0).error().code == SdlRendererErrorCode::InvalidArgument);

    auto texture = window.renderer->CreateStreamingTexture(3, 2);
    REQUIRE(texture);
    const auto good = Pixels(3, 2, 12);
    REQUIRE(window.renderer->UploadTexture(texture.value(), good, 12));

    // The buffer is long enough to read from either way: only the span given counts.
    const std::vector<std::byte> other(64, std::byte{0x55});
    const auto shortUpload = window.renderer->UploadTexture(texture.value(), std::span(other).first(23), 12);
    REQUIRE_MESSAGE(!shortUpload, "an upload shorter than the rows need was accepted");
    CHECK(shortUpload.error().code == SdlRendererErrorCode::InvalidArgument);
    const auto narrowPitch = window.renderer->UploadTexture(texture.value(), other, 11);
    REQUIRE_MESSAGE(!narrowPitch, "a row pitch below width * 4 was accepted");
    CHECK(narrowPitch.error().code == SdlRendererErrorCode::InvalidArgument);
    CHECK(window.renderer->UploadTexture({}, good, 12).error().code == SdlRendererErrorCode::InvalidArgument);
    // SDL takes the pitch as an int: a larger one would reach it negative.
    auto row = window.renderer->CreateStreamingTexture(3, 1);
    REQUIRE(row);
    const auto hugePitch = window.renderer->UploadTexture(row.value(), good, 0x80000000U);
    REQUIRE_MESSAGE(!hugePitch, "a row pitch above INT_MAX was accepted");
    CHECK(hugePitch.error().code == SdlRendererErrorCode::InvalidArgument);
    CHECK(window.renderer->CreateStreamingTexture(0x80000000U, 1).error().code ==
          SdlRendererErrorCode::InvalidArgument);
    CheckDrawn(*window.renderer, texture.value(), 3, 2);
}

TEST_CASE("T1: a released texture is no texture, also after its slot is reused") {
    Window window = Open();
    auto first = window.renderer->CreateStreamingTexture(2, 2);
    REQUIRE(first);
    REQUIRE(window.renderer->ReleaseTexture(first.value()));
    CHECK(NativeTexture(*window.renderer, first.value()) == nullptr);
    CHECK(window.renderer->ReleaseTexture(first.value()).error().code == SdlRendererErrorCode::InvalidArgument);

    auto second = window.renderer->CreateStreamingTexture(2, 2);
    REQUIRE(second);
    CHECK(second.value().index == first.value().index);
    CHECK_FALSE(second.value() == first.value());
    const auto pixels = Pixels(2, 2, 8);
    CHECK(window.renderer->UploadTexture(first.value(), pixels, 8).error().code ==
          SdlRendererErrorCode::InvalidArgument);
    CHECK(NativeTexture(*window.renderer, first.value()) == nullptr);
    CHECK(NativeTexture(*window.renderer, second.value()) != nullptr);
    CHECK(NativeTexture(*window.renderer, {}) == nullptr);
}
