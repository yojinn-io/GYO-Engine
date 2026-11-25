#pragma once

// sdl_window's own queue (SDL declaration section 9.1; not public): what the adapter translated from SDL events,
// waiting for the writer Systems of the World that lists sdl_window. Its items are the input and window Messages
// themselves (as Bevy's winit puts bevy_window / bevy_input types into its own queue, bevy_winit/src/state.rs:80).
//
// Bounded, with one bound for the input Messages and one for the window Messages, so a flood of mouse motion cannot
// crowd out a focus loss or a close request. One OS event becomes its Messages all together or not at all; an event
// that does not fit is dropped and counted (SDL-7 exception (1)), and the input writer reports the count as input's
// InputEventsDropped. Each bound is at most the kernel's Message capacity (1024 by default), so one frame's writes
// never overflow a kernel buffer.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "engine/input/Input.hpp"
#include "engine/window/Window.hpp"

namespace GYO::Plugins::SDLWindow {

using InputItem = std::variant<Engine::Input::KeyboardInput, Engine::Input::KeyboardFocusLost, Engine::Input::MouseButtonInput,
                               Engine::Input::CursorMoved, Engine::Input::MouseMotion, Engine::Input::CursorEntered,
                               Engine::Input::CursorLeft, Engine::Input::MouseWheel, Engine::Input::TextComposition,
                               Engine::Input::TextCommitted>;
using WindowItem = std::variant<Engine::Window::WindowResized, Engine::Window::WindowFocused, Engine::Window::WindowModeChanged,
                                Engine::Window::WindowCloseRequested, Engine::Window::ClipboardChanged>;

// The Messages one OS event becomes (0, 1 or 2 per the mapping table, sdl.zh-Hant.md SDL-7).
struct Translated final {
    std::vector<InputItem> input;
    std::vector<WindowItem> window;

    [[nodiscard]] std::size_t Size() const noexcept { return input.size() + window.size(); }
};

class EventQueue final {
public:
    EventQueue(std::size_t inputCapacity, std::size_t windowCapacity)
        : inputCapacity_(inputCapacity), windowCapacity_(windowCapacity) {}

    // All of one event's Messages, or none and the drop counted (false). An event with no Messages changes nothing.
    bool Accept(Translated translated) {
        if (translated.Size() == 0) return true;
        if (input_.size() + translated.input.size() > inputCapacity_ ||
            window_.size() + translated.window.size() > windowCapacity_) {
            ++dropped_;
            ++droppedTotal_;
            return false;
        }
        for (InputItem& item : translated.input) input_.push_back(std::move(item));
        for (WindowItem& item : translated.window) window_.push_back(std::move(item));
        return true;
    }
    [[nodiscard]] std::vector<InputItem> TakeInput() { return std::exchange(input_, {}); }
    [[nodiscard]] std::vector<WindowItem> TakeWindow() { return std::exchange(window_, {}); }
    // The events dropped since the last call.
    [[nodiscard]] std::uint64_t TakeDropped() noexcept { return std::exchange(dropped_, 0); }
    [[nodiscard]] std::uint64_t DroppedTotal() const noexcept { return droppedTotal_; }

private:
    std::size_t inputCapacity_;
    std::size_t windowCapacity_;
    std::vector<InputItem> input_;
    std::vector<WindowItem> window_;
    std::uint64_t dropped_ = 0;
    std::uint64_t droppedTotal_ = 0;
};

} // namespace GYO::Plugins::SDLWindow
