#include "platform/sdl/SdlPlatform.hpp"

#include <utility>

namespace Engine::Platform::Sdl {

namespace {

SdlPlatformError MakeSdlError(
    SdlPlatformErrorCode code,
    std::string message) {
    return SdlPlatformError::Make(code, std::move(message), SDL_GetError());
}

} // namespace

Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>
SdlPlatform::Create(const SdlPlatformOptions& options) {
    using Result = Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>;

    if (options.width <= 0 || options.height <= 0) {
        return Result::Err(SdlPlatformError::Make(
            SdlPlatformErrorCode::InvalidWindowSize,
            "SdlPlatform: window dimensions must be positive"));
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return Result::Err(MakeSdlError(
            SdlPlatformErrorCode::InitializationFailed,
            "SdlPlatform: SDL video initialization failed"));
    }

    const SDL_WindowFlags windowFlags = options.resizable ? SDL_WINDOW_RESIZABLE : 0;
    SDL_Window* window = SDL_CreateWindow(
        options.title.c_str(),
        options.width,
        options.height,
        windowFlags);

    if (window == nullptr) {
        auto error = MakeSdlError(
            SdlPlatformErrorCode::WindowCreationFailed,
            "SdlPlatform: SDL window creation failed");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return Result::Err(std::move(error));
    }

    return Result::Ok(std::unique_ptr<SdlPlatform>(new SdlPlatform(window)));
}

SdlPlatform::SdlPlatform(SDL_Window* window) noexcept
    : window_(window) {}

SdlPlatform::~SdlPlatform() {
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

Runtime::RuntimeControl SdlPlatform::PumpEvents(const NativeEventObserver& observer) {
    Runtime::RuntimeControl control = Runtime::RuntimeControl::Continue;
    const SDL_WindowID ownWindowId = SDL_GetWindowID(window_);

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (observer) {
            observer(event);
        }

        if (event.type == SDL_EVENT_QUIT) {
            control = Runtime::RuntimeControl::Stop;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.window.windowID == ownWindowId) {
            control = Runtime::RuntimeControl::Stop;
        }
    }

    return control;
}

SDL_Window* SdlPlatform::NativeWindow() const noexcept {
    return window_;
}

} // namespace Engine::Platform::Sdl
