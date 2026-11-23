#pragma once

// The input Module's data (docs/architecture/target/v2/input.zh-Hant.md; SDL declaration
// docs/architecture/target/v2/sdl-declaration.zh-Hant.md, sections 3.3 and 6.2): the Message types a backend writes,
// the state the input Module keeps from them (Bevy's ButtonInput and Accumulated* resources) and the text input
// request ui and ImGui write. No SDL and no backend interface: a backend (plugins/sdl_window), a test or a network
// injection writes these Messages from a System before the set kInputSet (InputModule.hpp).
//
// Positions are in logical pixels of the window, origin at its top-left corner, y down (Bevy's CursorMoved).
// Every Message carries the time it happened on the engine time base (Time::TimePoint; SDL-3): a backend converts
// its own clock at its edge.
//
// Hashing: none of these hash their content. They are what the OS sent, different on every machine and every run;
// no two runs have to agree on them (as camera's CameraTargetSizes, CM-5). The kernel's World::Summary still counts
// the Messages of each type (-kernel-summary-messages). A System that decides the game reads the author's per-tick
// input (rollback's Input<T>), never these (IN-5).

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Input {

// A physical key, by its place on the keyboard (Bevy's KeyCode). The values are the USB HID usage ids of the
// keyboard page (0x07), a standard of its own, so a backend that also speaks HID maps by value. Unidentified: a key
// the backend could not name.
enum class KeyCode : std::uint16_t {
    Unidentified = 0,
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit1 = 30, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9, Digit0,
    Enter = 40, Escape, Backspace, Tab, Space, Minus, Equal, BracketLeft, BracketRight, Backslash,
    IntlHash = 50, Semicolon, Quote, Backquote, Comma, Period, Slash, CapsLock,
    F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    PrintScreen = 70, ScrollLock, Pause, Insert, Home, PageUp, Delete, End, PageDown,
    ArrowRight = 79, ArrowLeft, ArrowDown, ArrowUp,
    NumLock = 83, NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadEnter,
    Numpad1 = 89, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9, Numpad0, NumpadDecimal,
    IntlBackslash = 100, ContextMenu,
    F13 = 104, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    ControlLeft = 224, ShiftLeft, AltLeft, SuperLeft, ControlRight, ShiftRight, AltRight, SuperRight,
};

enum class MouseButton : std::uint8_t {
    Left = 1,
    Middle,
    Right,
    Back,
    Forward,
};

enum class ButtonState : std::uint8_t {
    Pressed = 1,
    Released,
};

// The modifier keys held when a key event happened, as the backend reported them (left and right not told apart).
enum class KeyModifier : std::uint8_t {
    None = 0,
    Shift = 1U << 0U,
    Control = 1U << 1U,
    Alt = 1U << 2U,
    Super = 1U << 3U,
    CapsLock = 1U << 4U,
    NumLock = 1U << 5U,
};
struct KeyModifiers final {
    std::uint8_t bits = 0;

    [[nodiscard]] bool Has(KeyModifier m) const noexcept { return (bits & static_cast<std::uint8_t>(m)) != 0; }
    void Add(KeyModifier m) noexcept { bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(m)); }
    friend bool operator==(const KeyModifiers&, const KeyModifiers&) = default;
};

// ---- Messages (written before kInputSet; Bevy's bevy_input and the cursor half of bevy_window).

// A key went down or up (Bevy's KeyboardInput). repeat: the OS's key repeat while it is held (the state ignores it).
// character: the character the key gives on the current layout without modifiers (0: none), for shortcuts that
// follow the layout; text typed goes through TextCommitted, never this.
struct KeyboardInput final {
    KeyCode keyCode = KeyCode::Unidentified;
    ButtonState state = ButtonState::Pressed;
    bool repeat = false;
    char32_t character = 0;
    KeyModifiers modifiers{};
    Time::TimePoint time{};
};

// The keyboard focus went away (Bevy's KeyboardFocusLost, bevy_input keyboard.rs:160): the state releases every key.
struct KeyboardFocusLost final {
    Time::TimePoint time{};
};

