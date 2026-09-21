#pragma once

#include "engine/input/PhysicalInputFrame.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine::Input {

struct InputActionId final {
    std::uint64_t value{};
    std::string debugName;

    [[nodiscard]] static InputActionId FromString(std::string_view name);
    [[nodiscard]] bool IsValid() const noexcept { return value != 0; }

    // The hashed value is the identity. debugName is diagnostic metadata and
    // must not change equality because frame storage and hashing use value.
    friend bool operator==(const InputActionId& left,
                           const InputActionId& right) noexcept {
        return left.value == right.value;
    }
};

struct InputAxisId final {
    std::uint64_t value{};
    std::string debugName;

    [[nodiscard]] static InputAxisId FromString(std::string_view name);
    [[nodiscard]] bool IsValid() const noexcept { return value != 0; }

    friend bool operator==(const InputAxisId& left,
                           const InputAxisId& right) noexcept {
        return left.value == right.value;
    }
};

enum class PointerAxis {
    DeltaX,
    DeltaY,
};

struct InputActionState final {
    bool held{};
    bool pressed{};
    bool released{};
};

class InputActionFrame final {
public:
    [[nodiscard]] InputActionState Action(InputActionId id) const noexcept;
    [[nodiscard]] float Axis(InputAxisId id) const noexcept;

private:
    friend class InputActionMap;
    std::unordered_map<std::uint64_t, InputActionState> actions_;
    std::unordered_map<std::uint64_t, float> axes_;
};

class InputActionMap final {
public:
    void Bind(InputActionId action, Key key);
    void Bind(InputActionId action, MouseButton button);

    void BindDigitalAxis(
        InputAxisId axis,
        Key negative,
        Key positive,
        float scale = 1.0f);

    void BindPointerAxis(
        InputAxisId axis,
        PointerAxis pointerAxis,
        float scale = 1.0f);

    [[nodiscard]] InputActionFrame Evaluate(const PhysicalInputFrame& input) const;

private:
    struct ActionBinding final {
        InputActionId action;
        std::optional<Key> key;
        std::optional<MouseButton> mouseButton;
    };

    struct DigitalAxisBinding final {
        InputAxisId axis;
        Key negative{};
        Key positive{};
        float scale{1.0f};
    };

    struct PointerAxisBinding final {
        InputAxisId axis;
        PointerAxis pointerAxis{};
        float scale{1.0f};
    };

    std::vector<ActionBinding> actionBindings_;
    std::vector<DigitalAxisBinding> digitalAxisBindings_;
    std::vector<PointerAxisBinding> pointerAxisBindings_;
};

} // namespace Engine::Input

namespace std {

template <>
struct hash<Engine::Input::InputActionId> {
    size_t operator()(const Engine::Input::InputActionId& id) const noexcept {
        return static_cast<size_t>(id.value);
    }
};

template <>
struct hash<Engine::Input::InputAxisId> {
    size_t operator()(const Engine::Input::InputAxisId& id) const noexcept {
        return static_cast<size_t>(id.value);
    }
};

} // namespace std
