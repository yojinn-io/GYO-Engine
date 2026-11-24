#pragma once

// The mapping table of Design Invariant SDL-7 (docs/architecture/target/v2/sdl.zh-Hant.md), private to the adapter:
// one SDL event to the input and window Messages it becomes. Its own header so the adapter's tests
// (tests/adapters/sdl/) can feed it events and compare the Messages.

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>

#include "engine/input/Input.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "plugins/sdl/ClockPair.hpp"

#include "../../EventQueue.hpp"

namespace GYO::Plugins::SDLWindow::Adapter {

// SDL's scancodes are the USB HID usage ids input's KeyCode uses; a code input does not name is Unidentified.
[[nodiscard]] Engine::Input::KeyCode KeyCodeOf(SDL_Scancode scancode) noexcept;
[[nodiscard]] Engine::Input::KeyModifiers ModifiersOf(SDL_Keymod mod) noexcept;
// The character the key gives on the current layout without modifiers (0: none).
[[nodiscard]] char32_t CharacterOf(SDL_Scancode scancode) noexcept;
// Logical pixels per SDL window coordinate: pixel density / display scale (1 on Windows at 100 %, 1/1.5 at 150 %;
// 1 on a Retina Mac, whose window coordinates are already points). 1 when SDL gives no usable value.
[[nodiscard]] constexpr float LogicalPerCoordinate(const float pixelDensity, const float displayScale) noexcept {
    return pixelDensity > 0.0F && displayScale > 0.0F ? pixelDensity / displayScale : 1.0F;
}
[[nodiscard]] float LogicalPerWindowCoordinate(SDL_Window* window) noexcept;

// The unit of SDL_TextEditingEvent's start and length. SDL documents "UTF-8 characters" (code points,
// SDL_events.h:378-380) and its Cocoa, Wayland, X11, IBus and Fcitx back ends give code points
// (SDL_cocoakeyboard.m:145-147, SDL_waylandevents.c:3076-3077); its Windows back end gives the IMM's positions as
// they are, UTF-16 code units (SDL_windowskeyboard.c:811, :924-928).
enum class EditingUnit : std::uint8_t { CodePoint = 1, Utf16 = 2 };
// The unit of the video driver SDL runs ("windows": UTF-16; any other, or none: code points).
[[nodiscard]] EditingUnit EditingUnitOf(const char* videoDriver) noexcept;
// The byte offset in utf8 of the position units units from its start (a code point outside the Basic Multilingual
// Plane is two UTF-16 units; a position inside one rounds down), clamped to the text; -1 (not given) stays -1.
[[nodiscard]] std::int32_t EditingByteOffset(std::string_view utf8, std::int32_t units, EditingUnit unit) noexcept;

// The Messages event becomes (SDL-7's table); every Message carries the event's time converted with pair, no later
// than ceiling. window is the program's window: window events of another window become nothing.
[[nodiscard]] Translated Translate(const SDL_Event& event, const SDL::ClockPair& pair, Engine::Time::TimePoint ceiling,
                                   SDL_Window* window, std::size_t clipboardLimit);

// Within one pump the times strictly increase in the order SDL delivered the events: an event's time is raised to
// 1 ns after the one before when it is not later. SDL stamps some events with the OS's time and others when it
// queues them (window events, SDL_events.c:1802), so delivered order and stamps can disagree; input tells two
// Message types apart by time (input.zh-Hant.md), and the delivered order is the true one. last: the previous
// event's time in this pump (nullopt for the first).
void OrderTimes(Translated& translated, std::optional<Engine::Time::TimePoint>& last);

} // namespace GYO::Plugins::SDLWindow::Adapter