// The backend dropped this many events before writing them (its queue was full). What was dropped is unknown, so
// the state releases every key and button, as on a focus loss (IN-3).
struct InputEventsDropped final {
    std::uint64_t count = 0;
    Time::TimePoint time{};
};

struct MouseButtonInput final {
    MouseButton button = MouseButton::Left;
    ButtonState state = ButtonState::Pressed;
    // Where the cursor was, in logical pixels of the window.
    Math::Vec2 position{};
    Time::TimePoint time{};
};

// The cursor moved over the window (Bevy's CursorMoved, bevy_window event.rs:192): its new place, in logical pixels.
// While a button is held the OS keeps the mouse captured by the window: the place can then be outside the window
// (negative, or past its size) and no CursorLeft comes until the button is released. A reader that needs a place
// inside clamps it (Bevy filters such places out of Window::cursor_position, bevy_window window.rs:631-646).
struct CursorMoved final {
    Math::Vec2 position{};
    Time::TimePoint time{};
};

// The mouse moved (Bevy's MouseMotion, bevy_input mouse.rs:111), in the backend's relative units, not scaled to
// logical pixels. While window's WindowRequest::cursorGrab is Locked (Bevy's CursorGrabMode::Locked; SDL's relative
// mode) it is the device's own motion, reported even where the cursor could not move: Object_Connect's push past the
// window's edge (G4) needs Locked and a cursor drawn by the game. Otherwise it is the cursor's motion, zero where the
// cursor is stopped by an edge (SDL takes it from the absolute place, SDL_mouse.c:768).
struct MouseMotion final {
    Math::Vec2 delta{};
    Time::TimePoint time{};
};

// The cursor entered or left the window (Bevy's CursorEntered / CursorLeft, bevy_window event.rs:236, :253;
// Object_Connect G5).
struct CursorEntered final {
    Time::TimePoint time{};
};
struct CursorLeft final {
    Time::TimePoint time{};
};

// The wheel turned, in lines (Bevy's MouseWheel with MouseScrollUnit::Line): y up is positive, x right is positive.
struct MouseWheel final {
    Math::Vec2 delta{};
    Time::TimePoint time{};
};

// The input method's composition changed (Bevy's Ime::Preedit): the text being composed (UTF-8), the start and the
// length of the selected part as the backend gives them (SDL: in the platform's units, UTF-16 code units on Windows
// and macOS, SDL_windowskeyboard.c:924-928, SDL_cocoakeyboard.m:148; -1: not given). An empty text ends the
// composition.
struct TextComposition final {
    std::string text;
    std::int32_t cursor = -1;
    std::int32_t selectionLength = -1;
    Time::TimePoint time{};
};

// Text was typed or the composition was committed (Bevy's Ime::Commit and the text of KeyboardInput), in UTF-8.
struct TextCommitted final {
    std::string text;
    Time::TimePoint time{};
};

// ---- State (written only by the input Module's UpdateInputState, IN-2).

// Pressed buttons and what changed this frame (Bevy's ButtonInput<T>, bevy_input button_input.rs). A press of a
// pressed button (the OS's key repeat among them) and a release of a released one change nothing (IN-3). As in Bevy,
// a repeat of a key that is not held (held through a focus loss and still held on return) presses it.
template <class T>
class ButtonInput final {
public:
    [[nodiscard]] bool Pressed(T button) const { return Contains(pressed_, button); }
    [[nodiscard]] bool JustPressed(T button) const { return Contains(justPressed_, button); }
    [[nodiscard]] bool JustReleased(T button) const { return Contains(justReleased_, button); }
    [[nodiscard]] bool AnyPressed() const noexcept { return !pressed_.empty(); }
    [[nodiscard]] const std::vector<T>& AllPressed() const noexcept { return pressed_; }

