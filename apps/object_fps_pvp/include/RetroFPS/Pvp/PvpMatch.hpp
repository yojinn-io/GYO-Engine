#pragma once

#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace fps::pvp {

enum class MovementResetReason;

struct WorldSnapshot final {
    std::uint64_t tick{};
    std::vector<PlayerState> players;
};

// Sole gameplay authority. No player controller, camera, connection or clock.
class PvpMatch final {
public:
    explicit PvpMatch(Arena arena);
    [[nodiscard]] bool Join(PlayerId playerId, std::string& error);
    [[nodiscard]] bool Leave(PlayerId playerId);
    [[nodiscard]] bool SubmitInput(const PlayerInput& input);
    // Read-only ingress validation; the host does not mutate authority on I/O.
    [[nodiscard]] bool CanSubmitInput(const PlayerInput& input) const noexcept;
    void Tick(const Engine::Runtime::TickContext& tick);
    void Reset() noexcept;
    [[nodiscard]] WorldSnapshot Snapshot() const;
    [[nodiscard]] bool ContainsPlayer(PlayerId playerId) const noexcept;
    [[nodiscard]] std::uint64_t TickCount() const noexcept { return tick_; }
    [[nodiscard]] const Arena& GetArena() const noexcept { return arena_; }
    [[nodiscard]] static bool ValidInput(const PlayerInput& input) noexcept;

private:
    struct Participant final {
        PlayerState state;
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
    };
    [[nodiscard]] static std::uint32_t ContiguousPending(const Participant& player) noexcept;
    void ResetMovementEpoch(Participant& player, MovementResetReason reason);
    Arena arena_;
    std::map<PlayerId, Participant> players_;
    std::uint64_t tick_{};
};

} // namespace fps::pvp
