#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <span>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Render::Backend::Sdl {

// Zero is not a valid code; the numeric values are not a data contract.
enum class SdlRendererErrorCode {
    CreationFailed = 1,
    ClearFailed,
    PresentFailed,
    TextureFailed,
    InvalidArgument,
};

[[nodiscard]] constexpr const char* ToString(const SdlRendererErrorCode code) noexcept {
    switch (code) {
    case SdlRendererErrorCode::CreationFailed: return "CreationFailed";
    case SdlRendererErrorCode::ClearFailed: return "ClearFailed";
    case SdlRendererErrorCode::PresentFailed: return "PresentFailed";
    case SdlRendererErrorCode::TextureFailed: return "TextureFailed";
    case SdlRendererErrorCode::InvalidArgument: return "InvalidArgument";
    }
    return "Unknown";
}

using SdlRendererError = Base::Error<SdlRendererErrorCode>;
static_assert(Base::CodedError<SdlRendererError>);

struct SdlRendererOptions {
    bool vsync{true};
};

struct Color {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};
};

// A texture of one SdlRenderer. The default value is no texture; a released
// texture's handle stays invalid even when its slot is reused.
struct SdlTextureHandle final {
    std::uint32_t index{};
    std::uint32_t generation{};
    [[nodiscard]] explicit operator bool() const noexcept { return generation != 0; }
    friend bool operator==(const SdlTextureHandle&, const SdlTextureHandle&) = default;
};

class SdlRenderer final {
public:
    [[nodiscard]] static Base::Result<std::unique_ptr<SdlRenderer>, SdlRendererError>
    Create(
        Platform::Sdl::SdlPlatform& platform,
        const SdlRendererOptions& options = {});

    ~SdlRenderer();

    SdlRenderer(const SdlRenderer&) = delete;
    SdlRenderer& operator=(const SdlRenderer&) = delete;
    SdlRenderer(SdlRenderer&&) = delete;
    SdlRenderer& operator=(SdlRenderer&&) = delete;

    [[nodiscard]] Base::Result<void, SdlRendererError> Clear(Color color);
    [[nodiscard]] Base::Result<void, SdlRendererError> Present();

    // A texture the CPU fills, such as a preview read back from an offscreen
    // GPU device: width x height pixels of RGBA8 in sRGB. Zero sizes and sizes
    // above INT_MAX are InvalidArgument. Textures go with the renderer when it is destroyed.
    [[nodiscard]] Base::Result<SdlTextureHandle, SdlRendererError> CreateStreamingTexture(
        std::uint32_t width, std::uint32_t height);
    // Replaces every pixel: height rows of width * 4 bytes, each row starting
    // rowPitch bytes after the one before (the last row needs only width * 4).
    // InvalidArgument, with nothing read or written, when the handle is not a
    // live texture of this renderer, rowPitch is below width * 4 or above
    // INT_MAX, or bytes is shorter than the rows need.
    [[nodiscard]] Base::Result<void, SdlRendererError> UploadTexture(
        SdlTextureHandle texture, std::span<const std::byte> bytes, std::uint32_t rowPitch);
    // InvalidArgument when the handle is not a live texture of this renderer.
    [[nodiscard]] Base::Result<void, SdlRendererError> ReleaseTexture(SdlTextureHandle texture);

    // The native renderer, for adapter integrations such as an editor's UI
    // backend, is reachable only through the native layer: NativeRenderer() in
    // SdlRendererNative.hpp (target GYO::RenderBackendSDLNative), and so is a
    // texture's (NativeTexture).

private:
    struct Impl;
    explicit SdlRenderer(std::unique_ptr<Impl> impl) noexcept;

    friend class RendererNativeAccess;

    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Render::Backend::Sdl
