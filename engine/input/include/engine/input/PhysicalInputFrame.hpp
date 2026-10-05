#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Engine::Input {

// Physical keys by position (scancode), independent of the keyboard layout.
// The first fourteen keys keep their original names and values; the rest of a
// full keyboard follows them, so existing bindings and stored values stay valid.
enum class Key : std::size_t {
    W,
    A,
    S,
    D,
    Left,
    Right,
    Up,
    Down,
    R,
    Enter,  // Return and keypad Enter
    Escape,
    Space,
    H,
    F3,
    // Letters not listed above.
    B, C, E, F, G, I, J, K, L, M, N, O, P, Q, T, U, V, X, Y, Z,
    // The digit row.
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    // Function keys not listed above.
    F1, F2, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // Editing and navigation.
    Tab, Backspace, Insert, Delete, Home, End, PageUp, PageDown,
    // Modifiers (Gui is Command on macOS and the Windows key elsewhere).
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt, LeftGui, RightGui, CapsLock,
    // Punctuation, US layout positions.
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Grave,
    Comma, Period, Slash,
    // ISO and international layout keys (for example JIS Ro, Yen, Henkan,
    // Muhenkan, Katakana/Hiragana; Lang1/Lang2 are the Kana and Eisu keys).
    NonUsBackslash, NonUsHash,
    International1, International2, International3, International4, International5,
    Lang1, Lang2,
    // Keypad (keypad Enter is Enter).
    Keypad0, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
    KeypadPeriod, KeypadPlus, KeypadMinus, KeypadMultiply, KeypadDivide, NumLock,
    // System.
    PrintScreen, ScrollLock, Pause, Menu,
    Count,
};

enum class MouseButton : std::size_t {
    Left,
    Right,
    Middle,
    X1,
    X2,
    Count,
};

// held is the state at the end of the frame. pressed and released describe the
// last transition of the frame, so a press and a release in one frame leave
// pressed false; PhysicalInputFrame::events keeps every transition in order.
struct ButtonState final {
    bool held{};
    bool pressed{};
    bool released{};
};

struct PointerState final {
    float x{};
    float y{};
    float deltaX{};
    float deltaY{};
    bool relativeMode{};
};

enum class InputEventKind : std::uint8_t {
    KeyPressed,
    KeyReleased,
    MouseButtonPressed,
    MouseButtonReleased,
    WindowFocusGained,
    WindowFocusLost,
    WindowMoved,
    WindowResized,
    WindowMinimized,
    WindowRestored,
};

// One input or window event of this window, in arrival order. Only the fields
// named for its kind are meaningful.
struct InputEvent final {
    InputEventKind kind{};
    Key key{Key::Count};                           // Key events
    MouseButton button{MouseButton::Count};        // Mouse button events
    float x{};                                     // Mouse button events: window
    float y{};                                     // coordinates at the click
    bool insideWindow{};                           // Mouse button events
    int windowX{};                                 // WindowMoved: new position
    int windowY{};
    int windowWidth{};                             // WindowResized: new size
    int windowHeight{};
};

struct PhysicalInputFrame final {
    std::array<ButtonState, static_cast<std::size_t>(Key::Count)> keys{};
    std::array<ButtonState, static_cast<std::size_t>(MouseButton::Count)> mouseButtons{};
    PointerState pointer{};
    bool windowFocused{};
    // This frame's events of this window in arrival order. Key repeats are not
    // events; neither are the releases a focus loss applies to held buttons.
    std::vector<InputEvent> events;

    [[nodiscard]] const ButtonState& Get(Key key) const noexcept {
        return keys[static_cast<std::size_t>(key)];
    }

    [[nodiscard]] const ButtonState& Get(MouseButton button) const noexcept {
        return mouseButtons[static_cast<std::size_t>(button)];
    }
};

} // namespace Engine::Input
