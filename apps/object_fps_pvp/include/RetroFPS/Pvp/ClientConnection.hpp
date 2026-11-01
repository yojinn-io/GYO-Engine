#pragma once

#include "RetroFPS/Pvp/PvpMatch.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace fps::pvp {
enum class ConnectionPhase { Lobby, Requesting, Connecting, Playing };
struct LobbyRoom { std::string id; unsigned players{}; unsigned capacity{MaxPlayers}; };
// A copied, read-only view of the worker-owned bounded action transport.
struct ActionTransportState {
    std::size_t pending{}, retained{}, unconsumed{};
    ActionId allocatedThrough{}, acknowledgedThrough{}, retiredThrough{};
    std::uint64_t sentBatches{}, sentShots{}, sentAckOnlyBatches{}, rejectedResultBatches{};
    std::size_t maxPayloadBytes{}, maxDatagramBytes{}, maxBatchShots{};
};
// An arena this Client has installed (pv6 contract §2): id, format version and
// ArenaContentDigest.
struct ArenaIdentity {
    std::string id;
    std::uint32_t version{};
    std::uint64_t digest{};
};
struct ClientConnectionState {
    ConnectionPhase phase{ConnectionPhase::Lobby};
    // The installed arena the joined Match hosts; empty outside a session.
    std::string arenaId;
    std::vector<LobbyRoom> rooms;
    std::string error;
    PlayerId playerId{};
    std::optional<WorldSnapshot> snapshot;
    std::optional<CombatRules> combatRules;
    std::optional<MovementRules> movementRules;
    ActionTransportState actionTransport;
    // The connection generation this state belongs to; each session request starts a new one.
    std::uint64_t generation{};
    // Playing without a valid snapshot for ConnectionInterruptedAfter (the
    // Client's own observation, network-architecture §4): the session, its
    // window and its actions stay; interruptedSince is when the last valid
    // snapshot arrived. Display only: nothing that decides reads it.
    bool interrupted{};
    std::chrono::steady_clock::time_point interruptedSince{};
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
    // Diagnostics: when the network worker received each decision (same order
    // as decisions), unquantized by the frame that drains it.
    std::vector<std::chrono::steady_clock::time_point> decisionReceivedAt;
};

// The client simulation role's own receipt-stamped history, independent of
// Drain: a stalled main thread does not hold back the simulation.
struct ClientSimulationDrain {
    std::vector<ReceivedSnapshot> snapshots;
    std::uint64_t generation{};
    PlayerId playerId{};
    // The installed arena the joined Match hosts; empty outside a session.
    std::string arenaId;
    std::optional<MovementRules> movementRules;
    // Snapshots dropped from this history because it was full, cumulative within a generation.
    std::uint64_t overflowCount{};
    // One-shot: a snapshot of this batch ended an interruption (network-architecture
    // §4, recovery step 3). Set with that snapshot under one lock, so no batch
    // holds the snapshot without it; a connection generation boundary clears it.
    bool resumed{};
};

// The connection lines of the network worker (log-contract §1.1), without the
// "[ObjectFPS/PvP] " prefix the worker writes them with. Gaps are whole
// milliseconds since the last valid snapshot.
[[nodiscard]] std::string ConnectionInterruptedLine(PlayerId player, std::chrono::steady_clock::duration gap,
                                                    std::uint64_t epoch);
[[nodiscard]] std::string ConnectionResumedLine(PlayerId player, std::chrono::steady_clock::duration gap,
                                                std::uint64_t epochBefore, std::uint64_t epochAfter);
// The socket errors of a network that may come back (network-architecture §4,
// D172 D-7: connection reset or refused, network or host unreachable, network
// down): the worker counts them as no data this time, never a Failure.
[[nodiscard]] bool TransientSocketError(const std::error_code& error) noexcept;
// What the network worker does with the result of one send or receive:
// Proceed without an error; Blocked when the socket would block; Tolerate a
// transient error while playing (connected or interrupted: count it, no data
// this time); Fail otherwise, so during the handshake every socket error,
// transient or not, fails the join at once.
enum class SocketErrorAction { Proceed, Blocked, Tolerate, Fail };
[[nodiscard]] SocketErrorAction SocketErrorActionFor(const std::error_code& error, bool playing) noexcept;
// One joined connection, written when its transport closes.
struct ConnectionSummary final {
    PlayerId player{};
    std::uint64_t interruptions{};
    // The longest interval between two valid snapshots.
    std::chrono::steady_clock::duration maxGap{};
    std::uint64_t transientSocketErrors{};
};
[[nodiscard]] std::string ConnectionSummaryLine(const ConnectionSummary& summary);

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
    // The arena this Client loaded (pv6 contract §2): id, format version and
    // ArenaContentDigest. A zero digest is invalid and throws.
    void SetArenaIdentity(std::string id, std::uint32_t version, std::uint64_t digest);
    // Every installed arena. A join selects the one with the Match's id and
    // version, then compares its content; none installed is an identity
    // mismatch. Ids repeat or a zero digest throws.
    void SetArenaIdentities(std::vector<ArenaIdentity> installed);
    // AnimationContentDigest of the animation content this Client poses with
    // (protocol-v9 §4.6): a Welcome with another digest fails the join. Unset,
    // nothing is compared. Zero is invalid and throws.
    void SetAnimationDigest(std::uint64_t digest);
    // Repeated submissions contain the entire immutable unacknowledged window.
    // The worker coalesces complete windows; it never keeps only their last step.
    void SendInput(PlayerInput input);
    // The same, dropped when the connection generation is no longer the one the
    // window's authority came from (a caller on another thread than the session
    // changes, such as the simulation role).
    void SendInput(PlayerInput input, std::uint64_t generation);
    // Assigns a contiguous immutable ID only when the active player's bounded
    // window has room. Retries, decisions and ACKs stay alive across movement
    // epochs and stalled game frames. Nullopt never allocates/skips an ID.
    [[nodiscard]] std::optional<ActionId> SubmitShot(std::uint64_t observedAuthorityTick, float yaw, float pitch,
        const ShotContext& context = {});
    // context: E, S and the view tick of the request (pv7 §6, §7), carried as
    // they are; the Match only records them in v7.1.
    [[nodiscard]] std::optional<ActionId> SubmitAction(ActionKind kind, std::uint64_t lifeGeneration,
        std::uint64_t observedAuthorityTick, float yaw = 0, float pitch = 0, const ShotContext& context = {});
    [[nodiscard]] ClientConnectionState State() const;
    // Atomically consumes receipt-stamped history with its matching state and
    // lifecycle generation. Overflow count is cumulative within this generation.
    // Decisions transfer exactly once here, independently of snapshot overflow;
    // only transferred contiguous decisions become eligible for acknowledgement.
    [[nodiscard]] ClientConnectionDrain Drain();
    // Consumes the simulation role's own history. Snapshots, actions and
    // acknowledgements seen by Drain are unaffected. The history is kept only
    // after the first call, so a connection without a role copies nothing.
    [[nodiscard]] ClientSimulationDrain DrainSimulation();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
