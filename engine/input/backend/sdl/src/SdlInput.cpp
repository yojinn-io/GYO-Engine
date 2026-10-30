#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/input/backend/sdl/SdlInputInjection.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <optional>
#include <type_traits>
#include <utility>

namespace Engine::Input::Backend::Sdl {

// InputEvent::timestamp is declared with the standard type so that GYO::Input
// needs no GYO::Time; it must stay the Engine time base.
static_assert(std::is_same_v<decltype(InputEvent::timestamp), Time::TimePoint>);

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

// The keys printable keycodes stand for, independent of the layout that
// produced them (SDL keycodes of printable keys are their unshifted character).
constexpr std::array<std::pair<SDL_Keycode, Key>, 53> CharacterKeys{{
    {SDLK_A, Key::A}, {SDLK_B, Key::B}, {SDLK_C, Key::C}, {SDLK_D, Key::D}, {SDLK_E, Key::E},
    {SDLK_F, Key::F}, {SDLK_G, Key::G}, {SDLK_H, Key::H}, {SDLK_I, Key::I}, {SDLK_J, Key::J},
    {SDLK_K, Key::K}, {SDLK_L, Key::L}, {SDLK_M, Key::M}, {SDLK_N, Key::N}, {SDLK_O, Key::O},
    {SDLK_P, Key::P}, {SDLK_Q, Key::Q}, {SDLK_R, Key::R}, {SDLK_S, Key::S}, {SDLK_T, Key::T},
    {SDLK_U, Key::U}, {SDLK_V, Key::V}, {SDLK_W, Key::W}, {SDLK_X, Key::X}, {SDLK_Y, Key::Y},
    {SDLK_Z, Key::Z},
    {SDLK_0, Key::Digit0}, {SDLK_1, Key::Digit1}, {SDLK_2, Key::Digit2}, {SDLK_3, Key::Digit3},
    {SDLK_4, Key::Digit4}, {SDLK_5, Key::Digit5}, {SDLK_6, Key::Digit6}, {SDLK_7, Key::Digit7},
    {SDLK_8, Key::Digit8}, {SDLK_9, Key::Digit9},
    {SDLK_RETURN, Key::Enter}, {SDLK_ESCAPE, Key::Escape}, {SDLK_BACKSPACE, Key::Backspace},
    {SDLK_TAB, Key::Tab}, {SDLK_SPACE, Key::Space}, {SDLK_DELETE, Key::Delete},
    {SDLK_MINUS, Key::Minus}, {SDLK_EQUALS, Key::Equals}, {SDLK_LEFTBRACKET, Key::LeftBracket},
    {SDLK_RIGHTBRACKET, Key::RightBracket}, {SDLK_BACKSLASH, Key::Backslash},
    {SDLK_SEMICOLON, Key::Semicolon}, {SDLK_APOSTROPHE, Key::Apostrophe}, {SDLK_GRAVE, Key::Grave},
    {SDLK_COMMA, Key::Comma}, {SDLK_PERIOD, Key::Period}, {SDLK_SLASH, Key::Slash},
}};

// The key a keycode stands for: keycodes without a character (keypad, arrows,
// function keys) carry their scancode; printable ones are looked up by
// character. Count if none.
Key LayoutKey(const SDL_Keycode keycode) noexcept {
    for (const auto& [code, key] : CharacterKeys) {
        if (code == keycode) return key;
    }
    if ((keycode & SDLK_SCANCODE_MASK) != 0) {
        if (const auto key = ToKey(static_cast<SDL_Scancode>(keycode & ~SDLK_SCANCODE_MASK))) return *key;
    }
    return Key::Count;
}

KeyModifiers ToModifiers(const SDL_Keymod mod) noexcept {
    return {.shift = (mod & SDL_KMOD_SHIFT) != 0,
            .ctrl = (mod & SDL_KMOD_CTRL) != 0,
            .alt = (mod & SDL_KMOD_ALT) != 0,
            .gui = (mod & SDL_KMOD_GUI) != 0};
}

// The scancode an injected key carries: the first table entry for the key
// (Return for Enter).
SDL_Scancode ToScancode(const Key key) noexcept {
    for (const auto& [code, mapped] : ScancodeKeys) {
        if (mapped == key) return code;
    }
    return SDL_SCANCODE_UNKNOWN;
}

Uint8 ToSdlButton(const MouseButton button) noexcept {
    switch (button) {
    case MouseButton::Left: return SDL_BUTTON_LEFT;
    case MouseButton::Right: return SDL_BUTTON_RIGHT;
    case MouseButton::Middle: return SDL_BUTTON_MIDDLE;
    case MouseButton::X1: return SDL_BUTTON_X1;
    case MouseButton::X2: return SDL_BUTTON_X2;
    case MouseButton::Count: break;
    }
    return 0;
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

// Text for a diagnostic line: printable ASCII as is, every other code point
// as \u{XXXX}, so a full-width colon (U+FF1A) is told apart from ':'. Bytes
// that are not UTF-8 appear as \x{XX}.
std::string QuoteText(const char* text) {
    std::string out{"\""};
    if (text == nullptr) return "null";
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    char buffer[16];
    for (std::size_t index = 0; bytes[index] != 0;) {
        const unsigned char lead = bytes[index];
        std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
        std::uint32_t codePoint = length == 1 ? lead : length == 2 ? lead & 0x1F : length == 3 ? lead & 0x0F : lead & 0x07;
        for (std::size_t next = 1; next < length; ++next) {
            if ((bytes[index + next] & 0xC0) != 0x80) {
                length = 0;
                break;
            }
            codePoint = (codePoint << 6) | (bytes[index + next] & 0x3F);
        }
        if (length == 0) {
            std::snprintf(buffer, sizeof(buffer), "\\x{%02X}", lead);
            out += buffer;
            ++index;
            continue;
        }
        if (codePoint >= 0x20 && codePoint < 0x7F && codePoint != '"' && codePoint != '\\') {
            out += static_cast<char>(codePoint);
        } else {
            std::snprintf(buffer, sizeof(buffer), "\\u{%04X}", static_cast<unsigned>(codePoint));
            out += buffer;
        }
        index += length;
    }
    return out + "\"";
}

void LogText(const std::string& line) noexcept {
    Base::Log(Base::LogLevel::Info, "input.text", line);
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
    const SDL_WindowFlags flags = SDL_GetWindowFlags(Platform::Sdl::NativeWindow(platform));
    frame_.windowFocused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    frame_.pointer.relativeMode = SDL_GetWindowRelativeMouseMode(Platform::Sdl::NativeWindow(platform));
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
    frame_.text.clear();
}

void SdlInput::HandleEvent(const Platform::Sdl::PlatformEvent& platformEvent) noexcept {
    const SDL_Event& event = Platform::Sdl::NativeEvent(platformEvent);
    const SDL_WindowID ownWindowId = SDL_GetWindowID(Platform::Sdl::NativeWindow(*platform_));
    // Every InputEvent this platform event adds carries its time.
    const std::size_t firstAdded = frame_.events.size();
    struct StampAdded final {
        std::vector<InputEvent>& events;
        std::size_t first;
        Time::TimePoint time;
        ~StampAdded() {
            for (std::size_t index = first; index < events.size(); ++index) events[index].timestamp = time;
        }
    } stamp{frame_.events, firstAdded, platformEvent.Timestamp()};

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
    case SDL_EVENT_TEXT_INPUT: {
        // Whether text input is on is read from SDL at arrival (it may be
        // turned on or off by others, such as an editor's UI backend).
        const bool active = SDL_TextInputActive(Platform::Sdl::NativeWindow(*platform_));
        const bool taken = event.text.windowID == ownWindowId && event.text.text != nullptr && active;
        if (taken) {
            frame_.text.push_back({.kind = TextEvent::Kind::Text, .text = event.text.text});
        }
        if (textDiagnostics_) {
            try {
                LogText("text_input t=" + std::to_string(event.text.timestamp) +
                        " own_window=" + std::to_string(event.text.windowID == ownWindowId) +
                        " active=" + std::to_string(active) + " taken=" + std::to_string(taken) +
                        " text=" + QuoteText(event.text.text));
            } catch (...) {
            }
        }
        break;
    }
    case SDL_EVENT_TEXT_EDITING:
        // Not part of the frame (yet); seen only by the diagnostics.
        if (textDiagnostics_) {
            try {
                LogText("text_editing t=" + std::to_string(event.edit.timestamp) +
                        " own_window=" + std::to_string(event.edit.windowID == ownWindowId) +
                        " active=" + std::to_string(SDL_TextInputActive(Platform::Sdl::NativeWindow(*platform_))) +
                        " start=" + std::to_string(event.edit.start) + " length=" + std::to_string(event.edit.length) +
                        " text=" + QuoteText(event.edit.text));
            } catch (...) {
            }
        }
        break;
    case SDL_EVENT_TEXT_EDITING_CANDIDATES:
        if (textDiagnostics_) {
            try {
                LogText("text_candidates t=" + std::to_string(event.edit_candidates.timestamp) +
                        " own_window=" + std::to_string(event.edit_candidates.windowID == ownWindowId) +
                        " count=" + std::to_string(event.edit_candidates.num_candidates) +
                        " selected=" + std::to_string(event.edit_candidates.selected_candidate));
            } catch (...) {
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        // Text keys: presses only, repeats included, while text input is on.
        if (textDiagnostics_ && event.type == SDL_EVENT_KEY_DOWN &&
            SDL_TextInputActive(Platform::Sdl::NativeWindow(*platform_))) {
            try {
                char mod[8];
                std::snprintf(mod, sizeof(mod), "0x%04X", static_cast<unsigned>(event.key.mod));
                LogText("text_key t=" + std::to_string(event.key.timestamp) +
                        " own_window=" + std::to_string(event.key.windowID == ownWindowId) +
                        " scancode=" + QuoteText(SDL_GetScancodeName(event.key.scancode)) +
                        " key=" + QuoteText(SDL_GetKeyName(event.key.key)) + " mod=" + mod +
                        " repeat=" + std::to_string(event.key.repeat));
            } catch (...) {
            }
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == ownWindowId &&
            SDL_TextInputActive(Platform::Sdl::NativeWindow(*platform_))) {
            frame_.text.push_back({.kind = TextEvent::Kind::Key,
                                   .key = ToKey(event.key.scancode).value_or(Key::Count),
                                   .layoutKey = LayoutKey(event.key.key),
                                   .modifiers = ToModifiers(event.key.mod),
                                   .repeat = event.key.repeat});
        }
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
                SDL_GetWindowSize(Platform::Sdl::NativeWindow(*platform_), &width, &height);
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
SdlInput::SetTextInput(const bool enabled) {
    SDL_Window* window = Platform::Sdl::NativeWindow(*platform_);
    const bool changed = enabled ? SDL_StartTextInput(window) : SDL_StopTextInput(window);
    if (textDiagnostics_) {
        try {
            LogText("set_text_input enabled=" + std::to_string(enabled) + " ok=" + std::to_string(changed) +
                    " active=" + std::to_string(SDL_TextInputActive(window)));
        } catch (...) {
        }
    }
    if (!changed) {
        return Base::Err(SdlInputError::Make(
            SdlInputErrorCode::TextInputFailed,
            "SdlInput: unable to change text input",
            SDL_GetError()));
    }
    return {};
}

void SdlInput::SetTextDiagnostics(const bool enabled) noexcept {
    textDiagnostics_ = enabled;
    if (!enabled) return;
    try {
        const char* imeUi = SDL_GetHint(SDL_HINT_IME_IMPLEMENTED_UI);
        LogText(std::string{"diagnostics on ime_ui="} + QuoteText(imeUi == nullptr ? "(default)" : imeUi) +
                " active=" + std::to_string(SDL_TextInputActive(Platform::Sdl::NativeWindow(*platform_))));
    } catch (...) {
    }
}

Base::Result<void, SdlInputError>
SdlInput::SetRelativeMouseMode(const bool enabled) {
    if (frame_.pointer.relativeMode == enabled) {
        return {};
    }

    if (!SDL_SetWindowRelativeMouseMode(Platform::Sdl::NativeWindow(*platform_), enabled)) {
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

SdlInputInjector::SdlInputInjector(Platform::Sdl::SdlPlatform& platform) noexcept
    : platform_(&platform) {}

namespace {

Base::Result<void, SdlInputError> Push(Platform::Sdl::SdlPlatform& platform, SDL_Event& event) {
    if (!Platform::Sdl::PushInjectedEvent(platform, event)) {
        return Base::Err(SdlInputError::Make(SdlInputErrorCode::InjectionFailed,
                                             "SdlInputInjector: SDL did not queue the event", SDL_GetError()));
    }
    return {};
}

} // namespace

Base::Result<void, SdlInputError> SdlInputInjector::PushKey(const Key key, const bool down) {
    GYO_ASSERT(key != Key::Count);
    const SDL_Scancode scancode = ToScancode(key);
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(Platform::Sdl::NativeWindow(*platform_));
    event.key.scancode = scancode;
    event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
    event.key.mod = SDL_KMOD_NONE;
    event.key.down = down;
    event.key.repeat = false;
    return Push(*platform_, event);
}

Base::Result<void, SdlInputError> SdlInputInjector::PushMouseButton(const MouseButton button, const bool down) {
    GYO_ASSERT(button != MouseButton::Count);
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(Platform::Sdl::NativeWindow(*platform_));
    event.button.button = ToSdlButton(button);
    event.button.down = down;
    event.button.clicks = 1;
    return Push(*platform_, event);
}

Base::Result<void, SdlInputError> SdlInputInjector::PushMouseMotion(const float deltaX, const float deltaY) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(Platform::Sdl::NativeWindow(*platform_));
    event.motion.xrel = deltaX;
    event.motion.yrel = deltaY;
    return Push(*platform_, event);
}

Base::Result<void, SdlInputError> SdlInputInjector::PushFocus(const bool gained) {
    SDL_Event event{};
    event.type = gained ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.windowID = SDL_GetWindowID(Platform::Sdl::NativeWindow(*platform_));
    return Push(*platform_, event);
}

} // namespace Engine::Input::Backend::Sdl

