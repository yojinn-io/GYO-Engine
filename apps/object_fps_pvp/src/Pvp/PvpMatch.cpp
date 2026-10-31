#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/WireCounter.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace fps::pvp {
namespace {
std::uint64_t FutureTick(std::uint64_t tick, std::uint64_t duration) {
    const auto deadline = WireCounterAdd(tick, duration);
    if (!deadline) throw std::overflow_error("Authority deadline exhausted");
    return *deadline;
}
}
PvpMatch::PvpMatch(Arena arena, const CombatRules& rules, const StanceRules& stance)
    : arena_(std::move(arena)), rules_(rules), stance_(stance) {
    std::string error;
    if (!arena_.Validate(error)) throw std::invalid_argument(error);
    if (!ValidShotDamageBounds(rules_.shotDamage, rules_.maximumShotDamage, rules_.maximumHp) ||
        !ValidPartDamage(rules_) ||
        rules_.magazineCapacity == 0 || !std::isfinite(rules_.shotRange) || rules_.shotRange < 0 ||
        rules_.maximumReferenceAge < std::chrono::nanoseconds::zero())
        throw std::invalid_argument("Invalid combat rules");
    if (!ValidStanceRules(arena_, stance_)) throw std::invalid_argument("Invalid stance rules");
}

bool PvpMatch::Join(PlayerId playerId, std::string& error) {
    error.clear();
    if (playerId == 0) { error = "invalid_player"; return false; }
    if (players_.contains(playerId)) return true;
    if (players_.size() >= MaxPlayers) { error = "match_full"; return false; }
    if (const auto* spawn = FindSpawn(playerId)) {
        Participant player;
        player.state = {playerId, spawn->position, Engine::Math::WrapRadians(spawn->yaw), 0, 0};
        player.combat.playerId = playerId;
        player.combat.hp = rules_.maximumHp;
        player.combat.magazineAmmo = rules_.magazineCapacity;
        players_.emplace(playerId, player);
        return true;
    }
    error = "spawn_blocked";
    return false;
}

bool PvpMatch::Leave(PlayerId playerId) { return players_.erase(playerId) != 0; }

bool PvpMatch::ValidInput(const PlayerInput& input) noexcept {
    if (input.playerId == 0 || input.movementEpoch == 0 || input.lifeGeneration == 0 || input.commands.empty() ||
        input.commands.size() > MaxPendingCommands)
        return false;
    std::uint64_t previous{};
    for (const auto& command : input.commands) {
        if (!ValidMovementCommand(command) || command.sequence <= previous) return false;
        previous = command.sequence;
    }
    return true;
}

InputAdmission PvpMatch::AdmitInput(const PlayerInput& input) const noexcept {
    const auto found = players_.find(input.playerId);
    if (found == players_.end()) return InputAdmission::UnknownPlayer;
    if (!ValidInput(input)) return InputAdmission::Malformed;
    const auto& participant = found->second;
    const auto epoch = participant.state.movementEpoch;
    if (input.movementEpoch != epoch)
        return input.movementEpoch < epoch ? InputAdmission::EpochOld : InputAdmission::EpochFuture;
    const auto life = participant.state.lifeGeneration;
    if (input.lifeGeneration != life)
        return input.lifeGeneration < life ? InputAdmission::LifeOld : InputAdmission::LifeFuture;
    const auto cursor = participant.state.lastResolvedCommand;
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue; // An irrevocably resolved step.
        if (command.sequence - cursor > MaxFutureCommands) return InputAdmission::BeyondWindow;
        const auto queued = participant.commands.find(command.sequence);
        if (queued != participant.commands.end() && queued->second != command) return InputAdmission::ConflictQueued;
    }
    return InputAdmission::Accepted;
}

