#pragma once

#include "RetroFPS/Pvp/PvpMatch.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <optional>
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

// Product scheduler/host. Network I/O only exchanges values through bounded
// ingress/results and one replaceable snapshot; it never drives world ticks.
class MatchRuntimeHost final {
public:
    explicit MatchRuntimeHost(Arena arena);
    [[nodiscard]] bool QueueJoin(std::uint64_t requestId, PlayerId playerId);
    [[nodiscard]] bool QueueLeave(std::uint64_t requestId, PlayerId playerId);
    [[nodiscard]] bool SubmitInput(const PlayerInput& input);

    [[nodiscard]] Engine::Runtime::FixedTickAdvance Advance(double elapsedSeconds);
    void Run(std::stop_token stop);
    [[nodiscard]] std::optional<WorldSnapshot> TakeSnapshot();
    [[nodiscard]] std::vector<ControlResult> TakeControlResults();

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
        std::map<std::uint64_t, MovementCommand> commands;
        bool dirty{};
    };
    bool QueueControl(Control control);
    void ClearState();

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    PvpMatch match_;
    Engine::Runtime::FixedTickRuntime ticker_{AuthorityTickRate};
    std::deque<Control> controls_;
    std::map<PlayerId, PendingInput> pendingInputs_;
    std::vector<ControlResult> results_;
    std::optional<WorldSnapshot> snapshot_;
    std::optional<std::promise<void>> pendingReset_;
    std::atomic<bool> running_{};
};

} // namespace fps::pvp
