#pragma once

// The adapter's interface (Design Invariant SDL-1; v8-arch-gyo2-sdl-system-calls-via-adapter): what sdl_window's
// unit, runner and Systems call, in GYO's words. Implemented in src/adapters/sdl/WindowBackend.cpp, the only place
// of sdl_window that includes SDL3/. Every call is on the thread that made the backend (the main thread); the
// adapter asserts it (SDL-4).

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/window/Window.hpp"
#include "plugins/sdl/NativeWindow.hpp"

#include "EventQueue.hpp"

namespace GYO::Plugins::SDLWindow {

// Why the window could not be made (SDL's text in message). A Failure (E2, naming N25): the request was not done,
// nothing is broken; Configure reports it as the startup error SDL-E1.
enum class BackendFailureCode : std::uint8_t {
    VideoFailed = 1,
    WindowFailed,
};
[[nodiscard]] constexpr const char* ToString(const BackendFailureCode code) noexcept {
    switch (code) {
    case BackendFailureCode::VideoFailed: return "VideoFailed";
    case BackendFailureCode::WindowFailed: return "WindowFailed";
    }
    return "Unknown";
}
using BackendFailure = Engine::Base::Error<BackendFailureCode>;

// How the backend starts SDL (the Plugin's options, SDLWindowPlugin.hpp, as they reach the adapter).
struct BackendSettings final {
    Engine::Window::WindowSettings window;
    // SDL's IME hint: the program draws the composition itself (SDL_HINT_IME_IMPLEMENTED_UI "composition").
    bool drawComposition = false;
    // MouseMotionUnit::LogicalPixels: SDL's relative-mode system scale on, and MouseMotion in logical pixels.
    bool logicalMotion = false;
    // Longer clipboard texts are cut (ClipboardChanged::truncated).
    std::size_t clipboardLimit = 1U << 20U;
};

// What the window is now, asked of SDL (a new World's first round; SDL-7 exception (2)).
struct WindowSnapshot final {
    Engine::Math::Vec2i physicalSize{};
    float scaleFactor = 1.0F;
    bool focused = false;
    std::string clipboard;
    bool clipboardTruncated = false;
};

class WindowBackend final {
public:
    // Starts SDL's video (with GYO's hints: SDL installs no signal handlers, os owns them; closing the last window
    // does not make SDL quit) and makes the window. The error is SDL's text.
    [[nodiscard]] static Engine::Base::Result<std::unique_ptr<WindowBackend>, BackendFailure> Create(const BackendSettings& settings);
    ~WindowBackend();
    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;

    // Polls every pending OS event (SDL's whole pass), translated per the mapping table into queue, times converted
    // with one clock pair of this pump.
    void Pump(EventQueue& queue);
    // Waits up to ns for an OS event (nothing is taken from SDL's queue). SDL pumps while it waits, so the live frame
    // can run here too (its WindowResized goes into queue).
    void WaitForEvent(std::int64_t ns, EventQueue& queue);
    // The live frame (SDL declaration R3; live-frame-sim-thread declaration 4.1): while the OS holds the pump in its
    // modal loop (a live resize, a dragged title bar), SDL sends SDL_EVENT_WINDOW_EXPOSED with data1 1 and allows
    // drawing from an event watch. The watch runs frame only when all hold: that event, for this window, on the main
    // thread, while Pump or WaitForEvent is running, not already in frame (no reentry). Before frame the window's
    // size now goes into the queue as a WindowResized (the queued resize events wait for the pump to return: master's
    // pit 3). An exception leaving frame is kept and thrown again when Pump or WaitForEvent returns (it must not
    // cross SDL and the OS's modal loop: pit 4); frame does not run again in that pump. An empty function: no live
    // frame. The watch is added on the first non-empty one and removed with the backend (on the main thread: pit 5).
    // frame runs inside SDL's event-watcher lock: every other thread's SDL_PushEvent waits until it returns, so it
    // must not wait for a thread that pushes SDL events. frame must not call SetLiveFrame (it would destroy itself).
    void SetLiveFrame(std::function<void()> frame);
    // false: a live frame was asked for but SDL did not add the watch (the text in LastError()).
    [[nodiscard]] bool LiveFrameReady() const noexcept;
    // Inside frame (the requests wait for the next frame outside the OS's modal loop).
    [[nodiscard]] bool InLiveFrame() const noexcept;
    [[nodiscard]] WindowSnapshot Snapshot() const;
    // Where the cursor is over the window, in logical pixels; nullopt when it is not over the window.
    [[nodiscard]] std::optional<Engine::Math::Vec2> CursorNow() const;
    [[nodiscard]] SDL::NativeWindow* Native() const noexcept;

    // Requests. false: SDL refused; the text is in LastError(). The caller keeps what was applied (and so tries a
    // refused request again next frame).
    bool SetTextInput(const Engine::Input::TextInputRequest& request);
    bool SetTitle(const std::string& title);
    bool SetSize(Engine::Math::Vec2i logicalSize);
    bool SetCursorGrab(Engine::Window::CursorGrab grab);
    bool SetResizable(bool resizable);
    bool SetCursorIcon(Engine::Window::CursorIcon icon);
    bool SetCursorVisible(bool visible);
    bool WarpCursor(Engine::Math::Vec2 logicalPosition);
    bool SetClipboard(const std::string& text);
    void Close();
    [[nodiscard]] const std::string& LastError() const noexcept { return lastError_; }

    struct State;

private:
    void RethrowLiveException();
    explicit WindowBackend(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    std::string lastError_;
};

} // namespace GYO::Plugins::SDLWindow
