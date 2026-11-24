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

// The unit MouseMotion is written in (the Plugin's MouseMotionUnit as it reaches the adapter).
enum class MotionUnit : std::uint8_t { Device = 1, Logical = 2 };
// SDL's xrel, yrel in that unit: as they are, or times the logical pixels per window coordinate.
[[nodiscard]] constexpr Engine::Math::Vec2 MotionDelta(const float xrel, const float yrel, const float logicalPerCoordinate,
                                                       const MotionUnit unit) noexcept {
    return unit == MotionUnit::Logical ? Engine::Math::Vec2{xrel * logicalPerCoordinate, yrel * logicalPerCoordinate}
                                       : Engine::Math::Vec2{xrel, yrel};
}

// The unit of SDL_TextEditingEvent's start and length. SDL documents "UTF-8 characters" (code points,
// SDL_events.h:378-380) and its Cocoa, Wayland, X11, IBus and Fcitx back ends give code points
// (SDL_cocoakeyboard.m:145-147, SDL_waylandevents.c:3074-3077, SDL_x11keyboard.c:562-580, SDL_ibus.c:160-161,
// SDL_fcitx.c:187); its Windows back end gives the IMM's positions as they are, UTF-16 code units, in all three of
// its paths (cursor SDL_windowskeyboard.c:811, selection :845-878, reading string :924). Checked on SDL 3.4.0: the
// Windows unit contradicts SDL's own documentation, so an upgrade checks IME_SendEditingEvent again (sdl.zh-Hant.md,
// "升版"); within the Basic Multilingual Plane the two units agree, so a change there would only show beyond it.
enum class EditingUnit : std::uint8_t { CodePoint = 1, Utf16 = 2 };
// The unit of the video driver SDL runs ("windows": UTF-16; any other, or none: code points).
[[nodiscard]] EditingUnit EditingUnitOf(const char* videoDriver) noexcept;
// The byte offset in utf8 of the position units units from its start (a code point outside the Basic Multilingual
// Plane is two UTF-16 units; a position inside one rounds down), clamped to the text; -1 (not given) stays -1.
[[nodiscard]] std::int32_t EditingByteOffset(std::string_view utf8, std::int32_t units, EditingUnit unit) noexcept;

// The Messages event becomes (SDL-7's table); every Message carries the event's time converted with pair, no later
// than ceiling. window is the program's window: window events of another window become nothing. editingUnit: the
// unit of the input method's positions (EditingUnitOf the running video driver). motionUnit: MouseMotion's unit.
[[nodiscard]] Translated Translate(const SDL_Event& event, const SDL::ClockPair& pair, Engine::Time::TimePoint ceiling,
                                   SDL_Window* window, std::size_t clipboardLimit, EditingUnit editingUnit,
                                   MotionUnit motionUnit = MotionUnit::Device);

// Within one pump the times strictly increase in the order SDL delivered the events: an event's time is raised to
// 1 ns after the one before when it is not later. SDL stamps some events with the OS's time and others when it
// queues them (window events, SDL_events.c:1802), so delivered order and stamps can disagree; input tells two
// Message types apart by time (input.zh-Hant.md), and the delivered order is the true one. last: the previous
// event's time in this pump (nullopt for the first).
void OrderTimes(Translated& translated, std::optional<Engine::Time::TimePoint>& last);

} // namespace GYO::Plugins::SDLWindow::Adapter
