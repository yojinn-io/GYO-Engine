#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace fps::pvp {
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
        player.state = {playerId, p, std::remainder(spawn.yaw, 2 * std::numbers::pi_v<float>), 0, 0};
        players_.emplace(playerId, player);
        return true;
    }
    error = "spawn_blocked";
    return false;
}

bool PvpMatch::Leave(PlayerId playerId) { return players_.erase(playerId) != 0; }

bool PvpMatch::ValidInput(const PlayerInput& input) noexcept {
    if (input.playerId == 0 || input.movementEpoch == 0 || input.commands.empty() ||
        input.commands.size() > MaxPendingCommands)
        return false;
    std::uint64_t previous{};
    for (const auto& command : input.commands) {
        if (!ValidMovementCommand(command) || command.sequence <= previous) return false;
        previous = command.sequence;
    }
    return true;
}

bool PvpMatch::CanSubmitInput(const PlayerInput& input) const noexcept {
    const auto found = players_.find(input.playerId);
    if (found == players_.end() || !ValidInput(input)) return false;
    const auto& participant = found->second;
    if (input.movementEpoch != participant.state.movementEpoch) return false;
    const auto cursor = participant.state.lastResolvedCommand;
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue; // An irrevocably resolved step.
        if (command.sequence - cursor > MaxFutureCommands) return false;
        const auto queued = participant.commands.find(command.sequence);
        if (queued != participant.commands.end() && queued->second != command) return false;
    }
    return true;
}

bool PvpMatch::SubmitInput(const PlayerInput& input) {
    if (!CanSubmitInput(input)) return false;
    auto& participant = players_.at(input.playerId);
    for (const auto& command : input.commands) {
        if (command.sequence > participant.state.lastResolvedCommand)
            participant.commands.try_emplace(command.sequence, command);
    }
    return true;
}

std::uint32_t PvpMatch::ContiguousPending(const Participant& player) noexcept {
    auto sequence = player.state.lastResolvedCommand;
    std::uint32_t count{};
    while (sequence < (std::numeric_limits<std::uint64_t>::max)() &&
           player.commands.contains(sequence + 1)) {
        ++sequence;
        ++count;
    }
    return count;
}

void PvpMatch::ResetMovementEpoch(Participant& player, MovementResetReason reason) {
    if (player.state.movementEpoch == (std::numeric_limits<std::uint64_t>::max)())
        throw std::overflow_error("Movement epoch exhausted");
    ++player.state.movementEpoch;
    player.state.lastResolvedCommand = 0;
    player.state.contiguousPendingCommands = 0;
    player.commands.clear();
    player.lastActualCommand = {};
    player.missingInputTicks = 0;
    player.started = false;
    player.backlogSamples.clear();
    player.backlogSum = 0;
    player.fallbackSamples.clear();
    player.fallbackCount = 0;
    player.lastMovementResetTick = tick_;
    player.movementResetScheduled = false;
    player.movementResetReason = MovementResetReason::None;
    TraceMovement({.kind = MovementTraceKind::Reset, .playerId = player.state.playerId,
        .epoch = player.state.movementEpoch, .authorityTick = tick_, .resetReason = reason});
}

void PvpMatch::Tick(const Engine::Runtime::TickContext& tick) {
    if (tick.tickId == 0 || tick.tickId != tick_ + 1 || !std::isfinite(tick.deltaSeconds) || tick.deltaSeconds <= 0 ||
        std::abs(tick.deltaSeconds - MovementTickSeconds) > 1.0e-12)
        throw std::invalid_argument("Invalid match tick");
    tick_ = tick.tickId;
    for (auto& [id, player] : players_) {
        static_cast<void>(id);
        if (player.movementResetScheduled) {
            // The decision belongs to the previous completed tick. Rotation
            // preserves the world clock and pose, but performs no movement.
            ResetMovementEpoch(player, player.movementResetReason);
            continue;
        }
        if (!player.started) {
            if (!player.commands.contains(1)) continue;
            player.started = true;
        }
        if (player.state.lastResolvedCommand == (std::numeric_limits<std::uint64_t>::max)()) {
            ResetMovementEpoch(player, MovementResetReason::SequenceExhausted);
            continue;
        }
        const auto sequence = player.state.lastResolvedCommand + 1;
        auto command = player.lastActualCommand;
        auto source = MovementInputSource::Actual;
        const auto actual = player.commands.find(sequence);
        if (actual != player.commands.end()) {
            command = actual->second;
            player.lastActualCommand = command;
            player.missingInputTicks = 0;
            player.commands.erase(actual);
        } else {
            if (player.missingInputTicks >= InputHoldTicks) {
                command.moveForward = command.moveRight = 0;
                source = MovementInputSource::Neutral;
            } else {
                source = MovementInputSource::Held;
            }
            if (player.missingInputTicks < (std::numeric_limits<std::uint32_t>::max)())
                ++player.missingInputTicks;
        }
        command.sequence = sequence;
        player.state = StepMovement(arena_, player.state, command);
        const auto pending = ContiguousPending(player);
        player.state.contiguousPendingCommands = pending;
        TraceMovement({.kind = MovementTraceKind::Resolved, .playerId = id,
            .epoch = player.state.movementEpoch, .sequence = sequence, .authorityTick = tick_,
            .source = source, .queued = pending, .count = player.missingInputTicks});
        player.backlogSamples.push_back(pending);
        player.backlogSum += pending;
        player.fallbackSamples.push_back(source != MovementInputSource::Actual);
        player.fallbackCount += source != MovementInputSource::Actual;
        if (player.backlogSamples.size() > MovementBacklogSampleTicks) {
            player.backlogSum -= player.backlogSamples.front();
            player.backlogSamples.pop_front();
            player.fallbackCount -= player.fallbackSamples.front();
            player.fallbackSamples.pop_front();
        }
        if (player.lastMovementResetTick == 0 ||
            tick_ - player.lastMovementResetTick >= MovementResetCooldownTicks) {
            // A running cursor can permanently outrun a recovered client even
            // when occasional commands arrive in time. Require a whole window
            // without usable lead, plus actual substitution, before rebasing.
            // All-Actual just-in-time traffic must never trigger this fuse.
            const bool starved = player.backlogSamples.size() == MovementBacklogSampleTicks &&
                player.backlogSum == 0 && player.fallbackCount > 0 && player.commands.empty();
            const bool backlog = player.backlogSamples.size() == MovementBacklogSampleTicks &&
                player.backlogSum >= MovementBacklogCommandSum;
            if (starved || backlog) {
                player.movementResetScheduled = true;
                player.movementResetReason = starved ? MovementResetReason::Starvation :
                    MovementResetReason::Backlog;
            }
        }
    }
}

void PvpMatch::Reset() noexcept { players_.clear(); tick_ = 0; }

WorldSnapshot PvpMatch::Snapshot() const {
    WorldSnapshot result{tick_, {}};
    for (const auto& [id, participant] : players_) {
        static_cast<void>(id);
        auto state = participant.state;
        state.contiguousPendingCommands = ContiguousPending(participant);
        result.players.push_back(state);
    }
    return result;
}

bool PvpMatch::ContainsPlayer(PlayerId playerId) const noexcept { return players_.contains(playerId); }

} // namespace fps::pvp
