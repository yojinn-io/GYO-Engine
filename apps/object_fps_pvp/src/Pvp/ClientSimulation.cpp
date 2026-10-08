#include "RetroFPS/Pvp/ClientSimulation.hpp"

namespace fps::pvp {

ClientSimulation::ClientSimulation(const Arena& arena) : prediction_(arena) {}

void ClientSimulation::Reset() noexcept {
    lastSnapshotTick_ = 0;
    prediction_.Reset();
    elapsed_.Reset();
}

void ClientSimulation::SelectArena(const Arena& arena) {
    prediction_ = LocalPlayerPrediction(arena);
    elapsed_.Reset();
}

void ClientSimulation::ClearJumpRequest() noexcept { prediction_.ClearJumpRequest(); }

void ClientSimulation::Observe(const PlayerState& self, std::uint64_t tick, const std::optional<MovementRules>& rules) {
    if (tick <= lastSnapshotTick_) return;
    if (rules) prediction_.SetMovementRules(*rules);
    prediction_.Reconcile(self, tick);
    lastSnapshotTick_ = tick;
}

std::optional<PlayerInput> ClientSimulation::Frame(Clock::time_point now, const ClientInputSample& input) {
    const double seconds = elapsed_.Sample(now);
    if (!input.controls) prediction_.ClearJumpRequest();
    if (!prediction_.Advance(seconds, input.forward, input.right, input.yaw, input.pitch, input.controls && input.jump))
        return std::nullopt;
    return prediction_.PendingInput();
}

} // namespace fps::pvp
