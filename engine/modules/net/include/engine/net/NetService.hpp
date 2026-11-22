#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
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
    // A client over UDP: the server's address.
    OS::DatagramAddress server{};
    // Instead of UDP: a Link the product or a test gives (a MemoryLink end). A client then names the server's
    // Endpoint on that Link.
    std::shared_ptr<Link> link;
    Endpoint serverEndpoint{};
    // A client's opaque user id and token (carried, not checked).
    std::uint64_t userId = 0;
    std::vector<std::byte> token;
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

class NetService final {
public:
    NetService(NetProductOptions options, std::shared_ptr<Link> link, Endpoint serverEndpoint, std::uint64_t secret,
               std::uint64_t idSeed);

    [[nodiscard]] NetRole Role() const noexcept { return options_.role; }
    [[nodiscard]] NetTime Now() const;

    // The World's facts (its prepare function with newWorld, the send System every frame without). The first call
    // makes the server host or the client session. A client whose session ended gets a new one only for a new
    // World (D-8: a reconnect is a new connection in a new client World, the product's call).
    void SetFacts(const HandshakeFacts& facts, NetTime now, bool newWorld);
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
    // The local address of the UDP socket the Module opened, or nullopt (a Link the product gave).
    [[nodiscard]] std::optional<OS::DatagramAddress> UdpAddress() const;

    // What the Link reported (E2: kept here and counted, the World goes on), and the service's own drops (a
    // client's datagram from another endpoint: UnknownSource).
    [[nodiscard]] std::uint64_t LinkFailures() const noexcept { return linkFailures_; }
    [[nodiscard]] const std::optional<NetFailure>& LastLinkFailure() const noexcept { return lastLinkFailure_; }
    [[nodiscard]] const std::array<std::uint64_t, kDropReasonCount>& Drops() const noexcept { return drops_; }

    // The one World of the Product with a net role (declaration D-3). false when another World holds it.
    [[nodiscard]] bool Bind(const std::string& world);

private:
    void Failed(NetFailure failure);

    NetProductOptions options_;
    std::shared_ptr<Link> link_;
    Endpoint serverEndpoint_;
    std::uint64_t secret_;
    std::uint64_t idSeed_;
    std::optional<ServerHost> server_;
    std::optional<ClientSession> client_;
    std::optional<std::string> boundWorld_;
    std::uint64_t linkFailures_ = 0;
    std::optional<NetFailure> lastLinkFailure_;
    std::array<std::uint64_t, kDropReasonCount> drops_{};
};

} // namespace GYO::Engine::Net
