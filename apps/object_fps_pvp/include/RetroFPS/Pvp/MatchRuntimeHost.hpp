#pragma once

#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/MatchIngressLedger.hpp"
#include "RetroFPS/Pvp/MatchIngressTrace.hpp"
#include "RetroFPS/Pvp/NetworkStatistics.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "engine/threads/RoleThread.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace fps::pvp {

enum class ControlKind { Join, Leave };

struct ControlResult final {
    std::uint64_t requestId{};
    PlayerId playerId{};
    ControlKind kind{ControlKind::Join};
    bool accepted{};
    std::string error;
};

enum class EvictionReason { HighLatency, UnstableInput };

// A player the host removed for ConnectionQualityFailedWindows failed
// connection-quality windows in a row, with the last window's measurements.
struct Eviction final {
    PlayerId playerId{};
    EvictionReason reason{EvictionReason::UnstableInput};
    std::uint32_t referenceAgeMillis{};
    std::uint32_t substitutedPermille{};
    std::uint32_t movementResets{};
};

// Product scheduler/host. Network I/O only exchanges values through bounded
// ingress/results and one replaceable snapshot; it never drives world ticks.
// The host owns its simulation role: a GYO::Threads role thread that steps at
// absolute tick deadlines on the role's Waiter.
class MatchRuntimeHost final {
public:
    using ClockNow = std::function<std::chrono::steady_clock::time_point()>;
    explicit MatchRuntimeHost(Arena arena, ClockNow now = std::chrono::steady_clock::now);
    MatchRuntimeHost(const MatchRuntimeHost&) = delete;
    MatchRuntimeHost& operator=(const MatchRuntimeHost&) = delete;
    ~MatchRuntimeHost();

    // Starts the simulation role (gyo-match-sim). Returns false with an error
    // when the thread cannot start. Stop joins it; both run on the owner's
    // thread, never concurrently with other calls.
    [[nodiscard]] bool Start(std::string& error);
    void Stop();
    // The role's Runtime Error, if a step threw; the role then stopped stepping.
    [[nodiscard]] std::optional<std::string> Error() const;
    // Called outside the host's lock, by whoever runs Advance, after a step that
    // published a snapshot, control results or evictions, or completed a reset.
    // Set before Start. It must not block: the I/O layer only posts a wake-up.
    void SetPublishListener(std::function<void()> listener);
    [[nodiscard]] bool QueueJoin(std::uint64_t requestId, PlayerId playerId);
    [[nodiscard]] bool QueueLeave(std::uint64_t requestId, PlayerId playerId);
    [[nodiscard]] bool SubmitInput(const PlayerInput& input);
    // I/O validates and stages immutable requests; only Advance mutates Match.
    [[nodiscard]] ActionAdmission SubmitActions(const ActionBatch& batch);
    // Requests (Shot/Reload) and ACK validate together; rejection queues neither.
    [[nodiscard]] ActionAdmission SubmitActionBatch(const ActionBatch& batch, ActionId acknowledgedThrough);
    [[nodiscard]] bool QueueActionAcknowledgement(PlayerId playerId, ActionId through);
    // Owning, non-destructive results survive reads until a validated ACK retires them.
    [[nodiscard]] std::optional<ActionResults> GetActionResults(PlayerId playerId) const;

    [[nodiscard]] Engine::Runtime::FixedTickAdvance Advance(double elapsedSeconds);
    [[nodiscard]] std::optional<WorldSnapshot> TakeSnapshot();
    [[nodiscard]] std::vector<ControlResult> TakeControlResults();
    // Players already removed from the match; the I/O layer tells the Gateway.
    [[nodiscard]] std::vector<Eviction> TakeEvictions();
    // Diagnostics only, since the previous call (or the start): how the
    // simulation role's waits for the next tick ended, and how many published
    // snapshots were replaced before the I/O layer took them. Resets the window.
    struct Statistics final {
        TickWakeStatistics ticks;
        std::uint64_t snapshotOverwrites{};
    };
    [[nodiscard]] Statistics TakeStatistics();
    // Diagnostics only, since the previous call (or the start): what became
    // of every input and action batch the host received, per player. Inputs
    // and batches of a player the Match does not hold count under player 0.
    // Counts survive Leave, eviction and resets until taken. The result holds
    // player 0 and every current player, even with nothing counted, and any
    // other player with counts. Resets the window. final (the process's last
    // window, host stopped) first closes every open substitution record as end
    // and counts the slack samples still pending or unclaimed as such. Each
    // call also starts a new window for the detail file's rejection bound.
    [[nodiscard]] std::map<PlayerId, MatchIngressCounts> TakeIngressStatistics(bool final = false);
    // Diagnostics only, since the previous call: the bounded records of
    // match-ingress.jsonl (MatchIngressTrace.hpp). Rejections keep the first
    // MatchIngressRejectionsPerWindow per (player, reason) and statistics
    // window, substitutions are added as their records close; the rest is
    // counted as suppressed or dropped. Survives Leave and resets until taken.
    [[nodiscard]] IngressRecords DrainIngressRecords();
    // The I/O layer replaced a taken snapshot (latest wins) before writing
    // it; these players' slack samples in it never reach the Gateway.
    void NoteCoalescedSlackSamples(std::span<const PlayerId> players);
    // An action batch the I/O layer refused before it could become a request
    // (OverBatch or Malformed): counted like the host's own refusals.
    void NoteWireRejection(PlayerId playerId, IngressActionRejection reason, std::size_t shots);

