#pragma once

#include "RetroFPS/Pvp/Movement.hpp"

#include <cstdint>

#include <span>

namespace fps::pvp {

enum class ShotHitKind { Miss, World, Player };
// Combat.hpp (which includes this header) defines HitPart; PlayerHitModel.hpp
// defines the shape.
enum class HitPart : std::uint8_t;
struct PlayerWorldHitShape;

struct ShotHit final {
    ShotHitKind kind{ShotHitKind::Miss};
    PlayerId targetId{};
    float distance{};
    // The part of the hit shape for a Player hit; Unspecified (0) otherwise.
    HitPart part{};
};

// One living target and its shapes at the judged tick.
struct ShotTarget final {
    PlayerId playerId{};
    std::span<const PlayerWorldHitShape> shapes;
};

// Current authority poses only. Positive pitch points down, matching the camera.
// Angles use movement validation/normalization; range must be finite and >= 0.
// Invalid query inputs throw invalid_argument. Arena and poses are domain-owned.
// A miss has targetId 0 and distance == range. Equal-distance world hits win;
// equal-distance player hits select the lowest PlayerId regardless of input order.
// The shooter's eye follows its stance (protocol-v8 §4). A target's nearest
// shape decides its distance; shapes of one target at an equal distance take
// the higher part, Head over Body over Leg (v8-product-hitbox-tie (a)).
// targets are the living ones; the shooter's own entry is skipped.
[[nodiscard]] ShotHit QueryShot(const Arena& arena, const StanceRules& rules, const PlayerState& shooter,
    float yaw, float pitch, std::span<const ShotTarget> targets, float range);
// The synthetic model's shapes (one stance capsule, Body) for every living
// player other than the shooter.
[[nodiscard]] ShotHit QueryShot(const Arena& arena, const StanceRules& rules, const PlayerState& shooter,
    float yaw, float pitch, std::span<const PlayerState> players, float range);
// DefaultStanceRules; for callers without Match-delivered rules (tests).
[[nodiscard]] ShotHit QueryShot(const Arena& arena, const PlayerState& shooter,
    float yaw, float pitch, std::span<const PlayerState> players, float range);

} // namespace fps::pvp
