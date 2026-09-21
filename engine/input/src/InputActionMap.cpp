#include "engine/input/InputActionMap.hpp"

namespace Engine::Input {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t HashName(std::string_view value) noexcept {
    if (value.empty()) {
        return 0;
    }
    std::uint64_t result = kFnvOffset;
    for (const unsigned char character : value) {
        result ^= character;
        result *= kFnvPrime;
    }
    return result;
}

void Merge(InputActionState& destination, const ButtonState& source) noexcept {
    destination.held = destination.held || source.held;
    destination.pressed = destination.pressed || source.pressed;
    destination.released = destination.released || source.released;
}

} // namespace

InputActionId InputActionId::FromString(const std::string_view name) {
    return {HashName(name), std::string(name)};
}

InputAxisId InputAxisId::FromString(const std::string_view name) {
    return {HashName(name), std::string(name)};
}

InputActionState InputActionFrame::Action(const InputActionId id) const noexcept {
    const auto found = actions_.find(id.value);
    return found == actions_.end() ? InputActionState{} : found->second;
}

float InputActionFrame::Axis(const InputAxisId id) const noexcept {
    const auto found = axes_.find(id.value);
    return found == axes_.end() ? 0.0f : found->second;
}

void InputActionMap::Bind(const InputActionId action, const Key key) {
    if (action.IsValid()) {
        actionBindings_.push_back({action, key, std::nullopt});
    }
}

void InputActionMap::Bind(
    const InputActionId action,
    const MouseButton button) {
    if (action.IsValid()) {
        actionBindings_.push_back({action, std::nullopt, button});
    }
}

void InputActionMap::BindDigitalAxis(
    const InputAxisId axis,
    const Key negative,
    const Key positive,
    const float scale) {
    if (axis.IsValid()) {
        digitalAxisBindings_.push_back({axis, negative, positive, scale});
    }
}

void InputActionMap::BindPointerAxis(
    const InputAxisId axis,
    const PointerAxis pointerAxis,
    const float scale) {
    if (axis.IsValid()) {
        pointerAxisBindings_.push_back({axis, pointerAxis, scale});
    }
}

InputActionFrame InputActionMap::Evaluate(const PhysicalInputFrame& input) const {
    InputActionFrame result;

    for (const ActionBinding& binding : actionBindings_) {
        InputActionState& state = result.actions_[binding.action.value];
        if (binding.key.has_value()) {
            Merge(state, input.Get(*binding.key));
        } else if (binding.mouseButton.has_value()) {
            Merge(state, input.Get(*binding.mouseButton));
        }
    }

    for (const DigitalAxisBinding& binding : digitalAxisBindings_) {
        const float negative = input.Get(binding.negative).held ? 1.0f : 0.0f;
        const float positive = input.Get(binding.positive).held ? 1.0f : 0.0f;
        result.axes_[binding.axis.value] += (positive - negative) * binding.scale;
    }

    for (const PointerAxisBinding& binding : pointerAxisBindings_) {
        const float value = binding.pointerAxis == PointerAxis::DeltaX
            ? input.pointer.deltaX
            : input.pointer.deltaY;
        result.axes_[binding.axis.value] += value * binding.scale;
    }

    return result;
}

} // namespace Engine::Input
