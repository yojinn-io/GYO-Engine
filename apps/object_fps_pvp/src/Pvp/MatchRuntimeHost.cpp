#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace fps::pvp {
namespace {
constexpr std::size_t kMaximumControls = 64;
}

MatchRuntimeHost::MatchRuntimeHost(Arena arena) : match_(std::move(arena)) {}

bool MatchRuntimeHost::QueueControl(Control control) {
    std::lock_guard lock(mutex_);
    if (pendingReset_ || control.requestId == 0 || control.playerId == 0 ||
        controls_.size() + results_.size() >= kMaximumControls) return false;
    controls_.push_back(control);
    return true;
}

bool MatchRuntimeHost::QueueJoin(std::uint64_t requestId, PlayerId playerId) {
    return QueueControl({requestId, playerId, ControlKind::Join});
}

bool MatchRuntimeHost::QueueLeave(std::uint64_t requestId, PlayerId playerId) {
    return QueueControl({requestId, playerId, ControlKind::Leave});
}

bool MatchRuntimeHost::SubmitInput(const PlayerInput& input) {
    std::lock_guard lock(mutex_);
    if (pendingReset_ || !match_.CanSubmitInput(input)) return false;
    const auto snapshot = match_.Snapshot();
    const auto player = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [&](const auto& state) { return state.playerId == input.playerId; });
    const auto cursor = player->lastResolvedCommand;
    // Validate the complete merge before committing any command. Separate batches
    // must not erase one another or change an immutable pending command.
    auto merged = pendingInputs_[input.playerId];
    if (merged.movementEpoch != input.movementEpoch) {
        merged.commands.clear();
        merged.movementEpoch = input.movementEpoch;
    }
    std::vector<std::uint64_t> newlyAccepted;
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue;
        const auto [found, inserted] = merged.commands.try_emplace(command.sequence, command);
        if (!inserted && found->second != command) return false;
        if (inserted) newlyAccepted.push_back(command.sequence);
    }
    if (merged.commands.size() > MaxFutureCommands) return false;
    merged.dirty = merged.dirty || !newlyAccepted.empty();
    pendingInputs_[input.playerId] = std::move(merged);
    for (const auto sequence : newlyAccepted)
        TraceMovement({.kind = MovementTraceKind::HostAccepted, .playerId = input.playerId,
            .epoch = input.movementEpoch, .sequence = sequence, .authorityTick = match_.TickCount()});
    return true;
}

Engine::Runtime::FixedTickAdvance MatchRuntimeHost::Advance(double elapsedSeconds) {
    std::lock_guard lock(mutex_);
    if (pendingReset_) {
        ClearState();
        elapsedSeconds = 0;
        pendingReset_->set_value();
        pendingReset_.reset();
    }
    const auto advance = ticker_.Advance(elapsedSeconds, [this](const Engine::Runtime::TickContext& tick) {
        while (!controls_.empty()) {
            const auto control = controls_.front();
            controls_.pop_front();
            ControlResult result{control.requestId, control.playerId, control.kind, true, {}};
            if (control.kind == ControlKind::Join) {
                result.accepted = match_.Join(control.playerId, result.error);
            } else {
                static_cast<void>(match_.Leave(control.playerId));
                pendingInputs_.erase(control.playerId);
            }
            results_.push_back(std::move(result));
        }
        for (auto& [playerId, pending] : pendingInputs_) {
            if (!pending.dirty) continue;
            PlayerInput input{playerId, {}, pending.movementEpoch};
            for (const auto& [sequence, command] : pending.commands) {
                static_cast<void>(sequence);
                input.commands.push_back(command);
                if (input.commands.size() == MaxPendingCommands) {
                    static_cast<void>(match_.SubmitInput(input));
                    input.commands.clear();
                }
            }
            if (!input.commands.empty()) static_cast<void>(match_.SubmitInput(input));
            pending.dirty = false;
        }
        match_.Tick(tick);
        auto state = match_.Snapshot();
        // Retaining only unresolved tuples supplies bounded duplicate identity
        // across ingress handoffs. Epoch rotation and Leave discard it here.
        for (auto input = pendingInputs_.begin(); input != pendingInputs_.end();) {
            const auto player = std::find_if(state.players.begin(), state.players.end(),
                [&](const auto& candidate) { return candidate.playerId == input->first; });
            if (player == state.players.end() || player->movementEpoch != input->second.movementEpoch) {
                input = pendingInputs_.erase(input);
                continue;
            }
            auto& commands = input->second.commands;
            while (!commands.empty() && commands.begin()->first <= player->lastResolvedCommand)
                commands.erase(commands.begin());
            if (commands.empty()) input = pendingInputs_.erase(input);
            else ++input;
        }
        if (tick.tickId % SnapshotIntervalTicks == 0) {
            snapshot_ = std::move(state);
            TraceMovement({.kind = MovementTraceKind::SnapshotProduced, .authorityTick = tick.tickId});
        }
    });
    if (elapsedSeconds >= 0.1 || advance.droppedSeconds > 0)
        TraceMovement({.kind = MovementTraceKind::RuntimeGap, .authorityTick = match_.TickCount(),
            .droppedSeconds = advance.droppedSeconds, .frameSeconds = elapsedSeconds});
    return advance;
}

void MatchRuntimeHost::Run(std::stop_token stop) {
    if (running_.exchange(true)) throw std::logic_error("MatchRuntimeHost is already running");
    struct Guard final {
        std::atomic<bool>& running;
        ~Guard() { running.store(false); }
    } guard{running_};
    auto previous = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const auto result = Advance(std::chrono::duration<double>(now - previous).count());
        previous = now;
        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, stop, std::chrono::duration<double>(result.secondsUntilNextTick),
                       [this] { return pendingReset_.has_value(); });
    }
}

std::optional<WorldSnapshot> MatchRuntimeHost::TakeSnapshot() {
    std::lock_guard lock(mutex_);
    auto result = std::move(snapshot_);
    snapshot_.reset();
    return result;
}

std::vector<ControlResult> MatchRuntimeHost::TakeControlResults() {
    std::lock_guard lock(mutex_);
    std::vector<ControlResult> result;
    result.swap(results_);
    return result;
}

std::future<void> MatchRuntimeHost::RequestReset() {
    std::lock_guard lock(mutex_);
    if (pendingReset_) throw std::logic_error("A match reset is already pending");
    pendingReset_.emplace();
    auto future = pendingReset_->get_future();
    wake_.notify_all();
    return future;
}

void MatchRuntimeHost::ClearState() {
    match_.Reset();
    ticker_.Reset();
    controls_.clear();
    pendingInputs_.clear();
    results_.clear();
    snapshot_.reset();
}

void MatchRuntimeHost::Reset() {
    std::lock_guard lock(mutex_);
    if (running_.load()) throw std::logic_error("Use RequestReset while the match is running");
    ClearState();
    if (pendingReset_) {
        pendingReset_->set_value();
        pendingReset_.reset();
    }
}

} // namespace fps::pvp
