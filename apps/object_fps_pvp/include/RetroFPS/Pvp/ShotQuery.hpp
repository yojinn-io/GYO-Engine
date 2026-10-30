#pragma once

#include "RetroFPS/Pvp/Movement.hpp"

#include <span>

namespace fps::pvp {

enum class ShotHitKind { Miss, World, Player };

struct ShotHit final {
    ShotHitKind kind{ShotHitKind::Miss};
    PlayerId targetId{};
    float distance{};
};

// Current authority poses only. Positive pitch points down, matching the camera.
// Angles use movement validation/normalization; range must be finite and >= 0.
// Invalid query inputs throw invalid_argument. Arena and poses are domain-owned.
// A miss has targetId 0 and distance == range. Equal-distance world hits win;
// equal-distance player hits select the lowest PlayerId regardless of input order.
// The shooter's eye and every target capsule follow their stance (protocol-v8 §4).
[[nodiscard]] ShotHit QueryShot(const Arena& arena, const StanceRules& rules, const PlayerState& shooter,
    float yaw, float pitch, std::span<const PlayerState> players, float range);
// DefaultStanceRules; for callers without Match-delivered rules (tests).
[[nodiscard]] ShotHit QueryShot(const Arena& arena, const PlayerState& shooter,
    float yaw, float pitch, std::span<const PlayerState> players, float range);

} // namespace fps::pvp
