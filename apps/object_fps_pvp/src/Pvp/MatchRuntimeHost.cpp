#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"

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
    if (pendingReset_ || !match_.ContainsPlayer(input.playerId) || !PvpMatch::ValidInput(input))
        return false;
    const auto found = inputSequences_.find(input.playerId);
    if (found != inputSequences_.end() && found->second >= input.sequence) return false;
    latestInputs_[input.playerId] = input;
    inputSequences_[input.playerId] = input.sequence;
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
    return ticker_.Advance(elapsedSeconds, [this](const Engine::Runtime::TickContext& tick) {
        while (!controls_.empty()) {
            const auto control = controls_.front();
            controls_.pop_front();
            ControlResult result{control.requestId, control.playerId, control.kind, true, {}};
            if (control.kind == ControlKind::Join) {
                result.accepted = match_.Join(control.playerId, result.error);
            } else {
                static_cast<void>(match_.Leave(control.playerId));
                latestInputs_.erase(control.playerId);
                inputSequences_.erase(control.playerId);
            }
            results_.push_back(std::move(result));
        }
        for (const auto& [playerId, input] : latestInputs_) {
            static_cast<void>(playerId);
            static_cast<void>(match_.SubmitInput(input));
        }
        latestInputs_.clear();
        match_.Tick(tick);
        if (tick.tickId % 3 == 0) snapshot_ = match_.Snapshot();
    });
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
    latestInputs_.clear();
    inputSequences_.clear();
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
