#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/WireCounter.hpp"
#include "engine/base/Assert.hpp"
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
// A row the caller knows to exist: every entity has every component column,
// and every connected entity has both input-source columns (entity-model
// §3.2). Its absence is a Programmer Error, never undefined behaviour.
template <typename T>
T& Row(std::vector<T>& column, EntityId id) {
    auto* row = FindEntity(column, id);
    GYO_ASSERT(row);
    return *row;
}
template <typename T>
const T& Row(const std::vector<T>& column, EntityId id) {
    const auto* row = FindEntity(column, id);
    GYO_ASSERT(row);
    return *row;
}
std::uint64_t FutureTick(std::uint64_t tick, std::uint64_t duration) {
    const auto deadline = WireCounterAdd(tick, duration);
    if (!deadline) throw std::overflow_error("Authority deadline exhausted");
    return *deadline;
}
}
PvpMatch::PvpMatch(Arena arena, const CombatRules& rules, const StanceRules& stance,
                   const PlayerAnimationRules& animation, std::shared_ptr<const PlayerHitModel> hitModel)
    : arena_(std::move(arena)), rules_(rules), stance_(stance), animation_(animation), hitModel_(std::move(hitModel)) {
    if (!hitModel_) throw std::invalid_argument("PvpMatch needs a hit model");
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
    if (FindEntity(connections_, playerId)) return true;
    if (connections_.size() >= MaxPlayers) { error = "match_full"; return false; }
    if (const auto* spawn = FindSpawn(playerId)) {
        PlayerState state{playerId, spawn->position, Engine::Math::WrapRadians(spawn->yaw), 0, 0};
        CombatState combat;
        combat.playerId = playerId;
        combat.hp = rules_.maximumHp;
        combat.magazineAmmo = rules_.magazineCapacity;
        // A joining player's first value: a reset at the current tick.
        AnimationTrack track{playerId, AdvancePlayerAnimation(animation_, std::nullopt, nullptr, state, tick_), state};
        InsertEntity(bodies_, state);
        InsertEntity(combats_, combat);
        InsertEntity(animations_, std::move(track));
        InsertEntity(actionLedgers_, ActionLedger{playerId});
        InsertEntity(connections_, Connection{playerId});
        return true;
    }
    error = "spawn_blocked";
    return false;
}

bool PvpMatch::Leave(PlayerId playerId) {
    // Only a connected entity can leave; a connected entity has every column.
    if (!FindEntity(connections_, playerId)) return false;
    return EraseEntity(playerId);
}

bool PvpMatch::EraseEntity(EntityId entityId) {
    const bool body = ::fps::pvp::EraseEntity(bodies_, entityId);
    const bool combat = ::fps::pvp::EraseEntity(combats_, entityId);
    const bool animation = ::fps::pvp::EraseEntity(animations_, entityId);
    GYO_ASSERT(body == combat && body == animation);
    // Input sources only a connected entity has: absent is fine.
    static_cast<void>(::fps::pvp::EraseEntity(actionLedgers_, entityId));
    static_cast<void>(::fps::pvp::EraseEntity(connections_, entityId));
    // Tick caches are rebuilt each tick, but never let one outlive its entity.
    static_cast<void>(::fps::pvp::EraseEntity(tickShapes_, entityId));
    static_cast<void>(::fps::pvp::EraseEntity(actionStartCombats_, entityId));
    return body;
}

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
    const auto* connection = FindEntity(connections_, input.playerId);
    if (!connection) return InputAdmission::UnknownPlayer;
    if (!ValidInput(input)) return InputAdmission::Malformed;
    const auto* body = FindEntity(bodies_, input.playerId);
    if (!body) return InputAdmission::UnknownPlayer; // noexcept: no GYO_ASSERT here
    const auto& state = *body;
    const auto epoch = state.movementEpoch;
    if (input.movementEpoch != epoch)
        return input.movementEpoch < epoch ? InputAdmission::EpochOld : InputAdmission::EpochFuture;
    const auto life = state.lifeGeneration;
    if (input.lifeGeneration != life)
        return input.lifeGeneration < life ? InputAdmission::LifeOld : InputAdmission::LifeFuture;
    const auto cursor = state.lastResolvedCommand;
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue; // An irrevocably resolved step.
        if (command.sequence - cursor > MaxFutureCommands) return InputAdmission::BeyondWindow;
        const auto queued = connection->commands.find(command.sequence);
        if (queued != connection->commands.end() && queued->second != command) return InputAdmission::ConflictQueued;
    }
    return InputAdmission::Accepted;
}

