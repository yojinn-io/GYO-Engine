#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace fps::pvp {
namespace {
constexpr std::size_t kMaximumControls = 64;
// Bounds the host's own bookkeeping, not gameplay: substituted sequences kept
// for late arrivals, and input reference ages per connection-quality window.
constexpr std::size_t kMaximumSubstitutedSequences = 64;
constexpr std::size_t kMaximumReferenceAgeSamples = 2048;
constexpr std::int64_t kMaximumReferenceAgeMicros = 1'000'000;
std::int64_t Micros(std::chrono::steady_clock::duration value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
}
}

MatchRuntimeHost::MatchRuntimeHost(Arena arena, ClockNow now)
    : match_(std::move(arena)), now_(std::move(now)) {
    if (!now_) throw std::invalid_argument("MatchRuntimeHost needs a monotonic clock");
}

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
    if (merged.movementEpoch != input.movementEpoch || merged.lifeGeneration != input.lifeGeneration) {
        merged.commands.clear();
        merged.stagedSequences.clear();
        merged.movementEpoch = input.movementEpoch;
        merged.lifeGeneration = input.lifeGeneration;
    }
    std::vector<std::uint64_t> newlyAccepted;
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue;
        const auto [found, inserted] = merged.commands.try_emplace(command.sequence, command);
        if (!inserted && found->second != command) return false;
        if (inserted) { newlyAccepted.push_back(command.sequence); merged.stagedSequences.insert(command.sequence); }
    }
    if (merged.commands.size() > MaxFutureCommands) return false;
    merged.dirty = merged.dirty || !newlyAccepted.empty();
    pendingInputs_[input.playerId] = std::move(merged);
    // The clock is read only when something new needs a timestamp, so reads
    // and pure retransmissions leave it untouched.
    std::optional<std::chrono::steady_clock::time_point> received;
    const auto at = [&] { if (!received) received = now_(); return *received; };
    auto& track = slack_[input.playerId];
    if (track.movementEpoch != input.movementEpoch || track.lifeGeneration != input.lifeGeneration)
        track = {input.movementEpoch, input.lifeGeneration, cursor, {}, {}, std::nullopt};
    for (const auto sequence : newlyAccepted) track.receipts.try_emplace(sequence, at());
    for (const auto& command : input.commands) {
        // A command whose sequence was already substituted reports how late it came.
        if (command.sequence > cursor) continue;
        const auto late = track.substituted.find(command.sequence);
        if (late == track.substituted.end()) continue;
        const auto micros = -Micros(at() - late->second);
        if (!track.pending || micros < track.pending->second) track.pending = std::pair{command.sequence, micros};
        track.substituted.erase(late);
    }
    // Connection quality: the age of the snapshot this window says the Client
    // applied. A tick older than every retained reference is at least as old
    // as the oldest one; an unknown newer tick is ignored.
    if (const auto quality = quality_.find(input.playerId); quality != quality_.end() &&
        !newlyAccepted.empty() && input.observedAuthorityTick != 0 && !publishedReferences_.empty() &&
        quality->second.referenceAgesMicros.size() < kMaximumReferenceAgeSamples) {
        const auto reference = std::find_if(publishedReferences_.begin(), publishedReferences_.end(),
            [&](const auto& entry) { return entry.tick == input.observedAuthorityTick; });
        std::optional<std::int64_t> age;
        if (reference != publishedReferences_.end()) age = Micros(at() - reference->publishedAt);
        else if (input.observedAuthorityTick < publishedReferences_.front().tick)
            age = Micros(at() - publishedReferences_.front().publishedAt);
        if (age) quality->second.referenceAgesMicros.push_back(
            static_cast<std::uint32_t>(std::clamp<std::int64_t>(*age, 0, kMaximumReferenceAgeMicros)));
    }
    for (const auto sequence : newlyAccepted)
        TraceMovement({.kind = MovementTraceKind::HostAccepted, .playerId = input.playerId,
            .epoch = input.movementEpoch, .sequence = sequence, .authorityTick = match_.TickCount(),
            .lifeGeneration = input.lifeGeneration});
    return true;
}

ActionAdmission MatchRuntimeHost::SubmitActions(const ActionBatch& batch) {
    if (batch.shots.empty()) return ActionAdmission::InvalidBatch;
    return SubmitActionBatch(batch, 0);
}

