#pragma once

// The native layer of the SDL input backend (docs/architecture/platform-sdl.md).
// Linking GYO::InputBackendSDLNative declares direct use of SDL.

#include <SDL3/SDL_events.h>

#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

namespace Engine::Input::Backend::Sdl {

// Hands an event the caller built itself to the input backend, as if
// SdlPlatform::PumpEvents had delivered it with the given time (Engine time
// base). There is no default: the caller states the time.
inline void HandleNativeEvent(SdlInput& input, const SDL_Event& event, Time::TimePoint timestamp) noexcept {
    input.HandleEvent(Platform::Sdl::NativeAccess::View(event, timestamp));
}

} // namespace Engine::Input::Backend::Sdl
