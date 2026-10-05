#include "engine/input/backend/sdl/SdlInput.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <optional>
#include <utility>

namespace Engine::Input::Backend::Sdl {
namespace {

// Every SDL scancode the engine names. Return and keypad Enter both map to
// Key::Enter, as before the keyboard was completed.
constexpr std::array<std::pair<SDL_Scancode, Key>, 113> ScancodeKeys{{
    {SDL_SCANCODE_W, Key::W},
    {SDL_SCANCODE_A, Key::A},
    {SDL_SCANCODE_S, Key::S},
    {SDL_SCANCODE_D, Key::D},
    {SDL_SCANCODE_LEFT, Key::Left},
    {SDL_SCANCODE_RIGHT, Key::Right},
    {SDL_SCANCODE_UP, Key::Up},
    {SDL_SCANCODE_DOWN, Key::Down},
    {SDL_SCANCODE_R, Key::R},
    {SDL_SCANCODE_RETURN, Key::Enter},
    {SDL_SCANCODE_KP_ENTER, Key::Enter},
    {SDL_SCANCODE_ESCAPE, Key::Escape},
    {SDL_SCANCODE_SPACE, Key::Space},
    {SDL_SCANCODE_H, Key::H},
    {SDL_SCANCODE_F3, Key::F3},
    {SDL_SCANCODE_B, Key::B},
    {SDL_SCANCODE_C, Key::C},
    {SDL_SCANCODE_E, Key::E},
    {SDL_SCANCODE_F, Key::F},
    {SDL_SCANCODE_G, Key::G},
    {SDL_SCANCODE_I, Key::I},
    {SDL_SCANCODE_J, Key::J},
    {SDL_SCANCODE_K, Key::K},
    {SDL_SCANCODE_L, Key::L},
    {SDL_SCANCODE_M, Key::M},
    {SDL_SCANCODE_N, Key::N},
    {SDL_SCANCODE_O, Key::O},
    {SDL_SCANCODE_P, Key::P},
    {SDL_SCANCODE_Q, Key::Q},
    {SDL_SCANCODE_T, Key::T},
    {SDL_SCANCODE_U, Key::U},
    {SDL_SCANCODE_V, Key::V},
    {SDL_SCANCODE_X, Key::X},
    {SDL_SCANCODE_Y, Key::Y},
    {SDL_SCANCODE_Z, Key::Z},
    {SDL_SCANCODE_0, Key::Digit0},
    {SDL_SCANCODE_1, Key::Digit1},
    {SDL_SCANCODE_2, Key::Digit2},
    {SDL_SCANCODE_3, Key::Digit3},
    {SDL_SCANCODE_4, Key::Digit4},
    {SDL_SCANCODE_5, Key::Digit5},
    {SDL_SCANCODE_6, Key::Digit6},
    {SDL_SCANCODE_7, Key::Digit7},
    {SDL_SCANCODE_8, Key::Digit8},
    {SDL_SCANCODE_9, Key::Digit9},
    {SDL_SCANCODE_F1, Key::F1},
    {SDL_SCANCODE_F2, Key::F2},
    {SDL_SCANCODE_F4, Key::F4},
    {SDL_SCANCODE_F5, Key::F5},
    {SDL_SCANCODE_F6, Key::F6},
    {SDL_SCANCODE_F7, Key::F7},
    {SDL_SCANCODE_F8, Key::F8},
    {SDL_SCANCODE_F9, Key::F9},
    {SDL_SCANCODE_F10, Key::F10},
    {SDL_SCANCODE_F11, Key::F11},
    {SDL_SCANCODE_F12, Key::F12},
    {SDL_SCANCODE_TAB, Key::Tab},
    {SDL_SCANCODE_BACKSPACE, Key::Backspace},
    {SDL_SCANCODE_INSERT, Key::Insert},
    {SDL_SCANCODE_DELETE, Key::Delete},
    {SDL_SCANCODE_HOME, Key::Home},
    {SDL_SCANCODE_END, Key::End},
    {SDL_SCANCODE_PAGEUP, Key::PageUp},
    {SDL_SCANCODE_PAGEDOWN, Key::PageDown},
    {SDL_SCANCODE_LSHIFT, Key::LeftShift},
    {SDL_SCANCODE_RSHIFT, Key::RightShift},
    {SDL_SCANCODE_LCTRL, Key::LeftCtrl},
    {SDL_SCANCODE_RCTRL, Key::RightCtrl},
    {SDL_SCANCODE_LALT, Key::LeftAlt},
    {SDL_SCANCODE_RALT, Key::RightAlt},
    {SDL_SCANCODE_LGUI, Key::LeftGui},
    {SDL_SCANCODE_RGUI, Key::RightGui},
    {SDL_SCANCODE_CAPSLOCK, Key::CapsLock},
    {SDL_SCANCODE_MINUS, Key::Minus},
    {SDL_SCANCODE_EQUALS, Key::Equals},
    {SDL_SCANCODE_LEFTBRACKET, Key::LeftBracket},
    {SDL_SCANCODE_RIGHTBRACKET, Key::RightBracket},
    {SDL_SCANCODE_BACKSLASH, Key::Backslash},
    {SDL_SCANCODE_SEMICOLON, Key::Semicolon},
    {SDL_SCANCODE_APOSTROPHE, Key::Apostrophe},
    {SDL_SCANCODE_GRAVE, Key::Grave},
    {SDL_SCANCODE_COMMA, Key::Comma},
    {SDL_SCANCODE_PERIOD, Key::Period},
    {SDL_SCANCODE_SLASH, Key::Slash},
    {SDL_SCANCODE_NONUSBACKSLASH, Key::NonUsBackslash},
    {SDL_SCANCODE_NONUSHASH, Key::NonUsHash},
    {SDL_SCANCODE_INTERNATIONAL1, Key::International1},
    {SDL_SCANCODE_INTERNATIONAL2, Key::International2},
    {SDL_SCANCODE_INTERNATIONAL3, Key::International3},
    {SDL_SCANCODE_INTERNATIONAL4, Key::International4},
    {SDL_SCANCODE_INTERNATIONAL5, Key::International5},
    {SDL_SCANCODE_LANG1, Key::Lang1},
    {SDL_SCANCODE_LANG2, Key::Lang2},
    {SDL_SCANCODE_KP_0, Key::Keypad0},
    {SDL_SCANCODE_KP_1, Key::Keypad1},
    {SDL_SCANCODE_KP_2, Key::Keypad2},
    {SDL_SCANCODE_KP_3, Key::Keypad3},
    {SDL_SCANCODE_KP_4, Key::Keypad4},
    {SDL_SCANCODE_KP_5, Key::Keypad5},
    {SDL_SCANCODE_KP_6, Key::Keypad6},
    {SDL_SCANCODE_KP_7, Key::Keypad7},
    {SDL_SCANCODE_KP_8, Key::Keypad8},
    {SDL_SCANCODE_KP_9, Key::Keypad9},
    {SDL_SCANCODE_KP_PERIOD, Key::KeypadPeriod},
    {SDL_SCANCODE_KP_PLUS, Key::KeypadPlus},
    {SDL_SCANCODE_KP_MINUS, Key::KeypadMinus},
    {SDL_SCANCODE_KP_MULTIPLY, Key::KeypadMultiply},
    {SDL_SCANCODE_KP_DIVIDE, Key::KeypadDivide},
    {SDL_SCANCODE_NUMLOCKCLEAR, Key::NumLock},
    {SDL_SCANCODE_PRINTSCREEN, Key::PrintScreen},
    {SDL_SCANCODE_SCROLLLOCK, Key::ScrollLock},
    {SDL_SCANCODE_PAUSE, Key::Pause},
    {SDL_SCANCODE_APPLICATION, Key::Menu},
}};

std::optional<Key> ToKey(const SDL_Scancode scancode) noexcept {
    for (const auto& [code, key] : ScancodeKeys) {
        if (code == scancode) return key;
    }
    return std::nullopt;
}

std::optional<MouseButton> ToMouseButton(const Uint8 button) noexcept {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case SDL_BUTTON_X1:
        return MouseButton::X1;
    case SDL_BUTTON_X2:
        return MouseButton::X2;
    default:
        return std::nullopt;
    }
}

void ResetTransient(ButtonState& state) noexcept {
    state.pressed = false;
    state.released = false;
}

void ApplyTransition(ButtonState& state, const bool down) noexcept {
    if (down == state.held) {
        return;
    }

    state.held = down;
    state.pressed = down;
    state.released = !down;
}

} // namespace

