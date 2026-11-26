#pragma once

#include "object_fps_pvp/sim/LifeState.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace object_fps_pvp::present {

// First-person feedback when the local player is hit. Presentation only: it
// reads the own player's hit record and never changes the view the player
// controls, the input it sends or the shot rays (see ClientView).
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
    // The hit marker: an X around the crosshair when the own shot hits a player, four strokes from gapPixels to
    // gapPixels + lengthPixels out along the diagonals, thicknessPixels wide; headColor for a head.
    struct HitMarker final {
        float gapPixels{}, lengthPixels{}, thicknessPixels{};
        std::array<float, 3> color{};
        std::array<float, 3> headColor{};
        double seconds{};
    } hitMarker;
};

// Parses ui/hit_feedback.json (version 2); nullopt with an error otherwise.
[[nodiscard]] std::optional<HitFeedbackSettings> ParseHitFeedbackSettings(const std::string& text, std::string& error);

// The hits a player took in its current life: which life it is, and how many
// hits it has taken in it. A new life starts again from no hits.
struct LifeHitRecord final {
    std::uint64_t lifeGeneration{1};
    std::uint32_t damageCount{};
};

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

// What one Observe decided about new hits (the reason is this owner's own
// judgement, so a missing flash can be located in a log).
enum class HitFeedbackOutcome : std::uint8_t {
    None,              // no new hit in this state
    Played,            // new hits started the effect
    SuppressedDead,    // new hits, but the player is dead: no effect
    SuppressedNewLife, // the first state of a life (a join or a respawn) carries hits: never replayed
};
struct HitFeedbackObservation final {
    HitFeedbackOutcome outcome{HitFeedbackOutcome::None};
    std::uint32_t hits{}; // the damage count's increase this state brought (all of it for a new life)
};

class LocalHitFeedback final {
public:
    void Reset() noexcept { *this = {}; }
    // The own hit record of the newest state, with the own and the attacker's
    // positions from that same state (none when the attacker is not in it),
    // at a monotonic presentation time. A new hit is a damage count above the
    // last one seen in the same life: duplicate or late states and a count
    // that jumps several hits at once start one effect, and the first state
    // seen in a life (a join or a new life) never replays. Death suppresses
    // the effect.
    HitFeedbackObservation Observe(const LifeHitRecord& own, sim::LifeState lifeState,
                                   GYO::Engine::Math::Vec3 ownPosition,
                                   std::optional<GYO::Engine::Math::Vec3> attackerPosition,
                                   double nowSeconds) noexcept;
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

// The hit marker of the own shots (the HUD's X beside the crosshair, as in COD): the own player's last judged shot
// (the Match's ShotRecord), by its tick. A newer shot that hit a player shows it, in the head colour for a head; a
// newer miss does not take a shown marker away; the first record seen (a join, a scene) is not shown.
struct HitMarkerSample final {
    float alpha{};
    bool head{};
};
class HitMarker final {
public:
    void Observe(std::uint64_t shotTick, bool hitPlayer, bool head, double nowSeconds) noexcept;
    [[nodiscard]] HitMarkerSample Sample(double nowSeconds, const HitFeedbackSettings& settings) const noexcept;

private:
    bool seen_{};
    std::uint64_t tick_{};
    bool active_{};
    bool head_{};
    double startSeconds_{};
};

// The view the player controls and the camera that renders it. The mouse
// turns the view; the movement input, the shots and the HUD read Input(); the
// hit shake only reaches Camera().
struct ViewAngles final {
    float yaw{}, pitch{};
};
struct CameraAngles final {
    float pitch{}, yaw{}, roll{};
};

class ClientView final {
public:
    void Reset(float yaw, float pitch) noexcept;
    // One frame's relative mouse delta in counts; pitch clamps to the view limit.
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

} // namespace object_fps_pvp::present