ActionAdmission MatchRuntimeHost::SubmitActionBatch(const ActionBatch& batch, ActionId acknowledgedThrough) {
    std::lock_guard lock(mutex_);
    if (pendingReset_) return ActionAdmission::InvalidPlayer;
    if (!match_.ContainsPlayer(batch.playerId)) return ActionAdmission::InvalidPlayer;
    if (!match_.CanAcknowledgeActions(batch.playerId, acknowledgedThrough)) return ActionAdmission::InvalidBatch;
    const auto queuedAck = pendingActionAcknowledgements_.find(batch.playerId);
    const auto through = queuedAck == pendingActionAcknowledgements_.end() ? acknowledgedThrough :
        (std::max)(acknowledgedThrough, queuedAck->second);
    const auto pending = pendingActions_.find(batch.playerId);
    const std::span<const ShotRequest> staged = pending == pendingActions_.end()
        ? std::span<const ShotRequest>{} : pending->second;
    const auto admission = batch.shots.empty() ? ActionAdmission::Accepted :
        match_.CanSubmitActions(batch, staged, through);
    if (admission != ActionAdmission::Accepted) return admission;

    const auto retained = match_.GetActionResults(batch.playerId);
    auto merged = pending == pendingActions_.end() ? std::vector<ShotRequest>{} : pending->second;
    const auto floor = (std::max)(retained->retiredThrough, through);
    std::erase_if(merged, [=](const auto& shot) { return shot.actionId <= floor; });
    for (const auto& shot : batch.shots) {
        if (shot.actionId <= floor) continue;
        // Resolved duplicates already have their immutable answer. They need no
        // further simulation handoff; reads continue returning the same result.
        if (std::any_of(retained->decisions.begin(), retained->decisions.end(),
            [&](const auto& decision) { return decision.actionId == shot.actionId; })) continue;
        if (std::none_of(merged.begin(), merged.end(),
            [&](const auto& request) { return request.actionId == shot.actionId; }))
            merged.push_back(shot);
    }
    if (!merged.empty()) pendingActions_[batch.playerId] = std::move(merged);
    else pendingActions_.erase(batch.playerId);
    if (through > retained->retiredThrough) pendingActionAcknowledgements_[batch.playerId] = through;
    return ActionAdmission::Accepted;
}

bool MatchRuntimeHost::QueueActionAcknowledgement(PlayerId playerId, ActionId through) {
    std::lock_guard lock(mutex_);
    if (pendingReset_ || !match_.CanAcknowledgeActions(playerId, through)) return false;
    auto& pending = pendingActionAcknowledgements_[playerId];
    pending = std::max(pending, through);
    return true;
}

std::optional<ActionResults> MatchRuntimeHost::GetActionResults(PlayerId playerId) const {
    std::lock_guard lock(mutex_);
    if (pendingReset_) return std::nullopt;
    return match_.GetActionResults(playerId);
}