bool PvpMatch::CanSubmitInput(const PlayerInput& input) const noexcept {
    return AdmitInput(input) == InputAdmission::Accepted;
}

bool PvpMatch::SubmitInput(const PlayerInput& input) {
    if (!CanSubmitInput(input)) return false;
    const auto& state = Row(bodies_, input.playerId);
    auto& connection = Row(connections_, input.playerId);
    for (const auto& command : input.commands) {
        if (command.sequence > state.lastResolvedCommand)
            connection.commands.try_emplace(command.sequence, command);
    }
    return true;
}

ActionAdmission PvpMatch::CanSubmitActions(const ActionBatch& batch,
    std::span<const ShotRequest> staged, ActionId acknowledgedThrough) const {
    // A connected entity's batch (entity-model §3.3): an AI shooter has a
    // ledger too, but no batch of its own ever arrives.
    if (!FindEntity(connections_, batch.playerId)) return ActionAdmission::InvalidPlayer;
    const auto* ledger = &Row(actionLedgers_, batch.playerId);
    if (batch.shots.empty() || batch.shots.size() > MaxActionBatch)
        return ActionAdmission::InvalidBatch;
    if (staged.size() > MaxActionWindow) return ActionAdmission::Full;
    if (!CanAcknowledgeActions(batch.playerId, acknowledgedThrough)) return ActionAdmission::InvalidBatch;
    const auto floor = Engine::Math::Max(ledger->retiredThrough, acknowledgedThrough);
    // This bounded candidate also reserves room for every eventual decision.
    // Validation must finish before the live ledger (or host staging) changes.
    std::map<ActionId, ShotRequest> candidate;
    for (const auto& [id, entry] : ledger->actions)
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
            if (shot.actionId <= ledger->retiredThrough) continue;
            const auto retained = ledger->actions.find(shot.actionId);
            if (retained != ledger->actions.end() && retained->second.request != shot)
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
    auto& ledger = Row(actionLedgers_, batch.playerId);
    for (const auto& shot : batch.shots) {
        if (shot.actionId > ledger.retiredThrough)
            ledger.actions.try_emplace(shot.actionId, ActionEntry{shot, tick_ + 1, {}});
    }
    return ActionAdmission::Accepted;
}

bool PvpMatch::CanAcknowledgeActions(PlayerId playerId, ActionId through) const noexcept {
    if (!FindEntity(connections_, playerId)) return false;
    const auto* ledger = &Row(actionLedgers_, playerId);
    if (through <= ledger->retiredThrough) return true;
    if (through - ledger->retiredThrough > MaxActionWindow) return false;
    auto cursor = ledger->retiredThrough;
    while (cursor < through) {
        const auto entry = ledger->actions.find(++cursor);
        if (entry == ledger->actions.end() || !entry->second.decision) return false;
    }
    return true;
}

bool PvpMatch::AcknowledgeActions(PlayerId playerId, ActionId through) {
    if (!CanAcknowledgeActions(playerId, through)) return false;
    auto& ledger = Row(actionLedgers_, playerId);
    if (through > ledger.retiredThrough) {
        ledger.actions.erase(ledger.actions.begin(), ledger.actions.upper_bound(through));
        ledger.retiredThrough = through;
    }
    return true;
}

std::optional<ActionResults> PvpMatch::GetActionResults(PlayerId playerId) const {
    if (!FindEntity(connections_, playerId)) return std::nullopt;
    const auto* ledger = &Row(actionLedgers_, playerId);
    ActionResults result{playerId, ledger->retiredThrough, {}};
    for (const auto& [id, entry] : ledger->actions) {
        static_cast<void>(id);
        if (entry.decision) result.decisions.push_back(*entry.decision);
    }
    return result;
}

