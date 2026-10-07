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

// Sole gameplay authority. No player controller, camera, connection or clock.
// The spawn a joining or respawning player takes (pv6 contract §5): among the
// spawns where a body fits clear of the walls and of the given living players,
// the one farthest from the nearest of them, in content order on ties; the first
// placeable spawn when there is nobody else. Null when none fits.
[[nodiscard]] const SpawnPoint* SelectSpawn(const Arena& arena, std::span<const Engine::Math::Vec3> living);

class PvpMatch final {
public:
    explicit PvpMatch(Arena arena);
    [[nodiscard]] bool Join(PlayerId playerId, std::string& error);
    [[nodiscard]] bool Leave(PlayerId playerId);
    [[nodiscard]] bool SubmitInput(const PlayerInput& input);
    // Read-only ingress validation; the host does not mutate authority on I/O.
    [[nodiscard]] bool CanSubmitInput(const PlayerInput& input) const noexcept;
    [[nodiscard]] ActionAdmission SubmitActions(const ActionBatch& batch);
    // Host staging participates in the same capacity/immutability validation.
    [[nodiscard]] ActionAdmission CanSubmitActions(const ActionBatch& batch,
        std::span<const ShotRequest> staged = {}, ActionId acknowledgedThrough = 0) const;
    [[nodiscard]] bool CanAcknowledgeActions(PlayerId playerId, ActionId through) const noexcept;
    [[nodiscard]] bool AcknowledgeActions(PlayerId playerId, ActionId through);
    [[nodiscard]] std::optional<ActionResults> GetActionResults(PlayerId playerId) const;
    [[nodiscard]] std::optional<MovementQuality> GetMovementQuality(PlayerId playerId) const;
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
        std::uint64_t acceptedTick{};
        std::optional<ShotDecision> decision;
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
    void ResetMovementEpoch(Participant& player, MovementResetReason reason);
    void ResolveLifeBoundaries();
    [[nodiscard]] const SpawnPoint* FindSpawn(PlayerId playerId) const;
    void Kill(Participant& player);
    void ResolveActions(const ShotReferenceAge& referenceAge);
    Arena arena_;
    std::map<PlayerId, Participant> players_;
    std::uint64_t tick_{};
};

} // namespace fps::pvp
