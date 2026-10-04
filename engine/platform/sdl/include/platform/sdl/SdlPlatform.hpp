#pragma once

#include <functional>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/runtime/RuntimeControl.hpp"

namespace Engine::Platform::Sdl {

// Zero is not a valid code; the numeric values are not a data contract.
enum class SdlPlatformErrorCode {
    InvalidWindowSize = 1,
    InitializationFailed,
    WindowCreationFailed,
};

[[nodiscard]] constexpr const char* ToString(const SdlPlatformErrorCode code) noexcept {
    switch (code) {
    case SdlPlatformErrorCode::InvalidWindowSize: return "InvalidWindowSize";
    case SdlPlatformErrorCode::InitializationFailed: return "InitializationFailed";
    case SdlPlatformErrorCode::WindowCreationFailed: return "WindowCreationFailed";
    }
    return "Unknown";
}

using SdlPlatformError = Base::Error<SdlPlatformErrorCode>;
static_assert(Base::CodedError<SdlPlatformError>);

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
