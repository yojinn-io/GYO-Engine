#pragma once

#include <functional>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/runtime/RuntimeControl.hpp"

namespace Engine::Platform::Sdl {

enum class SdlPlatformErrorCode {
    None = 0,
    InvalidWindowSize,
    InitializationFailed,
    WindowCreationFailed,
};

using SdlPlatformError = Base::Error<SdlPlatformErrorCode>;

struct SdlPlatformOptions {
    std::string title{"GYO Runtime"};
    int width{1280};
    int height{720};
    bool resizable{true};
};

class SdlPlatform final {
public:
    using NativeEventObserver = std::function<void(const SDL_Event&)>;

    [[nodiscard]] static Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>
    Create(const SdlPlatformOptions& options = {});

    ~SdlPlatform();

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;
    SdlPlatform(SdlPlatform&&) = delete;
    SdlPlatform& operator=(SdlPlatform&&) = delete;

    [[nodiscard]] Runtime::RuntimeControl PumpEvents(
        const NativeEventObserver& observer = {});

    // This native handle belongs only to the concrete SDL adapter layer. It is
    // not part of GYO's backend-neutral runtime-facing API.
    [[nodiscard]] SDL_Window* NativeWindow() const noexcept;

private:
    explicit SdlPlatform(SDL_Window* window) noexcept;

    SDL_Window* window_{};
};

} // namespace Engine::Platform::Sdl
