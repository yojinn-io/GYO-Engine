#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "engine/net/Connection.hpp"
#include "engine/net/Handshake.hpp"

// The server end (declaration D-6, D-8): answers handshakes and keeps one Connection per accepted client. A
// pure state machine like ClientSession: datagrams with their source in, datagrams with their destination out,
// the time given by the caller.
//
// The challenge is stateless: it is a keyed hash of the request and its source (Detail::ChallengeOf), so the
// server keeps nothing for a handshake until a response carries the right challenge. Only then does it compare
// (CompareFacts), ask the product (Admission) and make the connection.

namespace GYO::Engine::Net {

enum class HostEventKind : std::uint8_t { Connected = 1, Disconnected };

struct HostEvent final {
    HostEventKind kind = HostEventKind::Connected;
    std::uint32_t connectionId = 0;
    std::uint64_t userId = 0;
    Endpoint endpoint;
    // Disconnected only.
    DisconnectReason reason = DisconnectReason::Closed;
};

class ServerHost final {
public:
    // The product's answer before a client is accepted: an Err refuses it (Refused, with the failure's message
    // as the detail).
    using Admission = std::function<NetResult<void>(const ConnectRequest& request, Endpoint from)>;

    // secret keys the challenges; idSeed draws the connection ids (random, non-zero, unique among the open ones).
    ServerHost(SessionSettings settings, HandshakeFacts facts, std::uint64_t secret, std::uint64_t idSeed,
               Admission admission = {});

    // The server World's facts (they may change before a client connects: a restored allotter range).
    void SetFacts(HandshakeFacts facts) { facts_ = std::move(facts); }
    [[nodiscard]] const HandshakeFacts& Facts() const noexcept { return facts_; }

    void Receive(Endpoint from, std::span<const std::byte> datagram, NetTime now);
    // Timeouts at now (the receive side calls it every frame).
    void Update(NetTime now);
    // Timeouts, then what to send: handshake answers, each connection's datagrams, keepalives.
    [[nodiscard]] std::vector<std::pair<Endpoint, std::vector<std::byte>>> Collect(NetTime now, Stamp stamp);
    // Closes one connection: its Disconnect packets go out on the next Collect, and it ends with Closed.
    void Close(std::uint32_t connectionId);

    // Connected and Disconnected, in the order they happened; taking empties the list.
    [[nodiscard]] std::vector<HostEvent> TakeEvents();

    [[nodiscard]] std::size_t ConnectionCount() const noexcept { return peers_.size(); }
    [[nodiscard]] std::vector<std::uint32_t> ConnectionIds() const;
    [[nodiscard]] Connection* Find(std::uint32_t connectionId) noexcept;
    [[nodiscard]] const std::array<std::uint64_t, kDropReasonCount>& Drops() const noexcept { return drops_; }

private:
    struct Peer final {
        Endpoint endpoint;
        std::uint64_t userId = 0;
        Connection connection;
        NetTime lastReceived{0};
        NetTime lastSent{0};
        bool closing = false;
    };
    struct Reply final {
        Endpoint to;
        PacketKind kind = PacketKind::Challenge;
        std::uint32_t connectionId = 0;
        std::vector<std::byte> payload;
    };

    void Respond(Endpoint from, std::span<const std::byte> body, NetTime now);
    void Reject(Endpoint to, Rejection rejection);
    void End(std::map<std::uint32_t, Peer>::iterator peer, DisconnectReason reason);
    [[nodiscard]] std::uint32_t NewConnectionId();
    void Count(DropReason reason) noexcept { ++drops_[static_cast<std::size_t>(reason)]; }

    SessionSettings settings_;
    HandshakeFacts facts_;
    std::uint64_t secret_;
    std::uint64_t idState_;
    Admission admission_;
    std::map<std::uint32_t, Peer> peers_;
    std::map<std::uint64_t, std::uint32_t> byEndpoint_;
    std::deque<Reply> replies_;
    std::vector<HostEvent> events_;
    std::array<std::uint64_t, kDropReasonCount> drops_{};
};

} // namespace GYO::Engine::Net
