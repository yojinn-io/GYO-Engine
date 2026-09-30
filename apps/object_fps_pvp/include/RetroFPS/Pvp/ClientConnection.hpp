#pragma once

#include "RetroFPS/Pvp/PvpMatch.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fps::pvp {
enum class ConnectionPhase { Lobby, Requesting, Connecting, Playing };
struct LobbyRoom { std::string id; unsigned players{}; unsigned capacity{2}; };
// A copied, read-only view of the worker-owned bounded action transport.
struct ActionTransportState {
    std::size_t pending{}, retained{}, unconsumed{};
    ActionId allocatedThrough{}, acknowledgedThrough{}, retiredThrough{};
    std::uint64_t sentBatches{}, sentShots{}, sentAckOnlyBatches{}, rejectedResultBatches{};
    std::size_t maxPayloadBytes{}, maxDatagramBytes{}, maxBatchShots{};
};
struct ClientConnectionState {
    ConnectionPhase phase{ConnectionPhase::Lobby};
    std::vector<LobbyRoom> rooms;
    std::string error;
    PlayerId playerId{};
    std::optional<WorldSnapshot> snapshot;
    std::optional<CombatRules> combatRules;
    std::optional<MovementRules> movementRules;
    ActionTransportState actionTransport;
};
inline constexpr std::size_t MaxReceivedSnapshots = 64;
struct ReceivedSnapshot {
    WorldSnapshot snapshot;
    std::chrono::steady_clock::time_point receivedAt;
};
struct ClientConnectionDrain {
    ClientConnectionState state;
    std::vector<ReceivedSnapshot> snapshots;
    std::uint64_t generation{};
    bool overflow{};
    std::uint64_t snapshotHistoryOverflowCount{};
    std::vector<ShotDecision> decisions;
};

// Owns background I/O. All values crossing this boundary own their storage.
class ClientConnection final {
public:
    ClientConnection();
    ~ClientConnection();
    ClientConnection(const ClientConnection&) = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;
    void Refresh(std::string gateway);
    void CreateAndJoin(std::string gateway);
    void Join(std::string gateway, std::string roomId);
    void Leave();
    void SetArenaIdentity(std::string id, std::uint32_t version);
    // Repeated submissions contain the entire immutable unacknowledged window.
    // The worker coalesces complete windows; it never keeps only their last step.
    void SendInput(PlayerInput input);
    // Assigns a contiguous immutable ID only when the active player's bounded
    // window has room. Retries, decisions and ACKs stay alive across movement
    // epochs and stalled game frames. Nullopt never allocates/skips an ID.
    [[nodiscard]] std::optional<ActionId> SubmitShot(std::uint64_t observedAuthorityTick, float yaw, float pitch);
    [[nodiscard]] std::optional<ActionId> SubmitAction(ActionKind kind, std::uint64_t lifeGeneration,
        std::uint64_t observedAuthorityTick, float yaw = 0, float pitch = 0);
    [[nodiscard]] ClientConnectionState State() const;
    // Atomically consumes receipt-stamped history with its matching state and
    // lifecycle generation. Overflow count is cumulative within this generation.
    // Decisions transfer exactly once here, independently of snapshot overflow;
    // only transferred contiguous decisions become eligible for acknowledgement.
    [[nodiscard]] ClientConnectionDrain Drain();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
