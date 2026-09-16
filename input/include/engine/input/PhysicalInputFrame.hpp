#pragma once

#include <array>
#include <cstddef>

namespace Engine::Input {

enum class Key : std::size_t {
    W,
    A,
    S,
    D,
    Left,
    Right,
    Up,
    Down,
    R,
    Enter,
    Escape,
    Space,
    H,
    Count,
};

enum class MouseButton : std::size_t {
    Left,
    Right,
    Middle,
    Count,
};

struct ButtonState final {
    bool held{};
    bool pressed{};
    bool released{};
};

struct PointerState final {
    float x{};
    float y{};
    float deltaX{};
    float deltaY{};
    bool relativeMode{};
};

struct PhysicalInputFrame final {
    std::array<ButtonState, static_cast<std::size_t>(Key::Count)> keys{};
    std::array<ButtonState, static_cast<std::size_t>(MouseButton::Count)> mouseButtons{};
    PointerState pointer{};
    bool windowFocused{};

    [[nodiscard]] const ButtonState& Get(Key key) const noexcept {
        return keys[static_cast<std::size_t>(key)];
    }

    [[nodiscard]] const ButtonState& Get(MouseButton button) const noexcept {
        return mouseButtons[static_cast<std::size_t>(button)];
    }
};

} // namespace Engine::Input
