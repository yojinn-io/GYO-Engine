#pragma once

#include "engine/net/Link.hpp"
#include "engine/os/DatagramSocket.hpp"

// The UDP Link: a thin bridge over OS::DatagramSocket, the only place that touches a socket (declaration D-1;
// engine/core/os is the adapter of the OS's sockets). It decides which OS failures mean what to the net
// Module (os.md Design Invariant 7 leaves that to the caller): WouldBlock on send is LinkBusy, TooLarge on
// receive is DatagramTooLarge, Unreachable stays Unreachable, the rest is LinkFailed with the cause.

namespace GYO::Engine::Net {

class UdpLink final : public Link {
public:
    // Binds a non-blocking UDP socket to local (port 0: the OS picks one).
    [[nodiscard]] static NetResult<UdpLink> Open(const OS::SocketAddress& local);

    // The Endpoint of an IPv4 address and port: the four octets and the port packed into the low 48 bits.
    [[nodiscard]] static constexpr Endpoint ToEndpoint(const OS::SocketAddress& address) noexcept {
        return Endpoint{(std::uint64_t{address.octets[0]} << 40) | (std::uint64_t{address.octets[1]} << 32) |
                        (std::uint64_t{address.octets[2]} << 24) | (std::uint64_t{address.octets[3]} << 16) |
                        std::uint64_t{address.port}};
    }
    [[nodiscard]] static constexpr OS::SocketAddress ToAddress(const Endpoint endpoint) noexcept {
        return OS::SocketAddress{{static_cast<std::uint8_t>(endpoint.value >> 40),
                                  static_cast<std::uint8_t>(endpoint.value >> 32),
                                  static_cast<std::uint8_t>(endpoint.value >> 24),
                                  static_cast<std::uint8_t>(endpoint.value >> 16)},
                                 static_cast<std::uint16_t>(endpoint.value)};
    }

    // The address the socket is bound to, with the port the OS picked.
    [[nodiscard]] NetResult<OS::SocketAddress> LocalAddress() const;

    [[nodiscard]] NetResult<void> Send(Endpoint to, std::span<const std::byte> datagram) override;
    [[nodiscard]] NetResult<std::optional<ReceivedDatagram>> Receive(std::span<std::byte> buffer) override;

private:
    explicit UdpLink(OS::DatagramSocket socket) noexcept;
    OS::DatagramSocket socket_;
};

} // namespace GYO::Engine::Net
