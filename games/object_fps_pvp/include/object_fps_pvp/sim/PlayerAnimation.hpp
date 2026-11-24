#pragma once

#include "object_fps_pvp/sim/CombatRules.hpp"

#include "engine/math/linear/Vec3.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// Which clip plays where, and at what time, from what a player does: the game's rules of its animation (the second
// layer the user left to the game, v8-arch-gyo2-skeletal-animation-layers). Pure: no model types. The Match runs it
// every step for the hit shapes, a Client every frame for the picture, both in seconds of the World's steps (a
// Client on the step its remote players' state belongs to); neither is decision data (port declaration PV6), so the
// two need not agree bit for bit.

// The presentation contract (characters/player/presentation.json, version 1) and the clip lengths it needs.
struct AnimationRules final {
    double referenceSpeed = 3.0;
    double walkNativeSpeed = 0.93;
    double jogNativeSpeed = 6.0;
    // A clip's length times its native speed: the distance one gait cycle covers.
    double walkCycleDistance = 1.0;
    double jogCycleDistance = 4.5;
    double transitionSeconds = 0.1;
    double maxFrameDeltaSeconds = 0.1;
    double shotSeconds = 1.0 / 6.0;
    double jumpStartSeconds = 0.1;
    double jumpLandSeconds = 0.1;
    double hitSeconds = 1.0 / 3.0;
    float aimUpRadians = 1.4849F;
    float aimDownRadians = 1.5236F;
};

// The presentation file's members (the clip lengths come from the model: MakeAnimationRules).
struct PresentationFile final {
    double referenceSpeed = 0;
    double walkNativeSpeed = 0;
    double jogNativeSpeed = 0;
    double transitionSeconds = 0;
    double maxFrameDeltaSeconds = 0;
    double shotSeconds = 0;
    double jumpStartSeconds = 0;
    double jumpLandSeconds = 0;
    double hitSeconds = 0;
    float aimUpRadians = 0;
    float aimDownRadians = 0;
    std::string upperBodyRoot;
    // The third-person weapon: the node it hangs from and its fixed placement there.
    std::string weaponNode;
    GYO::Engine::Math::Vec3 weaponTranslation{};
    float weaponRotation[4]{0, 0, 0, 1};  // x y z w
    float weaponScale = 1.0F;
};
[[nodiscard]] std::optional<PresentationFile> ParsePresentation(std::string_view text, std::string& error);
[[nodiscard]] AnimationRules MakeAnimationRules(const PresentationFile& file, double walkClipSeconds, double jogClipSeconds);

enum class JumpPhase : std::uint8_t { Grounded = 0, Start = 1, Airborne = 2, Land = 3 };

// What one player is doing at a moment, as both ends know it.
struct AnimationInput final {
    double seconds = 0;  // the moment, on the caller's clock
    GYO::Engine::Math::Vec3 feet{};
    float yaw = 0;
    float pitch = 0;
    bool grounded = true;
    bool crouched = false;
    bool dead = false;
    double lifeSeconds = 0;  // when this life (or this death) began
    std::optional<double> shotSeconds;
    std::optional<double> reloadStart;
    std::optional<double> reloadEnd;
    std::optional<double> hitSeconds;
    HitPart hitPart = HitPart::None;
};

// The locomotion one player carries from moment to moment.
struct Locomotion final {
    bool started = false;
    double seconds = 0;
    GYO::Engine::Math::Vec3 feet{};
    // Gait cycles walked so far (walk, jog and crouched moves share them); going back counts down.
    double cycles = 0;
    double moveWeight = 0;  // 0 standing, 1 moving (eased over transitionSeconds)
    double jogWeight = 0;   // the jog clip's share of the gait
    JumpPhase jump = JumpPhase::Grounded;
    double jumpSince = 0;
    bool grounded = true;
};

// Moves the locomotion to input.seconds. A step longer than maxFrameDeltaSeconds counts as that long; a planar jump
// longer than a stride in one step (a respawn, a correction) is not walked.
void Advance(Locomotion& state, const AnimationInput& input, const AnimationRules& rules) noexcept;

// What the lower and the upper body play.
enum class LowerClip : std::uint8_t { Locomotion = 0, JumpStart, JumpLoop, JumpLand, Death };
enum class UpperClip : std::uint8_t { Aim = 0, Shoot, Reload, Hit, HitHead };
struct ActionPose final {
    LowerClip lower = LowerClip::Locomotion;
    // JumpStart and JumpLand: the progress through the phase, 0 to 1 (its span maps onto the whole clip); JumpLoop and
    // Death: the seconds into the clip; Locomotion: unused (the gait's phase is the locomotion's).
    double lowerSeconds = 0;
    UpperClip upper = UpperClip::Aim;
    // The progress through the upper action, 0 to 1 (its span maps onto the whole clip).
    double upperProgress = 0;
    // The aim: -1 fully up, 0 neutral, 1 fully down.
    float aim = 0;
};
[[nodiscard]] ActionPose ResolveActions(const Locomotion& state, const AnimationInput& input, const AnimationRules& rules) noexcept;

} // namespace object_fps_pvp::sim
