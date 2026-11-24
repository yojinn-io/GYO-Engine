#include "object_fps_pvp/present/Controls.hpp"

#include "object_fps_pvp/present/HitFeedback.hpp"
#include "object_fps_pvp/sim/ViewLimits.hpp"

#include "engine/math/scalar/Angle.hpp"

#include <algorithm>

namespace object_fps_pvp::present {

void Controls::Face(const float yaw, const float pitch) noexcept {
    yaw_ = GYO::Engine::Math::WrapRadians(yaw);
    pitch_ = std::clamp(pitch, -sim::MaximumPitchRadians, sim::MaximumPitchRadians);
}

void Controls::Release() noexcept {
    captured_ = false;
    pendingFire_ = pendingJump_ = pendingReload_ = false;
}

void Controls::Apply(const ControlsFrame& frame) noexcept {
    if (!frame.focused) {
        Release();
    } else if (frame.escape) {
        Release();
    } else if (frame.tab) {
        if (captured_) {
            Release();
        } else {
            captured_ = true;
        }
    } else if (frame.click && frame.insideWindow && !captured_) {
        // The click that takes the pointer is not a shot.
        captured_ = true;
    } else if (captured_) {
        pendingFire_ = pendingFire_ || frame.click;
        pendingJump_ = pendingJump_ || frame.jump;
        pendingReload_ = pendingReload_ || frame.reload;
    }
    if (captured_) {
        yaw_ = GYO::Engine::Math::WrapRadians(yaw_ + frame.motion.x * MouseRadiansPerCount);
        pitch_ = std::clamp(pitch_ + frame.motion.y * MouseRadiansPerCount, -sim::MaximumPitchRadians, sim::MaximumPitchRadians);
    }
    held_ = frame;
}

StepInput Controls::Take() noexcept {
    StepInput input;
    input.present = true;
    input.yaw = yaw_;
    input.pitch = pitch_;
    if (captured_) {
        input.forward = (held_.forward ? 1.0F : 0.0F) - (held_.back ? 1.0F : 0.0F);
        input.right = (held_.right ? 1.0F : 0.0F) - (held_.left ? 1.0F : 0.0F);
        input.crouch = held_.crouch;
        input.fire = pendingFire_;
        input.jump = pendingJump_;
        input.reload = pendingReload_;
    }
    pendingFire_ = pendingJump_ = pendingReload_ = false;
    return input;
}

} // namespace object_fps_pvp::present
