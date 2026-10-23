#pragma once

#include <cstdint>
#include <memory>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Render::Backend::Sdl {

// Zero is not a valid code; the numeric values are not a data contract.
enum class SdlRendererErrorCode {
    CreationFailed = 1,
    ClearFailed,
    PresentFailed,
};

[[nodiscard]] constexpr const char* ToString(const SdlRendererErrorCode code) noexcept {
    switch (code) {
    case SdlRendererErrorCode::CreationFailed: return "CreationFailed";
    case SdlRendererErrorCode::ClearFailed: return "ClearFailed";
    case SdlRendererErrorCode::PresentFailed: return "PresentFailed";
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

    // The native renderer, for adapter integrations such as an editor's UI
    // backend, is reachable only through the native layer: NativeRenderer() in
    // SdlRendererNative.hpp (target GYO::RenderBackendSDLNative).

private:
    struct Impl;
    explicit SdlRenderer(std::unique_ptr<Impl> impl) noexcept;

    friend class RendererNativeAccess;

    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Render::Backend::Sdl
