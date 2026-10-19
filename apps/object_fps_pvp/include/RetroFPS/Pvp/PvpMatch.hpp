#pragma once

#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/Combat.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace fps::pvp {

enum class MovementResetReason;

struct WorldSnapshot final {
    std::uint64_t tick{};
    std::vector<PlayerState> players;
    std::vector<CombatState> combat;
};

// Cumulative movement resolution counts of one participant, for the host's
// connection-quality judgement; never gameplay input.
struct MovementQuality final {
    std::uint64_t resolved{};
    // Steps resolved with a Held or Neutral substitute instead of the command.
    std::uint64_t substituted{};
    // Starvation and Backlog epoch resets (not respawns or exhaustion).
    std::uint64_t resets{};
};

// Why the authority would refuse a whole input window (Accepted admits it):
// the first failing check in AdmitInput's order decides the reason.
enum class InputAdmission : std::uint8_t {
    Accepted, UnknownPlayer, Malformed, EpochOld, EpochFuture, LifeOld, LifeFuture, BeyondWindow, ConflictQueued
};

// Sole gameplay authority. No player controller, camera, connection or clock.
// The spawn a joining or respawning player takes (pv6 contract §5): among the
// spawns where a body fits clear of the walls and of the given living players,
// the one farthest from the nearest of them, in content order on ties; the first
// placeable spawn when there is nobody else. Null when none fits.
[[nodiscard]] const SpawnPoint* SelectSpawn(const Arena& arena, std::span<const Engine::Math::Vec3> living);

// When the authority judged one action (pv7 §6; diagnostics only, never
// gameplay input): its handoff tick A, its first due tick D and the tick J it
// was decided at (J > D after a cooldown hold or while a smaller ActionId of
// the same player was still waiting).
struct ActionTiming final {
    std::uint64_t handoffTick{};
    std::uint64_t dueTick{};
    std::uint64_t resolvedTick{};
    bool held{};
};

class PvpMatch final {
public:
    // rules defaults to the product policy; tests and future weapon data may
    // pass others (v7.1 D137). Invalid rules throw invalid_argument.
    explicit PvpMatch(Arena arena, const CombatRules& rules = PvpCombatRules);
    [[nodiscard]] bool Join(PlayerId playerId, std::string& error);
    [[nodiscard]] bool Leave(PlayerId playerId);
    [[nodiscard]] bool SubmitInput(const PlayerInput& input);
    // Read-only ingress validation; the host does not mutate authority on I/O.
    [[nodiscard]] InputAdmission AdmitInput(const PlayerInput& input) const noexcept;
    [[nodiscard]] bool CanSubmitInput(const PlayerInput& input) const noexcept;
    [[nodiscard]] ActionAdmission SubmitActions(const ActionBatch& batch);
    // Host staging participates in the same capacity/immutability validation.
    [[nodiscard]] ActionAdmission CanSubmitActions(const ActionBatch& batch,
        std::span<const ShotRequest> staged = {}, ActionId acknowledgedThrough = 0) const;
    [[nodiscard]] bool CanAcknowledgeActions(PlayerId playerId, ActionId through) const noexcept;
    [[nodiscard]] bool AcknowledgeActions(PlayerId playerId, ActionId through);
    [[nodiscard]] std::optional<ActionResults> GetActionResults(PlayerId playerId) const;
    [[nodiscard]] std::optional<MovementQuality> GetMovementQuality(PlayerId playerId) const;
    // Timing of a decided action still held in the ledger; nullopt otherwise.
    [[nodiscard]] std::optional<ActionTiming> GetActionTiming(PlayerId playerId, ActionId actionId) const;
    [[nodiscard]] const CombatRules& Rules() const noexcept { return rules_; }
    void Tick(const Engine::Runtime::TickContext& tick, const ShotReferenceAge& referenceAge = {});
    void Reset() noexcept;
    [[nodiscard]] WorldSnapshot Snapshot() const;
    [[nodiscard]] bool ContainsPlayer(PlayerId playerId) const noexcept;
    [[nodiscard]] std::uint64_t TickCount() const noexcept { return tick_; }
    [[nodiscard]] const Arena& GetArena() const noexcept { return arena_; }
    [[nodiscard]] static bool ValidInput(const PlayerInput& input) noexcept;

private:
    struct ActionEntry final {
        ShotRequest request;
        std::uint64_t acceptedTick{}; // A, the handoff tick
        std::optional<ShotDecision> decision;
        // A1 (pv7 §6, D-3): the reference age measured once at A; the first
        // due tick D (0 before); a Shot's one cooldown hold, until holdUntil.
        bool referenceMeasured{};
        std::optional<std::chrono::nanoseconds> referenceAge;
        std::uint64_t dueTick{};
        std::uint64_t holdUntil{};
        bool held{};
    };
    struct Participant final {
        PlayerState state;
        CombatState combat;
        ActionId retiredActionThrough{};
        std::map<ActionId, ActionEntry> actions;
        std::map<std::uint64_t, MovementCommand> commands;
        MovementCommand lastActualCommand;
        std::uint32_t missingInputTicks{};
        bool started{};
        std::deque<std::uint32_t> backlogSamples;
        std::uint32_t backlogSum{};
        std::deque<bool> fallbackSamples;
        std::uint32_t fallbackCount{};
        std::uint64_t lastMovementResetTick{};
        bool movementResetScheduled{};
        MovementResetReason movementResetReason{};
        MovementQuality quality;
    };
    [[nodiscard]] static std::uint32_t ContiguousPending(const Participant& player) noexcept;
    [[nodiscard]] bool ActionDue(const Participant& player, const ActionEntry& entry) const noexcept;
    // Decides one due action now, or starts its cooldown hold (returns false).
    [[nodiscard]] bool Decide(PlayerId playerId, Participant& player, ActionId actionId, ActionEntry& entry);
    void ResetMovementEpoch(Participant& player, MovementResetReason reason);
    void ResolveLifeBoundaries();
    [[nodiscard]] const SpawnPoint* FindSpawn(PlayerId playerId) const;
    void Kill(Participant& player);
    void ResolveActions(const ShotReferenceAge& referenceAge);
    Arena arena_;
    CombatRules rules_;
    std::map<PlayerId, Participant> players_;
    std::uint64_t tick_{};
};

} // namespace fps::pvp
