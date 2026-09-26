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
struct ClientConnectionState {
    ConnectionPhase phase{ConnectionPhase::Lobby};
    std::vector<LobbyRoom> rooms;
    std::string error;
    PlayerId playerId{};
    std::optional<WorldSnapshot> snapshot;
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
    [[nodiscard]] ClientConnectionState State() const;
    // Atomically consumes receipt-stamped history with its matching state and
    // lifecycle generation. Overflow count is cumulative within this generation.
    [[nodiscard]] ClientConnectionDrain Drain();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