    // IPC must wait for completion before accepting a replacement connection.
    // The future is completed by the next step (the role is woken at once),
    // which never waits for I/O; the publish listener then fires.
    [[nodiscard]] std::future<void> RequestReset();
    // Only for a stopped host or deterministic tests.
    void Reset();

private:
    struct Control final {
        std::uint64_t requestId{};
        PlayerId playerId{};
        ControlKind kind{ControlKind::Join};
    };
    struct PendingInput final {
        std::uint64_t movementEpoch{};
        std::uint64_t lifeGeneration{};
        std::map<std::uint64_t, MovementCommand> commands;
        // Only these commands have not crossed into Match; retained duplicates
        // are excluded from Host lifecycle-cancellation counts.
        std::set<std::uint64_t> stagedSequences;
        bool dirty{};
    };
    // Movement slack of one player's current epoch/life: first receipt of
    // each unresolved sequence, resolution time of recently substituted ones
    // (a late arrival reports how late it was) and the smallest sample since
    // the last publication. Published with snapshots; never fed to Match.
    struct SlackTrack final {
        std::uint64_t movementEpoch{};
        std::uint64_t lifeGeneration{};
        std::uint64_t lastResolved{};
        std::map<std::uint64_t, std::chrono::steady_clock::time_point> receipts;
        std::map<std::uint64_t, std::chrono::steady_clock::time_point> substituted;
        std::optional<std::pair<std::uint64_t, std::int64_t>> pending;
    };
    // Connection-quality window of one player: its start tick and movement
    // counters, the input reference ages seen in it and the failed windows in
    // a row. The first window after a join is not judged.
    struct QualityTrack final {
        std::uint64_t windowStartTick{};
        MovementQuality windowStart{};
        std::vector<std::uint32_t> referenceAgesMicros;
        bool judged{};
        std::uint32_t failures{};
    };
    struct PublishedReference final {
        std::uint64_t tick{};
        std::chrono::steady_clock::time_point publishedAt;
    };
    bool QueueControl(Control control);
    // The counts an input or batch of this player falls under (player 0 when
    // the Match does not hold it). Under the lock.
    [[nodiscard]] MatchIngressCounts& IngressBucket(PlayerId playerId);
    [[nodiscard]] PlayerId IngressBucketId(PlayerId playerId) const;
    // Keeps a detail record of a refused input if the buffer admits it. Under the lock.
    void RecordRejection(const PlayerInput& input, IngressInputRejection reason);
    // A slack sample candidate competes for the next publication; one that
    // loses (or is replaced) is merged. Under the lock.
    void OfferSlackSample(PlayerId playerId, SlackTrack& track, std::uint64_t sequence, std::int64_t micros);
    // A track's pending candidate dropped before any publication. Under the lock.
    void DiscardSlack(PlayerId playerId, const SlackTrack& track);
    // Adds one to field for every player whose slack sample this snapshot carries. Under the lock.
    void CountSlackSamples(const WorldSnapshot& snapshot, std::uint64_t MatchIngressCounts::*field);
    // Counts a substitution record the ledger closed. Under the lock.
    void SubstitutionClosed(const IngressSubstitution& record);
    // Advance's work under the lock; published tells Advance to notify.
    [[nodiscard]] Engine::Runtime::FixedTickAdvance Step(double elapsedSeconds, bool& published);
    void Body(Engine::Threads::RoleContext& context);
    void ClearState();
    // resolvedThrough: the player's resolved cursor for its staged epoch and
    // life; staged commands at or below it were executed, not discarded.
    void RemovePlayerState(PlayerId playerId, std::uint64_t resolvedThrough);
    // `at` is this Advance's single clock reading, taken on first use.
    void TrackSlack(WorldSnapshot& state, std::optional<std::chrono::steady_clock::time_point>& at);
    void JudgeConnectionQuality(WorldSnapshot& state);

    mutable std::mutex mutex_;
    PvpMatch match_;
    ClockNow now_;
    Engine::Runtime::FixedTickRuntime ticker_{AuthorityTickRate};
    std::deque<Control> controls_;
    std::map<PlayerId, PendingInput> pendingInputs_;
    std::map<PlayerId, std::vector<ShotRequest>> pendingActions_;
    std::map<PlayerId, ActionId> pendingActionAcknowledgements_;
    std::map<PlayerId, SlackTrack> slack_;
    std::map<PlayerId, QualityTrack> quality_;
    std::vector<Eviction> evictions_;
    std::deque<PublishedReference> publishedReferences_;
    std::vector<ControlResult> results_;
    std::optional<WorldSnapshot> snapshot_;
    std::optional<std::promise<void>> pendingReset_;
    Statistics statistics_;
    // Not part of the match state: ClearState and RemovePlayerState keep it.
    std::map<PlayerId, MatchIngressCounts> ingress_;
    // Substitution records; ClearState and RemovePlayerState close them.
    // Only observes: slack samples, quality and the Match never read it.
    MatchIngressLedger ledger_;
    // match-ingress.jsonl records waiting for DrainIngressRecords.
    MatchIngressRecordBuffer records_;
    std::optional<std::string> error_;
    std::function<void()> publishListener_;
    std::atomic<bool> running_{};
    // Last: destroyed first, so the role stops before the state it uses.
    std::optional<Engine::Threads::RoleThread> role_;
};

} // namespace fps::pvp
