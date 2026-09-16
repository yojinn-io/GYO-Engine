#include "input/backend/sdl/SdlInput.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <utility>

namespace Engine::Input::Backend::Sdl {
namespace {

std::optional<Key> ToKey(const SDL_Scancode scancode) noexcept {
    switch (scancode) {
    case SDL_SCANCODE_W:
        return Key::W;
    case SDL_SCANCODE_A:
        return Key::A;
    case SDL_SCANCODE_S:
        return Key::S;
    case SDL_SCANCODE_D:
        return Key::D;
    case SDL_SCANCODE_LEFT:
        return Key::Left;
    case SDL_SCANCODE_RIGHT:
        return Key::Right;
    case SDL_SCANCODE_UP:
        return Key::Up;
    case SDL_SCANCODE_DOWN:
        return Key::Down;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return Key::Enter;
    case SDL_SCANCODE_ESCAPE:
        return Key::Escape;
    case SDL_SCANCODE_SPACE:
        return Key::Space;
    case SDL_SCANCODE_H:
        return Key::H;
    default:
        return std::nullopt;
    }
}

std::optional<MouseButton> ToMouseButton(const Uint8 button) noexcept {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
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
}

void SdlInput::HandleEvent(const SDL_Event& event) noexcept {
    const SDL_WindowID ownWindowId = SDL_GetWindowID(platform_->NativeWindow());

    switch (event.type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (event.window.windowID == ownWindowId) {
            frame_.windowFocused = true;
            suppressRelativeMotion_ = true;
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (event.window.windowID == ownWindowId) {
            frame_.windowFocused = false;
            frame_.pointer.deltaX = 0.0f;
            frame_.pointer.deltaY = 0.0f;
            ReleaseAll();
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (event.key.windowID == ownWindowId && !event.key.repeat) {
            if (const auto key = ToKey(event.key.scancode)) {
                SetKey(*key, event.type == SDL_EVENT_KEY_DOWN);
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.windowID == ownWindowId) {
            frame_.pointer.x = event.button.x;
            frame_.pointer.y = event.button.y;
            if (const auto button = ToMouseButton(event.button.button)) {
                SetMouseButton(*button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
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
        return Base::Result<void, SdlInputError>::Ok();
    }

    if (!SDL_SetWindowRelativeMouseMode(platform_->NativeWindow(), enabled)) {
        return Base::Result<void, SdlInputError>::Err(SdlInputError::Make(
            SdlInputErrorCode::RelativeMouseModeFailed,
            "SdlInput: unable to change relative mouse mode",
            SDL_GetError()));
    }

    frame_.pointer.relativeMode = enabled;
    frame_.pointer.deltaX = 0.0f;
    frame_.pointer.deltaY = 0.0f;
    suppressRelativeMotion_ = true;
    return Base::Result<void, SdlInputError>::Ok();
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
