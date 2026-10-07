#pragma once

#include "RetroFPS/Pvp/Combat.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace fps::pvp {

// First-person feedback when the local player is hit. Presentation only: it
// reads the own combat state of snapshots and never changes the view the
// player controls, the commands it sends or the shot rays (see ClientView).
struct HitFeedbackSettings final {
    struct Flash final {
        std::array<float, 3> color{};
        float peakAlpha{};
        double seconds{};
    } flash;
    struct HpHighlight final {
        std::array<float, 3> color{};
        double seconds{};
    } hpHighlight;
    struct Direction final {
        float radiusPixels{}, arcRadians{}, dotPixels{};
        std::uint32_t dots{};
        std::array<float, 3> color{};
        double seconds{};
    } direction;
    struct Shake final {
        float pitchRadians{}, rollRadians{};
        double seconds{};
    } shake;
};

// Parses ui/hit_feedback.json (version 1); nullopt with an error otherwise.
[[nodiscard]] std::optional<HitFeedbackSettings> ParseHitFeedbackSettings(const std::string& text, std::string& error);

struct HitFeedbackSample final {
    float flashAlpha{};
    bool hpHighlighted{};
    // Bearing of the attacker relative to the view yaw (0 ahead, positive to
    // the right) and the indicator's alpha; none without a usable attacker.
    std::optional<float> directionRadians;
    float directionAlpha{};
    // Render-camera offsets only.
    float shakePitchRadians{}, shakeRollRadians{};
};

class LocalHitFeedback final {
public:
    void Reset() noexcept { *this = {}; }
    // The own combat state of the newest snapshot, with the own and the
    // attacker's positions from that same snapshot (none when the attacker is
    // not in it), at a monotonic presentation time. A new hit is a damage count
    // above the last one seen in the same life: duplicate or late snapshots
    // and a count that jumps several hits at once start one effect, and the
    // first state seen in a life (a join or a new life) never replays.
    // Death suppresses the effect.
    void Observe(const CombatState& own, LifeState lifeState, Engine::Math::Vec3 ownPosition,
                 std::optional<Engine::Math::Vec3> attackerPosition, double nowSeconds) noexcept;
    [[nodiscard]] HitFeedbackSample Sample(double nowSeconds, float viewYaw,
                                           const HitFeedbackSettings& settings) const noexcept;

private:
    bool seen_{};
    std::uint64_t lifeGeneration_{};
    std::uint32_t damageCount_{};
    bool active_{};
    double startSeconds_{};
    std::optional<float> attackerBearing_;
};

// The view the player controls and the camera that renders it. The mouse
// turns the view; Advance, SubmitAction and the HUD read Input(); the hit
// shake only reaches Camera().
struct ViewAngles final {
    float yaw{}, pitch{};
};
struct CameraAngles final {
    float pitch{}, yaw{}, roll{};
};

class ClientView final {
public:
    void Reset(float yaw, float pitch) noexcept;
    // One frame's relative mouse delta in counts; pitch clamps to the movement limit.
    void Turn(float deltaX, float deltaY) noexcept;
    // This frame's render-only offsets.
    void Present(const HitFeedbackSample& feedback) noexcept;
    [[nodiscard]] ViewAngles Input() const noexcept { return view_; }
    [[nodiscard]] CameraAngles Camera() const noexcept;

private:
    ViewAngles view_{};
    float shakePitch_{}, shakeRoll_{};
};

// One mouse count turns the view by this many radians.
inline constexpr float MouseRadiansPerCount = 0.0025F;

} // namespace fps::pvp
