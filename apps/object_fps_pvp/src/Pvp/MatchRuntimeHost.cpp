#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "engine/math/scalar/Scalar.hpp"

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
IngressInputRejection Rejection(InputAdmission admission) {
    switch (admission) {
    case InputAdmission::UnknownPlayer: return IngressInputRejection::UnknownPlayer;
    case InputAdmission::Malformed: return IngressInputRejection::Malformed;
    case InputAdmission::EpochOld: return IngressInputRejection::EpochOld;
    case InputAdmission::EpochFuture: return IngressInputRejection::EpochFuture;
    case InputAdmission::LifeOld: return IngressInputRejection::LifeOld;
    case InputAdmission::LifeFuture: return IngressInputRejection::LifeFuture;
    case InputAdmission::BeyondWindow: return IngressInputRejection::BeyondWindow;
    case InputAdmission::ConflictQueued: return IngressInputRejection::ConflictQueued;
    case InputAdmission::Accepted: break;
    }
    throw std::logic_error("An accepted input has no rejection reason");
}
IngressActionRejection Rejection(ActionAdmission admission) {
    switch (admission) {
    case ActionAdmission::InvalidPlayer: return IngressActionRejection::InvalidPlayer;
    case ActionAdmission::InvalidBatch: return IngressActionRejection::InvalidBatch;
    case ActionAdmission::Conflict: return IngressActionRejection::Conflict;
    case ActionAdmission::OutsideWindow: return IngressActionRejection::OutsideWindow;
    case ActionAdmission::Full: return IngressActionRejection::Full;
    case ActionAdmission::Accepted: break;
    }
    throw std::logic_error("An accepted action batch has no rejection reason");
}
}

MatchRuntimeHost::MatchRuntimeHost(Arena arena, ClockNow now)
    : match_(std::move(arena)), now_(std::move(now)) {
    if (!now_) throw std::invalid_argument("MatchRuntimeHost needs a monotonic clock");
}

MatchRuntimeHost::~MatchRuntimeHost() { Stop(); }

bool MatchRuntimeHost::Start(std::string& error) {
    if (role_) throw std::logic_error("MatchRuntimeHost is already started");
    auto started = Engine::Threads::RoleThread::Start({"gyo-match-sim", Engine::Threads::ThreadPriority::Interactive},
        [this](Engine::Threads::RoleContext& context) { Body(context); });
    if (!started) {
        error = Engine::Base::Describe(started.error());
        return false;
    }
    role_.emplace(std::move(*started));
    return true;
}

void MatchRuntimeHost::Stop() { role_.reset(); }

std::optional<std::string> MatchRuntimeHost::Error() const {
    std::lock_guard lock(mutex_);
    return error_;
}