std::optional<MovementQuality> PvpMatch::GetMovementQuality(PlayerId playerId) const {
    const auto* connection = FindEntity(connections_, playerId);
    if (!connection) return std::nullopt;
    return connection->quality;
}

bool PvpMatch::RequestMovementResync(PlayerId playerId) {
    auto* connection = FindEntity(connections_, playerId);
    if (!connection || connection->movementResetScheduled) return false;
    // Like the fuses' decisions, it rotates at the next tick boundary; unlike
    // them, the reset cooldown does not hold it back.
    connection->movementResetScheduled = true;
    connection->movementResetReason = MovementResetReason::Resync;
    return true;
}

std::optional<ActionTiming> PvpMatch::GetActionTiming(PlayerId playerId, ActionId actionId) const {
    if (!FindEntity(connections_, playerId)) return std::nullopt;
    const auto* ledger = &Row(actionLedgers_, playerId);
    const auto entry = ledger->actions.find(actionId);
    if (entry == ledger->actions.end() || !entry->second.decision) return std::nullopt;
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
    for (const auto& state : bodies_) {
        if (state.playerId == playerId || state.lifeState == LifeState::Dead) continue;
        living.push_back(state.position);
    }
    return SelectSpawn(arena_, living);
}

void PvpMatch::Kill(PlayerState& state, CombatState& combat) {
    state.respawnTick = FutureTick(tick_, rules_.respawnTicks);
    state.lifeState = LifeState::Dead;
    state.lifeStateTick = tick_;
    combat.reloadActionId = 0;
    combat.reloadStartTick = 0;
    combat.reloadEndTick = 0;
}

void PvpMatch::ResolveLifeBoundaries() {
    // Ascending EntityId makes concurrent respawns deterministic: later
    // entities see the live capsule placed by every earlier successful respawn.
    GYO_ASSERT(combats_.size() == bodies_.size());
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        auto& state = bodies_[i];
        auto& combat = combats_[i];
        GYO_ASSERT(combat.playerId == state.playerId);
        const auto id = state.playerId;
        if (state.lifeState == LifeState::Dead) {
            if (tick_ < state.respawnTick) continue;
            const auto* spawn = FindSpawn(id);
            if (!spawn) continue; // Keep the due deadline and retry next tick.
            if (WireCounterExhausted(state.lifeGeneration))
                throw std::overflow_error("Life generation exhausted");
            if (WireCounterExhausted(state.movementEpoch))
                throw std::overflow_error("Movement epoch exhausted");
            ++state.lifeGeneration;
            state.lifeState = LifeState::Alive;
            state.lifeStateTick = tick_;
            state.respawnTick = 0;
            state.position = spawn->position;
            state.yaw = Engine::Math::WrapRadians(spawn->yaw);
            state.pitch = 0;
            state.verticalVelocity = 0;
            state.grounded = true;
            state.crouched = false;
            state.sprinting = false;
            combat = CombatState{id};
            combat.hp = rules_.maximumHp;
            combat.magazineAmmo = rules_.magazineCapacity;
            combat.lifeGeneration = state.lifeGeneration;
            // Movement resets never clear the session's action ledger or ACK floor.
            ResetMovementEpoch(state, Row(connections_, id), MovementResetReason::LifeRespawn);
        } else if (combat.reloadActionId != 0 && tick_ >= combat.reloadEndTick) {
            combat.magazineAmmo = rules_.magazineCapacity;
            combat.reloadActionId = 0;
            combat.reloadStartTick = 0;
            combat.reloadEndTick = 0;
        }
    }
}

bool PvpMatch::ActionDue(const PlayerState& state, const ActionEntry& entry) const noexcept {
    // pv7 §6, after this tick's movement: another epoch (E = 0 included) or
    // life, the claimed command S already resolved, or the lead bound W.
    const auto& shot = entry.request;
    return tick_ >= entry.acceptedTick && (shot.movementEpoch != state.movementEpoch ||
        shot.lifeGeneration != state.lifeGeneration ||
        state.lastResolvedCommand >= shot.commandSequence ||
        tick_ - entry.acceptedTick >= MaxShotStepLead);
}

