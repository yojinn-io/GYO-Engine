// The Client's controls (present/Controls.hpp): the click or Tab that takes the pointer is not a shot; Tab, Escape
// or losing the focus gives it back; the keys make the step's input only while captured; a press waits until a step
// takes it, and goes to that one step only.
#include <doctest/doctest.h>

#include "object_fps_pvp/present/Controls.hpp"
#include "object_fps_pvp/present/HitFeedback.hpp"
#include "object_fps_pvp/sim/ViewLimits.hpp"

using namespace object_fps_pvp::present;

namespace {
ControlsFrame Focused() {
    ControlsFrame frame;
    frame.focused = true;
    frame.insideWindow = true;
    return frame;
}
} // namespace

TEST_CASE("PvP controls: the click that takes the pointer is not a shot; the next one is") {
    Controls controls;
    ControlsFrame click = Focused();
    click.click = true;
    controls.Apply(click);
    CHECK(controls.Captured());
    CHECK_FALSE(controls.Take().fire);
    controls.Apply(click);
    CHECK(controls.Take().fire);
}

TEST_CASE("PvP controls: Tab takes and gives back the pointer; Escape and losing the focus give it back") {
    Controls controls;
    ControlsFrame tab = Focused();
    tab.tab = true;
    controls.Apply(tab);
    CHECK(controls.Captured());
    controls.Apply(tab);
    CHECK_FALSE(controls.Captured());
    controls.Apply(tab);
    ControlsFrame escape = Focused();
    escape.escape = true;
    controls.Apply(escape);
    CHECK_FALSE(controls.Captured());
    controls.Apply(tab);
    ControlsFrame away = Focused();
    away.focused = false;
    controls.Apply(away);
    CHECK_FALSE(controls.Captured());
    // A click outside the window does not take it.
    ControlsFrame outside = Focused();
    outside.click = true;
    outside.insideWindow = false;
    controls.Apply(outside);
    CHECK_FALSE(controls.Captured());
}

TEST_CASE("PvP controls: keys move and the mouse turns only while captured") {
    Controls controls;
    controls.Face(1.0F, 0.0F);
    ControlsFrame keys = Focused();
    keys.forward = true;
    keys.right = true;
    keys.crouch = true;
    keys.motion = {100.0F, 40.0F};
    controls.Apply(keys);
    StepInput input = controls.Take();
    CHECK(input.present);
    CHECK(input.forward == 0.0F);
    CHECK(input.yaw == 1.0F);
    CHECK_FALSE(input.crouch);
    ControlsFrame tab = Focused();
    tab.tab = true;
    controls.Apply(tab);
    controls.Apply(keys);
    input = controls.Take();
    CHECK(input.forward == 1.0F);
    CHECK(input.right == 1.0F);
    CHECK(input.crouch);
    CHECK(input.yaw == doctest::Approx(1.0F + 100.0F * MouseRadiansPerCount));
    CHECK(input.pitch == doctest::Approx(40.0F * MouseRadiansPerCount));
    // The pitch stops short of straight down.
    ControlsFrame down = keys;
    down.motion = {0.0F, 100000.0F};
    controls.Apply(down);
    input = controls.Take();
    CHECK(input.pitch == object_fps_pvp::sim::MaximumPitchRadians);
}

TEST_CASE("PvP controls: the jump key held is carried with every step while captured, unlike the press") {
    Controls controls;
    ControlsFrame held = Focused();
    held.jumpHeld = true;
    // Not captured: not the player's.
    controls.Apply(held);
    CHECK_FALSE(controls.Take().jumpHeld);
    ControlsFrame tab = Focused();
    tab.tab = true;
    controls.Apply(tab);
    controls.Apply(held);
    // Held: every step gets it, however many steps a frame runs.
    CHECK(controls.Take().jumpHeld);
    CHECK(controls.Take().jumpHeld);
    controls.Apply(Focused());
    CHECK_FALSE(controls.Take().jumpHeld);
}

TEST_CASE("PvP controls: a press waits for a step and goes to that one step only") {
    Controls controls;
    ControlsFrame tab = Focused();
    tab.tab = true;
    controls.Apply(tab);
    ControlsFrame jump = Focused();
    jump.jump = true;
    jump.reload = true;
    // Pressed in a frame that runs no step, then two more frames without a step: the press is still there.
    controls.Apply(jump);
    controls.Apply(Focused());
    controls.Apply(Focused());
    const StepInput taken = controls.Take();
    CHECK(taken.jump);
    CHECK(taken.reload);
    // The next step (in the same frame or a later one) does not get it again.
    CHECK_FALSE(controls.Take().jump);
    controls.Apply(Focused());
    CHECK_FALSE(controls.Take().reload);
}