Engine::Runtime::FixedTickAdvance MatchRuntimeHost::Advance(double elapsedSeconds) {
    std::lock_guard lock(mutex_);
    if (pendingReset_) {
        ClearState();
        elapsedSeconds = 0;
        pendingReset_->set_value();
        pendingReset_.reset();
    }
    std::optional<WorldSnapshot> publication;
    // One clock reading per Advance serves slack timing and the publication reference.
    std::optional<std::chrono::steady_clock::time_point> advancedAt;
    const auto advance = ticker_.Advance(elapsedSeconds, [this, &publication, &advancedAt](const Engine::Runtime::TickContext& tick) {
        while (!controls_.empty()) {
            const auto control = controls_.front();
            controls_.pop_front();
            ControlResult result{control.requestId, control.playerId, control.kind, true, {}};
            if (control.kind == ControlKind::Join) {
                result.accepted = match_.Join(control.playerId, result.error);
                if (result.accepted) quality_[control.playerId] = {match_.TickCount(), {}, {}, false, 0};
            } else {
                static_cast<void>(match_.Leave(control.playerId));
                RemovePlayerState(control.playerId);
            }
            results_.push_back(std::move(result));
        }
        for (const auto& [playerId, through] : pendingActionAcknowledgements_)
            static_cast<void>(match_.AcknowledgeActions(playerId, through));
        pendingActionAcknowledgements_.clear();
        for (const auto& [playerId, shots] : pendingActions_) {
            for (std::size_t first = 0; first < shots.size(); first += MaxActionBatch) {
                const auto last = std::min(first + MaxActionBatch, shots.size());
                ActionBatch batch{playerId, {shots.begin() + first, shots.begin() + last}};
                const auto admission = match_.SubmitActions(batch);
                if (admission != ActionAdmission::Accepted)
                    throw std::logic_error("Validated action handoff lost its reservation");
            }
        }
        pendingActions_.clear();
        for (auto& [playerId, pending] : pendingInputs_) {
            if (!pending.dirty) continue;
            PlayerInput input{playerId, {}, pending.movementEpoch, pending.lifeGeneration};
            const auto handoff = [&] {
                // A snapshot may have advanced life after ingress validation.
                // Recheck the complete identity at the simulation boundary.
                if (!match_.CanSubmitInput(input)) {
                    const auto current = match_.Snapshot();
                    const auto player = std::find_if(current.players.begin(), current.players.end(),
                        [&](const auto& p) { return p.playerId == playerId; });
                    if (player != current.players.end() && player->lifeGeneration != pending.lifeGeneration) {
                        for (const auto& command : input.commands)
                            if (pending.stagedSequences.contains(command.sequence))
                                TraceMovement({.kind = MovementTraceKind::LifecycleCancelled, .playerId = playerId,
                                    .epoch = pending.movementEpoch, .sequence = command.sequence,
                                    .authorityTick = match_.TickCount(), .lifeGeneration = pending.lifeGeneration});
                    }
                } else static_cast<void>(match_.SubmitInput(input));
            };
            for (const auto& [sequence, command] : pending.commands) {
                static_cast<void>(sequence);
                input.commands.push_back(command);
                if (input.commands.size() == MaxPendingCommands) {
                    handoff();
                    input.commands.clear();
                }
            }
            if (!input.commands.empty()) handoff();
            pending.stagedSequences.clear();
            pending.dirty = false;
        }
        match_.Tick(tick, [this](std::uint64_t observedTick) -> std::optional<std::chrono::nanoseconds> {
            const auto reference = std::find_if(publishedReferences_.begin(), publishedReferences_.end(),
                [&](const auto& entry) { return entry.tick == observedTick; });
            if (reference == publishedReferences_.end()) return std::nullopt;
            return std::chrono::duration_cast<std::chrono::nanoseconds>(now_() - reference->publishedAt);
        });
        auto state = match_.Snapshot();
        TrackSlack(state, advancedAt);
        JudgeConnectionQuality(state);
        // Retaining only unresolved tuples supplies bounded duplicate identity
        // across ingress handoffs. Epoch rotation and Leave discard it here.
        for (auto input = pendingInputs_.begin(); input != pendingInputs_.end();) {
            const auto player = std::find_if(state.players.begin(), state.players.end(),
                [&](const auto& candidate) { return candidate.playerId == input->first; });
            if (player == state.players.end() || player->movementEpoch != input->second.movementEpoch ||
                player->lifeGeneration != input->second.lifeGeneration) {
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
            publication = std::move(state);
            for (const auto& player : publication->players)
                TraceMovement({.kind = MovementTraceKind::SnapshotProduced, .playerId = player.playerId,
                    .epoch = player.movementEpoch, .authorityTick = tick.tickId,
                    .lifeGeneration = player.lifeGeneration});
        }
    });
    if (publication) {
        // Only the final owning state enters the IPC handoff. Catch-up states
        // replaced inside this Advance were never published references.
        if (!advancedAt) advancedAt = now_();
        publishedReferences_.push_back({publication->tick, *advancedAt});
        if (publishedReferences_.size() > MaxPublishedShotReferences) publishedReferences_.pop_front();
        snapshot_ = std::move(publication);
        // Samples reach the Client through published snapshots only; catch-up
        // states replaced inside this Advance kept accumulating the minimum.
        for (auto& [playerId, track] : slack_) track.pending.reset();
    }
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

std::vector<Eviction> MatchRuntimeHost::TakeEvictions() {
    std::lock_guard lock(mutex_);
    std::vector<Eviction> result;
    result.swap(evictions_);
    return result;
}

void MatchRuntimeHost::RemovePlayerState(PlayerId playerId) {
    pendingInputs_.erase(playerId);
    pendingActions_.erase(playerId);
    pendingActionAcknowledgements_.erase(playerId);
    slack_.erase(playerId);
    quality_.erase(playerId);
}

void MatchRuntimeHost::TrackSlack(WorldSnapshot& state, std::optional<std::chrono::steady_clock::time_point>& at) {
    // Sequences resolved by this tick: received earlier means executed
    // (slack = now - first receipt); otherwise substituted, remembered so a
    // late arrival can report how late it was. The smallest sample since the
    // last publication rides on the snapshot.
    for (auto& player : state.players) {
        auto found = slack_.find(player.playerId);
        if (found == slack_.end() || found->second.movementEpoch != player.movementEpoch ||
            found->second.lifeGeneration != player.lifeGeneration)
            found = slack_.insert_or_assign(player.playerId, SlackTrack{player.movementEpoch, player.lifeGeneration,
                player.lastResolvedCommand, {}, {}, std::nullopt}).first;
        auto& track = found->second;
        if (player.lastResolvedCommand > track.lastResolved + kMaximumSubstitutedSequences)
            track.lastResolved = player.lastResolvedCommand - kMaximumSubstitutedSequences;
        for (auto sequence = track.lastResolved + 1; sequence <= player.lastResolvedCommand; ++sequence) {
            if (!at) at = now_();
            const auto receipt = track.receipts.find(sequence);
            if (receipt != track.receipts.end()) {
                const auto micros = Micros(*at - receipt->second);
                if (!track.pending || micros < track.pending->second) track.pending = std::pair{sequence, micros};
            } else track.substituted[sequence] = *at;
        }
        track.lastResolved = (std::max)(track.lastResolved, player.lastResolvedCommand);
        while (!track.receipts.empty() && track.receipts.begin()->first <= track.lastResolved)
            track.receipts.erase(track.receipts.begin());
        while (track.substituted.size() > kMaximumSubstitutedSequences) track.substituted.erase(track.substituted.begin());
        if (track.pending) {
            player.movementSlackSequence = track.pending->first;
            player.movementSlackMicros = static_cast<std::int32_t>(std::clamp<std::int64_t>(
                track.pending->second, -MaxMovementSlackMicros, MaxMovementSlackMicros));
        }
    }
    std::erase_if(slack_, [&](const auto& entry) {
        return std::none_of(state.players.begin(), state.players.end(),
            [&](const auto& player) { return player.playerId == entry.first; });
    });
}

void MatchRuntimeHost::JudgeConnectionQuality(WorldSnapshot& state) {
    const auto tick = match_.TickCount();
    std::vector<PlayerId> evicted;
    for (auto& [playerId, track] : quality_) {
        const auto quality = match_.GetMovementQuality(playerId);
        if (!quality) continue;
        if (tick - track.windowStartTick >= ConnectionQualityWindowTicks) {
            if (track.judged) {
                auto& ages = track.referenceAgesMicros;
                std::uint32_t referenceAgeMillis{};
                if (!ages.empty()) {
                    const auto middle = ages.begin() + static_cast<std::ptrdiff_t>(ages.size() / 2);
                    std::nth_element(ages.begin(), middle, ages.end());
                    referenceAgeMillis = *middle / 1000;
                }
                const auto resolved = quality->resolved - track.windowStart.resolved;
                const auto substituted = quality->substituted - track.windowStart.substituted;
                const auto resets = quality->resets - track.windowStart.resets;
                const auto permille = resolved ? static_cast<std::uint32_t>(substituted * 1000 / resolved) : 0U;
                const bool late = referenceAgeMillis > ConnectionQualityMaximumReferenceAgeMillis;
                const bool failed = late || permille > ConnectionQualityMaximumSubstitutedPermille || resets > 0;
                track.failures = failed ? track.failures + 1 : 0;
                if (track.failures >= ConnectionQualityFailedWindows) {
                    evictions_.push_back({playerId, late ? EvictionReason::HighLatency : EvictionReason::UnstableInput,
                        referenceAgeMillis, permille,
                        static_cast<std::uint32_t>((std::min)(resets, std::uint64_t{1'000'000}))});
                    evicted.push_back(playerId);
                }
            }
            // The first window after a join covers startup hitches and is not judged.
            track.judged = true;
            track.windowStartTick = tick;
            track.windowStart = *quality;
            track.referenceAgesMicros.clear();
        }
        const auto player = std::find_if(state.players.begin(), state.players.end(),
            [&](const auto& candidate) { return candidate.playerId == playerId; });
        if (player != state.players.end()) player->connectionQualityFailures = track.failures;
    }
    for (const auto playerId : evicted) {
        static_cast<void>(match_.Leave(playerId));
        RemovePlayerState(playerId);
        std::erase_if(state.players, [&](const auto& player) { return player.playerId == playerId; });
        std::erase_if(state.combat, [&](const auto& combat) { return combat.playerId == playerId; });
    }
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
    pendingActions_.clear();
    pendingActionAcknowledgements_.clear();
    slack_.clear();
    quality_.clear();
    evictions_.clear();
    publishedReferences_.clear();
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
