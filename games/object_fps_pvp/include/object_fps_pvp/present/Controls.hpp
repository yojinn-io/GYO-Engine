#pragma once

#include "engine/math/linear/Vec2.hpp"

#include <cstdint>

namespace object_fps_pvp::present {

// The Client's controls: when the game takes the pointer, how a frame of keys and mouse becomes the player's input
// for its next step, and which press is a shot. Master's PointerCapture rules (the click or Tab that takes the
// pointer is not a shot; Tab again, Escape or losing the focus gives it back), on the frame's state instead of an
// event list (the input Module keeps the state; the order inside one frame is not needed by these rules).

// One frame of what the player did, as the input and window Modules keep it.
struct ControlsFrame final {
    bool focused = false;
    bool insideWindow = false;
    // Pressed this frame.
    bool tab = false;
    bool escape = false;
    bool click = false;
    bool jump = false;
    bool reload = false;
    bool interact = false;
    // Held.
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool crouch = false;
    bool jumpHeld = false;
    // The mouse's motion this frame, in counts (input's AccumulatedMouseMotion; only used while captured).
    GYO::Engine::Math::Vec2 motion{};
};

// What the step input is made of (the World's PlayerInput carries the same fields).
struct StepInput final {
    float forward = 0.0F;
    float right = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    bool present = false;
    // jump: pressed since the last step took the presses; jumpHeld: the key is down (the step's rules decide what a
    // held key does: sim::StepJumps).
    bool jump = false;
    bool jumpHeld = false;
    bool crouch = false;
    bool fire = false;
    bool reload = false;
    bool interact = false;
    // Not the player's: the Frame World's readiness (the game loaded and drawn), carried with the controls.
    bool ready = false;
};

class Controls final {
public:
    // The view starts where the player looks (its spawn's yaw).
    void Face(float yaw, float pitch) noexcept;

    // Applies one frame of what the player did: the pointer, the view, the held keys; the presses wait in the
    // controls until a step takes them.
    void Apply(const ControlsFrame& frame) noexcept;

    // The input of the step that runs now: the held keys and the view of the last frame, and the presses made since
    // the last step took them (each goes to exactly one step, however many frames ran no step; lightyear writes the
    // input of the step itself before the step, crates/inputs/inputs/src/client.rs).
    [[nodiscard]] StepInput Take() noexcept;

    [[nodiscard]] bool Captured() const noexcept { return captured_; }
    [[nodiscard]] float Yaw() const noexcept { return yaw_; }
    [[nodiscard]] float Pitch() const noexcept { return pitch_; }

private:
    void Release() noexcept;

    bool captured_ = false;
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    ControlsFrame held_{};
    bool pendingFire_ = false;
    bool pendingJump_ = false;
    bool pendingReload_ = false;
    bool pendingInteract_ = false;
};

} // namespace object_fps_pvp::present
