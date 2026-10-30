#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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
    BaseDirectoryUnavailable,
    DisplayUnavailable,
    WindowOperationFailed,
};

[[nodiscard]] constexpr const char* ToString(const SdlPlatformErrorCode code) noexcept {
    switch (code) {
    case SdlPlatformErrorCode::InvalidWindowSize: return "InvalidWindowSize";
    case SdlPlatformErrorCode::InitializationFailed: return "InitializationFailed";
    case SdlPlatformErrorCode::WindowCreationFailed: return "WindowCreationFailed";
    case SdlPlatformErrorCode::BaseDirectoryUnavailable: return "BaseDirectoryUnavailable";
    case SdlPlatformErrorCode::DisplayUnavailable: return "DisplayUnavailable";
    case SdlPlatformErrorCode::WindowOperationFailed: return "WindowOperationFailed";
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
    // The program draws an input method's composition itself: the input
    // backend then reports it (TextEvent::Kind::Composition) and the
    // operating system no longer draws it; candidate lists stay native. Set
    // before SDL's video starts and process-wide (SDL's
    // SDL_HINT_IME_IMPLEMENTED_UI); a program that does not draw the
    // composition leaves it off, or typed text stays invisible until
    // committed. v8-engine-input-composition.
    bool imeComposition{false};
};

struct Rect final {
    int x{};
    int y{};
    int width{};
    int height{};
};

// Zeros where the platform does not report borders.
struct WindowBorders final {
    int top{};
    int left{};
    int bottom{};
    int right{};
};

struct WindowFlags final {
    bool occluded{};
    bool hidden{};
    bool minimized{};
    bool inputFocus{};
    bool relativeMouseMode{};
};

struct WindowState final {
    Rect rect;
    WindowBorders borders;
    WindowFlags flags;
};

struct DisplayState final {
    Rect usableBounds;      // zeros where the platform reports no usable area
    float refreshRate{};    // 0 when unknown
};

// Observed events (IP-4 ip4b). Kinds follow InputEventKind where it has them;
// the platform cannot depend on GYO::Input, so the names are repeated on
// purpose. Key events are not observed.
enum class ObservedEventKind : std::uint8_t {
    MouseMotion,
    MouseButtonPressed,
    MouseButtonReleased,
    WindowFocusGained,
    WindowFocusLost,
    WindowMoved,
    WindowResized,
    WindowMinimized,
    WindowRestored,
    WindowOccluded,
    WindowExposed,
    WindowHidden,
};

struct ObservedEvent final {
    ObservedEventKind kind{};
    std::uint32_t sourceId{};   // the device as the platform numbers it; for observation only
    float deltaX{};             // MouseMotion
    float deltaY{};
    bool injected{};            // pushed by PushInjectedEvent on this thread
    // When the event was queued, on the Engine time base, never later than
    // observedAt (the watch samples one clock pair per callback).
    Time::TimePoint timestamp{};
    Time::TimePoint observedAt{};
};

// Runs on whichever thread pushes the event, inside the platform's watcher
// lock. It must not wait for anything that may be held during a platform call.
using EventWatchCallback = void (*)(void* context, const ObservedEvent& event) noexcept;

// An observation of one window's events, from AddEventWatch. Movable. It keeps
// only the window's ID, never the platform, so it may outlive the platform (it
// then receives no more events: SDL never reuses a window ID). Destruction
// waits for a callback running on another thread; it happens on the main
// thread only, never inside a live frame or a watch callback (GYO_ASSERT; the
// platform boundary exception of error-handling.md).
class EventWatch final {
public:
    EventWatch() noexcept;  // inactive
    EventWatch(EventWatch&& other) noexcept;
    EventWatch& operator=(EventWatch&& other);
    ~EventWatch();
    EventWatch(const EventWatch&) = delete;
    EventWatch& operator=(const EventWatch&) = delete;

    [[nodiscard]] bool Active() const noexcept { return static_cast<bool>(node_); }

    struct Node;  // defined by the platform

private:
    explicit EventWatch(std::unique_ptr<Node> node) noexcept;
    void Remove();

    friend class SdlPlatform;

    std::unique_ptr<Node> node_;
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

    // Queries (docs/architecture/platform-sdl.md, Design Invariant 7): main
    // thread; they read SDL's state every time and keep no copy, and they may
    // run inside an observer and a live frame.
    [[nodiscard]] WindowState QueryWindow() const;
    [[nodiscard]] Base::Result<DisplayState, SdlPlatformError> QueryDisplay() const;
    [[nodiscard]] std::string_view VideoDriverName() const;
    // The X11 window number; empty unless the window is an X11 window.
    [[nodiscard]] std::optional<std::uint64_t> X11WindowNumber() const;

    // Operations: main thread; never inside an observer, a live frame or an
    // event watch callback (GYO_ASSERT). Real window operations: the events
    // they cause are queued when the OS reports them (Sync, then pump).
    [[nodiscard]] Base::Result<void, SdlPlatformError> SetWindowSize(int width, int height);
    [[nodiscard]] Base::Result<void, SdlPlatformError> SetWindowPosition(int x, int y);
    // A request; the result shows as WindowFocusGained.
    void Raise();
    // Waits for pending window operations; false when it timed out or the
    // platform cannot tell.
    [[nodiscard]] bool Sync();

    // Observes this window's events (ObservedEventKind) as they are queued.
    // Main thread; allowed inside an observer, never inside a live frame or a
    // watch callback (GYO_ASSERT). Throws std::bad_alloc when out of memory.
    [[nodiscard]] EventWatch AddEventWatch(EventWatchCallback callback, void* context);

    // The native window is reachable only through the native layer:
    // NativeWindow() in SdlPlatformNative.hpp (target GYO::PlatformSDLNative).

private:
    struct Impl;
    explicit SdlPlatform(std::unique_ptr<Impl> impl) noexcept;

    friend class NativeAccess;

    std::unique_ptr<Impl> impl_;
};

// The directory the platform loads the program's files from (SDL's base path:
// the executable's directory, or Contents/Resources inside a macOS bundle),
// read as UTF-8, never in the ANSI code page (SDL gives UTF-8; bytes that were
// not would throw std::system_error on Windows). Needs no initialization. Call
// it on the main thread, or before other threads start: the platform caches it
// without a lock. Text to a path elsewhere: Os::Files::PathFromUtf8.
[[nodiscard]] Base::Result<std::filesystem::path, SdlPlatformError> BaseDirectory();

// The platform's name, for diagnostics only: never branch on it.
[[nodiscard]] std::string_view PlatformName() noexcept;

// Runs a program body for the entry point (EntryPoint.hpp): returns what the
// body returns; an exception is logged (Base log, category "platform") and
// becomes exit code 1, so it never unwinds through the platform's C stack.
[[nodiscard]] int RunEntry(int (*body)(int, char*[]), int argc, char* argv[]) noexcept;

} // namespace Engine::Platform::Sdl
