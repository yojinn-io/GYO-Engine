#pragma once

// The native layer of the SDL platform (docs/architecture/platform-sdl.md).
// Including this header, or linking GYO::PlatformSDLNative, declares direct
// use of SDL. The neutral header SdlPlatform.hpp never names SDL.

#include <SDL3/SDL.h>

#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Platform::Sdl {

class NativeAccess final {
public:
    [[nodiscard]] static SDL_Window* Window(const SdlPlatform& platform) noexcept;

    [[nodiscard]] static const SDL_Event& Event(const PlatformEvent& event) noexcept {
        return *static_cast<const SDL_Event*>(event.native_);
    }

    // A neutral view of a native event, for code that builds SDL events itself
    // (tests, adapters). Valid as long as the event and the returned view live.
    [[nodiscard]] static PlatformEvent View(const SDL_Event& event) noexcept {
        return PlatformEvent(&event);
    }
};

[[nodiscard]] inline SDL_Window* NativeWindow(const SdlPlatform& platform) noexcept {
    return NativeAccess::Window(platform);
}

[[nodiscard]] inline const SDL_Event& NativeEvent(const PlatformEvent& event) noexcept {
    return NativeAccess::Event(event);
}

} // namespace Engine::Platform::Sdl