void MatchRuntimeHost::SetPublishListener(std::function<void()> listener) {
    if (role_) throw std::logic_error("Set the publish listener before Start");
    publishListener_ = std::move(listener);
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

PlayerId MatchRuntimeHost::IngressBucketId(PlayerId playerId) const {
    return match_.ContainsPlayer(playerId) ? playerId : PlayerId{};
}

MatchIngressCounts& MatchRuntimeHost::IngressBucket(PlayerId playerId) { return ingress_[IngressBucketId(playerId)]; }

void MatchRuntimeHost::SubstitutionClosed(const IngressSubstitution& record) {
    if (!record.firstArrival) ++ingress_[record.playerId].unarrived[IngressIndex(record.close)];
    records_.Add(record);
}

void MatchRuntimeHost::RecordRejection(const PlayerInput& input, IngressInputRejection reason) {
    if (!records_.AdmitRejection(IngressBucketId(input.playerId), reason)) return;
    // The trace's own steady clock, as the movement trace: the host's clock
    // (now_) serves timing and stays unread by refusals.
    IngressRejectionRecord record{.timeNs = MovementTraceNowNs(), .playerId = input.playerId, .reason = reason,
        .epoch = input.movementEpoch, .life = input.lifeGeneration, .commands = input.commands.size()};
    if (!input.commands.empty()) {
        record.firstSequence = input.commands.front().sequence;
        record.lastSequence = input.commands.back().sequence;
    }
    if (match_.ContainsPlayer(input.playerId)) {
        const auto snapshot = match_.Snapshot();
        const auto player = std::find_if(snapshot.players.begin(), snapshot.players.end(),
            [&](const auto& state) { return state.playerId == input.playerId; });
        if (player != snapshot.players.end()) {
            record.cursor = player->lastResolvedCommand;
            record.currentEpoch = player->movementEpoch;
            record.currentLife = player->lifeGeneration;
        }
    }
    records_.Add(std::move(record));
}

void MatchRuntimeHost::OfferSlackSample(PlayerId playerId, SlackTrack& track, std::uint64_t sequence, std::int64_t micros) {
    if (!track.pending || micros < track.pending->second) {
        if (track.pending) ++ingress_[playerId].slackMergedSamples;
        track.pending = std::pair{sequence, micros};
    } else ++ingress_[playerId].slackMergedSamples;
}

void MatchRuntimeHost::DiscardSlack(PlayerId playerId, const SlackTrack& track) {
    if (track.pending) ++ingress_[playerId].slackDiscardedSamples;
}

void MatchRuntimeHost::CountSlackSamples(const WorldSnapshot& snapshot, std::uint64_t MatchIngressCounts::*field) {
    for (const auto& player : snapshot.players)
        if (player.movementSlackSequence) ++(ingress_[player.playerId].*field);
}

void MatchRuntimeHost::NoteCoalescedSlackSamples(std::span<const PlayerId> players) {
    std::lock_guard lock(mutex_);
    for (const auto playerId : players) ++ingress_[playerId].slackCoalescedSamples;
}

bool MatchRuntimeHost::SubmitInput(const PlayerInput& input) {
    std::lock_guard lock(mutex_);
    // Every return below is counted exactly once: received = accepted + refused.
    auto& ingress = IngressBucket(input.playerId);
    ++ingress.received.inputs;
    ingress.received.commands += input.commands.size();
    const auto refuse = [&](IngressInputRejection reason) {
        auto& refused = ingress.rejected[IngressIndex(reason)];
        ++refused.inputs;
        refused.commands += input.commands.size();
        RecordRejection(input, reason);
        return false;
    };
    if (pendingReset_) return refuse(IngressInputRejection::Resetting);
    if (const auto admission = match_.AdmitInput(input); admission != InputAdmission::Accepted)
        return refuse(Rejection(admission));
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
    std::uint64_t pendingCopies{};
    for (const auto& command : input.commands) {
        if (command.sequence <= cursor) continue;
        const auto [found, inserted] = merged.commands.try_emplace(command.sequence, command);
        if (!inserted && found->second != command) return refuse(IngressInputRejection::ConflictStaged);
        if (inserted) { newlyAccepted.push_back(command.sequence); merged.stagedSequences.insert(command.sequence); }
        else ++pendingCopies;
    }
    // Should stay 0: admission already bounds each command to (cursor,
    // cursor + MaxFutureCommands] and staging only keeps commands past the cursor.
    if (merged.commands.size() > MaxFutureCommands) return refuse(IngressInputRejection::StagedOverWindow);
    merged.dirty = merged.dirty || !newlyAccepted.empty();
    pendingInputs_[input.playerId] = std::move(merged);
    ++ingress.acceptedInputs;
    ingress.classified[IngressIndex(IngressCommandClass::AcceptedNew)] += newlyAccepted.size();
    ingress.classified[IngressIndex(IngressCommandClass::PendingCopy)] += pendingCopies;
    // The clock is read only when something new needs a timestamp, so reads
    // and pure retransmissions leave it untouched.
    std::optional<std::chrono::steady_clock::time_point> received;
    const auto at = [&] { if (!received) received = now_(); return *received; };
    // The age of the snapshot this window says the Client applied. A tick
    // older than every retained reference is at least as old as the oldest
    // one; an unknown newer tick has no age.
    const auto referenceAge = [&]() -> std::optional<std::int64_t> {
        if (input.observedAuthorityTick == 0 || publishedReferences_.empty()) return std::nullopt;
        const auto reference = std::find_if(publishedReferences_.begin(), publishedReferences_.end(),
            [&](const auto& entry) { return entry.tick == input.observedAuthorityTick; });
        if (reference != publishedReferences_.end()) return Micros(at() - reference->publishedAt);
        if (input.observedAuthorityTick < publishedReferences_.front().tick)
            return Micros(at() - publishedReferences_.front().publishedAt);
        return std::nullopt;
    };
    auto& track = slack_[input.playerId];
    if (track.movementEpoch != input.movementEpoch || track.lifeGeneration != input.lifeGeneration) {
        DiscardSlack(input.playerId, track);
        track = {input.movementEpoch, input.lifeGeneration, cursor, {}, {}, std::nullopt};
    }
    for (const auto sequence : newlyAccepted) track.receipts.try_emplace(sequence, at());
    std::size_t lateCommands{};
    for (const auto& command : input.commands) {
        if (command.sequence > cursor) continue;
        // Counting only, by the ledger's substitution records; the ledger
        // reads the clock only for a late_first.
        const auto arrival = ledger_.Arrive(input.playerId, input.movementEpoch, input.lifeGeneration,
            command.sequence, cursor, at, referenceAge);
        ++ingress.classified[IngressIndex(arrival)];
        if (arrival == IngressCommandClass::LateFirst || arrival == IngressCommandClass::LateCopy) ++lateCommands;
        // A command whose sequence was already substituted reports how late it came.
        const auto late = track.substituted.find(command.sequence);
        if (late == track.substituted.end()) continue;
        const auto micros = -Micros(at() - late->second);
        ++ingress.slackLateSamples;
        OfferSlackSample(input.playerId, track, command.sequence, micros);
        track.substituted.erase(late);
    }
    if (lateCommands == input.commands.size()) ++ingress.lateOnlyInputs;
    // Connection quality samples the reference age of windows bringing new commands.
    if (const auto quality = quality_.find(input.playerId); quality != quality_.end() &&
        !newlyAccepted.empty() && quality->second.referenceAgesMicros.size() < kMaximumReferenceAgeSamples) {
        if (const auto age = referenceAge()) quality->second.referenceAgesMicros.push_back(
            static_cast<std::uint32_t>(Engine::Math::Clamp<std::int64_t>(*age, 0, kMaximumReferenceAgeMicros)));
    }
    for (const auto sequence : newlyAccepted)
        TraceMovement({.kind = MovementTraceKind::HostAccepted, .playerId = input.playerId,
            .epoch = input.movementEpoch, .sequence = sequence, .authorityTick = match_.TickCount(),
            .lifeGeneration = input.lifeGeneration});
    return true;
}

ActionAdmission MatchRuntimeHost::SubmitActions(const ActionBatch& batch) {
    if (batch.shots.empty()) {
        std::lock_guard lock(mutex_);
        auto& ingress = IngressBucket(batch.playerId);
        ++ingress.receivedActions.batches;
        ++ingress.rejectedActions[IngressIndex(IngressActionRejection::InvalidBatch)].batches;
        return ActionAdmission::InvalidBatch;
    }
    return SubmitActionBatch(batch, 0);
}

void MatchRuntimeHost::NoteWireRejection(PlayerId playerId, IngressActionRejection reason, std::size_t shots) {
    if (reason != IngressActionRejection::OverBatch && reason != IngressActionRejection::Malformed)
        throw std::logic_error("Only the wire refuses an action batch for its shape");
    std::lock_guard lock(mutex_);
    auto& ingress = IngressBucket(playerId);
    ++ingress.receivedActions.batches;
    ingress.receivedActions.shots += shots;
    auto& refused = ingress.rejectedActions[IngressIndex(reason)];
    ++refused.batches;
    refused.shots += shots;
}

ActionAdmission MatchRuntimeHost::SubmitActionBatch(const ActionBatch& batch, ActionId acknowledgedThrough) {
    std::lock_guard lock(mutex_);
    // Every return below is counted exactly once: received = accepted + refused.
    auto& ingress = IngressBucket(batch.playerId);
    ++ingress.receivedActions.batches;
    ingress.receivedActions.shots += batch.shots.size();
    const auto refuse = [&](IngressActionRejection reason, ActionAdmission admission) {
        auto& refused = ingress.rejectedActions[IngressIndex(reason)];
        ++refused.batches;
        refused.shots += batch.shots.size();
        return admission;
    };
    if (pendingReset_) return refuse(IngressActionRejection::Resetting, ActionAdmission::InvalidPlayer);
    if (!match_.ContainsPlayer(batch.playerId))
        return refuse(IngressActionRejection::InvalidPlayer, ActionAdmission::InvalidPlayer);
    if (!match_.CanAcknowledgeActions(batch.playerId, acknowledgedThrough))
        return refuse(IngressActionRejection::InvalidBatch, ActionAdmission::InvalidBatch);
    const auto queuedAck = pendingActionAcknowledgements_.find(batch.playerId);
    const auto through = queuedAck == pendingActionAcknowledgements_.end() ? acknowledgedThrough :
        Engine::Math::Max(acknowledgedThrough, queuedAck->second);
    const auto pending = pendingActions_.find(batch.playerId);
    const std::span<const ShotRequest> staged = pending == pendingActions_.end()
        ? std::span<const ShotRequest>{} : pending->second;
    const auto admission = batch.shots.empty() ? ActionAdmission::Accepted :
        match_.CanSubmitActions(batch, staged, through);
    if (admission != ActionAdmission::Accepted) return refuse(Rejection(admission), admission);

    const auto retained = match_.GetActionResults(batch.playerId);
    auto merged = pending == pendingActions_.end() ? std::vector<ShotRequest>{} : pending->second;
    const auto floor = Engine::Math::Max(retained->retiredThrough, through);
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
    ++ingress.acceptedActions.batches;
    ingress.acceptedActions.shots += batch.shots.size();
    return ActionAdmission::Accepted;
}

bool MatchRuntimeHost::QueueActionAcknowledgement(PlayerId playerId, ActionId through) {
    std::lock_guard lock(mutex_);
    if (pendingReset_ || !match_.CanAcknowledgeActions(playerId, through)) return false;
    auto& pending = pendingActionAcknowledgements_[playerId];
    pending = Engine::Math::Max(pending, through);
    return true;
}

std::optional<ActionResults> MatchRuntimeHost::GetActionResults(PlayerId playerId) const {
    std::lock_guard lock(mutex_);
    if (pendingReset_) return std::nullopt;
    return match_.GetActionResults(playerId);
}

Engine::Runtime::FixedTickAdvance MatchRuntimeHost::Advance(double elapsedSeconds) {
    bool published = false;
    const auto advance = Step(elapsedSeconds, published);
    if (published && publishListener_) publishListener_();
    return advance;
}

Engine::Runtime::FixedTickAdvance MatchRuntimeHost::Step(double elapsedSeconds, bool& published) {
    std::lock_guard lock(mutex_);
    if (pendingReset_) {
        ClearState();
        elapsedSeconds = 0;
        pendingReset_->set_value();
        pendingReset_.reset();
        published = true;
    }
    const auto resultsBefore = results_.size();
    const auto evictionsBefore = evictions_.size();
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
                // Staging was pruned to unresolved commands after the previous tick.
                RemovePlayerState(control.playerId, 0);
            }
            results_.push_back(std::move(result));
        }
        for (const auto& [playerId, through] : pendingActionAcknowledgements_)
            static_cast<void>(match_.AcknowledgeActions(playerId, through));
        pendingActionAcknowledgements_.clear();
        for (const auto& [playerId, shots] : pendingActions_) {
            for (std::size_t first = 0; first < shots.size(); first += MaxActionBatch) {
                const auto last = Engine::Math::Min(first + MaxActionBatch, shots.size());
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
                    // Should stay 0: rotation below discards a stale identity
                    // before the next handoff, and ingress checks the current one.
                    // Were it not, these commands stay staged and a rotation
                    // after this tick counts them again as rotation_discarded.
                    ingress_[playerId].discarded[IngressIndex(IngressStagedDiscard::HandoffRejected)] +=
                        static_cast<std::uint64_t>(std::count_if(input.commands.begin(), input.commands.end(),
                            [&](const auto& command) { return pending.stagedSequences.contains(command.sequence); }));
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
                const auto discard = player == state.players.end() ? IngressStagedDiscard::LeaveDiscarded
                                                                   : IngressStagedDiscard::RotationDiscarded;
                ingress_[input->first].discarded[IngressIndex(discard)] += input->second.commands.size();
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
        if (snapshot_) {
            ++statistics_.snapshotOverwrites;
            CountSlackSamples(*snapshot_, &MatchIngressCounts::slackOverwrittenSamples);
        }
        snapshot_ = std::move(publication);
        published = true;
        // Samples reach the Client through published snapshots only; catch-up
        // states replaced inside this Advance kept accumulating the minimum.
        // With a publication every tick, each pending sample is in this one.
        for (auto& [playerId, track] : slack_) {
            if (track.pending) {
                auto& counts = ingress_[playerId];
                ++counts.slackPublishedSamples;
                if (track.pending->second < 0) ++counts.slackPublishedNegativeSamples;
            }
            track.pending.reset();
        }
    }
    if (elapsedSeconds >= 0.1 || advance.droppedSeconds > 0)
        TraceMovement({.kind = MovementTraceKind::RuntimeGap, .authorityTick = match_.TickCount(),
            .droppedSeconds = advance.droppedSeconds, .frameSeconds = elapsedSeconds});
    published = published || results_.size() > resultsBefore || evictions_.size() > evictionsBefore;
    return advance;
}

