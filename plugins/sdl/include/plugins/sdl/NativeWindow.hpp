#pragma once

// The window handed from sdl_window to the other sdl* Plugins (SDL declaration section 9.1; Design Invariant SDL-1):
// an opaque handle, never SDL's type. sdl_window Creates a NativeWindows in the Product's buffer in its Configure and
// puts its window there; another sdl* Plugin Finds it and turns the handle back into SDL_Window* inside its own
// adapter (src/adapters/sdl/), the only place that names SDL.

namespace GYO::Plugins::SDL {

// Incomplete on purpose: only an adapter converts it (reinterpret_cast to SDL_Window*).
struct NativeWindow;

struct NativeWindows final {
    // GYO has one window (SDL declaration section 6.6); nullptr until sdl_window has made it.
    NativeWindow* main = nullptr;
};

} // namespace GYO::Plugins::SDL
