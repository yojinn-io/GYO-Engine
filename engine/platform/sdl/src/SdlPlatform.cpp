#include "engine/platform/sdl/SdlPlatform.hpp"

#include <exception>
#include <utility>

#include "engine/base/Assert.hpp"

namespace Engine::Platform::Sdl {

namespace {

// Sets a flag for a scope and restores its previous value when the scope ends,
// also when an observer throws, so nested scopes keep the outer one's value.
class FlagScope final {
public:
    explicit FlagScope(bool& flag) noexcept : flag_(&flag), previous_(flag) { *flag_ = true; }
    ~FlagScope() { *flag_ = previous_; }

    FlagScope(const FlagScope&) = delete;
    FlagScope& operator=(const FlagScope&) = delete;

private:
    bool* flag_;
    bool previous_;
};

SdlPlatformError MakeSdlError(
    SdlPlatformErrorCode code,
    std::string message) {
    return SdlPlatformError::Make(code, std::move(message), SDL_GetError());
}

} // namespace

Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>
SdlPlatform::Create(const SdlPlatformOptions& options) {

    if (options.width <= 0 || options.height <= 0) {
        return Base::Err(SdlPlatformError::Make(
            SdlPlatformErrorCode::InvalidWindowSize,
            "SdlPlatform: window dimensions must be positive"));
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return Base::Err(MakeSdlError(
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
        return Base::Err(std::move(error));
    }

    return std::unique_ptr<SdlPlatform>(new SdlPlatform(window));
}

SdlPlatform::SdlPlatform(SDL_Window* window) noexcept
    : window_(window) {}

SdlPlatform::~SdlPlatform() {
    SDL_RemoveEventWatch(&SdlPlatform::WatchEvent, this);
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SdlPlatform::SetLiveFrameHandler(LiveFrameHandler handler) {
    // Replacing the handler while it runs would destroy the running function.
    GYO_ASSERT(!inLiveFrame_);
    const bool wasSet = static_cast<bool>(liveFrameHandler_);
    liveFrameHandler_ = std::move(handler);
    const bool isSet = static_cast<bool>(liveFrameHandler_);
    // The watch exists only while a handler is set.
    if (isSet && !wasSet) {
        SDL_AddEventWatch(&SdlPlatform::WatchEvent, this);
    } else if (!isSet && wasSet) {
        SDL_RemoveEventWatch(&SdlPlatform::WatchEvent, this);
    }
}

bool SDLCALL SdlPlatform::WatchEvent(void* userdata, SDL_Event* event) {
    // Watches run on whichever thread pushes an event, so the platform's state
    // is read only after the main-thread check.
    if (event->type != SDL_EVENT_WINDOW_EXPOSED || event->window.data1 != 1 ||
        !SDL_IsMainThread()) {
        return true;
    }
    auto* platform = static_cast<SdlPlatform*>(userdata);
    if (event->window.windowID != SDL_GetWindowID(platform->window_) ||
        !platform->pumping_ || platform->inLiveFrame_ || !platform->liveFrameHandler_ ||
        platform->liveFrameError_) {
        return true;
    }

    const FlagScope live(platform->inLiveFrame_);
    // An exception must not unwind through SDL and the OS modal loop: it is
    // kept and rethrown by PumpEvents once SDL returns.
    try {
        platform->liveFrameHandler_();
    } catch (...) {
        if (!platform->liveFrameError_) platform->liveFrameError_ = std::current_exception();
    }
    return true;
}

Runtime::RuntimeControl SdlPlatform::PumpEvents(const NativeEventObserver& observer) {
    Runtime::RuntimeControl control = Runtime::RuntimeControl::Continue;
    const SDL_WindowID ownWindowId = SDL_GetWindowID(window_);

    // Live frames may run only from inside this function.
    const FlagScope pumping(pumping_);
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
    // The pump finishes its pass first: leaving SDL_PollEvent early would leave
    // SDL's end-of-pass marker queued and cut the next pump short.
    if (liveFrameError_) {
        std::rethrow_exception(std::exchange(liveFrameError_, nullptr));
    }

    return control;
}

SDL_Window* SdlPlatform::NativeWindow() const noexcept {
    return window_;
}

} // namespace Engine::Platform::Sdl