void MatchRuntimeHost::Body(Engine::Threads::RoleContext& context) {
    running_.store(true);
    struct Guard final {
        std::atomic<bool>& running;
        ~Guard() { running.store(false); }
    } guard{running_};
    // A thread entry point handles its own Runtime Errors: match_main reads
    // Error() and exits visibly.
    try {
        auto previous = std::chrono::steady_clock::now();
        while (!context.StopRequested()) {
            const auto sampled = std::chrono::steady_clock::now();
            const auto result = Advance(std::chrono::duration<double>(sampled - previous).count());
            previous = sampled;
            // The next tick grid point, as an absolute deadline: the time
            // Advance took does not delay it. A reset request wakes it early.
            const auto deadline = sampled + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(result.secondsUntilNextTick));
            const auto wake = context.WaitUntil(deadline);
            if (context.StopRequested()) break;
            std::lock_guard lock(mutex_);
            statistics_.ticks.Record(deadline, wake.woke, wake.reason == Engine::Time::WakeReason::Notified);
        }
    } catch (const std::exception& failure) {
        std::lock_guard lock(mutex_);
        error_ = failure.what();
    }
}

MatchRuntimeHost::Statistics MatchRuntimeHost::TakeStatistics() {
    std::lock_guard lock(mutex_);
    return std::exchange(statistics_, {});
}

