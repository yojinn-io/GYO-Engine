#pragma once

// The window Module's data (docs/architecture/target/v2/window.zh-Hant.md; SDL declaration
// docs/architecture/target/v2/sdl-declaration.zh-Hant.md, sections 3.3 and 6.2): the settings a backend creates the
// window with, the Message types it writes, the state the window Module keeps from them and the requests a game
// writes for the backend to apply. No SDL and no backend interface. The window is not an entity (GYO has one window,
// a service the backend holds; SDL declaration section 6.6): what Bevy keeps on its Window component as both state
// and request is split here into WindowState (from Messages) and WindowRequest (written by the game).
//
// Sizes: physicalSize is in pixels of the drawable area; logical pixels are physical pixels divided by scaleFactor
// (Bevy's Window::resolution). Positions are logical pixels, origin at the top-left corner, y down.
//
// Hashing: none of these hash their content (the window's size and focus differ between machines; as camera's
// CameraTargetSizes, CM-5). The kernel still counts the Messages of each type in World::Summary.

#include <cstdint>
#include <optional>
#include <string>

#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Window {

// How the window is made (ProductDescription::options; read by the backend in its Configure; Bevy's
// WindowPlugin::primary_window). None given: these defaults.
struct WindowSettings final {
    std::string title = "GYO";
    // The size asked for, in logical pixels.
    Math::Vec2i size{1280, 720};
    // false: the player cannot resize it (Object_Connect G6; Bevy's Window::resizable).
    bool resizable = true;
    // Made hidden (tests on a headless machine).
    bool hidden = false;
    // The first value of WindowRequest::closeWhenRequested in every World (Bevy's WindowPlugin::close_when_requested).
    bool closeWhenRequested = true;
};

// The system cursor's shape (Bevy's SystemCursorIcon, the shapes every desktop OS has).
enum class CursorIcon : std::uint8_t {
    Default = 0,
    Text,
    Pointer,
    Crosshair,
    Move,
    ResizeEw,
    ResizeNs,
    ResizeNwse,
    ResizeNesw,
    NotAllowed,
    Wait,
    Progress,
};

// ---- Messages (written before kWindowSet).

// The drawable area's size or its scale changed (Bevy's WindowResized, bevy_window event.rs:39, and
// WindowScaleFactorChanged in one).
struct WindowResized final {
    Math::Vec2i physicalSize{};
    float scaleFactor = 1.0F;
    Time::TimePoint time{};
};

// The window gained or lost the keyboard focus (Bevy's WindowFocused, event.rs:319). A backend writes input's
// KeyboardFocusLost with a loss as well (input does not depend on window).
struct WindowFocused final {
    bool focused = false;
    Time::TimePoint time{};
};

// Who asked to close.
enum class CloseSource : std::uint8_t {
    // The window's close button (or the OS's close shortcut for the window).
    Window = 1,
    // The OS asked the whole program to quit (macOS Cmd+Q, logging off).
    Application,
    // The process got a shutdown request (Ctrl+C, SIGTERM; os's InstallShutdownRequests).
    Shutdown,
};

// Somebody asked to close the window (Bevy's WindowCloseRequested, event.rs:103). Nothing closes by itself: the
// window Module's UpdateWindowState sets WindowRequest::close when closeWhenRequested is on (Bevy's
// close_when_requested); a game that wants to ask first turns it off and sets close itself.
struct WindowCloseRequested final {
    CloseSource source = CloseSource::Window;
    Time::TimePoint time{};
};

// The system clipboard's text changed (by this program or another). truncated: the text was longer than the
// backend's limit and is cut there (on a UTF-8 boundary).
struct ClipboardChanged final {
    std::string text;
    bool truncated = false;
    Time::TimePoint time{};
};

// Requests: written by the game, read by the backend at the end of the frame (Main/Last), applied a frame later.

// Puts the system cursor at a place of the window, in logical pixels (Bevy's Window::set_cursor_position;
// Object_Connect G3). The backend's writes after the warp report the cursor's new place as usual.
struct WarpCursor final {
    Math::Vec2 position{};
};

// Puts text on the system clipboard (ImGui's copy).
struct CopyToClipboard final {
    std::string text;
};

// ---- State (written only by the window Module's UpdateWindowState, WN-2).

struct WindowState final {
    Math::Vec2i physicalSize{};
    float scaleFactor = 1.0F;
    bool focused = false;
    // A WindowResized has arrived since the World was made (the size is known).
    bool known = false;

    [[nodiscard]] Math::Vec2 LogicalSize() const noexcept {
        return {static_cast<float>(physicalSize.x) / scaleFactor, static_cast<float>(physicalSize.y) / scaleFactor};
    }
};

// The last ClipboardChanged (a mirror, so a reader needs no backend call).
struct Clipboard final {
    std::string text;
    bool truncated = false;
};

// What the game asks of the window, read every frame by the backend (Main/Last) and applied a frame later; the
// backend applies a field only when it differs from what it applied last (Bevy's CachedWindow,
// bevy_winit/src/system.rs:302).
struct WindowRequest final {
    // nullopt: leave it as it is.
    std::optional<std::string> title;
    // The size, in logical pixels.
    std::optional<Math::Vec2i> size;
    std::optional<bool> resizable;
    CursorIcon cursorIcon = CursorIcon::Default;
    bool cursorVisible = true;
    // Close the window: the backend's runner ends its loop after this frame (WN-4).
    bool close = false;
    // UpdateWindowState sets close when a WindowCloseRequested arrives (WindowSettings::closeWhenRequested).
    bool closeWhenRequested = true;
};

// ---- Hashing: nothing (see the top of the file).
inline void HashValue(Kernel::Hasher&, const WindowResized&) {}
inline void HashValue(Kernel::Hasher&, const WindowFocused&) {}
inline void HashValue(Kernel::Hasher&, const WindowCloseRequested&) {}
inline void HashValue(Kernel::Hasher&, const ClipboardChanged&) {}
inline void HashValue(Kernel::Hasher&, const WarpCursor&) {}
inline void HashValue(Kernel::Hasher&, const CopyToClipboard&) {}
inline void HashValue(Kernel::Hasher&, const WindowState&) {}
inline void HashValue(Kernel::Hasher&, const Clipboard&) {}
inline void HashValue(Kernel::Hasher&, const WindowRequest&) {}

} // namespace GYO::Engine::Window