bool PvpMatch::Decide(PlayerId playerId, PlayerState& state, CombatState& combat, ActionId actionId,
                      ActionEntry& entry) {
    const auto& shot = entry.request;
    ShotDecision decision{actionId, tick_};
    decision.kind = shot.kind;
    decision.lifeGeneration = shot.lifeGeneration;
    const auto& age = entry.referenceAge;
    if (shot.lifeGeneration != state.lifeGeneration) {
        decision.rejection = shot.lifeGeneration < state.lifeGeneration ?
            ShotRejection::StaleLife : ShotRejection::InvalidLife;
    } else if (state.lifeState == LifeState::Dead) {
        decision.rejection = ShotRejection::Dead;
    } else if (!age || *age < std::chrono::nanoseconds::zero()) {
        decision.rejection = ShotRejection::InvalidReference;
    } else if (*age > rules_.maximumReferenceAge) {
        decision.rejection = ShotRejection::Expired;
    } else if (combat.reloadActionId != 0) {
        decision.rejection = ShotRejection::Reloading;
    } else if (shot.kind == ActionKind::Reload) {
        if (combat.magazineAmmo == rules_.magazineCapacity) {
            decision.rejection = ShotRejection::MagazineFull;
        } else {
            const auto endTick = FutureTick(tick_, rules_.reloadTicks);
            combat.reloadActionId = actionId;
            combat.reloadStartTick = tick_;
            combat.reloadEndTick = endTick;
            decision.accepted = true;
        }
    } else if (combat.magazineAmmo == 0) {
        decision.rejection = ShotRejection::EmptyMagazine;
    } else if (tick_ < combat.nextAllowedShotTick) {
        // Only the cooldown is missing: hold once, up to K ticks, then judge
        // everything again (pv7 §6, D-3; K = min(K_max, cooldown), D136).
        if (!entry.held && combat.nextAllowedShotTick - tick_ <= ShotHoldLimit(rules_.cooldownTicks)) {
            entry.held = true;
            entry.holdUntil = combat.nextAllowedShotTick;
            return false;
        }
        decision.rejection = ShotRejection::Cooldown;
    } else {
        const auto nextAllowedShotTick = FutureTick(tick_, rules_.cooldownTicks);
        // Build this view for each action: a preceding lethal hit has
        // already removed its target from the live hit population. The
        // shapes are this tick's, placed once (v8-product-hitbox-tick-pose (a)).
        const auto& shapes = TickShapes();
        std::vector<ShotTarget> targets;
        for (const auto& body : bodies_) { // ascending EntityId: the tie-break order
            if (body.lifeState != LifeState::Alive) continue;
            const auto* placed = FindEntity(shapes, body.playerId);
            GYO_ASSERT(placed); // every living body was placed this tick
            targets.push_back({body.playerId, placed->shapes});
        }
        const auto hit = QueryShot(arena_, stance_, state, shot.yaw, shot.pitch, targets, rules_.shotRange);
        --combat.magazineAmmo;
        combat.lastShotActionId = actionId;
        combat.lastShotTick = tick_;
        combat.nextAllowedShotTick = nextAllowedShotTick;
        decision.accepted = true;
        decision.hitKind = hit.kind;
        decision.targetId = hit.targetId;
        if (hit.kind == ShotHitKind::Player) {
            auto& targetState = Row(bodies_, hit.targetId);
            auto& targetCombat = Row(combats_, hit.targetId);
            decision.targetLifeGeneration = targetState.lifeGeneration;
            decision.hitPart = hit.part; // the hit shape's part (v8-product-hitbox-shot)
            decision.damage = ShotDamage(rules_, decision.hitPart, targetCombat.hp);
            targetCombat.hp -= decision.damage;
            ++targetCombat.damageCount;
            targetCombat.lastDamageTick = tick_;
            targetCombat.lastAttackerId = playerId;
            targetCombat.lastHitPart = hit.part;
            if (targetCombat.hp == 0) Kill(targetState, targetCombat);
        }
    }
    entry.decision = decision;
    return true;
}

