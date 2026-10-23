#pragma once

#include "engine/base/Result.hpp"
#include "engine/input/PhysicalInputFrame.hpp"
#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Input::Backend::Sdl {

// Test input for one window (docs/architecture/platform-sdl.md, Design
// Invariant 4): every call pushes one event through SDL's event queue (the
// platform's PushInjectedEvent); the next PumpEvents delivers it like real
// input. Main thread; never inside a live frame or an event watch callback.
//
// What it does not do: SDL's own keyboard, mouse and modifier state is not
// updated, no text input is produced, and an injected focus gain does not set
// the window's input focus flag. Key events carry the scancode (Enter: Return),
// SDL's keycode for it without modifiers, no modifiers and no repeat.
class SdlInputInjector final {
public:
    explicit SdlInputInjector(Platform::Sdl::SdlPlatform& platform) noexcept;

    [[nodiscard]] Base::Result<void, SdlInputError> PushKey(Key key, bool down);
    [[nodiscard]] Base::Result<void, SdlInputError> PushMouseButton(MouseButton button, bool down);
    [[nodiscard]] Base::Result<void, SdlInputError> PushMouseMotion(float deltaX, float deltaY);
    [[nodiscard]] Base::Result<void, SdlInputError> PushFocus(bool gained);

private:
    Platform::Sdl::SdlPlatform* platform_{};
};

} // namespace Engine::Input::Backend::Sdl