    void Press(T button) {
        if (Contains(pressed_, button)) return;
        pressed_.push_back(button);
        justPressed_.push_back(button);
    }
    void Release(T button) {
        const auto found = std::find(pressed_.begin(), pressed_.end(), button);
        if (found == pressed_.end()) return;
        pressed_.erase(found);
        justReleased_.push_back(button);
    }
    void ReleaseAll() {
        for (const T button : pressed_) justReleased_.push_back(button);
        pressed_.clear();
    }
    // The start of a frame: nothing is "just" anything any more.
    void Clear() {
        justPressed_.clear();
        justReleased_.clear();
    }

private:
    static bool Contains(const std::vector<T>& list, T button) {
        return std::find(list.begin(), list.end(), button) != list.end();
    }
    std::vector<T> pressed_;
    std::vector<T> justPressed_;
    std::vector<T> justReleased_;
};

// The sums of this frame's MouseMotion and MouseWheel (Bevy's AccumulatedMouseMotion / AccumulatedMouseScroll,
// bevy_input mouse.rs:264, :285); zero in a frame without any.
struct AccumulatedMouseMotion final {
    Math::Vec2 delta{};
};
struct AccumulatedMouseScroll final {
    Math::Vec2 delta{};
};

// Where the cursor is: the last CursorMoved since it entered, nullopt while it is outside the window (Bevy's
// Window::cursor_position).
struct CursorPosition final {
    std::optional<Math::Vec2> position;
};

// The composition in progress (the last TextComposition, cleared by a TextCommitted or an empty composition).
struct TextInput final {
    std::string composition;
    std::int32_t cursor = -1;
    std::int32_t selectionLength = -1;
};

// What ui or ImGui asks of the input method, read by the backend each frame and applied a frame later (Bevy's
// Window::ime_enabled / ime_position, bevy_window window.rs:279-285). A plain Resource: when two writers share a
// World (world_ui and ImGui), the one that runs later in the frame wins; the author orders them (IN-4).
struct TextInputRequest final {
    bool enabled = false;
    // The text field, in logical pixels of the window; the input method puts its candidate window by it.
    Math::Vec2 areaPosition{};
    Math::Vec2 areaSize{};
    // The caret, in logical pixels from the area's left edge.
    std::int32_t cursorOffset = 0;

    friend bool operator==(const TextInputRequest& a, const TextInputRequest& b) noexcept {
        return a.enabled == b.enabled && a.areaPosition.x == b.areaPosition.x && a.areaPosition.y == b.areaPosition.y &&
               a.areaSize.x == b.areaSize.x && a.areaSize.y == b.areaSize.y && a.cursorOffset == b.cursorOffset;
    }
};

// ---- Hashing: nothing (see the top of the file).
inline void HashValue(Kernel::Hasher&, const KeyboardInput&) {}
inline void HashValue(Kernel::Hasher&, const KeyboardFocusLost&) {}
inline void HashValue(Kernel::Hasher&, const InputEventsDropped&) {}
inline void HashValue(Kernel::Hasher&, const MouseButtonInput&) {}
inline void HashValue(Kernel::Hasher&, const CursorMoved&) {}
inline void HashValue(Kernel::Hasher&, const MouseMotion&) {}
inline void HashValue(Kernel::Hasher&, const CursorEntered&) {}
inline void HashValue(Kernel::Hasher&, const CursorLeft&) {}
inline void HashValue(Kernel::Hasher&, const MouseWheel&) {}
inline void HashValue(Kernel::Hasher&, const TextComposition&) {}
inline void HashValue(Kernel::Hasher&, const TextCommitted&) {}
template <class T>
void HashValue(Kernel::Hasher&, const ButtonInput<T>&) {}
inline void HashValue(Kernel::Hasher&, const AccumulatedMouseMotion&) {}
inline void HashValue(Kernel::Hasher&, const AccumulatedMouseScroll&) {}
inline void HashValue(Kernel::Hasher&, const CursorPosition&) {}
inline void HashValue(Kernel::Hasher&, const TextInput&) {}
inline void HashValue(Kernel::Hasher&, const TextInputRequest&) {}

} // namespace GYO::Engine::Input
