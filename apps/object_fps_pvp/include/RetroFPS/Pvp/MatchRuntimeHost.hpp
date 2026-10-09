#pragma once

#include "RetroFPS/Pvp/NetworkStatistics.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
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
class MatchRuntimeHost final {
public:
    using ClockNow = std::function<std::chrono::steady_clock::time_point()>;
    explicit MatchRuntimeHost(Arena arena, ClockNow now = std::chrono::steady_clock::now);
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
    void Run(std::stop_token stop);
    [[nodiscard]] std::optional<WorldSnapshot> TakeSnapshot();
    [[nodiscard]] std::vector<ControlResult> TakeControlResults();
    // Players already removed from the match; the I/O layer tells the Gateway.
    [[nodiscard]] std::vector<Eviction> TakeEvictions();
    // Diagnostics only: how Run's waits for the next tick ended since the
    // previous call (or the start); resets the window.
    [[nodiscard]] TickWakeStatistics TakeTickWakeStatistics();

    // IPC must wait for completion before accepting a replacement connection.
    // The future is completed by the simulation thread, which never waits I/O.
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
    void ClearState();
    void RemovePlayerState(PlayerId playerId);
    // `at` is this Advance's single clock reading, taken on first use.
    void TrackSlack(WorldSnapshot& state, std::optional<std::chrono::steady_clock::time_point>& at);
    void JudgeConnectionQuality(WorldSnapshot& state);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
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
    TickWakeStatistics tickWakes_;
    std::atomic<bool> running_{};
};

} // namespace fps::pvp
