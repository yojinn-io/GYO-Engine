#pragma once

#include "RetroFPS/Pvp/PvpMatch.hpp"
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
    void SendInput(PlayerInput input);
    [[nodiscard]] ClientConnectionState State() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
