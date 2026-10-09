#include "RetroFPS/Pvp/ClientSimulation.hpp"

#include <algorithm>

namespace fps::pvp {

ClientSimulation::ClientSimulation(const Arena& arena) : prediction_(arena) {}

void ClientSimulation::Reset() noexcept {
    lastSnapshotTick_ = 0;
    samples_.clear();
    prediction_.Reset();
    elapsed_.Reset();
}

void ClientSimulation::SelectArena(const Arena& arena) {
    prediction_ = LocalPlayerPrediction(arena);
    elapsed_.Reset();
}

void ClientSimulation::ClearJumpRequest() noexcept { prediction_.ClearJumpRequest(); }

void ClientSimulation::ObserveSample(const WorldSnapshot& snapshot, PlayerId self) {
    if (snapshot.tick <= lastSnapshotTick_) return;
    for (const auto& player : snapshot.players)
        if (player.playerId == self) { samples_.push_back({snapshot.tick, player}); return; }
}

void ClientSimulation::Observe(const PlayerState& self, std::uint64_t tick, const std::optional<MovementRules>& rules) {
    if (tick <= lastSnapshotTick_) { samples_.clear(); return; }
    if (rules) prediction_.SetMovementRules(*rules);
    std::stable_sort(samples_.begin(), samples_.end(), [](const Sample& a, const Sample& b) { return a.tick < b.tick; });
    std::uint64_t taken = lastSnapshotTick_;
    // A state that reseeds starts a new phase: older samples describe the one
    // it discards, and a correction they triggered would never be slewed.
    if (prediction_.Reseeds(self)) samples_.clear();
    for (const auto& sample : samples_) {
        if (sample.tick <= taken || sample.tick >= tick || sample.self.playerId != self.playerId) continue;
        prediction_.ObservePhaseSample(sample.self);
        taken = sample.tick;
    }
    samples_.clear();
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
