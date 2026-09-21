#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/input/InputActionMap.hpp"

using namespace Engine::Input;

TEST_CASE("an action merges keyboard and mouse bindings") {
    const InputActionId fire = InputActionId::FromString("fire");
    InputActionMap map;
    map.Bind(fire, Key::Enter);
    map.Bind(fire, MouseButton::Left);

    PhysicalInputFrame physical;
    physical.mouseButtons[static_cast<std::size_t>(MouseButton::Left)] = {
        .held = true,
        .pressed = true,
    };

    const InputActionState state = map.Evaluate(physical).Action(fire);
    CHECK(state.held);
    CHECK(state.pressed);
    CHECK_FALSE(state.released);
}

TEST_CASE("digital and pointer axes remain independent of physical devices") {
    const InputAxisId move = InputAxisId::FromString("move_x");
    const InputAxisId look = InputAxisId::FromString("look_x");
    InputActionMap map;
    map.BindDigitalAxis(move, Key::A, Key::D);
    map.BindPointerAxis(look, PointerAxis::DeltaX, 0.5f);

    PhysicalInputFrame physical;
    physical.keys[static_cast<std::size_t>(Key::D)].held = true;
    physical.pointer.deltaX = 7.0f;

    const InputActionFrame frame = map.Evaluate(physical);
    CHECK(frame.Axis(move) == doctest::Approx(1.0f));
    CHECK(frame.Axis(look) == doctest::Approx(3.5f));
}

TEST_CASE("unknown actions and axes are neutral") {
    InputActionMap map;
    const InputActionFrame frame = map.Evaluate({});

    CHECK_FALSE(frame.Action(InputActionId::FromString("missing")).held);
    CHECK(frame.Axis(InputAxisId::FromString("missing")) == 0.0f);
}

TEST_CASE("space and H preserve independent action edges") {
    const InputActionId primary = InputActionId::FromString("primary");
    const InputActionId secondary = InputActionId::FromString("secondary");
    InputActionMap map;
    map.Bind(primary, Key::Space);
    map.Bind(secondary, Key::H);
    PhysicalInputFrame physical;
    physical.keys[static_cast<std::size_t>(Key::Space)] = {true, true, false};
    physical.keys[static_cast<std::size_t>(Key::H)] = {false, false, true};
    const auto frame = map.Evaluate(physical);
    CHECK(frame.Action(primary).held);
    CHECK(frame.Action(primary).pressed);
    CHECK_FALSE(frame.Action(primary).released);
    CHECK_FALSE(frame.Action(secondary).held);
    CHECK_FALSE(frame.Action(secondary).pressed);
    CHECK(frame.Action(secondary).released);
}

TEST_CASE("action and axis identity ignore diagnostic names") {
    const InputActionId actionA{42, "first"};
    const InputActionId actionB{42, "second"};
    const InputActionId otherAction{43, "first"};
    CHECK(actionA == actionB);
    CHECK_FALSE(actionA == otherAction);
    CHECK(std::hash<InputActionId>{}(actionA) ==
          std::hash<InputActionId>{}(actionB));

    const InputAxisId axisA{84, "horizontal"};
    const InputAxisId axisB{84, "vertical"};
    const InputAxisId otherAxis{85, "horizontal"};
    CHECK(axisA == axisB);
    CHECK_FALSE(axisA == otherAxis);
    CHECK(std::hash<InputAxisId>{}(axisA) ==
          std::hash<InputAxisId>{}(axisB));
}
