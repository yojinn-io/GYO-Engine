#pragma once

// sdl_window's own queue (SDL declaration section 9.1; not public): what the adapter translated from SDL events,
// waiting for the writer Systems of the World that lists sdl_window. Its items are the input and window Messages
// themselves (as Bevy's winit puts bevy_window / bevy_input types into its own queue, bevy_winit/src/state.rs:80).
//
// Bounded: one OS event becomes its Messages all together or not at all; an event that does not fit is dropped and
// counted (SDL-7 exception (1)), and the input writer reports the count as input's InputEventsDropped. The bound is
// at most the kernel's Message capacity (1024 by default), so one frame's writes never overflow a kernel buffer.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "engine/input/Input.hpp"
#include "engine/window/Window.hpp"

namespace GYO::Plugins::SdlWindow {

using InputItem = std::variant<Engine::Input::KeyboardInput, Engine::Input::KeyboardFocusLost, Engine::Input::MouseButtonInput,
                               Engine::Input::CursorMoved, Engine::Input::MouseMotion, Engine::Input::CursorEntered,
                               Engine::Input::CursorLeft, Engine::Input::MouseWheel, Engine::Input::TextComposition,
                               Engine::Input::TextCommitted>;
using WindowItem = std::variant<Engine::Window::WindowResized, Engine::Window::WindowFocused,
                                Engine::Window::WindowCloseRequested, Engine::Window::ClipboardChanged>;

// The Messages one OS event becomes (0, 1 or 2 per the mapping table, sdl.zh-Hant.md SDL-7).
struct Translated final {
    std::vector<InputItem> input;
    std::vector<WindowItem> window;

    [[nodiscard]] std::size_t Size() const noexcept { return input.size() + window.size(); }
};

class EventQueue final {
public:
    explicit EventQueue(std::size_t capacity) : capacity_(capacity) {}

    // All of one event's Messages, or none and the drop counted. An event with no Messages changes nothing.
    void Accept(Translated translated) {
        if (translated.Size() == 0) return;
        if (input_.size() + window_.size() + translated.Size() > capacity_) {
            ++dropped_;
            ++droppedTotal_;
            return;
        }
        for (InputItem& item : translated.input) input_.push_back(std::move(item));
        for (WindowItem& item : translated.window) window_.push_back(std::move(item));
    }
    [[nodiscard]] std::vector<InputItem> TakeInput() { return std::exchange(input_, {}); }
    [[nodiscard]] std::vector<WindowItem> TakeWindow() { return std::exchange(window_, {}); }
    // The events dropped since the last call.
    [[nodiscard]] std::uint64_t TakeDropped() noexcept { return std::exchange(dropped_, 0); }
    [[nodiscard]] std::uint64_t DroppedTotal() const noexcept { return droppedTotal_; }
    [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }

private:
    std::size_t capacity_;
    std::vector<InputItem> input_;
    std::vector<WindowItem> window_;
    std::uint64_t dropped_ = 0;
    std::uint64_t droppedTotal_ = 0;
};

} // namespace GYO::Plugins::SdlWindow
