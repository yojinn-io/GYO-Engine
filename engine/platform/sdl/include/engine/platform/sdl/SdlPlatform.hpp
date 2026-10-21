#pragma once

#include <functional>
#include <memory>
#include <string>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/runtime/RuntimeControl.hpp"
#include "engine/time/MonotonicClock.hpp"

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

class NativeAccess; // defined only by the native layer (SdlPlatformNative.hpp)
class SdlPlatform;

// One event handed to a PumpEvents observer. It refers to the backend's event
// and is valid only during the observer call: it cannot be copied, moved or
// kept. Only the input backend and the native layer read what it carries.
class PlatformEvent final {
public:
    PlatformEvent(const PlatformEvent&) = delete;
    PlatformEvent& operator=(const PlatformEvent&) = delete;
    PlatformEvent(PlatformEvent&&) = delete;
    PlatformEvent& operator=(PlatformEvent&&) = delete;

    // When the event happened, on the Engine time base. Never later than its
    // delivery; see the timestamp contract in docs/architecture/platform-sdl.md.
    [[nodiscard]] Time::TimePoint Timestamp() const noexcept { return timestamp_; }

private:
    PlatformEvent(const void* native, Time::TimePoint timestamp) noexcept
        : native_(native), timestamp_(timestamp) {}

    friend class NativeAccess;
    friend class SdlPlatform;

    const void* native_{};
    Time::TimePoint timestamp_{};
};

class SdlPlatform final {
public:
    using EventObserver = std::function<void(const PlatformEvent&)>;
    using LiveFrameHandler = std::function<void()>;

    [[nodiscard]] static Base::Result<std::unique_ptr<SdlPlatform>, SdlPlatformError>
    Create(const SdlPlatformOptions& options = {});

    ~SdlPlatform();

    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;
    SdlPlatform(SdlPlatform&&) = delete;
    SdlPlatform& operator=(SdlPlatform&&) = delete;

    [[nodiscard]] Runtime::RuntimeControl PumpEvents(
        const EventObserver& observer = {});

    // An OS modal loop can hold PumpEvents for as long as the user drags: a live
    // window resize on macOS, a window move or resize on Windows. SDL then asks
    // for redraws at about 60 Hz (SDL_EVENT_WINDOW_EXPOSED with data1 = 1) and
    // allows drawing from an event watcher. The handler runs for those requests
    // of this window: on the main thread, only while PumpEvents is running, and
    // never re-entrantly. Input that arrives meanwhile stays queued for
    // PumpEvents. Typically it runs RuntimeLoop::RunLiveFrame. On Windows the
    // same redraw requests also come while the window's system menu is open.
    // An exception from the handler is kept until SDL returns and then
    // rethrown by PumpEvents, which stops pumping. The handler must not replace
    // itself. An empty handler turns this off, which is the default.
    void SetLiveFrameHandler(LiveFrameHandler handler);

    // The clipboard's text; empty when there is none or reading failed (the
    // platform does not tell the two apart). Main thread.
    [[nodiscard]] std::string ClipboardText() const;

    // The native window is reachable only through the native layer:
    // NativeWindow() in SdlPlatformNative.hpp (target GYO::PlatformSDLNative).

private:
    struct Impl;
    explicit SdlPlatform(std::unique_ptr<Impl> impl) noexcept;

    friend class NativeAccess;

    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Platform::Sdl
