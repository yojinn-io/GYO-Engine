#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
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

// Installs a value for a scope and restores the previous one when the scope
// ends, also when an observer throws (nested pumps, Design Invariant 7).
template <class T>
class ValueScope final {
public:
    ValueScope(T& slot, T value) noexcept : slot_(&slot), previous_(std::move(slot)) { *slot_ = std::move(value); }
    ~ValueScope() { *slot_ = std::move(previous_); }

    ValueScope(const ValueScope&) = delete;
    ValueScope& operator=(const ValueScope&) = delete;

private:
    T* slot_;
    T previous_;
};

SdlPlatformError MakeSdlError(
    SdlPlatformErrorCode code,
    std::string message) {
    return SdlPlatformError::Make(code, std::move(message), SDL_GetError());
}

} // namespace

struct SdlPlatform::Impl final {
    static bool SDLCALL WatchEvent(void* userdata, SDL_Event* event);

    SDL_Window* window{};
    // The pair of the innermost running pump (Design Invariant 3). A nested
    // pump keeps its own and restores the outer one when it returns.
    std::optional<ClockPair> pumpPair;
    LiveFrameHandler liveFrameHandler;
    std::exception_ptr liveFrameError;
    bool pumping{};
    bool inLiveFrame{};
};

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

    auto impl = std::make_unique<Impl>();
    impl->window = window;
    return std::unique_ptr<SdlPlatform>(new SdlPlatform(std::move(impl)));
}

SdlPlatform::SdlPlatform(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SdlPlatform::~SdlPlatform() {
    SDL_RemoveEventWatch(&Impl::WatchEvent, impl_.get());
    if (impl_->window != nullptr) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SdlPlatform::SetLiveFrameHandler(LiveFrameHandler handler) {
    // Replacing the handler while it runs would destroy the running function.
    GYO_ASSERT(!impl_->inLiveFrame);
    const bool wasSet = static_cast<bool>(impl_->liveFrameHandler);
    impl_->liveFrameHandler = std::move(handler);
    const bool isSet = static_cast<bool>(impl_->liveFrameHandler);
    // The watch exists only while a handler is set.
    if (isSet && !wasSet) {
        SDL_AddEventWatch(&Impl::WatchEvent, impl_.get());
    } else if (!isSet && wasSet) {
        SDL_RemoveEventWatch(&Impl::WatchEvent, impl_.get());
    }
}

bool SDLCALL SdlPlatform::Impl::WatchEvent(void* userdata, SDL_Event* event) {
    // Watches run on whichever thread pushes an event, so the platform's state
    // is read only after the main-thread check.
    if (event->type != SDL_EVENT_WINDOW_EXPOSED || event->window.data1 != 1 ||
        !SDL_IsMainThread()) {
        return true;
    }
    auto* platform = static_cast<Impl*>(userdata);
    if (event->window.windowID != SDL_GetWindowID(platform->window) ||
        !platform->pumping || platform->inLiveFrame || !platform->liveFrameHandler ||
        platform->liveFrameError) {
        return true;
    }

    const FlagScope live(platform->inLiveFrame);
    // An exception must not unwind through SDL and the OS modal loop: it is
    // kept and rethrown by PumpEvents once SDL returns.
    try {
        platform->liveFrameHandler();
    } catch (...) {
        if (!platform->liveFrameError) platform->liveFrameError = std::current_exception();
    }
    return true;
}

Runtime::RuntimeControl SdlPlatform::PumpEvents(const EventObserver& observer) {
    Runtime::RuntimeControl control = Runtime::RuntimeControl::Continue;
    const SDL_WindowID ownWindowId = SDL_GetWindowID(impl_->window);

    // Live frames may run only from inside this function.
    const FlagScope pumping(impl_->pumping);
    // One clock pair per pump; a nested pump's pair lives only while it runs.
    const ValueScope<std::optional<ClockPair>> pair(impl_->pumpPair, SampleClockPair());
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (observer) {
            const PlatformEvent view(&event, ConvertSdlTimestamp(event.common.timestamp, *impl_->pumpPair, Time::Now()));
            observer(view);
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
    if (impl_->liveFrameError) {
        std::rethrow_exception(std::exchange(impl_->liveFrameError, nullptr));
    }

    return control;
}

SDL_Window* NativeAccess::Window(const SdlPlatform& platform) noexcept {
    return platform.impl_->window;
}

ClockPair MakeClockPair(const Time::TimePoint engineBefore, const std::uint64_t sdlNs,
                        const Time::TimePoint engineAfter) noexcept {
    return {engineBefore + (engineAfter - engineBefore) / 2, sdlNs, engineAfter - engineBefore};
}

ClockPair SampleClockPairWith(const ClockReaders& readers) noexcept {
    // Separate statements: the three reads happen in this order.
    const Time::TimePoint before = readers.engine();
    const std::uint64_t sdl = readers.sdl();
    const Time::TimePoint after = readers.engine();
    return MakeClockPair(before, sdl, after);
}

ClockPair SampleClockPair() noexcept {
    static constexpr ClockReaders real{
        [] () noexcept { return Time::Now(); },
        [] () noexcept -> std::uint64_t { return SDL_GetTicksNS(); },
    };
    return SampleClockPairWith(real);
}

Time::TimePoint ConvertSdlTimestamp(const std::uint64_t sdlNs, const ClockPair& pair,
                                    const Time::TimePoint ceiling) noexcept {
    if (sdlNs == 0) return ceiling;  // added without a time (SDL_PeepEvents)
    // (int64)(sdlNs - pair.sdlNs): the unsigned difference converted modulo
    // 2^64 (C++20), so an event before the pair maps before it, without UB.
    const std::chrono::nanoseconds since{static_cast<std::int64_t>(sdlNs - pair.sdlNs)};
    const Time::TimePoint converted = pair.engine + std::chrono::duration_cast<Time::Duration>(since);
    return converted < ceiling ? converted : ceiling;
}

std::optional<ClockPair> NativeAccess::PumpPair(const SdlPlatform& platform) {
    return platform.impl_->pumpPair;
}

std::optional<ClockPair> CurrentClockPairForTesting(const SdlPlatform& platform) {
    return NativeAccess::PumpPair(platform);
}

} // namespace Engine::Platform::Sdl
