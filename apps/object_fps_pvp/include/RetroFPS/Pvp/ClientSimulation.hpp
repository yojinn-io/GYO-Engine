#pragma once

#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/PredictionElapsedTime.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace fps::pvp {

// One frame's movement intent as sampled by the caller.
struct ClientInputSample final {
    float forward{};
    float right{};
    float yaw{};
    float pitch{};
    bool jump{};
    // False drops any pending jump request and suppresses this frame's jump edge.
    bool controls{true};
};

// The single client command-generation path shared by the application and the
// headless acceptance probes: authority observation, elapsed sampling at the
// prediction call site, fixed-step advance and the window to publish.
class ClientSimulation final {
public:
    using Clock = PredictionElapsedTime::Clock;

    explicit ClientSimulation(const Arena& arena);

    // Session, epoch or player change: prediction, elapsed sampling and the
    // snapshot gate start over.
    void Reset() noexcept;
    // A different arena: prediction and elapsed sampling restart; the snapshot
    // gate is kept because the session reset that follows clears it.
    void SelectArena(const Arena& arena);
    void ClearJumpRequest() noexcept;

    // The newest authoritative state of the local player in this drained batch.
    // Ticks not newer than the last observed one are ignored.
    void Observe(const PlayerState& self, std::uint64_t tick, const std::optional<MovementRules>& rules);

    // Samples elapsed time at `now` and advances. Returns the complete window to
    // publish when it changed; the caller forwards it to the connection.
    [[nodiscard]] std::optional<PlayerInput> Frame(Clock::time_point now, const ClientInputSample& input);

    [[nodiscard]] PlayerInput PendingInput() const { return prediction_.PendingInput(); }
    [[nodiscard]] std::optional<FireGateTiming> ShotTiming() const noexcept { return prediction_.ShotTiming(); }
    [[nodiscard]] const LocalMovementObservation& Observation() const noexcept { return prediction_.Observation(); }

private:
    LocalPlayerPrediction prediction_;
    PredictionElapsedTime elapsed_;
    std::uint64_t lastSnapshotTick_{};
};

} // namespace fps::pvp