std::map<PlayerId, MatchIngressCounts> MatchRuntimeHost::TakeIngressStatistics(bool final) {
    std::lock_guard lock(mutex_);
    if (final) {
        ledger_.Clear(IngressSubstitutionClose::End, [this](const auto& record) { SubstitutionClosed(record); });
        for (auto& [playerId, track] : slack_) {
            DiscardSlack(playerId, track);
            track.pending.reset();
        }
        if (snapshot_) CountSlackSamples(*snapshot_, &MatchIngressCounts::slackUnclaimedSamples);
        snapshot_.reset();
    }
    records_.NewWindow();
    ingress_.try_emplace(PlayerId{});
    for (const auto& player : match_.Snapshot().players) ingress_.try_emplace(player.playerId);
    return std::exchange(ingress_, {});
}

IngressRecords MatchRuntimeHost::DrainIngressRecords() {
    std::lock_guard lock(mutex_);
    return records_.Drain();
}

std::optional<WorldSnapshot> MatchRuntimeHost::TakeSnapshot() {
    std::lock_guard lock(mutex_);
    auto result = std::move(snapshot_);
    snapshot_.reset();
    if (result) CountSlackSamples(*result, &MatchIngressCounts::slackTakenSamples);
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

void MatchRuntimeHost::RemovePlayerState(PlayerId playerId, std::uint64_t resolvedThrough) {
    if (const auto pending = pendingInputs_.find(playerId); pending != pendingInputs_.end()) {
        const auto& commands = pending->second.commands;
        ingress_[playerId].discarded[IngressIndex(IngressStagedDiscard::LeaveDiscarded)] +=
            static_cast<std::uint64_t>(std::distance(commands.upper_bound(resolvedThrough), commands.end()));
    }
    if (const auto actions = pendingActions_.find(playerId); actions != pendingActions_.end())
        ingress_[playerId].leaveDiscardedActionShots += actions->second.size();
    ledger_.Remove(playerId, [this](const auto& record) { SubstitutionClosed(record); });
    if (const auto track = slack_.find(playerId); track != slack_.end()) DiscardSlack(playerId, track->second);
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
            found->second.lifeGeneration != player.lifeGeneration) {
            if (found != slack_.end()) DiscardSlack(player.playerId, found->second);
            found = slack_.insert_or_assign(player.playerId, SlackTrack{player.movementEpoch, player.lifeGeneration,
                player.lastResolvedCommand, {}, {}, std::nullopt}).first;
        }
        auto& track = found->second;
        if (player.lastResolvedCommand > track.lastResolved + kMaximumSubstitutedSequences)
            track.lastResolved = player.lastResolvedCommand - kMaximumSubstitutedSequences;
        // The ledger follows the sequences this loop visits, from the same
        // starting cursor, and takes its times from the same clock reading.
        const auto closed = [this](const auto& record) { SubstitutionClosed(record); };
        ledger_.Observe(player.playerId, player.movementEpoch, player.lifeGeneration, track.lastResolved, closed);
        for (auto sequence = track.lastResolved + 1; sequence <= player.lastResolvedCommand; ++sequence) {
            if (!at) at = now_();
            const auto receipt = track.receipts.find(sequence);
            if (receipt != track.receipts.end()) {
                const auto micros = Micros(*at - receipt->second);
                ++ingress_[player.playerId].slackExecutedSamples;
                OfferSlackSample(player.playerId, track, sequence, micros);
                ledger_.Resolved(player.playerId, sequence, std::nullopt, match_.TickCount(), closed);
            } else {
                track.substituted[sequence] = *at;
                ++ingress_[player.playerId].substitutedCommands;
                ledger_.Resolved(player.playerId, sequence, *at, match_.TickCount(), closed);
            }
        }
        track.lastResolved = Engine::Math::Max(track.lastResolved, player.lastResolvedCommand);
        while (!track.receipts.empty() && track.receipts.begin()->first <= track.lastResolved)
            track.receipts.erase(track.receipts.begin());
        while (track.substituted.size() > kMaximumSubstitutedSequences) track.substituted.erase(track.substituted.begin());
        if (track.pending) {
            player.movementSlackSequence = track.pending->first;
            player.movementSlackMicros = static_cast<std::int32_t>(Engine::Math::Clamp<std::int64_t>(
                track.pending->second, -MaxMovementSlackMicros, MaxMovementSlackMicros));
        }
    }
    std::erase_if(slack_, [&](const auto& entry) {
        const bool gone = std::none_of(state.players.begin(), state.players.end(),
            [&](const auto& player) { return player.playerId == entry.first; });
        if (gone) DiscardSlack(entry.first, entry.second);
        return gone;
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
                        static_cast<std::uint32_t>(Engine::Math::Min(resets, std::uint64_t{1'000'000}))});
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
        // Eviction runs before staging is pruned for this tick: what this tick
        // executed is not a discard. A staged identity this tick rotated away
        // is wholly unresolved.
        const auto player = std::find_if(state.players.begin(), state.players.end(),
            [&](const auto& candidate) { return candidate.playerId == playerId; });
        const auto pending = pendingInputs_.find(playerId);
        const bool current = player != state.players.end() && pending != pendingInputs_.end() &&
            player->movementEpoch == pending->second.movementEpoch && player->lifeGeneration == pending->second.lifeGeneration;
        const auto resolvedThrough = current ? player->lastResolvedCommand : 0;
        static_cast<void>(match_.Leave(playerId));
        RemovePlayerState(playerId, resolvedThrough);
        std::erase_if(state.players, [&](const auto& player) { return player.playerId == playerId; });
        std::erase_if(state.combat, [&](const auto& combat) { return combat.playerId == playerId; });
    }
}

std::future<void> MatchRuntimeHost::RequestReset() {
    std::lock_guard lock(mutex_);
    if (pendingReset_) throw std::logic_error("A match reset is already pending");
    pendingReset_.emplace();
    auto future = pendingReset_->get_future();
    if (role_) role_->Notify();
    return future;
}

void MatchRuntimeHost::ClearState() {
    // Staging holds only unresolved commands of current identities here:
    // ingress admits the current identity and each tick prunes the rest.
    for (const auto& [playerId, pending] : pendingInputs_)
        ingress_[playerId].resetDiscardedCommands += pending.commands.size();
    for (const auto& [playerId, shots] : pendingActions_)
        ingress_[playerId].resetDiscardedActionShots += shots.size();
    for (const auto& [playerId, through] : pendingActionAcknowledgements_) {
        static_cast<void>(through);
        ++ingress_[playerId].resetDiscardedActionAcks;
    }
    ledger_.Clear(IngressSubstitutionClose::Reset, [this](const auto& record) { SubstitutionClosed(record); });
    for (const auto& [playerId, track] : slack_) DiscardSlack(playerId, track);
    if (snapshot_) CountSlackSamples(*snapshot_, &MatchIngressCounts::slackUnclaimedSamples);
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
