#pragma once

#include "RetroFPS/Pvp/Arena.hpp"
#include "engine/runtime/FixedTickRuntime.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fps::pvp {

using PlayerId = std::uint64_t;

struct PlayerInput final {
    PlayerId playerId{};
    std::uint64_t sequence{};
    std::uint64_t clientTick{};
    float moveForward{};
    float moveRight{};
    float yaw{};
    float pitch{};
};

struct PlayerState final {
    PlayerId playerId{};
    Float3 position{}; // feet, in arena world units
    float yaw{};
    float pitch{};
    std::uint64_t lastInputSequence{};
};

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
        PlayerInput input;
        std::uint32_t inputAgeTicks{15};
        bool hasInput{};
    };
    Arena arena_;
    std::map<PlayerId, Participant> players_;
    std::uint64_t tick_{};
};

} // namespace fps::pvp
