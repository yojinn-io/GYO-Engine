#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace fps::pvp {
namespace {
constexpr float kMaximumPitch = 89.0F * std::numbers::pi_v<float> / 180.0F;
constexpr std::uint32_t kInputTimeoutTicks = 15;
}

PvpMatch::PvpMatch(Arena arena) : arena_(std::move(arena)) {
    std::string error;
    if (!arena_.Validate(error)) throw std::invalid_argument(error);
}

bool PvpMatch::Join(PlayerId playerId, std::string& error) {
    error.clear();
    if (playerId == 0) { error = "invalid_player"; return false; }
    if (players_.contains(playerId)) return true;
    if (players_.size() >= 2) { error = "match_full"; return false; }
    std::vector<Engine::Collision::VerticalCapsule> blockers;
    for (const auto& [id, player] : players_) {
        static_cast<void>(id);
        const auto p = player.state.position;
        blockers.push_back({{p.x, p.y, p.z}, arena_.bodyHeight, arena_.radius});
    }
    for (const auto& spawn : arena_.spawns) {
        const auto p = spawn.position;
        if (!CanPlaceCharacterBody({{p.x, p.y, p.z}, arena_.bodyHeight, arena_.radius},
                                   arena_.walls, blockers)) continue;
        Participant player;
        player.state = {playerId, p, spawn.yaw, 0, 0};
        players_.emplace(playerId, player);
        return true;
    }
    error = "spawn_blocked";
    return false;
}

bool PvpMatch::Leave(PlayerId playerId) { return players_.erase(playerId) != 0; }

bool PvpMatch::ValidInput(const PlayerInput& input) noexcept {
    return input.playerId != 0 && input.sequence != 0 &&
        std::isfinite(input.moveForward) && std::abs(input.moveForward) <= 1 &&
        std::isfinite(input.moveRight) && std::abs(input.moveRight) <= 1 &&
        std::isfinite(input.yaw) && std::isfinite(input.pitch);
}

bool PvpMatch::SubmitInput(const PlayerInput& input) {
    const auto found = players_.find(input.playerId);
    if (found == players_.end() || !ValidInput(input)) return false;
    auto& participant = found->second;
    if (participant.hasInput && input.sequence <= participant.input.sequence) return false;
    participant.input = input;
    participant.inputAgeTicks = 0;
    participant.hasInput = true;
    return true;
}

void PvpMatch::Tick(const Engine::Runtime::TickContext& tick) {
    if (tick.tickId == 0 || tick.tickId != tick_ + 1 || !std::isfinite(tick.deltaSeconds) || tick.deltaSeconds <= 0 ||
        tick.deltaSeconds > 0.1) throw std::invalid_argument("Invalid match tick");
    tick_ = tick.tickId;
    for (auto& [id, player] : players_) {
        static_cast<void>(id);
        const bool active = player.hasInput && player.inputAgeTicks < kInputTimeoutTicks;
        if (active) {
            player.state.yaw = std::remainder(player.input.yaw, 2 * std::numbers::pi_v<float>);
            player.state.pitch = std::clamp(player.input.pitch, -kMaximumPitch, kMaximumPitch);
            player.state.lastInputSequence = player.input.sequence;
        }
        const auto displacement = ComputePlanarDisplacement(
            active ? player.input.moveForward : 0, active ? player.input.moveRight : 0,
            player.state.yaw, arena_.movementSpeed, static_cast<float>(tick.deltaSeconds));
        const auto p = player.state.position;
        player.state.position = MoveCharacterBody(
            {{p.x, p.y, p.z}, arena_.bodyHeight, arena_.radius},
            {displacement.x, 0, displacement.z}, arena_.walls, {}, true);
        if (player.inputAgeTicks < kInputTimeoutTicks) ++player.inputAgeTicks;
    }
}

void PvpMatch::Reset() noexcept { players_.clear(); tick_ = 0; }

WorldSnapshot PvpMatch::Snapshot() const {
    WorldSnapshot result{tick_, {}};
    for (const auto& [id, participant] : players_) {
        static_cast<void>(id);
        result.players.push_back(participant.state);
    }
    return result;
}

bool PvpMatch::ContainsPlayer(PlayerId playerId) const noexcept { return players_.contains(playerId); }

} // namespace fps::pvp