SdlInput::SdlInput(Platform::Sdl::SdlPlatform& platform) noexcept
    : platform_(&platform) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(platform.NativeWindow());
    frame_.windowFocused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    frame_.pointer.relativeMode = SDL_GetWindowRelativeMouseMode(platform.NativeWindow());
}

void SdlInput::BeginFrame() noexcept {
    for (ButtonState& state : frame_.keys) {
        ResetTransient(state);
    }
    for (ButtonState& state : frame_.mouseButtons) {
        ResetTransient(state);
    }
    frame_.pointer.deltaX = 0.0f;
    frame_.pointer.deltaY = 0.0f;
    frame_.events.clear();
}

void SdlInput::HandleEvent(const SDL_Event& event) noexcept {
    const SDL_WindowID ownWindowId = SDL_GetWindowID(platform_->NativeWindow());

    switch (event.type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (event.window.windowID == ownWindowId) {
            frame_.windowFocused = true;
            suppressRelativeMotion_ = true;
            frame_.events.push_back({.kind = InputEventKind::WindowFocusGained});
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (event.window.windowID == ownWindowId) {
            frame_.windowFocused = false;
            frame_.pointer.deltaX = 0.0f;
            frame_.pointer.deltaY = 0.0f;
            ReleaseAll();
            frame_.events.push_back({.kind = InputEventKind::WindowFocusLost});
        }
        break;
    case SDL_EVENT_WINDOW_MOVED:
        if (event.window.windowID == ownWindowId) {
            frame_.events.push_back({.kind = InputEventKind::WindowMoved,
                                     .windowX = event.window.data1,
                                     .windowY = event.window.data2});
        }
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if (event.window.windowID == ownWindowId) {
            frame_.events.push_back({.kind = InputEventKind::WindowResized,
                                     .windowWidth = event.window.data1,
                                     .windowHeight = event.window.data2});
        }
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        if (event.window.windowID == ownWindowId) {
            frame_.events.push_back({.kind = InputEventKind::WindowMinimized});
        }
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        if (event.window.windowID == ownWindowId) {
            frame_.events.push_back({.kind = InputEventKind::WindowRestored});
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (event.key.windowID == ownWindowId && !event.key.repeat) {
            if (const auto key = ToKey(event.key.scancode)) {
                const bool down = event.type == SDL_EVENT_KEY_DOWN;
                SetKey(*key, down);
                frame_.events.push_back({.kind = down ? InputEventKind::KeyPressed : InputEventKind::KeyReleased,
                                         .key = *key});
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.windowID == ownWindowId) {
            frame_.pointer.x = event.button.x;
            frame_.pointer.y = event.button.y;
            if (const auto button = ToMouseButton(event.button.button)) {
                const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                SetMouseButton(*button, down);
                int width{}, height{};
                SDL_GetWindowSize(platform_->NativeWindow(), &width, &height);
                const bool inside = event.button.x >= 0.0f && event.button.y >= 0.0f &&
                    event.button.x < static_cast<float>(width) && event.button.y < static_cast<float>(height);
                frame_.events.push_back({.kind = down ? InputEventKind::MouseButtonPressed
                                                      : InputEventKind::MouseButtonReleased,
                                         .button = *button,
                                         .x = event.button.x,
                                         .y = event.button.y,
                                         .insideWindow = inside});
            }
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (event.motion.windowID == ownWindowId) {
            frame_.pointer.x = event.motion.x;
            frame_.pointer.y = event.motion.y;
            if (!suppressRelativeMotion_ && frame_.windowFocused) {
                frame_.pointer.deltaX += event.motion.xrel;
                frame_.pointer.deltaY += event.motion.yrel;
            }
        }
        break;
    default:
        break;
    }
}

void SdlInput::EndFrame() noexcept {
    if (!frame_.pointer.relativeMode) {
        SDL_GetMouseState(&frame_.pointer.x, &frame_.pointer.y);
    }
    if (!frame_.windowFocused) {
        frame_.pointer.deltaX = 0.0f;
        frame_.pointer.deltaY = 0.0f;
    }
    suppressRelativeMotion_ = false;
}

Base::Result<void, SdlInputError>
SdlInput::SetRelativeMouseMode(const bool enabled) {
    if (frame_.pointer.relativeMode == enabled) {
        return {};
    }

    if (!SDL_SetWindowRelativeMouseMode(platform_->NativeWindow(), enabled)) {
        return Base::Err(SdlInputError::Make(
            SdlInputErrorCode::RelativeMouseModeFailed,
            "SdlInput: unable to change relative mouse mode",
            SDL_GetError()));
    }

    frame_.pointer.relativeMode = enabled;
    frame_.pointer.deltaX = 0.0f;
    frame_.pointer.deltaY = 0.0f;
    suppressRelativeMotion_ = true;
    return {};
}

const PhysicalInputFrame& SdlInput::Snapshot() const noexcept {
    return frame_;
}

void SdlInput::SetKey(const Key key, const bool down) noexcept {
    ApplyTransition(frame_.keys[static_cast<std::size_t>(key)], down);
}

void SdlInput::SetMouseButton(
    const MouseButton button,
    const bool down) noexcept {
    ApplyTransition(frame_.mouseButtons[static_cast<std::size_t>(button)], down);
}

void SdlInput::ReleaseAll() noexcept {
    for (ButtonState& state : frame_.keys) {
        ApplyTransition(state, false);
    }
    for (ButtonState& state : frame_.mouseButtons) {
        ApplyTransition(state, false);
    }
}

} // namespace Engine::Input::Backend::Sdl