void PvpMatch::ResolveActions(const ShotReferenceAge& referenceAge) {
    actionStartCombats_ = combats_; // same order: ascending EntityId
    std::vector<std::tuple<std::uint64_t, PlayerId, ActionId>> order;
    std::map<PlayerId, std::vector<ActionId>> ascending;
    for (auto& ledger : actionLedgers_) { // ascending EntityId
        const auto playerId = ledger.id;
        const auto& state = Row(bodies_, playerId);
        for (auto& [actionId, entry] : ledger.actions) {
            if (entry.decision) continue;
            if (!entry.referenceMeasured) {
                // D-3 (a): measured once, at A (the first tick that sees the
                // action), and kept for the decision however late it comes.
                const auto& shot = entry.request;
                entry.referenceMeasured = true;
                entry.referenceAge = referenceAge && shot.observedAuthorityTick != 0 &&
                    shot.observedAuthorityTick < tick_ ? referenceAge(shot.observedAuthorityTick) : std::nullopt;
            }
            if (entry.dueTick == 0 && ActionDue(state, entry)) entry.dueTick = tick_;
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
        auto& state = Row(bodies_, playerId);
        auto& combat = Row(combats_, playerId);
        auto& entry = FindEntity(actionLedgers_, playerId)->actions.at(actionId);
        const bool ready = entry.dueTick != 0 && tick_ >= entry.holdUntil;
        if (!ready || !Decide(playerId, state, combat, actionId, entry)) waiting.insert(playerId);
    }
}

std::uint32_t PvpMatch::ContiguousPending(const PlayerState& state, const Connection& connection) noexcept {
    auto sequence = state.lastResolvedCommand;
    std::uint32_t count{};
    while (!WireCounterExhausted(sequence) &&
           connection.commands.contains(sequence + 1)) {
        ++sequence;
        ++count;
    }
    return count;
}

void PvpMatch::ResetMovementEpoch(PlayerState& state, Connection& connection, MovementResetReason reason) {
    if (WireCounterExhausted(state.movementEpoch))
        throw std::overflow_error("Movement epoch exhausted");
    const auto cancelled = connection.commands.size();
    ++state.movementEpoch;
    state.lastResolvedCommand = 0;
    state.contiguousPendingCommands = 0;
    connection.commands.clear();
    connection.lastActualCommand = {};
    connection.missingInputTicks = 0;
    connection.started = false;
    connection.backlogSamples.clear();
    connection.backlogSum = 0;
    connection.fallbackSamples.clear();
    connection.fallbackCount = 0;
    connection.lastMovementResetTick = tick_;
    if (reason == MovementResetReason::Starvation || reason == MovementResetReason::Backlog ||
        reason == MovementResetReason::Resync) ++connection.quality.resets;
    if (reason == MovementResetReason::Resync) ++connection.quality.resyncs;
    connection.movementResetScheduled = false;
    connection.movementResetReason = MovementResetReason::None;
    TraceMovement({.kind = MovementTraceKind::Reset, .playerId = state.playerId,
        .epoch = state.movementEpoch, .authorityTick = tick_,
        .count = reason == MovementResetReason::LifeRespawn ? cancelled : 0,
        .resetReason = reason, .lifeGeneration = state.lifeGeneration});
}

void PvpMatch::Tick(const Engine::Runtime::TickContext& tick, const ShotReferenceAge& referenceAge) {
    if (WireCounterExhausted(tick_) ||
        tick.tickId == 0 || tick.tickId != tick_ + 1 || !std::isfinite(tick.deltaSeconds) || tick.deltaSeconds <= 0 ||
        std::abs(tick.deltaSeconds - MovementTickSeconds) > 1.0e-12)
        throw std::invalid_argument("Invalid match tick");
    tick_ = tick.tickId;
    ResolveLifeBoundaries();
    for (auto& state : bodies_) {
        const auto id = state.playerId;
        auto& connection = Row(connections_, id);
        if (connection.lastMovementResetTick == tick_) continue; // Successful respawn reset boundary.
        if (connection.movementResetScheduled) {
            // The decision belongs to the previous completed tick. Rotation
            // preserves the world clock and pose, but performs no movement.
            ResetMovementEpoch(state, connection, connection.movementResetReason);
            continue;
        }
        if (!connection.started) {
            if (!connection.commands.contains(1)) continue;
            connection.started = true;
        }
        if (WireCounterExhausted(state.lastResolvedCommand)) {
            ResetMovementEpoch(state, connection, MovementResetReason::SequenceExhausted);
            continue;
        }
        const auto sequence = state.lastResolvedCommand + 1;
        auto command = connection.lastActualCommand;
        auto source = MovementInputSource::Actual;
        const auto actual = connection.commands.find(sequence);
        if (actual != connection.commands.end()) {
            command = actual->second;
            connection.lastActualCommand = command;
            connection.missingInputTicks = 0;
            connection.commands.erase(actual);
        } else {
            command.jumpRequested = false; // An edge never repeats in held/neutral substitution.
            if (connection.missingInputTicks >= InputHoldTicks) {
                command.moveForward = command.moveRight = 0;
                command.sprintHeld = false;
                source = MovementInputSource::Neutral;
            } else {
                source = MovementInputSource::Held;
            }
            if (connection.missingInputTicks < (std::numeric_limits<std::uint32_t>::max)())
                ++connection.missingInputTicks;
        }
        command.sequence = sequence;
        ++connection.quality.resolved;
        if (source != MovementInputSource::Actual) ++connection.quality.substituted;
        state = StepMovement(arena_, stance_, state, command);
        const auto pending = ContiguousPending(state, connection);
        state.contiguousPendingCommands = pending;
        TraceMovement({.kind = MovementTraceKind::Resolved, .playerId = id,
            .epoch = state.movementEpoch, .sequence = sequence, .authorityTick = tick_,
            .source = source, .queued = pending, .count = connection.missingInputTicks,
            .lifeGeneration = state.lifeGeneration});
        connection.backlogSamples.push_back(pending);
        connection.backlogSum += pending;
        connection.fallbackSamples.push_back(source != MovementInputSource::Actual);
        connection.fallbackCount += source != MovementInputSource::Actual;
        if (connection.backlogSamples.size() > MovementBacklogSampleTicks) {
            connection.backlogSum -= connection.backlogSamples.front();
            connection.backlogSamples.pop_front();
            connection.fallbackCount -= connection.fallbackSamples.front();
            connection.fallbackSamples.pop_front();
        }
        if (connection.lastMovementResetTick == 0 ||
            tick_ - connection.lastMovementResetTick >= MovementResetCooldownTicks) {
            // A running cursor can permanently outrun a recovered client even
            // when occasional commands arrive in time. Require a whole window
            // without usable lead, plus actual substitution, before rebasing.
            // All-Actual just-in-time traffic must never trigger this fuse.
            const bool starved = connection.backlogSamples.size() == MovementBacklogSampleTicks &&
                connection.backlogSum == 0 && connection.fallbackCount > 0 && connection.commands.empty();
            const bool backlog = connection.backlogSamples.size() == MovementBacklogSampleTicks &&
                connection.backlogSum >= MovementBacklogCommandSum;
            if (starved || backlog) {
                connection.movementResetScheduled = true;
                connection.movementResetReason = starved ? MovementResetReason::Starvation :
                    MovementResetReason::Backlog;
            }
        }
    }
    ResolveActions(referenceAge);
    // Every entity once a tick, after movement, life boundaries and shots,
    // so a respawn or a skipped movement tick cannot be missed (A1 review S1).
    GYO_ASSERT(animations_.size() == bodies_.size());
    for (std::size_t i = 0; i < animations_.size(); ++i) {
        GYO_ASSERT(animations_[i].id == bodies_[i].playerId);
        AdvanceAnimation(animations_[i], bodies_[i]);
    }
}

void PvpMatch::AdvanceAnimation(AnimationTrack& track, const PlayerState& state) {
    track.value = AdvancePlayerAnimation(animation_, track.value, &track.before, state, tick_);
    track.before = state;
}

const std::vector<PvpMatch::EntityShapes>& PvpMatch::TickShapes() {
    if (tickShapesTick_ == tick_) return tickShapes_;
    tickShapes_.clear();
    GYO_ASSERT(animations_.size() == bodies_.size());
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        const auto& state = bodies_[i];
        const auto& track = animations_[i];
        GYO_ASSERT(track.id == state.playerId);
        if (state.lifeState != LifeState::Alive) continue;
        // The animation of this tick before the tick ends advances it: the
        // same pure step on the same values, not written back. It reads
        // movement and life only, so for a target still alive at the end of
        // the tick it equals the value the snapshot of this tick carries.
        const auto animation = AdvancePlayerAnimation(animation_, track.value, &track.before, state, tick_);
        const auto* actionStart = FindEntity(actionStartCombats_, state.playerId);
        GYO_ASSERT(actionStart);
        // bodies_ is ascending, so appending keeps tickShapes_ ascending.
        auto& placed = tickShapes_.emplace_back(EntityShapes{state.playerId, {}});
        hitModel_->Shapes({arena_, stance_, state, *actionStart, animation, tick_}, placed.shapes);
    }
    // A cache is a column too (entity-model E-8): rebuilt whole, ascending, no stale rows.
    GYO_ASSERT(StrictlyAscending(std::span<const EntityShapes>(tickShapes_)));
    tickShapesTick_ = tick_;
    return tickShapes_;
}

