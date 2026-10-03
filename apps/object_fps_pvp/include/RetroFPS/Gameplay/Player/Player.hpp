#pragma once

#include "RetroFPS/World/GroundPoint.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace fps {

class PlayerController;

class Player final {
public:
    Player() noexcept = default;

    [[nodiscard]] GroundPoint GetPositionXZ() const noexcept;
    [[nodiscard]] float GetFeetY() const noexcept { return feetY_; }
    [[nodiscard]] float GetVerticalVelocity() const noexcept { return verticalVelocity_; }
    [[nodiscard]] bool IsGrounded() const noexcept { return grounded_; }
    [[nodiscard]] Engine::Math::Vec3 GetEyePosition(float eyeHeight) const noexcept {
        return {positionXZ_.x, feetY_ + eyeHeight, positionXZ_.z};
    }
    [[nodiscard]] float GetYawRadians() const noexcept;
    [[nodiscard]] float GetPitchRadians() const noexcept;
    [[nodiscard]] float GetRecoilDegrees() const noexcept;

private:
    friend class PlayerController;

    void Reset(GroundPoint spawnPosition, float yawRadians, float pitchRadians) noexcept;
    void SetPositionXZ(GroundPoint position) noexcept;
    void SetLookAngles(float yawRadians, float pitchRadians) noexcept;
    void SetRecoilPitchRadians(float recoilPitchRadians) noexcept;

    [[nodiscard]] float GetAimPitchRadians() const noexcept;

    GroundPoint positionXZ_{};
    float feetY_ = 0.0f;
    float verticalVelocity_ = 0.0f;
    bool grounded_ = true;
    float yawRadians_ = 0.0f;
    float pitchRadians_ = 0.0f;
    float recoilPitchRadians_ = 0.0f;
};

} // namespace fps
