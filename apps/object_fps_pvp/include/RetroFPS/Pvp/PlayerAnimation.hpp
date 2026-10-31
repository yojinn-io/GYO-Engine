#pragma once

#include "RetroFPS/Pvp/Movement.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace fps::pvp {

// The lower body's jump phase (protocol-v9 §4.3). Unspecified is invalid on
// the wire.
enum class JumpPhase : std::uint8_t { Unspecified = 0, Grounded = 1, Start = 2, Airborne = 3, Land = 4 };

// The Match's authoritative animation state of one player (protocol-v9 §4.3,
// §4.4). These quantized values are the authority itself: the Match keeps no
// finer state, so a decision, a snapshot and a Client read the same values.
struct PlayerAnimation final {
    PlayerId playerId{};
    std::uint64_t lifeGeneration{};
    // Unwrapped gait cycles x 65536, modulo 2^32; walk, jog, crouch_move and
    // sprint share it, and it decreases while moving backward.
    std::uint32_t gaitPhaseQ16{};
    std::uint32_t moveWeightPermille{}, jogWeightPermille{};
    JumpPhase jumpPhase{JumpPhase::Grounded};
    // The zero point of the phase's clip time, not the tick the change was seen.
    std::uint64_t jumpPhaseTick{};
    std::uint32_t sprintWeightPermille{}; // always 0 before sprint presentation
    bool operator==(const PlayerAnimation&) const = default;
};

// What the animation rules are made from: the presentation contract's values
// and the authored lengths of the walk and jog clips.
struct PlayerAnimationSource final {
    double referenceSpeed{}, maxFrameDeltaSeconds{};
    double walkNativeSpeed{}, jogNativeSpeed{};
    double walkClipSeconds{}, jogClipSeconds{};
    double transitionSeconds{}, jumpStartSeconds{}, jumpLandSeconds{};
};

struct PlayerAnimationRules final {
    double walkNativeSpeed{}, jogNativeSpeed{};
    // The distance one gait cycle covers at each clip's native speed.
    double walkCycleDistance{}, jogCycleDistance{};
    // A larger planar step in one tick is a teleport, never walked.
    double teleportDistance{};
    std::uint32_t transitionTicks{}, jumpStartTicks{}, jumpLandTicks{};
    bool operator==(const PlayerAnimationRules&) const = default;
};

// Rules for callers without the product content: unit tests and the authority
// digest, whose results then do not move with the player clips. Never the
// product's rules: the Match passes LoadPlayerAnimationRules' result.
inline constexpr PlayerAnimationRules SyntheticPlayerAnimationRules{
    .walkNativeSpeed = 1.0, .jogNativeSpeed = 6.0, .walkCycleDistance = 1.0, .jogCycleDistance = 4.5,
    .teleportDistance = 0.3, .transitionTicks = 6, .jumpStartTicks = 6, .jumpLandTicks = 6};

// The one derivation of the rules, shared by the Match and the Client. Every
// span must be a whole number of ticks (protocol-v9 §4.4).
[[nodiscard]] std::optional<PlayerAnimationRules> MakePlayerAnimationRules(const PlayerAnimationSource& source,
                                                                           std::string& error);

// The walk to jog blend at a planar speed: the exact ratio in [0, 1], and its
// quantized authority value.
[[nodiscard]] double PlayerJogRatio(const PlayerAnimationRules& rules, double planarSpeed) noexcept;
[[nodiscard]] std::uint32_t PlayerJogWeightPermille(const PlayerAnimationRules& rules, double planarSpeed) noexcept;
// The distance one gait cycle covers at a jog ratio.
[[nodiscard]] double PlayerCycleDistance(const PlayerAnimationRules& rules, double jogRatio) noexcept;

// The single-message checks of protocol-v9 §4.5, shared by every reader: one
// value per player, in any order, of the player's life; weights <= 1000 and no sprint
// weight yet; a valid phase whose zero point is not after the snapshot tick;
// and, for a living player, Start/Airborne exactly when not grounded.
[[nodiscard]] bool ValidPlayerAnimations(std::span<const PlayerState> players,
                                         std::span<const PlayerAnimation> animation, std::uint64_t tick) noexcept;

// One authority tick of one player (protocol-v9 §4.4). `previous` is this
// state machine's value at the tick before, `before` the player's authority
// state then; both are absent for a player seen for the first time.
[[nodiscard]] PlayerAnimation AdvancePlayerAnimation(const PlayerAnimationRules& rules,
    const std::optional<PlayerAnimation>& previous, const PlayerState* before, const PlayerState& now,
    std::uint64_t tick);

} // namespace fps::pvp
