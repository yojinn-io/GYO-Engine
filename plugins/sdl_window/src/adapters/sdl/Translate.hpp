#pragma once

// The mapping table of Design Invariant SDL-7 (docs/architecture/target/v2/sdl.zh-Hant.md), private to the adapter:
// one SDL event to the input and window Messages it becomes. Its own header so the adapter's tests
// (tests/adapters/sdl/) can feed it events and compare the Messages.

#include <SDL3/SDL.h>

#include <cstddef>

#include "engine/input/Input.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "plugins/sdl/ClockPair.hpp"

#include "../../EventQueue.hpp"

namespace GYO::Plugins::SdlWindow::Adapter {

// SDL's scancodes are the USB HID usage ids input's KeyCode uses; a code input does not name is Unidentified.
[[nodiscard]] Engine::Input::KeyCode KeyCodeOf(SDL_Scancode scancode) noexcept;
[[nodiscard]] Engine::Input::KeyModifiers ModifiersOf(SDL_Keymod mod) noexcept;
// The character the key gives on the current layout without modifiers (0: none).
[[nodiscard]] char32_t CharacterOf(SDL_Scancode scancode) noexcept;
// Logical pixels per SDL window coordinate: pixel density / display scale (1 on Windows at 100 %, 1/1.5 at 150 %;
// 1 on a Retina Mac, whose window coordinates are already points).
[[nodiscard]] float LogicalPerWindowCoordinate(SDL_Window* window) noexcept;

// The Messages event becomes (SDL-7's table); every Message carries the event's time converted with pair, no later
// than ceiling. window is the program's window: window events of another window become nothing.
[[nodiscard]] Translated Translate(const SDL_Event& event, const Sdl::ClockPair& pair, Engine::Time::TimePoint ceiling,
                                   SDL_Window* window, std::size_t clipboardLimit);

} // namespace GYO::Plugins::SdlWindow::Adapter