void PvpMatch::Reset() noexcept {
    bodies_.clear();
    combats_.clear();
    animations_.clear();
    actionLedgers_.clear();
    connections_.clear();
    tick_ = 0;
    tickShapesTick_ = 0;
    tickShapes_.clear();
    actionStartCombats_.clear();
}

void RequireValidAnimation(const WorldSnapshot& snapshot) {
    GYO_ASSERT(ValidPlayerAnimations(snapshot.players, snapshot.animation, snapshot.tick));
}

void RequireEntityOrder(const WorldSnapshot& snapshot) {
    GYO_ASSERT(StrictlyAscending(std::span<const PlayerState>(snapshot.players)));
    GYO_ASSERT(snapshot.combat.size() == snapshot.players.size());
    GYO_ASSERT(snapshot.animation.size() == snapshot.players.size());
    for (std::size_t i = 0; i < snapshot.players.size(); ++i) {
        GYO_ASSERT(snapshot.combat[i].playerId == snapshot.players[i].playerId);
        GYO_ASSERT(snapshot.animation[i].playerId == snapshot.players[i].playerId);
    }
}

void RequireValidSnapshot(const WorldSnapshot& snapshot) {
    RequireEntityOrder(snapshot);
    RequireValidAnimation(snapshot);
}

bool EraseEntity(WorldSnapshot& snapshot, PlayerId entityId) {
    const auto players = std::erase_if(snapshot.players, [&](const auto& p) { return p.playerId == entityId; });
    const auto combat = std::erase_if(snapshot.combat, [&](const auto& c) { return c.playerId == entityId; });
    const auto animation = std::erase_if(snapshot.animation, [&](const auto& a) { return a.playerId == entityId; });
    GYO_ASSERT(players == combat && players == animation && players <= 1);
    return players == 1;
}

WorldSnapshot PvpMatch::Snapshot() const {
    WorldSnapshot result{tick_, {}, {}};
    GYO_ASSERT(combats_.size() == bodies_.size() && animations_.size() == bodies_.size());
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        auto state = bodies_[i];
        GYO_ASSERT(combats_[i].playerId == state.playerId && animations_[i].id == state.playerId);
        state.contiguousPendingCommands = ContiguousPending(state, Row(connections_, state.playerId));
        result.players.push_back(state);
        result.combat.push_back(combats_[i]);
        result.animation.push_back(animations_[i].value);
    }
    RequireEntityOrder(result);
    RequireValidAnimation(result);
    return result;
}

bool PvpMatch::ContainsPlayer(PlayerId playerId) const noexcept { return FindEntity(connections_, playerId) != nullptr; }

} // namespace fps::pvp
