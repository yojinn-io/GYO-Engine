#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "engine/net/ClientSession.hpp"
#include "engine/net/Handshake.hpp"
#include "engine/net/Link.hpp"
#include "engine/net/ServerHost.hpp"
#include "engine/os/DatagramSocket.hpp"

// The Product's network object (declaration D-3, the transport object; orchestration O-15): the Link and the
// connection layer, one per Product, in the Product's buffer. NetModule makes it in Configure when the product
// description carries NetProductOptions; without them nothing is made and no socket is opened (G-1). The World's
// receive and send Systems reach it through a Resource NetModule puts there when the World is prepared.

namespace GYO::Engine::Net {

enum class NetRole : std::uint8_t { Server = 1, Client };

// ProductDescription::options.
struct NetProductOptions final {
    NetRole role = NetRole::Server;
    // UDP: the local address to bind (a server's listening port; a client usually any address, port 0).
    OS::DatagramAddress bind = OS::DatagramAddress::Any(0);
    // Instead of UDP: a Link the product or a test gives (a MemoryLink end).
    std::shared_ptr<Link> link;
    // Whom a client connects to is not here: each client World says it (NetConnectTarget, net page D-8).
    // The thresholds and the transport's channels (both ends list the same channels).
    SessionSettings session;
    // A server's product decides before a client is accepted (an Err: Refused).
    ServerHost::Admission admission;
    // The time of the IO layer (liveness and RTT only, never decision data, D-9); none: core/time's monotonic
    // clock.
    std::function<NetTime()> clock;
    // Seeds a server's challenge secret and connection ids; none: std::random_device.
    std::optional<std::uint64_t> seed;
};

// The connection a client World makes (net page D-8, net-connect-target declaration): a Resource of the World, put
// there by a prepare function (WorldDescription::prepare, a unit's) or a replacement's extra (ReplaceRequest::extra).
// The net Module reads it at the World's first frame: a client World without one connects to nothing; a
// new World with the same target as the live connection keeps it (and asks for a resync, R2); another target closes
// the old connection and opens a new one. Like the World's other facts, it is not decided state.
struct NetConnectTarget final {
    // Over UDP: the server's address. Over a Link the product gave: the server's Endpoint on it. The other kind, or
    // none, connects to nothing (a wrong kind is counted as a link failure: NetFailureCode::LinkFailed).
    std::variant<std::monostate, OS::DatagramAddress, Endpoint> server;
    // The client's opaque user id and token (carried, not checked).
    std::uint64_t userId = 0;
    std::vector<std::byte> token;
};

class NetService final {
public:
    NetService(NetProductOptions options, std::shared_ptr<Link> link, std::uint64_t secret, std::uint64_t idSeed);

    [[nodiscard]] NetRole Role() const noexcept { return options_.role; }
    [[nodiscard]] NetTime Now() const;

    // The World's facts (a server World's prepare function with newWorld, the send System every frame without). The
    // first call makes the server host; a client's makes its session once it has a target (SetTarget) and none.
    void SetFacts(const HandshakeFacts& facts, NetTime now, bool newWorld);
    // A client World's connection, at its first frame (D-8): none (null, or a server of the other transport's kind)
    // closes a live session and connects to nothing; the target of the live session keeps it; another closes it
    // (its last datagrams go out now) and the next SetFacts opens a new one; a session that ended is dropped, so the
    // next SetFacts opens a new one (a reconnect is a new client World).
    // epoch: the World's (its NetReplicaState::epoch); the new connection's request carries it (R2).
    void SetTarget(const NetConnectTarget* target, NetTime now, std::uint32_t epoch);
    // Takes every waiting datagram into the connection layer, then the liveness at now.
    void Pump(NetTime now);
    // Sends what the connection layer has for now.
    void Flush(NetTime now, Stamp stamp);

    // The product's handle: a client closes its connection, a server every connection.
    void Close();

    [[nodiscard]] ServerHost* Server() noexcept { return server_ ? &*server_ : nullptr; }
    [[nodiscard]] ClientSession* Client() noexcept { return client_ ? &*client_ : nullptr; }
    [[nodiscard]] const ClientSession* Client() const noexcept { return client_ ? &*client_ : nullptr; }
    [[nodiscard]] const NetProductOptions& Options() const noexcept { return options_; }
    // The local address of the UDP socket the Module opened, or nullopt (a Link the product gave, or a client whose
    // socket is not open yet: it opens with the first NetConnectTarget).
    [[nodiscard]] std::optional<OS::DatagramAddress> UdpAddress() const;

    // What the Link reported (E2: kept here and counted, the World goes on), and the service's own drops (a
    // client's datagram from another endpoint: UnknownSource).
    [[nodiscard]] std::uint64_t LinkFailures() const noexcept { return linkFailures_; }
    [[nodiscard]] const std::optional<NetFailure>& LastLinkFailure() const noexcept { return lastLinkFailure_; }
    [[nodiscard]] const std::array<std::uint64_t, kDropReasonCount>& Drops() const noexcept { return drops_; }

    // A server World was built (startup or replacement): the World generation goes up (D-5). The current one.
    std::uint32_t BeginServerWorld() noexcept { return ++generation_; }
    [[nodiscard]] std::uint32_t Generation() const noexcept { return generation_; }
    // A client World was built: how many so far (a second one asks the server for everything again).
    std::uint32_t BeginClientWorld() noexcept { return ++clientWorlds_; }

    // The one World of the Product with a net role (declaration D-3). false when another World holds it.
    [[nodiscard]] bool Bind(const std::string& world);

private:
    void Failed(NetFailure failure);
    // Closes the client session and sends its last datagrams now (to the endpoint it was made for).
    void EndClient(NetTime now);

    struct Target {
        Endpoint endpoint;
        std::uint64_t userId = 0;
        std::vector<std::byte> token;
        friend bool operator==(const Target&, const Target&) = default;
    };

    NetProductOptions options_;
    std::shared_ptr<Link> link_;
    std::optional<Target> target_;
    std::uint32_t targetEpoch_ = 0;
    std::uint64_t secret_;
    std::uint64_t idSeed_;
    std::optional<ServerHost> server_;
    std::optional<ClientSession> client_;
    std::optional<std::string> boundWorld_;
    std::uint32_t generation_ = 0;
    std::uint32_t clientWorlds_ = 0;
    std::uint64_t lastSalt_ = 0;
    std::uint64_t linkFailures_ = 0;
    std::optional<NetFailure> lastLinkFailure_;
    std::array<std::uint64_t, kDropReasonCount> drops_{};
};

} // namespace GYO::Engine::Net
