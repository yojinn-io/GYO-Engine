#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "engine/net/Connection.hpp"
#include "engine/net/Handshake.hpp"

// The client end of one connection (declaration D-6, D-8): the handshake, liveness and the keepalive above one
// Connection. Still a pure state machine: datagrams from the server in, datagrams to it out, the time given by
// the caller; it touches no socket and reads no clock.
//
//   Connecting   requests (resent every keepaliveInterval) until a challenge, then responses until Accepted or
//                Rejected; HandshakeTimeout after handshakeTimeout.
//   Connected    the transport runs; any datagram of this connection refreshes "last received".
//   Interrupted  nothing for longer than interruptAfter (the connection is kept; anything arriving goes back to
//                Connected).
//   Disconnected nothing for longer than timeoutAfter (Timeout), a Disconnect from the server (ClosedByRemote),
//                Close (Closed), or the handshake's rejection or timeout.

namespace GYO::Engine::Net {

enum class ClientState : std::uint8_t { Connecting = 1, Connected, Interrupted, Disconnected };

class ClientSession final {
public:
    ClientSession(SessionSettings settings, ConnectRequest request, NetTime now);

    // A datagram from the server. Every drop is counted (Drops, or the Connection's own counts).
    void Receive(std::span<const std::byte> datagram, NetTime now);
    // Liveness and the handshake's timeout at now; then what to send to the server.
    [[nodiscard]] std::vector<std::vector<std::byte>> Collect(NetTime now, Stamp stamp);
    // Liveness at now without sending (the receive side calls it every frame).
    void Update(NetTime now);
    // Closes a made connection (its Disconnect packets go out on the next Collect) or gives up a handshake.
    void Close(NetTime now);

    [[nodiscard]] ClientState State() const noexcept { return state_; }
    // Why it ended, once Disconnected.
    [[nodiscard]] const std::optional<Rejection>& Ended() const noexcept { return ended_; }
    [[nodiscard]] const std::optional<ConnectAccepted>& Accepted() const noexcept { return accepted_; }
    // When the Accepted arrived.
    [[nodiscard]] NetTime AcceptedAt() const noexcept { return acceptedAt_; }
    // The transport, once Accepted.
    [[nodiscard]] Connection* Transport() noexcept { return connection_ ? &*connection_ : nullptr; }
    [[nodiscard]] const Connection* Transport() const noexcept { return connection_ ? &*connection_ : nullptr; }
    // The connection layer's own drops (UnknownSource, Unexpected, DecodeFailed of a handshake datagram).
    [[nodiscard]] const std::array<std::uint64_t, kDropReasonCount>& Drops() const noexcept { return drops_; }
    [[nodiscard]] const ConnectRequest& Request() const noexcept { return request_; }

private:
    void End(DisconnectReason reason, std::string detail);
    void Count(DropReason reason) noexcept { ++drops_[static_cast<std::size_t>(reason)]; }

    SessionSettings settings_;
    ConnectRequest request_;
    std::vector<std::byte> requestBody_;
    ClientState state_ = ClientState::Connecting;
    NetTime started_;
    std::optional<NetTime> lastHandshakeSent_;
    std::optional<std::uint64_t> challenge_;
    std::optional<ConnectAccepted> accepted_;
    std::optional<Connection> connection_;
    NetTime lastReceived_{0};
    NetTime acceptedAt_{0};
    NetTime lastSent_{0};
    std::optional<Rejection> ended_;
    std::array<std::uint64_t, kDropReasonCount> drops_{};
};

} // namespace GYO::Engine::Net
