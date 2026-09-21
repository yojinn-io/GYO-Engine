#pragma once

#include <cstdint>
#include <memory>

#include <SDL3/SDL_render.h>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "platform/sdl/SdlPlatform.hpp"

namespace Engine::Render::Backend::Sdl {

enum class SdlRendererErrorCode {
    None = 0,
    CreationFailed,
    ClearFailed,
    PresentFailed,
};

using SdlRendererError = Base::Error<SdlRendererErrorCode>;

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

    // Available only on this concrete backend for adapter integrations such as
    // the optional Sandbox. It is not a public runtime resource handle.
    [[nodiscard]] SDL_Renderer* NativeRenderer() const noexcept;

private:
    explicit SdlRenderer(SDL_Renderer* renderer) noexcept;

    SDL_Renderer* renderer_{};
};

} // namespace Engine::Render::Backend::Sdl