bool PvpMatch::CanSubmitInput(const PlayerInput& input) const noexcept {
    return AdmitInput(input) == InputAdmission::Accepted;
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

ActionAdmission PvpMatch::CanSubmitActions(const ActionBatch& batch,
    std::span<const ShotRequest> staged, ActionId acknowledgedThrough) const {
    const auto found = players_.find(batch.playerId);
    if (found == players_.end()) return ActionAdmission::InvalidPlayer;
    if (batch.shots.empty() || batch.shots.size() > MaxActionBatch)
        return ActionAdmission::InvalidBatch;
    if (staged.size() > MaxActionWindow) return ActionAdmission::Full;
    const auto& player = found->second;
    if (!CanAcknowledgeActions(batch.playerId, acknowledgedThrough)) return ActionAdmission::InvalidBatch;
    const auto floor = Engine::Math::Max(player.retiredActionThrough, acknowledgedThrough);
    // This bounded candidate also reserves room for every eventual decision.
    // Validation must finish before the live ledger (or host staging) changes.
    std::map<ActionId, ShotRequest> candidate;
    for (const auto& [id, entry] : player.actions)
        if (id > floor) candidate.emplace(id, entry.request);
    std::map<ActionId, ShotRequest> incoming;
    const auto merge = [&](std::span<const ShotRequest> shots) {
        for (const auto& shot : shots) {
            if (shot.lifeGeneration == 0 ||
                (shot.kind != ActionKind::Shot && shot.kind != ActionKind::Reload) ||
                (shot.kind == ActionKind::Reload && (shot.yaw != 0 || shot.pitch != 0)) ||
                !ValidMovementCommand({shot.actionId, 0, 0, shot.yaw, shot.pitch}))
                return ActionAdmission::InvalidBatch;
            // Even retired IDs cannot make a self-conflicting batch well formed.
            // We compare supplied content only; retired historical content is gone.
            const auto [supplied, first] = incoming.try_emplace(shot.actionId, shot);
            if (!first && supplied->second != shot) return ActionAdmission::Conflict;
            if (shot.actionId <= player.retiredActionThrough) continue;
            const auto retained = player.actions.find(shot.actionId);
            if (retained != player.actions.end() && retained->second.request != shot)
                return ActionAdmission::Conflict;
            if (shot.actionId <= floor) continue;
            // Subtraction only after the floor check avoids R+32 overflow.
            if (shot.actionId - floor > MaxActionWindow)
                return ActionAdmission::OutsideWindow;
            const auto [entry, inserted] = candidate.try_emplace(shot.actionId, shot);
            if (!inserted && entry->second != shot) return ActionAdmission::Conflict;
            if (candidate.size() > MaxActionWindow) return ActionAdmission::Full;
        }
        return ActionAdmission::Accepted;
    };
    const auto stagingResult = merge(staged);
    return stagingResult == ActionAdmission::Accepted ? merge(batch.shots) : stagingResult;
}

ActionAdmission PvpMatch::SubmitActions(const ActionBatch& batch) {
    const auto admission = CanSubmitActions(batch);
    if (admission != ActionAdmission::Accepted) return admission;
    if (WireCounterExhausted(tick_))
        throw std::overflow_error("Authority tick exhausted");
    auto& player = players_.at(batch.playerId);
    for (const auto& shot : batch.shots) {
        if (shot.actionId > player.retiredActionThrough)
            player.actions.try_emplace(shot.actionId, ActionEntry{shot, tick_ + 1, {}});
    }
    return ActionAdmission::Accepted;
}

bool PvpMatch::CanAcknowledgeActions(PlayerId playerId, ActionId through) const noexcept {
    const auto found = players_.find(playerId);
    if (found == players_.end()) return false;
    const auto& player = found->second;
    if (through <= player.retiredActionThrough) return true;
    if (through - player.retiredActionThrough > MaxActionWindow) return false;
    auto cursor = player.retiredActionThrough;
    while (cursor < through) {
        const auto entry = player.actions.find(++cursor);
        if (entry == player.actions.end() || !entry->second.decision) return false;
    }
    return true;
}

bool PvpMatch::AcknowledgeActions(PlayerId playerId, ActionId through) {
    if (!CanAcknowledgeActions(playerId, through)) return false;
    auto& player = players_.at(playerId);
    if (through > player.retiredActionThrough) {
        player.actions.erase(player.actions.begin(), player.actions.upper_bound(through));
        player.retiredActionThrough = through;
    }
    return true;
}

std::optional<ActionResults> PvpMatch::GetActionResults(PlayerId playerId) const {
    const auto found = players_.find(playerId);
    if (found == players_.end()) return std::nullopt;
    ActionResults result{playerId, found->second.retiredActionThrough, {}};
    for (const auto& [id, entry] : found->second.actions) {
        static_cast<void>(id);
        if (entry.decision) result.decisions.push_back(*entry.decision);
    }
    return result;
}

std::optional<MovementQuality> PvpMatch::GetMovementQuality(PlayerId playerId) const {
    const auto found = players_.find(playerId);
    if (found == players_.end()) return std::nullopt;
    return found->second.quality;
}

bool PvpMatch::RequestMovementResync(PlayerId playerId) {
    const auto found = players_.find(playerId);
    if (found == players_.end() || found->second.movementResetScheduled) return false;
    // Like the fuses' decisions, it rotates at the next tick boundary; unlike
    // them, the reset cooldown does not hold it back.
    found->second.movementResetScheduled = true;
    found->second.movementResetReason = MovementResetReason::Resync;
    return true;
}

std::optional<ActionTiming> PvpMatch::GetActionTiming(PlayerId playerId, ActionId actionId) const {
    const auto found = players_.find(playerId);
    if (found == players_.end()) return std::nullopt;
    const auto entry = found->second.actions.find(actionId);
    if (entry == found->second.actions.end() || !entry->second.decision) return std::nullopt;
    return ActionTiming{entry->second.acceptedTick, entry->second.dueTick, entry->second.decision->resolvedTick,
        entry->second.held};
}

const SpawnPoint* SelectSpawn(const Arena& arena, const std::span<const Engine::Math::Vec3> living) {
    std::vector<Engine::Collision::VerticalCapsule> blockers;
    for (const auto& position : living) blockers.push_back({position, arena.bodyHeight, arena.radius});
    const SpawnPoint* selected = nullptr;
    double bestDistance = -1;
    for (const auto& spawn : arena.spawns) {
        const auto p = spawn.position;
        if (!CanPlaceCharacterBody({p, arena.bodyHeight, arena.radius}, arena.walls, blockers)) continue;
        double nearest = (std::numeric_limits<double>::max)();
        for (const auto& blocker : blockers) {
            // Float difference, then double squares: the established spawn distance.
            nearest = Engine::Math::Min(nearest,
                Engine::Math::LengthSquared(Engine::Math::ToVec3d(p - blocker.feet)));
        }
        // Strict comparison leaves equal-distance choices in content order.
        if (!selected || nearest > bestDistance) {
            selected = &spawn;
            bestDistance = nearest;
        }
    }
    return selected;
}

const SpawnPoint* PvpMatch::FindSpawn(PlayerId playerId) const {
    std::vector<Engine::Math::Vec3> living;
    for (const auto& [id, player] : players_) {
        if (id == playerId || player.state.lifeState == LifeState::Dead) continue;
        living.push_back(player.state.position);
    }
    return SelectSpawn(arena_, living);
}

void PvpMatch::Kill(Participant& player) {
    player.state.respawnTick = FutureTick(tick_, rules_.respawnTicks);
    player.state.lifeState = LifeState::Dead;
    player.state.lifeStateTick = tick_;
    player.combat.reloadActionId = 0;
    player.combat.reloadStartTick = 0;
    player.combat.reloadEndTick = 0;
}

void PvpMatch::ResolveLifeBoundaries() {
    // std::map order makes concurrent respawns deterministic: later players
    // see the live capsule placed by every earlier successful respawn.
    for (auto& [id, player] : players_) {
        if (player.state.lifeState == LifeState::Dead) {
            if (tick_ < player.state.respawnTick) continue;
            const auto* spawn = FindSpawn(id);
            if (!spawn) continue; // Keep the due deadline and retry next tick.
            if (WireCounterExhausted(player.state.lifeGeneration))
                throw std::overflow_error("Life generation exhausted");
            if (WireCounterExhausted(player.state.movementEpoch))
                throw std::overflow_error("Movement epoch exhausted");
            ++player.state.lifeGeneration;
            player.state.lifeState = LifeState::Alive;
            player.state.lifeStateTick = tick_;
            player.state.respawnTick = 0;
            player.state.position = spawn->position;
            player.state.yaw = Engine::Math::WrapRadians(spawn->yaw);
            player.state.pitch = 0;
            player.state.verticalVelocity = 0;
            player.state.grounded = true;
            player.state.crouched = false;
            player.state.sprinting = false;
            player.combat = CombatState{id};
            player.combat.hp = rules_.maximumHp;
            player.combat.magazineAmmo = rules_.magazineCapacity;
            player.combat.lifeGeneration = player.state.lifeGeneration;
            // Movement resets never clear the session's action ledger or ACK floor.
            ResetMovementEpoch(player, MovementResetReason::LifeRespawn);
        } else if (player.combat.reloadActionId != 0 && tick_ >= player.combat.reloadEndTick) {
            player.combat.magazineAmmo = rules_.magazineCapacity;
            player.combat.reloadActionId = 0;
            player.combat.reloadStartTick = 0;
            player.combat.reloadEndTick = 0;
        }
    }
}

bool PvpMatch::ActionDue(const Participant& player, const ActionEntry& entry) const noexcept {
    // pv7 §6, after this tick's movement: another epoch (E = 0 included) or
    // life, the claimed command S already resolved, or the lead bound W.
    const auto& shot = entry.request;
    return tick_ >= entry.acceptedTick && (shot.movementEpoch != player.state.movementEpoch ||
        shot.lifeGeneration != player.state.lifeGeneration ||
        player.state.lastResolvedCommand >= shot.commandSequence ||
        tick_ - entry.acceptedTick >= MaxShotStepLead);
}

bool PvpMatch::Decide(PlayerId playerId, Participant& player, ActionId actionId, ActionEntry& entry) {
    const auto& shot = entry.request;
    ShotDecision decision{actionId, tick_};
    decision.kind = shot.kind;
    decision.lifeGeneration = shot.lifeGeneration;
    const auto& age = entry.referenceAge;
    if (shot.lifeGeneration != player.state.lifeGeneration) {
        decision.rejection = shot.lifeGeneration < player.state.lifeGeneration ?
            ShotRejection::StaleLife : ShotRejection::InvalidLife;
    } else if (player.state.lifeState == LifeState::Dead) {
        decision.rejection = ShotRejection::Dead;
    } else if (!age || *age < std::chrono::nanoseconds::zero()) {
        decision.rejection = ShotRejection::InvalidReference;
    } else if (*age > rules_.maximumReferenceAge) {
        decision.rejection = ShotRejection::Expired;
    } else if (player.combat.reloadActionId != 0) {
        decision.rejection = ShotRejection::Reloading;
    } else if (shot.kind == ActionKind::Reload) {
        if (player.combat.magazineAmmo == rules_.magazineCapacity) {
            decision.rejection = ShotRejection::MagazineFull;
        } else {
            const auto endTick = FutureTick(tick_, rules_.reloadTicks);
            player.combat.reloadActionId = actionId;
            player.combat.reloadStartTick = tick_;
            player.combat.reloadEndTick = endTick;
            decision.accepted = true;
        }
    } else if (player.combat.magazineAmmo == 0) {
        decision.rejection = ShotRejection::EmptyMagazine;
    } else if (tick_ < player.combat.nextAllowedShotTick) {
        // Only the cooldown is missing: hold once, up to K ticks, then judge
        // everything again (pv7 §6, D-3; K = min(K_max, cooldown), D136).
        if (!entry.held && player.combat.nextAllowedShotTick - tick_ <= ShotHoldLimit(rules_.cooldownTicks)) {
            entry.held = true;
            entry.holdUntil = player.combat.nextAllowedShotTick;
            return false;
        }
        decision.rejection = ShotRejection::Cooldown;
    } else {
        const auto nextAllowedShotTick = FutureTick(tick_, rules_.cooldownTicks);
        // Build this view for each action: a preceding lethal hit has
        // already removed its target from the live hit population.
        std::vector<PlayerState> targets;
        for (const auto& [id, target] : players_) {
            static_cast<void>(id);
            if (target.state.lifeState == LifeState::Alive) targets.push_back(target.state);
        }
        const auto hit = QueryShot(arena_, stance_, player.state, shot.yaw, shot.pitch, targets, rules_.shotRange);
        --player.combat.magazineAmmo;
        player.combat.lastShotActionId = actionId;
        player.combat.lastShotTick = tick_;
        player.combat.nextAllowedShotTick = nextAllowedShotTick;
        decision.accepted = true;
        decision.hitKind = hit.kind;
        decision.targetId = hit.targetId;
        if (hit.kind == ShotHitKind::Player) {
            auto& target = players_.at(hit.targetId);
            decision.targetLifeGeneration = target.state.lifeGeneration;
            decision.hitPart = HitPart::Body; // v7.1: no hitbox yet (pv7 §7)
            decision.damage = ShotDamage(rules_, decision.hitPart, target.combat.hp);
            target.combat.hp -= decision.damage;
            ++target.combat.damageCount;
            target.combat.lastDamageTick = tick_;
            target.combat.lastAttackerId = playerId;
            target.combat.lastHitPart = HitPart::Body;
            if (target.combat.hp == 0) Kill(target);
        }
    }
    entry.decision = decision;
    return true;
}

void PvpMatch::ResolveActions(const ShotReferenceAge& referenceAge) {
    std::vector<std::tuple<std::uint64_t, PlayerId, ActionId>> order;
    std::map<PlayerId, std::vector<ActionId>> ascending;
    for (auto& [playerId, player] : players_) {
        for (auto& [actionId, entry] : player.actions) {
            if (entry.decision) continue;
            if (!entry.referenceMeasured) {
                // D-3 (a): measured once, at A (the first tick that sees the
                // action), and kept for the decision however late it comes.
                const auto& shot = entry.request;
                entry.referenceMeasured = true;
                entry.referenceAge = referenceAge && shot.observedAuthorityTick != 0 &&
                    shot.observedAuthorityTick < tick_ ? referenceAge(shot.observedAuthorityTick) : std::nullopt;
            }
            if (entry.dueTick == 0 && ActionDue(player, entry)) entry.dueTick = tick_;
            order.emplace_back(entry.acceptedTick, playerId, actionId);
            ascending[playerId].push_back(actionId); // std::map: ascending already
        }
    }
    // Across players (A, PlayerId, ActionId); one player's actions take its
    // slots in ascending ActionId, and one that cannot be decided yet (not due,
    // or held) keeps every larger one waiting (pv7 §6 "順序與上界").
    std::sort(order.begin(), order.end());
    std::map<PlayerId, std::size_t> next;
    std::set<PlayerId> waiting;
    for (const auto& [handoff, playerId, slot] : order) {
        static_cast<void>(handoff);
        static_cast<void>(slot);
        const auto actionId = ascending.at(playerId)[next[playerId]++];
        if (waiting.contains(playerId)) continue;
        auto& player = players_.at(playerId);
        auto& entry = player.actions.at(actionId);
        const bool ready = entry.dueTick != 0 && tick_ >= entry.holdUntil;
        if (!ready || !Decide(playerId, player, actionId, entry)) waiting.insert(playerId);
    }
}

std::uint32_t PvpMatch::ContiguousPending(const Participant& player) noexcept {
    auto sequence = player.state.lastResolvedCommand;
    std::uint32_t count{};
    while (!WireCounterExhausted(sequence) &&
           player.commands.contains(sequence + 1)) {
        ++sequence;
        ++count;
    }
    return count;
}

void PvpMatch::ResetMovementEpoch(Participant& player, MovementResetReason reason) {
    if (WireCounterExhausted(player.state.movementEpoch))
        throw std::overflow_error("Movement epoch exhausted");
    const auto cancelled = player.commands.size();
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
    if (reason == MovementResetReason::Starvation || reason == MovementResetReason::Backlog ||
        reason == MovementResetReason::Resync) ++player.quality.resets;
    if (reason == MovementResetReason::Resync) ++player.quality.resyncs;
    player.movementResetScheduled = false;
    player.movementResetReason = MovementResetReason::None;
    TraceMovement({.kind = MovementTraceKind::Reset, .playerId = player.state.playerId,
        .epoch = player.state.movementEpoch, .authorityTick = tick_,
        .count = reason == MovementResetReason::LifeRespawn ? cancelled : 0,
        .resetReason = reason, .lifeGeneration = player.state.lifeGeneration});
}

void PvpMatch::Tick(const Engine::Runtime::TickContext& tick, const ShotReferenceAge& referenceAge) {
    if (WireCounterExhausted(tick_) ||
        tick.tickId == 0 || tick.tickId != tick_ + 1 || !std::isfinite(tick.deltaSeconds) || tick.deltaSeconds <= 0 ||
        std::abs(tick.deltaSeconds - MovementTickSeconds) > 1.0e-12)
        throw std::invalid_argument("Invalid match tick");
    tick_ = tick.tickId;
    ResolveLifeBoundaries();
    for (auto& [id, player] : players_) {
        static_cast<void>(id);
        if (player.lastMovementResetTick == tick_) continue; // Successful respawn reset boundary.
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
        if (WireCounterExhausted(player.state.lastResolvedCommand)) {
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
            command.jumpRequested = false; // An edge never repeats in held/neutral substitution.
            if (player.missingInputTicks >= InputHoldTicks) {
                command.moveForward = command.moveRight = 0;
                command.sprintHeld = false;
                source = MovementInputSource::Neutral;
            } else {
                source = MovementInputSource::Held;
            }
            if (player.missingInputTicks < (std::numeric_limits<std::uint32_t>::max)())
                ++player.missingInputTicks;
        }
        command.sequence = sequence;
        ++player.quality.resolved;
        if (source != MovementInputSource::Actual) ++player.quality.substituted;
        player.state = StepMovement(arena_, stance_, player.state, command);
        const auto pending = ContiguousPending(player);
        player.state.contiguousPendingCommands = pending;
        TraceMovement({.kind = MovementTraceKind::Resolved, .playerId = id,
            .epoch = player.state.movementEpoch, .sequence = sequence, .authorityTick = tick_,
            .source = source, .queued = pending, .count = player.missingInputTicks,
            .lifeGeneration = player.state.lifeGeneration});
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
    ResolveActions(referenceAge);
}

void PvpMatch::Reset() noexcept { players_.clear(); tick_ = 0; }

WorldSnapshot PvpMatch::Snapshot() const {
    WorldSnapshot result{tick_, {}, {}};
    for (const auto& [id, participant] : players_) {
        static_cast<void>(id);
        auto state = participant.state;
        state.contiguousPendingCommands = ContiguousPending(participant);
        result.players.push_back(state);
        result.combat.push_back(participant.combat);
    }
    return result;
}

bool PvpMatch::ContainsPlayer(PlayerId playerId) const noexcept { return players_.contains(playerId); }

} // namespace fps::pvp
