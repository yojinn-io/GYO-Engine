#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <exception>
#include <optional>
#include <utility>

#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"

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

// Depth of SDL event watch callbacks on this thread (the live frame and event
// watches). SDL marks its watcher list as dispatching with a flag, not a
// depth, so adding or removing a watch, and pushing, inside a callback is
// unsafe (Design Invariant 7).
thread_local int dispatchDepth = 0;

class DispatchScope final {
public:
    DispatchScope() noexcept { ++dispatchDepth; }
    ~DispatchScope() { --dispatchDepth; }
    DispatchScope(const DispatchScope&) = delete;
    DispatchScope& operator=(const DispatchScope&) = delete;
};

// Set on this thread while an injected event is pushed (read by event watches,
// which SDL calls on the pushing thread).
thread_local bool injecting = false;

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

    const DispatchScope dispatch;
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

Base::Result<std::filesystem::path, SdlPlatformError> BaseDirectory() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::BaseDirectoryUnavailable,
                                      "SdlPlatform: the base directory is unavailable"));
    }
    return PathFromUtf8(base);
}

std::filesystem::path PathFromUtf8(const std::string_view utf8) {
    std::u8string text;
    text.reserve(utf8.size());
    for (const char c : utf8) text.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(text);
}

std::string_view PlatformName() noexcept {
    return SDL_GetPlatform();
}

int RunEntry(int (*body)(int, char*[]), const int argc, char* argv[]) noexcept {
    try {
        return body(argc, argv);
    } catch (const std::exception& error) {
        // Two lines, so reporting allocates nothing in this noexcept function.
        Base::Log(Base::LogLevel::Error, "platform", "unhandled exception:");
        Base::Log(Base::LogLevel::Error, "platform", error.what());
    } catch (...) {
        Base::Log(Base::LogLevel::Error, "platform", "unhandled exception");
    }
    return 1;
}

std::string SdlPlatform::ClipboardText() const {
    char* text = SDL_GetClipboardText();
    std::string result = text != nullptr ? text : "";
    SDL_free(text);
    return result;
}

WindowState SdlPlatform::QueryWindow() const {
    SDL_Window* window = impl_->window;
    WindowState state;
    static_cast<void>(SDL_GetWindowPosition(window, &state.rect.x, &state.rect.y));
    static_cast<void>(SDL_GetWindowSize(window, &state.rect.width, &state.rect.height));
    // Platforms without border information report failure: zeros stay.
    if (!SDL_GetWindowBordersSize(window, &state.borders.top, &state.borders.left, &state.borders.bottom,
                                  &state.borders.right)) {
        state.borders = {};
    }
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    state.flags.occluded = (flags & SDL_WINDOW_OCCLUDED) != 0;
    state.flags.hidden = (flags & SDL_WINDOW_HIDDEN) != 0;
    state.flags.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    state.flags.inputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    state.flags.relativeMouseMode = SDL_GetWindowRelativeMouseMode(window);
    return state;
}

Base::Result<DisplayState, SdlPlatformError> SdlPlatform::QueryDisplay() const {
    const SDL_DisplayID display = SDL_GetDisplayForWindow(impl_->window);
    if (display == 0) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::DisplayUnavailable,
                                      "SdlPlatform: the window has no display"));
    }
    DisplayState state;
    SDL_Rect bounds{};
    if (SDL_GetDisplayUsableBounds(display, &bounds)) {
        state.usableBounds = {bounds.x, bounds.y, bounds.w, bounds.h};
    }
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display)) {
        state.refreshRate = mode->refresh_rate;
    }
    return state;
}

std::string_view SdlPlatform::VideoDriverName() const {
    const char* name = SDL_GetCurrentVideoDriver();
    return name != nullptr ? std::string_view(name) : std::string_view();
}

std::optional<std::uint64_t> SdlPlatform::X11WindowNumber() const {
    const Sint64 number = SDL_GetNumberProperty(SDL_GetWindowProperties(impl_->window),
                                                SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (number == 0) return std::nullopt;
    return static_cast<std::uint64_t>(number);
}

Base::Result<void, SdlPlatformError> SdlPlatform::SetWindowSize(const int width, const int height) {
    GYO_ASSERT(width > 0 && height > 0);
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    if (!SDL_SetWindowSize(impl_->window, width, height)) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::WindowOperationFailed,
                                      "SdlPlatform: setting the window size failed"));
    }
    return {};
}

Base::Result<void, SdlPlatformError> SdlPlatform::SetWindowPosition(const int x, const int y) {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    if (!SDL_SetWindowPosition(impl_->window, x, y)) {
        return Base::Err(MakeSdlError(SdlPlatformErrorCode::WindowOperationFailed,
                                      "SdlPlatform: setting the window position failed"));
    }
    return {};
}

void SdlPlatform::Raise() {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    static_cast<void>(SDL_RaiseWindow(impl_->window));
}

bool SdlPlatform::Sync() {
    GYO_ASSERT(!impl_->pumping && dispatchDepth == 0);
    return SDL_SyncWindow(impl_->window);
}

bool PushInjectedEvent(SdlPlatform& platform, SDL_Event& event) {
    static_cast<void>(platform);
    GYO_ASSERT(dispatchDepth == 0);
    const FlagScope injected(injecting);
    return SDL_PushEvent(&event);
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
